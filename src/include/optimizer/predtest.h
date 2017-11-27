/*-------------------------------------------------------------------------
 *
 * predtest.h
 *	  prototypes for predtest.c
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/optimizer/predtest.h,v 1.7 2009/01/01 17:24:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PREDTEST_H
#define PREDTEST_H

#include "nodes/primnodes.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"


extern bool predicate_implied_by(List *predicate_list,
					 List *restrictinfo_list);
extern bool predicate_refuted_by(List *predicate_list,
					 List *restrictinfo_list);

/***************************************************************************************
 * BEGIN functions and structures for determining a set of possible values from a clause
 */
typedef struct
{
	/**
	 * the set of possible values, if this can be determined from the clause.
	 */
	HTAB *set;

	/**
	 * The memory context that contains the hashtable and its set of possible values
	 */
	MemoryContext memoryContext;

	/**
	 * if true then set should be ignored and instead we know that we don't know anything about the set of values
	 */
	bool isAnyValuePossible;
} PossibleValueSet;

extern PossibleValueSet
DeterminePossibleValueSet( Node *clause, Node *variable);

/* returns a newly allocated list */
extern Node **
GetPossibleValuesAsArray( PossibleValueSet *pvs, int *numValuesOut );

extern void
DeletePossibleValueSetData(PossibleValueSet *pvs);

extern void
InitPossibleValueSetData(PossibleValueSet *pvs);

/**
 * END functions and structures for determining set of possible values from a clause
 ***********************************************************************************
 */

#endif   /* PREDTEST_H */
