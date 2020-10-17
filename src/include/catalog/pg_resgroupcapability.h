/*-------------------------------------------------------------------------
 *
 * pg_resgroupcapability.h
 *	  definition of the system "resource group capability" relation (pg_resgroupcapability).
 *
 *
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_RESGROUPCAPABILITY_H
#define PG_RESGROUPCAPABILITY_H

#include "catalog/genbki.h"
#include "catalog/pg_resgroupcapability_d.h"

CATALOG(pg_resgroupcapability,6439,ResGroupCapabilityRelationId) BKI_SHARED_RELATION
{
	Oid			resgroupid;	/* OID of the group with this capability  */

	int16		reslimittype;	/* resource limit type id (RESGROUP_LIMIT_TYPE_XXX) */

#ifdef CATALOG_VARLEN
	text		value;		/* resource limit (opaque type)  */
#endif
} FormData_pg_resgroupcapability;


/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(resgroupid REFERENCES pg_resgroup(oid));

/* ----------------
 *	Form_pg_resgroupcapability corresponds to a pointer to a tuple with
 *	the format of pg_resgroupcapability relation.
 * ----------------
 */
typedef FormData_pg_resgroupcapability *Form_pg_resgroupcapability;

#endif   /* PG_RESGROUPCAPABILITY_H */
