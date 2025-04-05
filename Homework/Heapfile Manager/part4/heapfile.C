#include "heapfile.h"
#include "error.h"

// routine to create a heapfile
const Status createHeapFile(const string fileName)
{
    File* 		file;
    Status 		status;
    FileHdrPage*	hdrPage;
    int			hdrPageNo;
    int			newPageNo;
    Page*		newPage;

    // try to open the file. This should return an error
    status = db.openFile(fileName, file);
    if (status != OK)
    {
		// file doesn't exist. First create it and allocate
		// an empty header page and data page.
        status = db.createFile(fileName);
        if(status != OK){
            return status;
        }

        // Open newly created file
        status = db.openFile(fileName, file);
        if(status != OK){
            return status;
        }

        // Allocate header page
        status = bufMgr->allocPage(file, hdrPageNo, newPage);
        if(status != OK) {
            return status;
        }

        // Cast header page to proper structure
        hdrPage = (FileHdrPage*) newPage;

        // Init header page
        strncpy(hdrPage->fileName, fileName.c_str(), MAXNAMESIZE);
        hdrPage->fileName[MAXNAMESIZE-1] = '\0';

        // Allocate first data page
        status = bufMgr->allocPage(file, newPageNo, newPage);
        if(status != OK){
            bufMgr->unPinPage(file, hdrPageNo, true);
            db.closeFile(file);
            return status;
        }
        newPage->init(newPageNo);

        // Continue setting up header page
        hdrPage->firstPage = newPageNo;
        hdrPage->lastPage = newPageNo;
        hdrPage->pageCnt = 2;
        hdrPage -> recCnt = 0;

        // Keeping in for now, may not need
        bufMgr->unPinPage(file, hdrPageNo, true);
        bufMgr->unPinPage(file, newPageNo, true);

        return OK;
    }
    return FILEEXISTS;
}

// routine to destroy a heapfile
const Status destroyHeapFile(const string fileName)
{
	return (db.destroyFile (fileName));
}

// constructor opens the underlying file

/**
 * This method first opens the appropriate file by calling db.openFile() Next, it reads and pins the header page for the file in the buffer pool, 
 * initializing the private data members headerPage, headerPageNo, and hdrDirtyFlag. Finally, it reads and pins the first page of the file into the buffer pool,
 * initializing the values of curPage, curPageNo, and curDirtyFlag appropriately and sets curRec to NULLRID
 */
HeapFile::HeapFile(const string & fileName, Status& returnStatus)
{
    Status 	status;
    Page*	pagePtr;

    cout << "opening file " << fileName << endl;

    // open the file and read in the header page and the first data page
    if ((status = db.openFile(fileName, filePtr)) == OK)
    {
        // Grabs the header Page Number
		if ((status = filePtr->getFirstPage(headerPageNo)) == OK)
        {
            // next step - read page
            if ((status = bufMgr->readPage(filePtr, headerPageNo, pagePtr)) == OK)
            {
                // pin page? -- want to recheck step
                headerPage = (FileHdrPage*) pagePtr;
                hdrDirtyFlag = false; // false since it has not been updated
                // read and pin the first page of the file into the buffer pool
                if ((status = bufMgr->readPage(filePtr, headerPage->firstPage, pagePtr)) == OK)
                {
                    // initialization
                    curPage = pagePtr;
                    curPageNo = headerPage->firstPage;
                    curDirtyFlag = false;
                    curRec = NULLRID;
                    // return
                    returnStatus = status;
                    return;
                }
                else
                {
                    returnStatus = status;
                    return;
                }
            }
            else 
            {
                returnStatus = status;
                return;
            }
        }
        else 
        {
            returnStatus = status;
            return;
        }
    }
    else
    {
    	cerr << "open of heap file failed\n";
		returnStatus = status;
		return;
    }
}

// the destructor closes the file
HeapFile::~HeapFile()
{
    Status status;
    cout << "invoking heapfile destructor on file " << headerPage->fileName << endl;

    // see if there is a pinned data page. If so, unpin it 
    if (curPage != NULL)
    {
    	status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
		curPage = NULL;
		curPageNo = 0;
		curDirtyFlag = false;
		if (status != OK) cerr << "error in unpin of date page\n";
    }
	
	 // unpin the header page
    status = bufMgr->unPinPage(filePtr, headerPageNo, hdrDirtyFlag);
    if (status != OK) cerr << "error in unpin of header page\n";
	
	// status = bufMgr->flushFile(filePtr);  // make sure all pages of the file are flushed to disk
	// if (status != OK) cerr << "error in flushFile call\n";
	// before close the file
	status = db.closeFile(filePtr);
    if (status != OK)
    {
		cerr << "error in closefile call\n";
		Error e;
		e.print (status);
    }
}

// Return number of records in heap file

const int HeapFile::getRecCnt() const
{
  return headerPage->recCnt;
}

// retrieve an arbitrary record from a file.
// if record is not on the currently pinned page, the current page
// is unpinned and the required page is read into the buffer pool
// and pinned.  returns a pointer to the record via the rec parameter
const Status HeapFile::getRecord(const RID & rid, Record & rec)
{
    Status status;

    // If the desired record is on the currently pinned page, simply invoke
	// curPage->getRecord(rid, rec) to get the record
	if (curPage != NULL && curPageNo == rid.pageNo) {
		status = curPage->getRecord(rid, rec);
		return status;	
	}

	// Otherwise, you need to unpin the currently pinned page (assuming a page is pinned) and use
	// pageNo field of the RID to read the page into the buffer pool.
	if (curPage != NULL) {
		status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
		curPage = NULL;
        if (status != OK) {
            return status;
        }
	}


	// If yes, you need to read the right page (the one with the requested record on it) into the buffer.
	//Make sure when you do this you set the fields curPage, curPageNo, curDirtyFlag, and curRec of the HeapFile object appropriately
	status = bufMgr->readPage(filePtr, rid.pageNo, curPage);
    if (status != OK) {
        return status;
    }

	status = curPage->getRecord(rid, rec);
    if (status != OK) {
        return status;
    }
    // save properties
    curRec = rid;
    curPageNo = rid.pageNo;
    curDirtyFlag = false;

	return status;
	
}

HeapFileScan::HeapFileScan(const string & name,
			   Status & status) : HeapFile(name, status)
{
    filter = NULL;
}

const Status HeapFileScan::startScan(const int offset_,
				     const int length_,
				     const Datatype type_, 
				     const char* filter_,
				     const Operator op_)
{
    if (!filter_) {                        // no filtering requested
        filter = NULL;
        return OK;
    }
    
    if ((offset_ < 0 || length_ < 1) ||
        (type_ != STRING && type_ != INTEGER && type_ != FLOAT) ||
        (type_ == INTEGER && length_ != sizeof(int)
         || type_ == FLOAT && length_ != sizeof(float)) ||
        (op_ != LT && op_ != LTE && op_ != EQ && op_ != GTE && op_ != GT && op_ != NE))
    {
        return BADSCANPARM;
    }

    offset = offset_;
    length = length_;
    type = type_;
    filter = filter_;
    op = op_;

    return OK;
}


const Status HeapFileScan::endScan()
{
    Status status;
    // generally must unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        curPage = NULL;
        curPageNo = 0;
		curDirtyFlag = false;
        return status;
    }
    return OK;
}

HeapFileScan::~HeapFileScan()
{
    endScan();
}

const Status HeapFileScan::markScan()
{
    // make a snapshot of the state of the scan
    markedPageNo = curPageNo;
    markedRec = curRec;
    return OK;
}

const Status HeapFileScan::resetScan()
{
    Status status;
    if (markedPageNo != curPageNo) 
    {
		if (curPage != NULL)
		{
			status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
			if (status != OK) return status;
		}
		// restore curPageNo and curRec values
		curPageNo = markedPageNo;
		curRec = markedRec;
		// then read the page
		status = bufMgr->readPage(filePtr, curPageNo, curPage);
		if (status != OK) return status;
		curDirtyFlag = false; // it will be clean
    }
    else curRec = markedRec;
    return OK;
}

/**
 * For each page, this method uses the firstRecord() and nextRecord() methods of the Page class to get the rids of all the records on the page. Then it converts the rid to a 
 * pointer to the record data and invoke matchRec() to determine if record satisfies the filter associated with the scan. If so, it stores the rid in curRec
 * @return the RID of the next record that satisfies the scan predicate, OK if no errors occurred. Otherwise, return the error code of the first error that occurred.
 */
const Status HeapFileScan::scanNext(RID& outRid)
{
    Status 	status = OK;
    RID		nextRid;
    RID		tmpRid;
    int 	nextPageNo;
    Record      rec;

    // handle edge case first by setting a curPage if previously set to null
    if (curPage == NULL) {
        curPageNo = headerPage->firstPage;
        status = bufMgr->readPage(filePtr, curPageNo, curPage);
        if (status != OK) {
            return status;
        }
    }
    // go into an infinite loop to find next record to satisfy scan predicate
    while (true) {
        if (curRec.pageNo == curPageNo) {
            status = curPage->nextRecord(curRec, nextRid);
        } else {
            status = curPage->firstRecord(nextRid);
        }
        if (status != OK) {
            return status;
        }
        // create another loop for all records within the page
        while (status == OK) {
            tmpRid = nextRid;
            // grabs record
            status = curPage->getRecord(tmpRid, rec);
            if (status != OK) {
                return status;
            }
            if (matchRec(rec)) {
                outRid = tmpRid;
                curRec = tmpRid;
                return OK;
            }
            status = curPage->nextRecord(tmpRid, nextRid);
        }
        // unpin the current page, move to the next
        status = curPage->getNextPage(nextPageNo);
        if (status != OK) {
            return status;
        }
        status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        if (status != OK) {
            return status;
        }
        curDirtyFlag = false; // can now be set to false after unpinning
        curPageNo = nextPageNo; // updates current page Number
        status = bufMgr->readPage(filePtr, curPageNo, curPage);
        if (status != OK) {
            return status; // Error reading next page
        }
    }
    return BUFFEREXCEEDED; // should never hit this case
}


// returns pointer to the current record.  page is left pinned
// and the scan logic is required to unpin the page 

const Status HeapFileScan::getRecord(Record & rec)
{
    return curPage->getRecord(curRec, rec);
}

// delete record from file. 
const Status HeapFileScan::deleteRecord()
{
    Status status;

    // delete the "current" record from the page
    status = curPage->deleteRecord(curRec);
    curDirtyFlag = true;

    // reduce count of number of records in the file
    headerPage->recCnt--;
    hdrDirtyFlag = true; 
    return status;
}


// mark current page of scan dirty
const Status HeapFileScan::markDirty()
{
    curDirtyFlag = true;
    return OK;
}

const bool HeapFileScan::matchRec(const Record & rec) const
{
    // no filtering requested
    if (!filter) return true;

    // see if offset + length is beyond end of record
    // maybe this should be an error???
    if ((offset + length -1 ) >= rec.length)
	return false;

    float diff = 0;                       // < 0 if attr < fltr
    switch(type) {

    case INTEGER:
        int iattr, ifltr;                 // word-alignment problem possible
        memcpy(&iattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ifltr,
               filter,
               length);
        diff = iattr - ifltr;
        break;

    case FLOAT:
        float fattr, ffltr;               // word-alignment problem possible
        memcpy(&fattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ffltr,
               filter,
               length);
        diff = fattr - ffltr;
        break;

    case STRING:
        diff = strncmp((char *)rec.data + offset,
                       filter,
                       length);
        break;
    }

    switch(op) {
    case LT:  if (diff < 0.0) return true; break;
    case LTE: if (diff <= 0.0) return true; break;
    case EQ:  if (diff == 0.0) return true; break;
    case GTE: if (diff >= 0.0) return true; break;
    case GT:  if (diff > 0.0) return true; break;
    case NE:  if (diff != 0.0) return true; break;
    }

    return false;
}

InsertFileScan::InsertFileScan(const string & name,
                               Status & status) : HeapFile(name, status)
{
  //Do nothing. Heapfile constructor will bread the header page and the first
  // data page of the file into the buffer pool
}

InsertFileScan::~InsertFileScan()
{
    Status status;
    // unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, true);
        curPage = NULL;
        curPageNo = 0;
        if (status != OK) cerr << "error in unpin of data page\n";
    }
}

// Insert a record into the file
const Status InsertFileScan::insertRecord(const Record & rec, RID& outRid)
{
    Page*	newPage;
    int		newPageNo;
    Status	status, unpinstatus;
    RID		rid;

    // checks if curpage is null or the last page, if so, changes the curpage to the last page
    if (curPage == NULL || curPageNo != headerPage->lastPage) {
        // unpin if curpageNo is not the last page
        if (curPage != NULL) {
            unpinstatus = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
            if (unpinstatus != OK) {
                return unpinstatus;
            }
            curDirtyFlag = false;
        }
        curPageNo = headerPage->lastPage;
        status = bufMgr->readPage(filePtr, curPageNo, curPage);
        if (status != OK) {
            return status;
        }
    }
    // attempt to insert record
    status = curPage->insertRecord(rec, rid);
    // if able to insert a record
    if (status == OK) {
        // Book keeping
        outRid = rid;
        hdrDirtyFlag = true;
        curDirtyFlag = true;
        headerPage->recCnt++;
        return status;
        // If unable to insert a record
    } else {
        status = bufMgr->allocPage(filePtr, newPageNo, newPage);
        if (status != OK) {
            return status;
        }
        // initialization 
        newPage->init(newPageNo);
        // update fields
        headerPage->pageCnt++;
        headerPage->lastPage = newPageNo;
        hdrDirtyFlag = true;
        // LINK PAGE ??? HOPE IT WORKS
        status = curPage->setNextPage(newPageNo);
        if (status != OK) {
            return status;
        }
        // make current page to be the newly allocated page
        unpinstatus = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        if (unpinstatus != OK) {
            return unpinstatus;
        }
        curDirtyFlag = false; // now can be set to false
        curPage = newPage;
        curPageNo = newPageNo;
        // Retry inserting a record
        status = curPage->insertRecord(rec, rid);
        if (status != OK) {
            return status;
        }
        // on success, update properties
        outRid = rid;
        hdrDirtyFlag = true;
        curDirtyFlag = true;
        headerPage->recCnt++;
        return status;
    }



    // // Ensure current page is the last page
    // // If curPage is NULL or not the last page,
    // // unpin the page and load in the last page
    // if(curPage == NULL || curPageNo != headerPage->lastPage){
    //     if(curPage != NULL) {
    //         unpinstatus = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
    //         if(status != OK)
    //             return status;
    //     }
    //     int lastPageNo = headerPage->lastPage;
    //     status = bufMgr->readPage(filePtr, lastPageNo, curPage);
    //     if(status != OK);
    //         return status;
    //     curPageNo = lastPageNo;
    //     curDirtyFlag = false;
    // }

    // // Attempt to insert the record into the last page
    // status = curPage->insertRecord(rec, outRid);

    // // Alloc new page if insufficient space
    // if(status == NOSPACE){
    //     status = bufMgr->allocPage(filePtr, newPageNo, newPage);
    //     if(status != OK)
    //         return status;
        
    //     // Init new page
    //     newPage->init(newPageNo);
    //     headerPage->lastPage = newPageNo;
    //     headerPage->pageCnt++;
    //     hdrDirtyFlag = true;

    //     // Unpin current page
    //     unpinstatus = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
    //     if(status != OK)
    //         return status;
        
    //     // Set new page as current
    //     curPage = newPage;
    //     curPageNo = newPageNo;
    //     curDirtyFlag = false;

    //     // Attempt again to insert the record
    //     status = curPage->insertRecord(rec, outRid);
    //     if(status != OK)
    //         return status;
    // } else if (status != OK){
    //     return status; // return any non-NOSPACE errors
    // }

    // headerPage->recCnt++;
    // hdrDirtyFlag = true;

    // return OK;
}


