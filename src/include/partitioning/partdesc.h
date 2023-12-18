/*-------------------------------------------------------------------------
 *
 * partdesc.h
 *
 * Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * src/include/utils/partdesc.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef PARTDESC_H
#define PARTDESC_H

#include "partitioning/partdefs.h"
#include "utils/relcache.h"

/*
 * Information about partitions of a partitioned table.
 */
typedef struct PartitionDescData
{
	int			nparts;			/* Number of partitions */
	Oid		   *oids;			/* Array of 'nparts' elements containing
								 * partition OIDs in order of the their bounds */
	bool	   *is_leaf;		/* Array of 'nparts' elements storing whether
								 * the corresponding 'oids' element belongs to
								 * a leaf partition or not */
	PartitionBoundInfo boundinfo;	/* collection of partition bounds */

	/* Caching fields to cache lookups in get_partition_for_tuple() */

	/*
	 * Index into the PartitionBoundInfo's datum array for the last found
	 * partition or -1 if none.
	 */
	int			last_found_datum_index;

	/*
	 * Partition index of the last found partition or -1 if none has been
	 * found yet.
	 */
	int			last_found_part_index;

	/*
	 * For LIST partitioning, this is the number of times in a row that the
	 * datum we're looking for a partition for matches the datum in the
	 * last_found_datum_index index of the boundinfo->datums array.  For RANGE
	 * partitioning, this is the number of times in a row we've found that the
	 * datum we're looking for a partition for falls into the range of the
	 * partition corresponding to the last_found_datum_index index of the
	 * boundinfo->datums array.
	 */
	int			last_found_count;
} PartitionDescData;

extern PartitionDesc RelationRetrievePartitionDesc(Relation rel);
extern void RelationBuildPartitionDesc(Relation rel);
extern void RelationValidatePartitionDesc(Relation rel);

extern PartitionDirectory CreatePartitionDirectory(MemoryContext mcxt);
extern PartitionDesc PartitionDirectoryLookup(PartitionDirectory, Relation);
extern void DestroyPartitionDirectory(PartitionDirectory pdir);

extern Oid	get_default_oid_from_partdesc(PartitionDesc partdesc);

extern bool equalPartitionDescs(PartitionKey key, PartitionDesc partdesc1,
								PartitionDesc partdesc2);

#endif							/* PARTCACHE_H */
