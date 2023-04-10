/*-------------------------------------------------------------------------
 *
 * predtest_valueset.h
 *	  functions for "value set" tests.
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/optimizer/predtest_valueset.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PREDTEST_VALUESET_H
#define PREDTEST_VALUESET_H

#include "nodes/primnodes.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

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

extern PossibleValueSet DeterminePossibleValueSet(Node *clause, Node *variable, Oid opfamily);

/* returns a newly allocated list */
extern Node **GetPossibleValuesAsArray(PossibleValueSet *pvs, int *numValuesOut);

extern void DeletePossibleValueSetData(PossibleValueSet *pvs);

extern void InitPossibleValueSetData(PossibleValueSet *pvs);

extern void AddUnmatchingValues(PossibleValueSet *pvs, PossibleValueSet *toCheck);
extern void RemoveUnmatchingValues(PossibleValueSet *pvs, PossibleValueSet *toCheck);
extern bool TryProcessExprForPossibleValues(Node *expr, Node *variable, Oid opfamily, PossibleValueSet *resultOut);

#endif   /* PREDTEST_VALUESET_H */
