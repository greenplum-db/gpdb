#include "postgres.h"

#include "access/aocssegfiles.h"
#include "catalog/pg_appendonly_fn.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbvars.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "miscadmin.h"

/**
 * Statistics related parameters.
 */

double			analyze_relative_error = 0.25;
bool			gp_statistics_pullup_from_child_partition = FALSE;
bool			gp_statistics_use_fkeys = FALSE;
int				gp_statistics_blocks_target = 25;
double			gp_statistics_ndistinct_scaling_ratio_threshold = 0.10;
double			gp_statistics_sampling_threshold = 10000;

/**
 * This method estimates the number of tuples and pages in a heaptable relation. Getting the number of blocks is straightforward.
 * Estimating the number of tuples is a little trickier. There are two factors that complicate this:
 * 	1. Tuples may be of variable length.
 * 	2. There may be dead tuples lying around.
 * To do this, it chooses a certain number of blocks (as determined by a guc) randomly. The process of choosing is not strictly
 * uniformly random since we have a target number of blocks in mind. We start processing blocks in order and choose an block 
 * with a probability p determined by the ratio of target to total blocks. It is possible that we get really unlucky and reject
 * a large number of blocks up front. We compensate for this by increasing p dynamically. Thus, we are guaranteed to choose the target number
 * of blocks. We read all heaptuples from these blocks and keep count of number of live tuples. We scale up this count to
 * estimate reltuples. Relpages is an exact value.
 * 
 * Input:
 * 	rel - Relation. Must be a heaptable. 
 * 
 * Output:
 * 	reltuples - estimated number of tuples in relation.
 * 	relpages  - exact number of pages.
 */
static void
gp_statistics_estimate_reltuples_relpages_heap(Relation rel, float4 *reltuples, float4 *relpages)
{
	float4		nrowsseen = 0;	/* # rows seen (including dead rows) */
	float4		nrowsdead = 0;	/* # rows dead */
	float4		totalEmptyPages = 0; /* # of empty pages with only dead rows */
	float4		totalSamplePages = 0; /* # of pages sampled */

	BlockNumber nblockstotal = 0;	/* nblocks in relation */
	BlockNumber nblockstarget = (BlockNumber) gp_statistics_blocks_target; 
	BlockNumber nblocksseen = 0;
	int			j;		/* counter */
	
	/**
	 * Ensure that the right kind of relation with the right kind of storage is passed to us.
	 */
	Assert(rel->rd_rel->relkind == RELKIND_RELATION);
	Assert(RelationIsHeap(rel));
					
	nblockstotal = RelationGetNumberOfBlocks(rel);

	if (nblockstotal == 0 || nblockstarget == 0)
	{		
		/**
		 * If there are no blocks, there cannot be tuples.
		 */
		*reltuples = 0.0;
		*relpages = 0.0;
		return; 
	}
		
	Snapshot	snapshot = RegisterSnapshot(GetLatestSnapshot());
	for (j = 0; j < nblockstotal; j++)
	{
		/**
		 * Threshold is dynamically adjusted based on how many blocks we need to examine and how many blocks
		 * are left.
		 */
		double threshold = ((double) nblockstarget - nblocksseen)/((double) nblockstotal - j);
		
		/**
		 * Random dice thrown to determine if current block is chosen.
		 */
		double diceValue = ((double) random()) / ((double) MAX_RANDOM_VALUE);
		
		if (threshold >= 1.0 || diceValue <= threshold)
		{
			totalSamplePages++;
			/**
			 * Block j shall be examined!
			 */
			BlockNumber targblock = j;
			Buffer		targbuffer;
			Page		targpage;
			OffsetNumber targoffset,
						maxoffset;

			/**
			 * Check for cancellations.
			 */
			CHECK_FOR_INTERRUPTS();

			/*
			 * We must maintain a pin on the target page's buffer to ensure that
			 * the maxoffset value stays good (else concurrent VACUUM might delete
			 * tuples out from under us).  Hence, pin the page until we are done
			 * looking at it.  We don't maintain a lock on the page, so tuples
			 * could get added to it, but we ignore such tuples.
			 */

			targbuffer = ReadBuffer(rel, targblock);
			LockBuffer(targbuffer, BUFFER_LOCK_SHARE);
			targpage = BufferGetPage(targbuffer);
			maxoffset = PageGetMaxOffsetNumber(targpage);

			/* Figure out overall nrowsdead/nrowsseen ratio */
			/* Figure out # of empty pages based on page level #rowsseen and #rowsdead.*/
			float4 pageRowsSeen = 0.0;
			float4 pageRowsDead = 0.0;

			/* Inner loop over all tuples on the selected block. */
			for (targoffset = FirstOffsetNumber; targoffset <= maxoffset; targoffset++)
			{
				ItemId itemid;
				itemid = PageGetItemId(targpage, targoffset);
				nrowsseen++;
				pageRowsSeen++;
				if(!ItemIdIsNormal(itemid))
				{
					nrowsdead += 1;
					pageRowsDead++;
				}
				else
				{
					HeapTupleData targtuple;
					ItemPointerSet(&targtuple.t_self, targblock, targoffset);
					targtuple.t_data = (HeapTupleHeader) PageGetItem(targpage, itemid);
					targtuple.t_len = ItemIdGetLength(itemid);

					if(!HeapTupleSatisfiesVisibility(rel, &targtuple, snapshot, targbuffer))
					{
						nrowsdead += 1;
						pageRowsDead++;
					}
				}
			}

			/* Now release the pin on the page */
			UnlockReleaseBuffer(targbuffer);

			/* detect empty pages: pageRowsSeen == pageRowsDead, also log the nrowsseen (total) and nrowsdead (total) */
			if (pageRowsSeen == pageRowsDead && pageRowsSeen > 0)
			{
				totalEmptyPages++;
			}

			nblocksseen++;
		}		
	}

	Assert(nblocksseen > 0);
	/**
	 * To calculate reltuples, scale up the number of live rows per block seen to the total number
	 * of blocks. 
	 */
	*reltuples = ceil((nrowsseen - nrowsdead) * nblockstotal / nblocksseen);
	*relpages = nblockstotal;

	if (totalSamplePages * 0.5 <= totalEmptyPages && totalSamplePages != 0)
	{
		/*
		 * LOG empty pages of bloated table for each segments.
		 */
		elog(DEBUG1, "ANALYZE detected 50%% or more empty pages (%f empty out of %f pages), please run VACUUM FULL for accurate estimation.", totalEmptyPages, totalSamplePages);
	}

	UnregisterSnapshot(snapshot);

	return;
}

/**
 * This method estimates the number of tuples and pages in an vertical oriented append-only relation. 
 * Require access to segment catalogs to determine reltuples.
 * Relpages is obtained by fudging AO block sizes.
 * 
 * Input:
 * 	rel - Relation. Must be an AO CS table.
 * 
 * Output:
 * 	reltuples - estimated number of tuples in relation.
 * 	relpages  - exact number of pages.
 */

static void
gp_statistics_estimate_reltuples_relpages_ao_cs(Relation rel, float4 *reltuples, float4 *relpages)
{
	AOCSFileSegInfo	**aocsInfo = NULL;
	int				nsegs = 0;
	double			totalBytes = 0;
	int64 hidden_tupcount;
	AppendOnlyVisimap visimap;
	Snapshot	snapshot = RegisterSnapshot(GetLatestSnapshot());

	/**
	 * Ensure that the right kind of relation with the right type of storage is passed to us.
	 */
	Assert(rel->rd_rel->relkind == RELKIND_RELATION);
	Assert(RelationIsAoCols(rel));
	
	*reltuples = 0.0;
	*relpages = 0.0;
	
    /* get table level statistics from the pg_aoseg table */
	aocsInfo = GetAllAOCSFileSegInfo(rel, snapshot, &nsegs);
	if (aocsInfo)
	{
		int i = 0;
		int j = 0;
		for(i = 0; i < nsegs; i++)
		{
			for(j = 0; j < RelationGetNumberOfAttributes(rel); j++)
			{
				AOCSVPInfoEntry *e = getAOCSVPEntry(aocsInfo[i], j);
				Assert(e);
				totalBytes += e->eof_uncompressed;
			}

			/* Do not include tuples from an awaiting drop segment file */
			if (aocsInfo[i]->state != AOSEG_STATE_AWAITING_DROP)
			{
				*reltuples += aocsInfo[i]->total_tupcount;
			}
		}
		/**
		 * The planner doesn't understand AO's blocks, so need this method to try to fudge up a number for
		 * the planner. 
		 */
		*relpages = RelationGuessNumberOfBlocks(totalBytes);
	}

	AppendOnlyVisimap_Init(&visimap, rel->rd_appendonly->visimaprelid, rel->rd_appendonly->visimapidxid, AccessShareLock, snapshot);
	hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&visimap);
	AppendOnlyVisimap_Finish(&visimap, AccessShareLock);

	(*reltuples) -= hidden_tupcount;

	UnregisterSnapshot(snapshot);
	  
	return;
}
/**
 * This method estimates the number of visible tuples and pages in an append-only relation. AO tables maintain accurate
 * tuple counts in the catalog. Therefore, we will require access to segment catalogs to determine reltuples.
 * Relpages is obtained by fudging AO block sizes. In addition, we substract the number of invisible
 * tuples from the total number of tuples.
 * 
 * Input:
 * 	rel - Relation. Must be an AO table.
 * 
 * Output:
 * 	reltuples - estimated number of tuples in relation.
 * 	relpages  - exact number of pages.
 */

static void gp_statistics_estimate_reltuples_relpages_ao_rows(Relation rel, float4 *reltuples, float4 *relpages)
{
	FileSegTotals		*fstotal;
	AppendOnlyVisimap visimap;
	int64 hidden_tupcount = 0;
	Snapshot	snapshot = RegisterSnapshot(GetLatestSnapshot());

	/**
	 * Ensure that the right kind of relation with the right type of storage is passed to us.
	 */
	Assert(rel->rd_rel->relkind == RELKIND_RELATION);
	Assert(RelationIsAoRows(rel));
	
	fstotal = GetSegFilesTotals(rel, snapshot);
	Assert(fstotal);
	/**
	 * The planner doesn't understand AO's blocks, so need this method to try to fudge up a number for
	 * the planner.
	 */
	if (fstotal->totalbytes > 0)
	{
		*relpages = RelationGuessNumberOfBlocks((double)fstotal->totalbytes);
	}

	AppendOnlyVisimap_Init(&visimap,
						   rel->rd_appendonly->visimaprelid,
						   rel->rd_appendonly->visimapidxid,
						   AccessShareLock,
						   snapshot);
	hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&visimap);
	AppendOnlyVisimap_Finish(&visimap, AccessShareLock);

	/**
	 * The number of tuples in AO table is known accurately. Therefore, we just utilize this value.
	 */
	*reltuples = (double)(fstotal->totaltuples - hidden_tupcount);

	pfree(fstotal);
	UnregisterSnapshot(snapshot);
	return;
}

/**
 * Given the oid of a relation, this method calculates reltuples, relpages. This only looks up
 * local information (on master or segments). It produces meaningful values for AO and
 * heap tables and returns [0.0,0.0] for all other relations.
 * Input: 
 * 	relationoid
 * Output:
 * 	array of two values [reltuples,relpages]
 */
Datum
gp_statistics_estimate_reltuples_relpages_oid(PG_FUNCTION_ARGS)
{
	
	float4		relpages = 0.0;		
	float4		reltuples = 0.0;			
	Oid			relOid = PG_GETARG_OID(0);
	Datum		values[2];
	ArrayType   *result;
	
	Relation rel = try_relation_open(relOid, AccessShareLock, false);

	if (rel != NULL)
	{
		if (rel->rd_rel->relkind == RELKIND_RELATION)
		{
			if (RelationIsHeap(rel))
			{
				gp_statistics_estimate_reltuples_relpages_heap(rel, &reltuples, &relpages);
			}
			else if (RelationIsAoRows(rel))
			{
				gp_statistics_estimate_reltuples_relpages_ao_rows(rel, &reltuples, &relpages);
			}
			else if	(RelationIsAoCols(rel))
			{
				gp_statistics_estimate_reltuples_relpages_ao_cs(rel, &reltuples, &relpages);
			}
		}
		else if (rel->rd_rel->relkind == RELKIND_INDEX)
		{
			reltuples = 1.0;
			relpages = RelationGetNumberOfBlocks(rel);
		}
		else
		{
			/**
			 * Should we silently return [0.0,0.0] or error out? Currently, we choose option 1.
			 */
		}
		relation_close(rel, AccessShareLock);
	}
	else
	{
		/**
		 * Should we silently return [0.0,0.0] or error out? Currently, we choose option 1.
		 */
	}
	
	values[0] = Float4GetDatum(reltuples);
	values[1] = Float4GetDatum(relpages);

	result = construct_array(values, 2,
					FLOAT4OID,
					sizeof(float4), true, 'i');

	PG_RETURN_ARRAYTYPE_P(result);
}
