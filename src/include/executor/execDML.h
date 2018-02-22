/*-------------------------------------------------------------------------
 *
 * execDML.h
 *	  Prototypes for execDML.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/execDML.h
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

/*
 * In PostgreSQL, ExecInsert, ExecDelete and ExecUpdate are static in nodeModifyTable.c.
 * In GPDB, they're exported.
 */
extern TupleTableSlot *
ExecInsert(TupleTableSlot *slot,
		   TupleTableSlot *planSlot,
		   EState *estate,
		   PlanGenerator planGen,
		   bool isUpdate);

extern TupleTableSlot *
ExecDelete(ItemPointer tupleid,
		   TupleTableSlot *planSlot,
		   EPQState *epqstate,
		   EState *estate,
		   PlanGenerator planGen,
		   bool isUpdate);

extern TupleTableSlot *
ExecUpdate(ItemPointer tupleid,
		   TupleTableSlot *slot,
		   TupleTableSlot *planSlot,
		   EPQState *epqstate,
		   EState *estate);


#endif   /* EXECDML_H */

