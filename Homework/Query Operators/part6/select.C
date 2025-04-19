#include "catalog.h"
#include "query.h"
#include "stdio.h"
#include "stdlib.h"

// forward declaration
const Status ScanSelect(const string & result, 
			const int projCnt, 
			const AttrDesc projNames[],
			const AttrDesc *attrDesc, 
			const Operator op, 
			const char *filter,
			const int reclen);

/*
 * Selects records from the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Select(const string & result, 
		       const int projCnt, 
		       const attrInfo projNames[],
		       const attrInfo *attr, 
		       const Operator op, 
		       const char *attrValue)
{
   // Qu_Select sets up things and then calls ScanSelect to do the actual work
    cout << "Doing QU_Select " << endl;
	
	// need check for no projections? -> default to ALL in this case?

	string relname = projNames[0].relName;

	// Get schema for the above relation via the attribute catalog
	int fullAttrCnt = 0;
	AttrDesc *fullAttrs = nullptr;
	Status status = attrCat->getRelInfo(relname, fullAttrCnt, fullAttrs);
	
	if(status!=OK)
		return status;

	// Compute record length
	int recordLen = 0;
	for(int i = 0; i < fullAttrCnt; i++){
		recordLen += fullAttrs[i].attrLen;
	}

	// Build array of AttrDesc objects
	AttrDesc *projDescs = new AttrDesc[projCnt];
	for(int i = 0; i < projCnt; i++) {
		bool found=false;
		for(int j = 0; j < fullAttrCnt; j++){
			if (string(projNames[i].attrName) == string(fullAttrs[j].attrName)) {
				projDescs[i] = fullAttrs[j];
				found=true;
				break;
			}
		}
		if(!found){
			cout << "Error: Projection attribute " << projNames[i].attrName 
        	<< " not found in relation " << relname << endl;
			delete [] projDescs;
			delete [] fullAttrs;
			return ATTRNOTFOUND;
		}
	}

	// Retrieve selection predicate descriptor if applicable
	AttrDesc predAttrDesc;
	AttrDesc *predAttrDescPtr = NULL;
	if(attr != NULL) {
		status = attrCat->getInfo(relname, string(attr->attrName), predAttrDesc);
		if(status != OK){
			delete [] projDescs;
			delete [] fullAttrs;
			return status;
		}
		predAttrDescPtr = &predAttrDesc;
	}

	// Call ScanSelect to perform the selction and projection
	// converting to appropriate type
	if (predAttrDesc.attrType == INTEGER) {
        // make integer
        int intVal = std::atoi(attrValue);
        status = ScanSelect(result, projCnt, projDescs, predAttrDescPtr, op, (char*)&intVal, recordLen);
        delete [] projDescs;
	    return status;
    } else if (predAttrDesc.attrType == FLOAT) {
        // make float
        float fltVal = std::atof(attrValue);
		status = ScanSelect(result, projCnt, projDescs, predAttrDescPtr, op, (char*)&fltVal, recordLen);
        delete [] projDescs;
	    return status;
    } else {
		status = ScanSelect(result, projCnt, projDescs, predAttrDescPtr, op, attrValue, recordLen);
        delete [] projDescs;
		return status;
    }

}


const Status ScanSelect(const string & result, 
#include "stdio.h"
#include "stdlib.h"
			const int projCnt, 
			const AttrDesc projNames[],
			const AttrDesc *attrDesc, 
			const Operator op, 
			const char *filter,
			const int reclen)
{
    cout << "Doing HeapFileScan Selection using ScanSelect()" << endl;

	string relname = projNames[0].relName;
	Status status;

	// Create HeapFileScan for input relation
	HeapFileScan scan(relname, status);
	if(status!=OK) {
		cout << "Error: unable to create HeapFileScan object" << endl;
		return status;
	}

	// Start scanning. Include values if selection pred provided
	if(attrDesc != NULL){
		status = scan.startScan(attrDesc->attrOffset, attrDesc->attrLen,
								(Datatype)attrDesc->attrType, filter, op);
	} else {
		// Unconditional scan (no predicate)
		status = scan.startScan(0, 0, STRING, NULL, op);
	}
	if(status!=OK) {
		cout << "Error: unable to start scan" << endl;
		return status;
	}

	// Open result relation
	InsertFileScan insert(result, status);
	if(status!=OK) {
		cout << "Error: unable to open result relation (InserFileScan object)" << endl;
		return status;
	}
	
	// Go through and perform projection for matching records
	RID rid;
	Record rec;
	while((status = scan.scanNext(rid)) == OK){
		status = scan.getRecord(rec);
		if(status!=OK) {
			cout << "Error: unable to retrieve record " << endl;
			return status;
		}

		// Allocate buffer for tuple
		int projRecLen = 0;
		for(int i = 0; i < projCnt; i++){
			projRecLen += projNames[i].attrLen;
		}
		char* projTuple = new char[projRecLen];
		int destOffset = 0;

		// For each attribute, copy corresponding bytes from the record
		// Get items from the catalog
		for(int i = 0; i < projCnt; i++){
			memcpy(projTuple + destOffset, (char *)rec.data + projNames[i].attrOffset, projNames[i].attrLen);
			destOffset += projNames[i].attrLen;
		}

		// create new Record for the tuple
		Record projRec;
		projRec.data = (void*)projTuple;
		projRec.length = projRecLen;

		// Insert into the result relation
		RID newRID;
		status = insert.insertRecord(projRec, newRID);
		if(status != OK) {
			cout << "Error: unable to insert record to result relation" << endl;
			delete [] projTuple;
			return status; // break or return? -> missing error code if break?
		}

		// Free buffer
		delete [] projTuple;
	}

	// End scan
	status = scan.endScan();
	return status;
}
