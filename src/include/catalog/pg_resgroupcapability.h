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

CATALOG(pg_resgroupcapability,6439) BKI_SHARED_RELATION
{
	Oid		resgroupid;	/* OID of the group with this capability  */

	int16		reslimittype;	/* resource limit type id (RESGROUP_LIMIT_TYPE_XXX) */

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

DATA(insert ( 6437, 2, 30, 30 ));

DATA(insert ( 6437, 3, 0, 0 ));

DATA(insert ( 6437, 4, 80, 80 ));

DATA(insert ( 6437, 5, "128 MB", "128 MB" ));

DATA(insert ( 6437, 6, 0, 0 ));

DATA(insert ( 6437, 7, "-1", "-1" ));

DATA(insert ( 6438, 1, 10, 10 ));

DATA(insert ( 6438, 2, 10, 10 ));

DATA(insert ( 6438, 3, 10, 10 ));

DATA(insert ( 6438, 4, 80, 80 ));

DATA(insert ( 6438, 5, "128 MB", "128 MB" ));

DATA(insert ( 6438, 6, 0, 0 ));

DATA(insert ( 6438, 7, "-1", "-1" ));

#endif   /* PG_RESGROUPCAPABILITY_H */
