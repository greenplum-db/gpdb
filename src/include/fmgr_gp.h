/*-------------------------------------------------------------------------
 *
 * fmgr_gp.h
 *	  Definitions for the extra function-call interface.
 *
 * This file must be included by all Postgres modules that either define
 * or call fmgr-callable functions.
 *
 *
 * Portions Copyright (c) 2017-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/fmgr.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef FMGR_GP_H
#define FMGR_GP_H

/* These are for invocation of a specifically named function with a
 * directly-computed parameter list.  Note that the arguments are not allowed
 * to be NULL, but the result is.
 */
extern Datum DirectNullFunctionCall1Coll(PGFunction func, Oid collation,
										 Datum arg1, bool *isNull);

/* These macros allow the collation argument to be omitted (with a default of
 * InvalidOid, ie, no collation).  They exist mostly for backwards
 * compatibility of source code.
 */
#define DirectNullFunctionCall1(func, arg1, isNull) \
	DirectNullFunctionCall1Coll(func, InvalidOid, arg1, isNull)

#endif							/* FMGR_GP_H */
