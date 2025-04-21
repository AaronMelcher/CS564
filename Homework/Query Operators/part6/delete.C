#include "catalog.h"
#include "query.h"
#include <cstdlib>

/*
 * Deletes records from a specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Delete(const string & relation, 
		       const string & attrName, 
		       const Operator op,
		       const Datatype type, 
		       const char *attrValue)
{

	//cout << "Relation: " << relation << " attrName: " << attrName << " op: " << op << " type: " << type << " attrValue: " << attrValue << endl;
	Status status;

	if (relation.empty()) {
		return BADSCANPARM;
	}

	HeapFileScan scan(relation, status);
	if (status != OK) {
		return status;
	}

	// prep filter info
	int intVal = 0;
	float floatVal = 0;
	char *strBuf = nullptr;
	const char *filterPtr = nullptr;
	int filterLen = 0;
	int filterOffset = 0;

	if(!attrName.empty()){
		// lookup attr
		AttrDesc desc;
		status = attrCat->getInfo(relation, attrName, desc);
		if(status!=OK) 
			return status;

		// type check
		if(type != (Datatype)desc.attrType || desc.attrLen == 0)
			return ATTRTYPEMISMATCH;

		filterOffset = desc.attrOffset;
		filterLen = desc.attrLen;

		switch(type) {
			case INTEGER:
				intVal = atoi(attrValue);
				filterPtr = (char*)&intVal;
				break;
			case FLOAT:
				floatVal = atof(attrValue);
				filterPtr = (char*)&floatVal;
				break;
			case STRING:
				strBuf = new char[filterLen];
				memset(strBuf, 0, filterLen);
				strncpy(strBuf, attrValue, filterLen);
				filterPtr = strBuf;
				break;
			default:
				return BADSCANPARM;
		}
		// Start scan
		status = scan.startScan(filterOffset, filterLen, type, filterPtr, op);
		
		if(status != OK)
			return status;
	} else {
		status = scan.startScan(0, 0, STRING, nullptr, EQ); // or should be STRING and EQ for type and op?
		if(status != OK)
			return status;
	}

	// perform deletion
	Status next;
	RID rid;
	while((next = scan.scanNext(rid)) == OK){
		status = scan.deleteRecord();
		if(status != OK){
			scan.endScan();
			return status;
		}
	}

	if(next != FILEEOF){
		scan.endScan();
		return next;
	}

	Status end = scan.endScan();
	return end; 
}


