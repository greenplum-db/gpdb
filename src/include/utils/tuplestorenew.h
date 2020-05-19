/*
 * tuplestorenew.h
 * 	A better tuple store
 */

#ifndef TUPSTORE_NEW_H
#define TUPSTORE_NEW_H

#include "executor/tuptable.h"
#include "utils/workfile_mgr.h"

typedef struct NTupleStorePos
{
	long blockn;
	int slotn;
} NTupleStorePos;

/* Opaque data types */
typedef struct NTupleStoreAccessor NTupleStoreAccessor;
typedef struct NTupleStore NTupleStore;

/* Instrument tuple store 
 * Caller must ensure ins ptr remain valid during the lifetype of the tuple store 
 */
void ntuplestore_setinstrument(NTupleStore* ts, struct Instrumentation *ins);

/* Tuple store method */
extern NTupleStore *ntuplestore_create(int64 maxBytes, char *operation_name);
extern NTupleStore *ntuplestore_create_readerwriter(const char* filename, int64 maxBytes, bool isWriter);
extern bool ntuplestore_is_readerwriter_reader(NTupleStore* nts);
extern void ntuplestore_flush(NTupleStore *ts);
extern void ntuplestore_destroy(NTupleStore *ts);

/* Tuple store accessor method 
 * Create Accessor: current we support 1 writer, many reader per store.  After created, the accessor
 * is positioned at the first tuple or eof (if there is no tuple).
 */
extern NTupleStoreAccessor *ntuplestore_create_accessor(NTupleStore *ts, bool isWriter);
extern void ntuplestore_destroy_accessor(NTupleStoreAccessor *acc);

/* Put slot/data and automatically position the accessor to the last entry */
extern void ntuplestore_acc_put_tupleslot(NTupleStoreAccessor *tsa, TupleTableSlot *slot);
extern void ntuplestore_acc_put_data(NTupleStoreAccessor *tsa, void *data, int len);

/* return true if success, false if beyond last (first) valid position */
extern bool ntuplestore_acc_advance(NTupleStoreAccessor *tsa, int n);

/* Get data.  The slot/pointer returned is guaranteed to be valid till the accessor
 * call advance.
 * NOTE: trim may make current position of an accessor invalid.  It is caller's reponsibilty
 * to make sure trim does not trim too far ahead
 */
extern bool ntuplestore_acc_current_tupleslot(NTupleStoreAccessor *tsa, TupleTableSlot *slot);
extern bool ntuplestore_acc_current_data(NTupleStoreAccessor *tsa, void **data, int *len);

/* Tell/seek position of accessor. */

/* Tell fill in the pos, return false if accessor points to an invalid position.
 * Caller can pass in pos = NULL to simply tell if the accessor is at a valid position.
 */
extern bool ntuplestore_acc_tell(NTupleStoreAccessor *tsa, NTupleStorePos *pos);

/* put the current row at a certain pos (or first/last).  Return false pos is invalid */
extern bool ntuplestore_acc_seek(NTupleStoreAccessor *tsa, NTupleStorePos *pos);
extern bool ntuplestore_acc_seek_first(NTupleStoreAccessor *tsa);
extern bool ntuplestore_acc_seek_last(NTupleStoreAccessor *tsa);
extern void ntuplestore_acc_seek_bof(NTupleStoreAccessor *tsa);
extern void ntuplestore_acc_seek_eof(NTupleStoreAccessor *tsa);

#endif /* TUPSTORE_NEW_H */
