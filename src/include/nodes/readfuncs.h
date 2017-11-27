/*-------------------------------------------------------------------------
 *
 * readfuncs.h
 *	  header file for read.c and readfuncs.c. These functions are internal
 *	  to the stringToNode interface and should not be used by anyone else.
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/nodes/readfuncs.h,v 1.25 2009/01/01 17:24:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef READFUNCS_H
#define READFUNCS_H

#include "nodes/nodes.h"

/*
 * prototypes for functions in read.c (the lisp token parser)
 */
extern char *pg_strtok(int *length);
extern char *debackslash(char *token, int length);
extern void *nodeRead(char *token, int tok_len);

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

#endif   /* READFUNCS_H */
