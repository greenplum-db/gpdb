/*-------------------------------------------------------------------------
 *
 * aoseg.h
 *	  This file provides some definitions to support creation of aoseg tables
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/aoseg.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AOSEG_H
#define AOSEG_H

/*
 * aoseg.c prototypes
 */
extern void AlterTableCreateAoSegTable(Oid relOid, bool is_part_child);

#endif   /* AOSEG_H */
