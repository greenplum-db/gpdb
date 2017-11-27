/*-------------------------------------------------------------------------
 *
 * toasting.h
 *	  This file provides some definitions to support creation of toast tables
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/toasting.h,v 1.5 2009/01/01 17:23:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TOASTING_H
#define TOASTING_H

/*
 * toasting.c prototypes
 */
extern void AlterTableCreateToastTable(Oid relOid, bool is_part_child);

extern void BootstrapToastTable(char *relName,
					Oid toastOid, Oid toastIndexOid);

/*
 * This macro is just to keep the C compiler from spitting up on the
 * upcoming commands for genbki.sh.
 */
#define DECLARE_TOAST(name,toastoid,indexoid) extern int no_such_variable


/*
 * What follows are lines processed by genbki.sh to create the statements
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
DECLARE_TOAST(pg_statistic, 2840, 2841);

/* shared catalogs */
DECLARE_TOAST(pg_authid, 2842, 2843);
#define PgAuthidToastTable 2842
#define PgAuthidToastIndex 2843
DECLARE_TOAST(pg_database, 2844, 2845);
#define PgDatabaseToastTable 2844
#define PgDatabaseToastIndex 2845
DECLARE_TOAST(pg_shdescription, 2846, 2847);
#define PgShdescriptionToastTable 2846
#define PgShdescriptionToastIndex 2847

/* relation id: 5036 - gp_segment_configuration 20101122 */
DECLARE_TOAST(gp_segment_configuration, 6092, 6093);
#define GpSegmentConfigToastTable	6092
#define GpSegmentConfigToastIndex	6093
/* relation id: 5033 - pg_filespace_entry 20101122 */
DECLARE_TOAST(pg_filespace_entry, 6094, 6095);
#define PgFileSpaceEntryToastTable	6094
#define PgFileSpaceEntryToastIndex	6095
/* relation id: 3231 - pg_attribute_encoding 20110727 */
DECLARE_TOAST(pg_attribute_encoding, 3233, 3234);
#define PgAttributeEncodingToastTable	3233
#define PgAttributeEncodingToastIndex	3234
/* relation id: 3220 - pg_type_encoding 20110727 */
DECLARE_TOAST(pg_type_encoding, 3222, 3223);
#define PgTypeEncodingToastTable	3222
#define PgTypeEncodingToastIndex	3223
/* relation id: 9903 - pg_partition_encoding 20110814 */
DECLARE_TOAST(pg_partition_encoding, 9905, 9906);
#define PgPartitionEncodingToastTable	9905
#define PgPartitionEncodingToastIndex	9906

#endif   /* TOASTING_H */
