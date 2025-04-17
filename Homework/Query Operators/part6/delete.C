#include "catalog.h"
#include "query.h"


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

	// part 6

	// passed relation, attrName, operator, type, pointer to attribute value

	cout << "Doing QU_Delete on relation " << relation << "attrName: " << attrName << " OP: " << op << "type: " <<type << " attrValue: " << attrValue << endl;
	Status status;

	// This function will delete all tuples in relation satisfying the predicate specified by attrName, op, and the constant attrValue. 
	// type denotes the type of the attribute. You can locate all the qualifying tuples using a filtered HeapFileScan.

	HeapFileScan scan(relation, status);

	if (attrName == "") {
		status = scan.startScan(0, 0, type, nullptr, op);
	} else {
		AttrDesc attr;
		status = attrCat->getInfo(relation, attrName, attr);
		if (status != OK) {
			return status;
		}

		cout << "attr.attrType: " << attr.attrType << "\n";

		status = scan.startScan(attr.attrOffset, attr.attrLen, type, attrValue, op);
		if (status != OK) {
			return status;
		}
	}

	// Loop over matches and delete them
    RID rid;
    while ((status = scan.scanNext(rid)) == OK) {
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


