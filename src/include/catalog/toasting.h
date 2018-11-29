/*-------------------------------------------------------------------------
 *
 * toasting.h
 *	  This file provides some definitions to support creation of toast tables
 *
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/toasting.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef TOASTING_H
#define TOASTING_H

#include "storage/lock.h"

/*
 * toasting.c prototypes
 */
extern void NewRelationCreateToastTable(Oid relOid, Datum reloptions,
									   bool is_part_child, bool is_part_parent);
extern void NewHeapCreateToastTable(Oid relOid, Datum reloptions,
									LOCKMODE lockmode,
									bool is_part_child, bool is_part_parent);
extern void AlterTableCreateToastTable(Oid relOid, Datum reloptions,
									   LOCKMODE lockmode,
									   bool is_part_child, bool is_part_parent);
extern void BootstrapToastTable(char *relName,
					Oid toastOid, Oid toastIndexOid);

/*
 * This macro is just to keep the C compiler from spitting up on the
 * upcoming commands for genbki.pl.
 */
#define DECLARE_TOAST(name,toastoid,indexoid) extern int no_such_variable


/*
 * What follows are lines processed by genbki.pl to create the statements
 * the bootstrap parser will turn into BootstrapToastTable commands.
 * Each line specifies the system catalog that needs a toast table,
 * the OID to assign to the toast table, and the OID to assign to the
 * toast table's index.  The reason we hard-wire these OIDs is that we
 * need stable OIDs for shared relations, and that includes toast tables
 * of shared relations.
 */

/* normal catalogs */
DECLARE_TOAST(pg_extension, 5510, 5511);
DECLARE_TOAST(pg_attrdef, 2830, 2831);
DECLARE_TOAST(pg_constraint, 2832, 2833);
DECLARE_TOAST(pg_description, 2834, 2835);
DECLARE_TOAST(pg_proc, 2836, 2837);
DECLARE_TOAST(pg_rewrite, 2838, 2839);
DECLARE_TOAST(pg_seclabel, 3598, 3599);
DECLARE_TOAST(pg_statistic, 2840, 2841);
DECLARE_TOAST(pg_trigger, 2336, 2337);

/* shared catalogs */
DECLARE_TOAST(pg_shdescription, 2846, 2847);
#define PgShdescriptionToastTable 2846
#define PgShdescriptionToastIndex 2847
DECLARE_TOAST(pg_db_role_setting, 2966, 2967);
#define PgDbRoleSettingToastTable 2966
#define PgDbRoleSettingToastIndex 2967

/* relation id: 5036 - gp_segment_configuration 20101122 */
DECLARE_TOAST(gp_segment_configuration, 6092, 6093);
#define GpSegmentConfigToastTable	6092
#define GpSegmentConfigToastIndex	6093
/* relation id: 6231 - pg_attribute_encoding 20110727 */
DECLARE_TOAST(pg_attribute_encoding, 6233, 6234);
#define PgAttributeEncodingToastTable	6233
#define PgAttributeEncodingToastIndex	6234
/* relation id: 6220 - pg_type_encoding 20110727 */
DECLARE_TOAST(pg_type_encoding, 6222, 6223);
#define PgTypeEncodingToastTable	6222
#define PgTypeEncodingToastIndex	6223
/* relation id: 9903 - pg_partition_encoding 20110814 */
DECLARE_TOAST(pg_partition_encoding, 9905, 9906);
#define PgPartitionEncodingToastTable	9905
#define PgPartitionEncodingToastIndex	9906

#endif   /* TOASTING_H */
