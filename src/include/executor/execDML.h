/*-------------------------------------------------------------------------
 *
 * execDML.h
 *	  Prototypes for execDML.c
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

extern TupleTableSlot *
reconstructPartitionTupleSlot(TupleTableSlot *parentSlot,
							  ResultRelInfo *childInfo);

#endif   /* EXECDML_H */

