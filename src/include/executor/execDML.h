/*-------------------------------------------------------------------------
 *
 * execDML.h
 *	  Prototypes for execDML.
 *
 * Copyright (c) 2012, EMC Corp.
 *
 *-------------------------------------------------------------------------
 */

#ifndef EXECDML_H
#define EXECDML_H

extern void
reconstructTupleValues(AttrMap *map,
					Datum *oldValues, bool *oldIsnull, int oldNumAttrs,
					Datum *newValues, bool *newIsnull, int newNumAttrs);

extern TupleTableSlot *
reconstructMatchingTupleSlot(TupleTableSlot *slot, ResultRelInfo *resultRelInfo);

extern void
ExecInsert(TupleTableSlot *slot,
		   DestReceiver *dest,
		   EState *estate,
		   PlanGenerator planGen,
	           bool isUpdate,
	           bool isLateExecfunc);

extern void
ExecDelete(ItemPointer tupleid,
		   TupleTableSlot *planSlot,
		   DestReceiver *dest,
		   EState *estate,
		   PlanGenerator planGen,
		   bool isUpdate);

extern void
ExecUpdate(TupleTableSlot *slot,
		   ItemPointer tupleid,
		   TupleTableSlot *planSlot,
		   DestReceiver *dest,
		   EState *estate);


#endif   /* EXECDML_H */

