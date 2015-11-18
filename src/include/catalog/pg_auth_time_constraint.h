/*-------------------------------------------------------------------------
 *
 * pg_auth_time_constraint.h
 *    definition of the time-based authorization relation (pg_auth_time_constraint)
 *
 * Copyright (c) 2006-2011, Greenplum inc.
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

/* TIDYCAT_BEGINDEF

   CREATE TABLE pg_auth_time_constraint
   with (CamelCase=AuthTimeConstraint, shared=true, oid=false, relid=2914)
   (
   authid oid, -- foreign key to pg_authid.oid,
   start_day smallint, -- [0,6] denoting start of interval
   start_time time, -- optional time denoting start of interval
   end_day smallint, -- [0,6] denoting end of interval
   end_time time -- optional time denoting end of interval
   );

   ALTER TABLE pg_auth_time_constraint ADD FK authid on pg_authid(oid);

   TIDYCAT_ENDDEF
*/

/* TIDYCAT_BEGIN_CODEGEN 

   WARNING: DO NOT MODIFY THE FOLLOWING SECTION: 
   Generated by tidycat.pl version 33
   on Tue Aug 14 18:59:15 2012
*/


/*
 TidyCat Comments for pg_auth_time_constraint:
  Table is shared, so catalog.c:IsSharedRelation is updated.
  Table does not have an Oid column.
  Table has static type (see pg_types.h).
  Table has weird hack for time column.
 
*/

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
#define AuthTimeConstraintRelationId	2914

CATALOG(pg_auth_time_constraint,2914) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	Oid		authid;		/* foreign key to pg_authid.oid, */
	int2	start_day;	/* [0,6] denoting start of interval */
	time	start_time;	/* optional time denoting start of interval */
	int2	end_day;	/* [0,6] denoting end of interval */
	time	end_time;	/* optional time denoting end of interval */
} FormData_pg_auth_time_constraint;

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


/* TIDYCAT_END_CODEGEN */

#endif   /* PG_AUTH_TIME_CONSTRAINT_H */
