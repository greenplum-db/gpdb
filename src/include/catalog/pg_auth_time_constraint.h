/*-------------------------------------------------------------------------
 *
 * pg_auth_time_constraint.h
 *    definition of the time-based authorization relation (pg_auth_time_constraint)
 *
 * Portions Copyright (c) 2006-2011, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * NOTES
 *    the genbki.sh script reads this file and generates .bki
 *    information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_AUTH_TIME_CONSTRAINT_H
#define PG_AUTH_TIME_CONSTRAINT_H

#include "catalog/genbki.h"
#include "utils/date.h"

/*
 * The CATALOG definition has to refer to the type of "start_time" et al as
 * "time" (lower case) so that bootstrap mode recognizes it.  But
 * the C header files define this type as TimeADT.	So we use a sleazy trick.
 *
 */

#define time TimeADT

/* ----------------
 *		pg_auth_time_constraint definition.  cpp turns this into
 *		typedef struct FormData_pg_auth_time_constraint
 * ----------------
 */
#define AuthTimeConstraintRelationId	6070
#define AuthTimeConstraint_Rowtype_Id	6071

CATALOG(pg_auth_time_constraint,6070) BKI_SHARED_RELATION BKI_WITHOUT_OIDS BKI_ROWTYPE_OID(6071)
{
	Oid		authid;		/* foreign key to pg_authid.oid, */
	int2	start_day;	/* [0,6] denoting start of interval */
	time	start_time;	/* optional time denoting start of interval */
	int2	end_day;	/* [0,6] denoting end of interval */
	time	end_time;	/* optional time denoting end of interval */
} FormData_pg_auth_time_constraint;


/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(authid REFERENCES pg_authid(oid));

#undef time


/* ----------------
 *		Form_pg_auth_time_constraint corresponds to a pointer to a tuple with
 *		the format of pg_auth_time_constraint relation.
 * ----------------
 */
typedef FormData_pg_auth_time_constraint *Form_pg_auth_time_constraint;


/* ----------------
 *		compiler constants for pg_auth_time_constraint
 * ----------------
 */
#define Natts_pg_auth_time_constraint			5
#define Anum_pg_auth_time_constraint_authid		1
#define Anum_pg_auth_time_constraint_start_day	2
#define Anum_pg_auth_time_constraint_start_time	3
#define Anum_pg_auth_time_constraint_end_day	4
#define Anum_pg_auth_time_constraint_end_time	5

#define Schema_pg_auth_time_constraint \
{6070, {"authid"}    ,   26, -1, 0, 4, 1, 0, -1, -1, true           , 'p' ,'i', true, false, false, true, 0, { 0 } }, \
{6070, {"start_day"} ,   21, -1, 0, 2, 2, 0, -1, -1, true           , 'p' ,'s', true, false, false, true, 0, { 0 } }, \
{6070, {"start_time"}, 1083, -1, 0, 8, 3, 0, -1, -1, FLOAT8PASSBYVAL, 'p' ,'d', true, false, false, true, 0, { 0 } }, \
{6070, {"end_day"}   ,   21, -1, 0, 2, 4, 0, -1, -1, true           , 'p' ,'s', true, false, false, true, 0, { 0 } }, \
{6070, {"end_time"}  , 1083, -1, 0, 8, 5, 0, -1, -1, FLOAT8PASSBYVAL, 'p' ,'d', true, false, false, true, 0, { 0 } }

#endif   /* PG_AUTH_TIME_CONSTRAINT_H */
