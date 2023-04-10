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
#include "catalog/pg_resgroup_d.h"

/* ----------------
 *	pg_resgroup definition.  cpp turns this into
 *	typedef struct FormData_pg_resgroup
 * ----------------
 */

CATALOG(pg_resgroup,6436,ResGroupRelationId) BKI_SHARED_RELATION
{
	Oid			oid;			/* oid */

	NameData	rsgname;		/* name of resource group */

	Oid			parent;			/* parent resource group */
} FormData_pg_resgroup;

/* no foreign keys */

/* ----------------
 *	Form_pg_resgroup corresponds to a pointer to a tuple with
 *	the format of pg_resgroup relation.
 * ----------------
 */
typedef FormData_pg_resgroup *Form_pg_resgroup;

/* ----------------
 *	pg_resgroupcapability definition.  cpp turns this into
 *	typedef struct FormData_pg_resgroupcapability
 * ----------------
 */

typedef enum ResGroupLimitType
{
	RESGROUP_LIMIT_TYPE_UNKNOWN = 0,

	RESGROUP_LIMIT_TYPE_CONCURRENCY,	/* concurrency */
	RESGROUP_LIMIT_TYPE_CPU,			/* cpu_hard_quota_limit */
	RESGROUP_LIMIT_TYPE_CPU_SHARES,		/* cpu_soft_priority */
	RESGROUP_LIMIT_TYPE_CPUSET,			/* cpuset */
	RESGROUP_LIMIT_TYPE_MEMORY_LIMIT,		/* memory_limit */

	RESGROUP_LIMIT_TYPE_COUNT,
} ResGroupLimitType;

#endif   /* PG_RESGROUP_H */
