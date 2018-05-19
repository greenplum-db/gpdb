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

typedef enum ResGroupLimitType
{
	RESGROUP_LIMIT_TYPE_UNKNOWN = 0,

	RESGROUP_LIMIT_TYPE_CONCURRENCY,
	RESGROUP_LIMIT_TYPE_CPU,
	RESGROUP_LIMIT_TYPE_MEMORY,
	RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA,
	RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO,
	RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR,

	RESGROUP_LIMIT_TYPE_COUNT,
} ResGroupLimitType;

#endif   /* PG_RESGROUP_H */
