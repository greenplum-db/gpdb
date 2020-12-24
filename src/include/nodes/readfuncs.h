/*-------------------------------------------------------------------------
 *
 * readfuncs.h
 *	  header file for read.c and readfuncs.c. These functions are internal
 *	  to the stringToNode interface and should not be used by anyone else.
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/nodes/readfuncs.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef READFUNCS_H
#define READFUNCS_H

#include "nodes/nodes.h"

/*
 * variable in read.c that needs to be accessible to readfuncs.c
 */
#ifdef WRITE_READ_PARSE_PLAN_TREES
extern bool restore_location_fields;
#endif

/*
 * prototypes for functions in read.c (the lisp token parser)
 */
extern const char *pg_strtok(int *length);
extern char *debackslash(const char *token, int length);
extern void *nodeRead(const char *token, int tok_len);

/*
 * nodeReadSkip
 *    Skips next item (a token, list or subtree).
 */
void
nodeReadSkip(void);

/*
 * pg_strtok_peek_fldname
 *    Peeks at the token that will be returned by the next call to
 *    pg_strtok.  Returns true if the token is, case-sensitively,
 *          :fldname
 */
bool
pg_strtok_peek_fldname(const char *fldname);

/*-------------------------------------------------------------------------
 * prototypes for functions in readfuncs.c
 */
extern Node *parseNodeString(void);

#endif							/* READFUNCS_H */
