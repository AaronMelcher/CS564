#include "catalog.h"
#include "query.h"


/*
 * Inserts a record into the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Insert(const string & relation, 
	const int attrCnt, 
	const attrInfo attrList[])
{
	cout << "Doing QU_Insert on relation " << relation << endl;
	Status status;

	// Retrieve full schema for this relation
	int fullAttrCnt = 0;
	AttrDesc *fullAttrs = nullptr;
	status = attrCat->getRelInfo(relation, fullAttrCnt, fullAttrs);
	if(status!=OK){
		cout << "Error: could not retrieve schema for " << relation << endl;
		return status;
	}

	// compute record length
	int recLen = 0;
	for(int i = 0; i < fullAttrCnt; i++){
		recLen += fullAttrs[i].attrLen;
	}

	// allocate buffer for new record
	char *tuple = new char[recLen];

	// find all values for each attribute in the schema
	// and copy into the tuple buffer
	for(int i = 0; i < fullAttrCnt; i++){
		const char *attrName = fullAttrs[i].attrName;
		bool found = false;

		for(int j = 0; j < attrCnt; j++){
			if(strcmp(attrList[j].attrName, attrName) == 0){
				// Copy binary val
				memcpy(tuple + fullAttrs[i].attrOffset, 
						attrList[j].attrValue, fullAttrs[i].attrLen);
				found = true;
				break;
			}
		}

		if(!found){
			cout << "Error: missing value for attribute" << endl;
			delete [] tuple;
			delete [] fullAttrs;
			return ATTRNOTFOUND; // is this the right code?
		}

		// open InsertFileScan
		InsertFileScan insert(relation, status);
		if(status != OK) {
			cout << "Error: unable to open InsertFileScan" << endl;
			delete [] tuple;
			delete [] fullAttrs;
			return status;
		}

		Record rec;
		rec.data = (void*) tuple;
		rec.length = recLen;
		RID outRid;

		status = insert.insertRecord(rec, outRid);
		if(status != OK){
			cout << "Error: failed to insert record" << endl;
		}

		delete [] tuple;
		delete [] fullAttrs;
		return status;
	}
}

