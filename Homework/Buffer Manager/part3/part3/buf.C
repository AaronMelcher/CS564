#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include "page.h"
#include "buf.h"

#define ASSERT(c)  { if (!(c)) { \
		       cerr << "At line " << __LINE__ << ":" << endl << "  "; \
                       cerr << "This condition should hold: " #c << endl; \
                       exit(1); \
		     } \
                   }

//----------------------------------------
// Constructor of the class BufMgr
//----------------------------------------

BufMgr::BufMgr(const int bufs)
{
    numBufs = bufs;

    bufTable = new BufDesc[bufs];
    memset(bufTable, 0, bufs * sizeof(BufDesc));
    for (int i = 0; i < bufs; i++) 
    {
        bufTable[i].frameNo = i;
        bufTable[i].valid = false;
    }

    bufPool = new Page[bufs];
    memset(bufPool, 0, bufs * sizeof(Page));

    int htsize = ((((int) (bufs * 1.2))*2)/2)+1;
    hashTable = new BufHashTbl (htsize);  // allocate the buffer hash table

    clockHand = bufs - 1;
}


BufMgr::~BufMgr() {

    // flush out all unwritten pages
    for (int i = 0; i < numBufs; i++) 
    {
        BufDesc* tmpbuf = &bufTable[i];
        if (tmpbuf->valid == true && tmpbuf->dirty == true) {

#ifdef DEBUGBUF
            cout << "flushing page " << tmpbuf->pageNo
                 << " from frame " << i << endl;
#endif

            tmpbuf->file->writePage(tmpbuf->pageNo, &(bufPool[i]));
        }
    }

    delete [] bufTable;
    delete [] bufPool;
}

/**
 * Allocates a free frame using the clock algorithm; if necessary, writes a dirty page back to disk. This private method will get called by the readPage()
 * and allocPage() methods described below. Will clear frame and then return it
 * @returns OK on success, BUFFEREXCEEDED if all buffer frames are pinned, UNIXERR if the call to the I/O layer returned an error when a dirty page was being written to disk
 */
const Status BufMgr::allocBuf(int & frame) 
{
    advanceClock(); // want to advance clock before while loop starts to use the frame after the most recent one
    int framesProcessed = 0; // for tracking purposes to prevent a cycle
    while (framesProcessed < numBufs) {
        BufDesc &curFrame = bufTable[clockHand]; // grabs the current frame
        // checks refbit, if set, then remove as per rules
        if (curFrame.refbit) {  
            curFrame.refbit = false;
        // checks for unpinned frame, then replace
        } else if (curFrame.pinCnt == 0) {
            // write the dirty page back to disk
            if (curFrame.dirty == true) {
                Status writeStatus = curFrame.file->writePage(curFrame.pageNo, &(bufPool[clockHand]));
                // call to the I/O layer returned an error
                if (writeStatus != OK) {
                    return UNIXERR;
                }
            }
            // remove entry from hashtable, then clear it
            hashTable->remove(curFrame.file, curFrame.pageNo);
            curFrame.Clear();
            // return frame
            frame = clockHand;
            return OK;
        }
        // go to next frame and advances the clock
        advanceClock();
        framesProcessed++;
    }
    return BUFFEREXCEEDED; // all frames are pinned
}

/**
 * Reads the specified PageNo in the File
 * 
 */
const Status BufMgr::readPage(File* file, const int PageNo, Page*& page)
{
    // Check to see if page is in hash table
    int frameNo = -1;
    int frame = -1;
    Status inTable = hashTable->lookup(file, PageNo, frameNo);

    // Case 1: page is NOT in the buffer pool
    if(inTable != OK) { // frame is still -1
        // Allocate frame
        Status frameStatus = allocBuf(frame);
        // Check frame status
        if(frameStatus != OK) {
            return frameStatus; // Return error if unable to allocate the frame
        }

        // Are we skipping a step? call the method file->readPage() to read the page from disk into the buffer pool frame --> Krishaan
        Status readStatus = file->readPage(PageNo, &bufPool[frame]);
        if (readStatus != OK) {
            return readStatus;
        }

        // insert page into the hash table 
        Status insertStatus = hashTable->insert(file, PageNo, frame);
        if(insertStatus != OK) {
            return insertStatus;
        }

        // Init the buffer desc for the frame
        bufTable[frame].Set(file, PageNo);

        // return pointer to the page in the pool
        page = &bufPool[frame];
    }
    // Case 2: page is in the buffer pool 
    else {
        // increment pin count
        bufTable[frameNo].pinCnt++;
        // mark refbit
        bufTable[frameNo].refbit = true;
        // return ptr to the page
        page = &bufPool[frameNo];
    }

    return OK;
}


const Status BufMgr::unPinPage(File* file, const int PageNo, 
			       const bool dirty) 
{
    // lookup frame
    int frameNo = -1;
    Status inTable = hashTable->lookup(file, PageNo, frameNo);

    // if page isnt found, return error code
    if(inTable != OK) {
        return HASHNOTFOUND;
    }

    // Get buffer desc
    BufDesc &desc = bufTable[frameNo];

    // Check if currently pinned
    if(desc.pinCnt <= 0) {
        return PAGENOTPINNED;
    }

    // decrement pin cnt
    desc.pinCnt--;

    // set dirty bit to what was passed in
    desc.dirty = dirty;

    return OK;
}

/*
* The first step is to to allocate an empty page in the specified file by invoking the file->allocatePage() method. 
* This method will return the page number of the newly allocated page. Then allocBuf() is called to obtain a buffer pool frame.
* Next, an entry is inserted into the hash table and Set() is invoked on the frame to set it up properly. The method returns both the page number
* of the newly allocated page to the caller via the pageNo parameter and a pointer to the buffer frame allocated for the page via the page parameter.
* Returns OK if no errors occurred, UNIXERR if a Unix error occurred, BUFFEREXCEEDED if all buffer frames are pinned and HASHTBLERROR if a hash table error occurred.
*/
const Status BufMgr::allocPage(File* file, int& pageNo, Page*& page) 
{
    Status status = OK;
    // allocate an empty page in the specified file by invoking the file->allocatePage() method
    status = file->allocatePage(pageNo);
    if (status != OK) {
        return status;
    }
    // allocBuf() is called to obtain a buffer pool frame.
    int frame;
    status = allocBuf(frame);
    if (status != OK) {
        return status;
    }
    // an entry is inserted into the hash table and Set() is invoked on the frame to set it up properly
    // insert(const File* file, const int pageNo, const int frameNo)
    status = hashTable->insert(file, pageNo, frame);
    if (status != OK) {
        return status;  // Return a hash table error if insertion failed
    }
    bufTable[frame].Set(file, pageNo);
    page = &bufPool[frame];
    // Returns OK if no errors occurred
    return status;
}

const Status BufMgr::disposePage(File* file, const int pageNo) 
{
    // see if it is in the buffer pool
    Status status = OK;
    int frameNo = 0;
    status = hashTable->lookup(file, pageNo, frameNo);
    if (status == OK)
    {
        // clear the page
        bufTable[frameNo].Clear();
    }
    status = hashTable->remove(file, pageNo);

    // deallocate it in the file
    return file->disposePage(pageNo);
}

const Status BufMgr::flushFile(const File* file) 
{
  Status status;

  for (int i = 0; i < numBufs; i++) {
    BufDesc* tmpbuf = &(bufTable[i]);
    if (tmpbuf->valid == true && tmpbuf->file == file) {

      if (tmpbuf->pinCnt > 0)
	  return PAGEPINNED;

      if (tmpbuf->dirty == true) {
#ifdef DEBUGBUF
	cout << "flushing page " << tmpbuf->pageNo
             << " from frame " << i << endl;
#endif
	if ((status = tmpbuf->file->writePage(tmpbuf->pageNo,
					      &(bufPool[i]))) != OK)
	  return status;

	tmpbuf->dirty = false;
      }

      hashTable->remove(file,tmpbuf->pageNo);

      tmpbuf->file = NULL;
      tmpbuf->pageNo = -1;
      tmpbuf->valid = false;
    }

    else if (tmpbuf->valid == false && tmpbuf->file == file)
      return BADBUFFER;
  }
  
  return OK;
}


void BufMgr::printSelf(void) 
{
    BufDesc* tmpbuf;
  
    cout << endl << "Print buffer...\n";
    for (int i=0; i<numBufs; i++) {
        tmpbuf = &(bufTable[i]);
        cout << i << "\t" << (char*)(&bufPool[i]) 
             << "\tpinCnt: " << tmpbuf->pinCnt;
    
        if (tmpbuf->valid == true)
            cout << "\tvalid\n";
        cout << endl;
    };
}


