
/*-------------------------------------------------------------------------
 *
 * pg_resgroup.h
 *	  definition of the system "resource group" relation (pg_resgroup).
 *
 *
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_RESGROUP_H
#define PG_RESGROUP_H

#include "catalog/genbki.h"

/* ----------------
 *	pg_resgroup definition.  cpp turns this into
 *	typedef struct FormData_pg_resgroup
 * ----------------
 */
#define ResGroupRelationId	6436

CATALOG(pg_resgroup,6436) BKI_SHARED_RELATION
{
	NameData	rsgname;		/* name of resource group */

	Oid		parent;			/* parent resource group */
} FormData_pg_resgroup;

/* no foreign keys */

/* ----------------
 *	Form_pg_resgroup corresponds to a pointer to a tuple with
 *	the format of pg_resgroup relation.
 * ----------------
 */
typedef FormData_pg_resgroup *Form_pg_resgroup;

/* ----------------
 *	compiler constants for pg_resqueue
 * ----------------
 */
#define Natts_pg_resgroup			2
#define Anum_pg_resgroup_rsgname		1
#define Anum_pg_resgroup_parent			2

/* Create initial default resource group */

DATA(insert OID = 6437 ( default_group, 0 ));

DATA(insert OID = 6438 ( admin_group, 0 ));

#define DEFAULTRESGROUP_OID 	6437
#define ADMINRESGROUP_OID 	6438

/* ----------------
 *	pg_resgroupcapability definition.  cpp turns this into
 *	typedef struct FormData_pg_resgroupcapability
 * ----------------
 */
#define ResGroupCapabilityRelationId		6439

#define RESGROUP_LIMIT_TYPE_UNKNOWN		0
#define RESGROUP_LIMIT_TYPE_CONCURRENCY	1
#define RESGROUP_LIMIT_TYPE_CPU			2
#define RESGROUP_LIMIT_TYPE_MEMORY		3
#define RESGROUP_LIMIT_TYPE_MEMORY_REDZONE	4
#define RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA 5
#define RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO  6

#define RESGROUP_CONCURRENCY_UNLIMITED (-1)

CATALOG(pg_resgroupcapability,6439) BKI_SHARED_RELATION
{
	Oid		resgroupid;	/* OID of the group with this capability  */

	int2		reslimittype;	/* resource limit type id (RESGROUP_LIMIT_TYPE_XXX) */

	text		value;		/* resource limit (opaque type)  */

	text		proposed; 	/* most of the capabilities cannot be updated immediately, we
					 * do it in an asynchronous way to merge the proposed value 
					 * with the working one */
} FormData_pg_resgroupcapability;


/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(resgroupid REFERENCES pg_resgroup(oid));

/* ----------------
 *	Form_pg_resgroupcapability corresponds to a pointer to a tuple with
 *	the format of pg_resgroupcapability relation.
 * ----------------
 */
typedef FormData_pg_resgroupcapability *Form_pg_resgroupcapability;

/* ----------------
 *	compiler constants for pg_resgroupcapability
 * ----------------
 */
#define Natts_pg_resgroupcapability		4
#define Anum_pg_resgroupcapability_resgroupid	1
#define Anum_pg_resgroupcapability_reslimittype 2
#define Anum_pg_resgroupcapability_value	3
#define Anum_pg_resgroupcapability_proposed	4

DATA(insert ( 6437, 1, 20, 20 ));

DATA(insert ( 6437, 2, 0.3, 0.3 ));

DATA(insert ( 6437, 3, 0.3, 0.3 ));

DATA(insert ( 6437, 4, 0.8, 0.8 ));

DATA(insert ( 6438, 1, -1, -1 ));

DATA(insert ( 6438, 2, 0.1, 0.1 ));

DATA(insert ( 6438, 3, 0.1, 0.1 ));

DATA(insert ( 6438, 4, 0.8, 0.8 ));

#endif   /* PG_RESGROUP_H */
