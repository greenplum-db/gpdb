/*-------------------------------------------------------------------------
 *
 * aovisimap.h
 *   Definitions to support creation of aovisimap tables.
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/aovisimap.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AOVISIMAP_H
#define AOVISIMAP_H

/*
 * Macros to the attribute number for each attribute
 * in the AO visimap relation.
 */
#define Natts_pg_aovisimap              3
#define Anum_pg_aovisimap_segno         1
#define Anum_pg_aovisimap_firstrownum   2
#define Anum_pg_aovisimap_visimap       3

extern void AlterTableCreateAoVisimapTable(Oid relOid, bool is_part_child);

#endif
