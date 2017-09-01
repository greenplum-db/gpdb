/*-------------------------------------------------------------------------
 *
 * aoblkdir.h
 *   Definitions to support creation of aoblkdir tables.
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/aoblkdir.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AOBLKDIR_H
#define AOBLKDIR_H

/*
 * Macros to the attribute number for each attribute
 * in the block directory relation.
 */
#define Natts_pg_aoblkdir              4
#define Anum_pg_aoblkdir_segno         1
#define Anum_pg_aoblkdir_columngroupno 2
#define Anum_pg_aoblkdir_firstrownum   3
#define Anum_pg_aoblkdir_minipage      4

extern void AlterTableCreateAoBlkdirTable(Oid relOid, bool is_part_child);

#endif
