/*-------------------------------------------------------------------------
 *
 * tablespacedirnode.h
 *	  Physical access information for tablespace directories.
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/storage/tablespacedirnode.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef TABLESPACEDIRNODE_H
#define TABLESPACEDIRNODE_H

/*
 * Represents a tablespace directory within a filespace.
 */
typedef struct TablespaceDirNode
{
	Oid		filespace;

	Oid		tablespace;
} TablespaceDirNode;

#endif   /* TABLESPACEDIRNODE_H */

