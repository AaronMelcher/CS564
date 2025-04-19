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

	if (relation == "") {
		return BADSCANPARM;
	}

	HeapFileScan scan(relation, status);
	if (status != OK) {
		return status;
	}

	if (attrName == "") {
		status = scan.startScan(0, 0, type, nullptr, op);
	} else {
		AttrDesc attr;
		status = attrCat->getInfo(relation, attrName, attr);
		if (status != OK) {
			return status;
		}

		if (type == INTEGER) {
			// make integer
			int intVal = std::atoi(attrValue);
			status = scan.startScan(attr.attrOffset, attr.attrLen, type, (char*)&intVal, op);
            if (status != OK) {
               return status;
            }
		} else if (type == FLOAT) {
			// make float
			float fltVal = std::atof(attrValue);
			status = scan.startScan(attr.attrOffset, attr.attrLen, type, (char*)&fltVal, op);
            if (status != OK) {
               return status;
            }
		} else {
			status = scan.startScan(attr.attrOffset, attr.attrLen, type, attrValue, op);
	        if (status != OK) {
	           return status;
	        }
		}
	}

	RID rid;
	// Loop to find matches and delete them
    while (scan.scanNext(rid) == OK) {
		status = scan.deleteRecord();
		if (status != OK) {
			scan.endScan();
			return status;
		}
	}

    // End scan
    scan.endScan();
	return OK;

}


