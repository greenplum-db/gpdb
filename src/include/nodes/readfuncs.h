/*-------------------------------------------------------------------------
 *
 * readfuncs.h
 *	  header file for read.c and readfuncs.c. These functions are internal
 *	  to the stringToNode interface and should not be used by anyone else.
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/nodes/readfuncs.h,v 1.24 2008/01/01 19:45:58 momjian Exp $
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

/*
 * pg_strtok_prereq
 *    If the next tokens to be returned by pg_strtok are, case-sensitively,
 *          :prereq <featurename>
 *    then this function consumes them and returns true.  Otherwise false
 *    is returned and no tokens are consumed.
 */
bool
pg_strtok_prereq(const char *featurename);


/*-------------------------------------------------------------------------
 * prototypes for functions in readfuncs.c
 */
extern Node *parseNodeString(void);

#endif   /* READFUNCS_H */
