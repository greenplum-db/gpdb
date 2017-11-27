/*-------------------------------------------------------------------------
 *
 * combocid.h
 *	  Combo command ID support routines
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/combocid.h,v 1.3 2009/01/01 17:24:02 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef COMBOCID_H
#define COMBOCID_H

#include "postgres.h"
#include "utils/combocid.h"

/*
 * HeapTupleHeaderGetCmin and HeapTupleHeaderGetCmax function prototypes
 * are in access/htup.h, because that's where the macro definitions that
 * those functions replaced used to be.
 */
 
/* CDB TODO: This is the max number of combo cids that we copy into the Shared
 * snapshot to the reader gangs.  As a result, transactions that have more than
 * 128 update/delete statements within them can result in the reader gangs
 * reading a tuple whose visibility they cannot determine because of not having
 * the entire combocids table.  Those statements will result in the reader gangs
 * throwing an error.  Ultimately, we need to find another way of serializing
 * this information that can support the arbitrary size of the combocids 
 * datastructure.
 */
#define MaxComboCids 256

/* Key and entry structures for the hash table */
typedef struct
{
	CommandId		cmin;
	CommandId		cmax;
	TransactionId	xmin;
} ComboCidKeyData;

typedef ComboCidKeyData *ComboCidKey;

extern volatile ComboCidKey comboCids;
extern volatile int usedComboCids;
extern volatile int sizeComboCids;

extern void AtEOXact_ComboCid(void);

#endif   /* COMBOCID_H */
