/*-------------------------------------------------------------------------
 *
 * gp_partition_template.h
 *
 *	  definition of the "partitioned table" storing sub partition template
 *	  system catalog (gp_partition_template)
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/gp_partition_template.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */
#ifndef GP_PARTITION_TEMPLATE_H
#define GP_PARTITION_TEMPLATE_H

#include "catalog/genbki.h"
#include "catalog/gp_partition_template_d.h"
#include "nodes/parsenodes.h"

/* ----------------
 *		gp_partition_template definition.  cpp turns this into
 *		typedef struct FormData_gp_partition_template
 * ----------------
 */
CATALOG(gp_partition_template,5022,PartitionTemplateRelationId)
{
	Oid			relid;		/* partitioned table oid */
	int16       level;

#ifdef CATALOG_VARLEN
	pg_node_tree template;
#endif
} FormData_gp_partition_template;

FOREIGN_KEY(relid REFERENCES pg_class(oid));

/* ----------------
 *		Form_gp_partition_template corresponds to a pointer to a tuple with
 *		the format of gp_partition_template relation.
 * ----------------
 */
typedef FormData_gp_partition_template *Form_gp_partition_template;

extern void StoreGpPartitionTemplate(Oid relid, int32 level,
									 GpPartitionDefinition *gpPartDef);
extern GpPartitionDefinition *GetGpPartitionTemplate(Oid relid, int32 level);
extern void RemoveGpPartitionTemplateByRelId(Oid relid);
extern bool RemoveGpPartitionTemplate(Oid relid, int32 level);

#endif							/* GP_PARTITION_TEMPLATE_H */
