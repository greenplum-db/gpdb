/*-------------------------------------------------------------------------
 *
 * cdbutil.c
 *	  Internal utility support functions for Greenplum Database/PostgreSQL.
 *
 * Portions Copyright (c) 2005-2011, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbutil.c
 *
 * NOTES
 *
 *	- According to src/backend/executor/execHeapScan.c
 *		"tuples returned by heap_getnext() are pointers onto disk
 *		pages and were not created with palloc() and so should not
 *		be pfree()'d"
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "catalog/gp_segment_config.h"
#include "nodes/makefuncs.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "catalog/gp_id.h"
#include "catalog/gp_segment_config.h"
#include "catalog/indexing.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbmotion.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/ml_ipc.h"			/* listener_setup */
#include "cdb/cdbtm.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "libpq/ip.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbfts.h"
#include "storage/ipc.h"
#include "postmaster/fts.h"
#include "catalog/namespace.h"
#include "utils/gpexpand.h"

#define MAX_CACHED_1_GANGS 1

#define INCR_COUNT(cdbinfo, arg) \
	(cdbinfo)->arg++; \
	(cdbinfo)->cdbs->arg++;

#define DECR_COUNT(cdbinfo, arg) \
	(cdbinfo)->arg--; \
	(cdbinfo)->cdbs->arg--; \
	Assert((cdbinfo)->arg >= 0); \
	Assert((cdbinfo)->cdbs->arg >= 0); \

MemoryContext CdbComponentsContext = NULL;
static CdbComponentDatabases *cdb_component_dbs = NULL;

/*
 * Helper Functions
 */
static CdbComponentDatabases *getCdbComponentInfo(bool DnsLookupFailureIsError);
static void cleanupComponentIdleQEs(CdbComponentDatabaseInfo *cdi, bool includeWriter);

static int	CdbComponentDatabaseInfoCompare(const void *p1, const void *p2);

static void getAddressesForDBid(CdbComponentDatabaseInfo *c, int elevel);
static HTAB *hostSegsHashTableInit(void);

static HTAB *segment_ip_cache_htab = NULL;

int numsegmentsFromQD = -1;

typedef struct SegIpEntry
{
	char		key[NAMEDATALEN];
	char		hostinfo[NI_MAXHOST];
} SegIpEntry;

typedef struct HostSegsEntry
{
	char		hostip[INET6_ADDRSTRLEN];
	int			segmentCount;
} HostSegsEntry;

/*
 *  Internal function to initialize each component info
 */
static CdbComponentDatabases *
getCdbComponentInfo(bool DNSLookupAsError)
{
	MemoryContext oldContext;
	CdbComponentDatabaseInfo *pOld = NULL;
	CdbComponentDatabaseInfo *cdbInfo;
	CdbComponentDatabases *component_databases = NULL;

	Relation	gp_seg_config_rel;
	HeapTuple	gp_seg_config_tuple = NULL;
	HeapScanDesc gp_seg_config_scan;

	/*
	 * Initial size for info arrays.
	 */
	int			segment_array_size = 500;
	int			entry_array_size = 4;	/* we currently support a max of 2 */

	/*
	 * isNull and attr are used when getting the data for a specific column
	 * from a HeapTuple
	 */
	bool		isNull;
	Datum		attr;

	/*
	 * Local variables for fields from the rows of the tables that we are
	 * reading.
	 */
	int			dbid;
	int			content;

	char		role;
	char		preferred_role;
	char		mode = 0;
	char		status = 0;

	int			i;
	int			x = 0;

	bool		found;
	HostSegsEntry *hsEntry;

	if (!CdbComponentsContext)
		CdbComponentsContext = AllocSetContextCreate(TopMemoryContext, "cdb components Context",
								ALLOCSET_DEFAULT_MINSIZE,
								ALLOCSET_DEFAULT_INITSIZE,
								ALLOCSET_DEFAULT_MAXSIZE);

	oldContext = MemoryContextSwitchTo(CdbComponentsContext);

	HTAB	   *hostSegsHash = hostSegsHashTableInit();

	/*
	 * Allocate component_databases return structure and
	 * component_databases->segment_db_info array with an initial size of 128,
	 * and component_databases->entry_db_info with an initial size of 4.  If
	 * necessary during row fetching, we grow these by doubling each time we
	 * run out.
	 */
	component_databases = palloc0(sizeof(CdbComponentDatabases));

	component_databases->numActiveQEs = 0;
	component_databases->numIdleQEs = 0;
	component_databases->qeCounter = 0;

	component_databases->segment_db_info =
		(CdbComponentDatabaseInfo *) palloc0(sizeof(CdbComponentDatabaseInfo) * segment_array_size);

	component_databases->entry_db_info =
		(CdbComponentDatabaseInfo *) palloc0(sizeof(CdbComponentDatabaseInfo) * entry_array_size);

	gp_seg_config_rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);

	gp_seg_config_scan = heap_beginscan_catalog(gp_seg_config_rel, 0, NULL);

	while (HeapTupleIsValid(gp_seg_config_tuple = heap_getnext(gp_seg_config_scan, ForwardScanDirection)))
	{
		/*
		 * Grab the fields that we need from gp_segment_configuration.  We do
		 * this first, because until we read them, we don't know whether this
		 * is an entry database row or a segment database row.
		 */
		CdbComponentDatabaseInfo *pRow;

		/*
		 * dbid
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_dbid, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		dbid = DatumGetInt16(attr);

		/*
		 * content
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_content, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		content = DatumGetInt16(attr);

		/*
		 * role
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_role, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		role = DatumGetChar(attr);

		/*
		 * preferred-role
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_preferred_role, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		preferred_role = DatumGetChar(attr);

		/*
		 * mode
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_mode, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		mode = DatumGetChar(attr);

		/*
		 * status
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_status, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		status = DatumGetChar(attr);

		/*
		 * Determine which array to place this rows data in: entry or segment,
		 * based on the content field.
		 */
		if (content >= 0)
		{
			/*
			 * if we have a dbid bigger than our array we'll have to grow the
			 * array. (MPP-2104)
			 */
			if (dbid >= segment_array_size || component_databases->total_segment_dbs >= segment_array_size)
			{
				/*
				 * Expand CdbComponentDatabaseInfo array if we've used up
				 * currently allocated space
				 */
				segment_array_size = Max((segment_array_size * 2), dbid * 2);
				pOld = component_databases->segment_db_info;
				component_databases->segment_db_info = (CdbComponentDatabaseInfo *)
					repalloc(pOld, sizeof(CdbComponentDatabaseInfo) * segment_array_size);
			}

			pRow = &component_databases->segment_db_info[component_databases->total_segment_dbs];
			component_databases->total_segment_dbs++;
		}
		else
		{
			if (component_databases->total_entry_dbs >= entry_array_size)
			{
				/*
				 * Expand CdbComponentDatabaseInfo array if we've used up
				 * currently allocated space
				 */
				entry_array_size *= 2;
				pOld = component_databases->entry_db_info;
				component_databases->entry_db_info = (CdbComponentDatabaseInfo *)
					repalloc(pOld, sizeof(CdbComponentDatabaseInfo) * entry_array_size);
			}

			pRow = &component_databases->entry_db_info[component_databases->total_entry_dbs];
			component_databases->total_entry_dbs++;
		}

		pRow->cdbs = component_databases;
		pRow->freelist = NIL;
		pRow->dbid = dbid;
		pRow->segindex = content;
		pRow->role = role;
		pRow->preferred_role = preferred_role;
		pRow->mode = mode;
		pRow->status = status;
		pRow->numIdleQEs = 0;
		pRow->numActiveQEs = 0;

		/*
		 * hostname
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_hostname, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		pRow->hostname = TextDatumGetCString(attr);

		/*
		 * address
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_address, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		pRow->address = TextDatumGetCString(attr);

		/*
		 * port
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_port, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		pRow->port = DatumGetInt32(attr);

		pRow->hostip = NULL;
		getAddressesForDBid(pRow, DNSLookupAsError ? ERROR : LOG);

		/*
		 * We make sure we get a valid hostip for primary here,
		 * if hostip for mirrors can not be get, ignore the error.
		 */
		if (pRow->hostaddrs[0] == NULL &&
			pRow->role == GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY)
			ereport(DNSLookupAsError ? ERROR : LOG,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					errmsg("cannot resolve network address for dbid=%d", dbid)));

		if (pRow->hostaddrs[0] != NULL)
			pRow->hostip = pstrdup(pRow->hostaddrs[0]);
		AssertImply(pRow->hostip, strlen(pRow->hostip) <= INET6_ADDRSTRLEN);

		if (pRow->role != GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY || pRow->hostip == NULL)
			continue;

		hsEntry = (HostSegsEntry *) hash_search(hostSegsHash, pRow->hostip, HASH_ENTER, &found);
		if (found)
			hsEntry->segmentCount++;
		else
			hsEntry->segmentCount = 1;
	}

	/*
	 * We're done with the catalog entries, cleanup them up, closing all the
	 * relations we opened.
	 */
	heap_endscan(gp_seg_config_scan);
	heap_close(gp_seg_config_rel, AccessShareLock);

	/*
	 * Validate that there exists at least one entry and one segment database
	 * in the configuration
	 */
	if (component_databases->total_segment_dbs == 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_CARDINALITY_VIOLATION),
				 errmsg("Greenplum Database number of segment databases cannot be 0")));
	}
	if (component_databases->total_entry_dbs == 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_CARDINALITY_VIOLATION),
				 errmsg("Greenplum Database number of entry databases cannot be 0")));
	}

	/*
	 * Now sort the data by segindex, isprimary desc
	 */
	qsort(component_databases->segment_db_info,
		  component_databases->total_segment_dbs, sizeof(CdbComponentDatabaseInfo),
		  CdbComponentDatabaseInfoCompare);

	qsort(component_databases->entry_db_info,
		  component_databases->total_entry_dbs, sizeof(CdbComponentDatabaseInfo),
		  CdbComponentDatabaseInfoCompare);

	/*
	 * Now count the number of distinct segindexes. Since it's sorted, this is
	 * easy.
	 */
	for (i = 0; i < component_databases->total_segment_dbs; i++)
	{
		if (i == 0 ||
			(component_databases->segment_db_info[i].segindex != component_databases->segment_db_info[i - 1].segindex))
		{
			component_databases->total_segments++;
		}
	}

	/*
	 * Now validate that our identity is present in the entry databases
	 */
	for (i = 0; i < component_databases->total_entry_dbs; i++)
	{
		cdbInfo = &component_databases->entry_db_info[i];

		if (cdbInfo->dbid == GpIdentity.dbid && cdbInfo->segindex == GpIdentity.segindex)
		{
			break;
		}
	}
	if (i == component_databases->total_entry_dbs)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("Cannot locate entry database represented by this db in gp_segment_configuration: dbid %d content %d",
						GpIdentity.dbid, GpIdentity.segindex)));
	}

	/*
	 * Now validate that the segindexes for the segment databases are between
	 * 0 and (numsegments - 1) inclusive, and that we hit them all.
	 * Since it's sorted, this is relatively easy.
	 */
	x = 0;
	for (i = 0; i < component_databases->total_segments; i++)
	{
		int			this_segindex = -1;

		while (x < component_databases->total_segment_dbs)
		{
			this_segindex = component_databases->segment_db_info[x].segindex;
			if (this_segindex < i)
				x++;
			else if (this_segindex == i)
				break;
			else if (this_segindex > i)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("Content values not valid in %s table.  They must be in the range 0 to %d inclusive",
								GpSegmentConfigRelationName, component_databases->total_segments - 1)));
			}
		}
		if (this_segindex != i)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("Content values not valid in %s table.  They must be in the range 0 to %d inclusive",
							GpSegmentConfigRelationName, component_databases->total_segments - 1)));
		}
	}

	for (i = 0; i < component_databases->total_segment_dbs; i++)
	{
		cdbInfo = &component_databases->segment_db_info[i];

		if (cdbInfo->role != GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY || cdbInfo->hostip == NULL)
			continue;

		hsEntry = (HostSegsEntry *) hash_search(hostSegsHash, cdbInfo->hostip, HASH_FIND, &found);
		Assert(found);
		cdbInfo->hostSegs = hsEntry->segmentCount;
	}

	for (i = 0; i < component_databases->total_entry_dbs; i++)
	{
		cdbInfo = &component_databases->entry_db_info[i];

		if (cdbInfo->role != GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY || cdbInfo->hostip == NULL)
			continue;

		hsEntry = (HostSegsEntry *) hash_search(hostSegsHash, cdbInfo->hostip, HASH_FIND, &found);
		Assert(found);
		cdbInfo->hostSegs = hsEntry->segmentCount;
	}

	hash_destroy(hostSegsHash);

	MemoryContextSwitchTo(oldContext);

	return component_databases;
}

/*
 * Helper function to clean up the idle segdbs list of
 * a segment component.
 */
static void
cleanupComponentIdleQEs(CdbComponentDatabaseInfo *cdi, bool includeWriter)
{
	SegmentDatabaseDescriptor	*segdbDesc;
	MemoryContext				oldContext;
	ListCell 					*curItem = NULL;
	ListCell					*nextItem = NULL;
	ListCell 					*prevItem = NULL;

	Assert(CdbComponentsContext);
	oldContext = MemoryContextSwitchTo(CdbComponentsContext);
	curItem = list_head(cdi->freelist);

	while (curItem != NULL)
	{
		segdbDesc = (SegmentDatabaseDescriptor *)lfirst(curItem);
		nextItem = lnext(curItem);
		Assert(segdbDesc);

		if (segdbDesc->isWriter && !includeWriter)
		{
			prevItem = curItem;
			curItem = nextItem;
			continue;
		}

		cdi->freelist = list_delete_cell(cdi->freelist, curItem, prevItem); 
		DECR_COUNT(cdi, numIdleQEs);

		cdbconn_termSegmentDescriptor(segdbDesc);

		curItem = nextItem;

	}

	MemoryContextSwitchTo(oldContext);
}

void
cdbcomponent_cleanupIdleQEs(bool includeWriter)
{
	CdbComponentDatabases	*cdbs;
	int						i;

	/* use cdb_component_dbs directly */
	cdbs = cdb_component_dbs;

	if (cdbs == NULL)		
		return;

	if (cdbs->segment_db_info != NULL)
	{
		for (i = 0; i < cdbs->total_segment_dbs; i++)
		{
			CdbComponentDatabaseInfo *cdi = &cdbs->segment_db_info[i];
			cleanupComponentIdleQEs(cdi, includeWriter);
		}
	}

	if (cdbs->entry_db_info != NULL)
	{
		for (i = 0; i < cdbs->total_entry_dbs; i++)
		{
			CdbComponentDatabaseInfo *cdi = &cdbs->entry_db_info[i];
			cleanupComponentIdleQEs(cdi, includeWriter);
		}
	}

	return;
}

/* 
 * This function is called when current global transaction is set,
 * the snapshot of segments info will not changed within a global
 * transaction
 */
void
cdbcomponent_updateCdbComponents(void)
{
	uint8 ftsVersion= getFtsVersion();
	int expandVersion = GetGpExpandVersion();

	PG_TRY();
	{
		if (cdb_component_dbs == NULL)
		{
			cdb_component_dbs = getCdbComponentInfo(true);
			cdb_component_dbs->fts_version = ftsVersion;
			cdb_component_dbs->expand_version = GetGpExpandVersion();
		}
		else if ((cdb_component_dbs->fts_version != ftsVersion ||
				 cdb_component_dbs->expand_version != expandVersion))
		{
			if (TempNamespaceOidIsValid())
			{
				/*
				 * Do not update here, otherwise, temp files will be lost 
				 * in segments;
				 */
			}
			else
			{
				ELOG_DISPATCHER_DEBUG("FTS rescanned, get new component databases info.");
				cdbcomponent_destroyCdbComponents();
				cdb_component_dbs = getCdbComponentInfo(true);
				cdb_component_dbs->fts_version = ftsVersion;
				cdb_component_dbs->expand_version = expandVersion;
			}
		}
	}
	PG_CATCH();
	{
		FtsNotifyProber();

		PG_RE_THROW();
	}
	PG_END_TRY();

	Assert(cdb_component_dbs->numActiveQEs == 0);
}

/*
 * cdbcomponent_getCdbComponents 
 *
 *
 * Storage for the SegmentInstances block and all subsidiary
 * strucures are allocated from the caller's context.
 */
CdbComponentDatabases *
cdbcomponent_getCdbComponents(bool DNSLookupAsError)
{
	PG_TRY();
	{
		if (cdb_component_dbs == NULL)
		{
			cdb_component_dbs = getCdbComponentInfo(DNSLookupAsError);
			cdb_component_dbs->fts_version = getFtsVersion();
			cdb_component_dbs->expand_version = GetGpExpandVersion();
		}
	}
	PG_CATCH();
	{
		FtsNotifyProber();

		PG_RE_THROW();
	}
	PG_END_TRY();

	return cdb_component_dbs;
}

/*
 * cdbcomponet_destroyCdbComponents 
 *
 * Disconnect and destroy all idle QEs, releases the memory
 * occupied by the CdbComponentDatabases
 *
 * callers must clean up QEs used by dispatcher states.
 */
void
cdbcomponent_destroyCdbComponents(void)
{
	/* caller must clean up all segdbs used by dispatcher states */
	Assert(!cdbcomponent_activeQEsExist());

	hash_destroy(segment_ip_cache_htab);
	segment_ip_cache_htab = NULL;

	/* disconnect and destroy idle QEs include writers */
	cdbcomponent_cleanupIdleQEs(true);

	/* delete the memory context */
	if (CdbComponentsContext)
		MemoryContextDelete(CdbComponentsContext);	
	CdbComponentsContext = NULL;
	cdb_component_dbs = NULL;
}

/*
 * Allocated a segdb
 *
 * If thers is idle segdb in the freelist, return it, otherwise, initialize
 * a new segdb.
 *
 * idle segdbs has an established connection with segment, but new segdb is
 * not setup yet, callers need to establish the connection by themselves.
 */
SegmentDatabaseDescriptor *
cdbcomponent_allocateIdleQE(int contentId, SegmentType segmentType)
{
	SegmentDatabaseDescriptor	*segdbDesc = NULL;
	CdbComponentDatabaseInfo	*cdbinfo;
	ListCell 					*curItem = NULL;
	ListCell 					*nextItem = NULL;
	ListCell					*prevItem = NULL;
	MemoryContext 				oldContext;
	bool						isWriter;

	cdbinfo = cdbcomponent_getComponentInfo(contentId);	

	oldContext = MemoryContextSwitchTo(CdbComponentsContext);

	curItem = list_head(cdbinfo->freelist);
	while (curItem != NULL)
	{
		SegmentDatabaseDescriptor *tmp =
				(SegmentDatabaseDescriptor *)lfirst(curItem);

		nextItem = lnext(curItem);
		Assert(tmp);

		if ((segmentType == SEGMENTTYPE_EXPLICT_WRITER && !tmp->isWriter) ||
			(segmentType == SEGMENTTYPE_EXPLICT_READER && tmp->isWriter))
		{
			prevItem = curItem;
			curItem = nextItem;
			continue;
		}

		cdbinfo->freelist = list_delete_cell(cdbinfo->freelist, curItem, prevItem); 
		/* update numIdleQEs */
		DECR_COUNT(cdbinfo, numIdleQEs);

		segdbDesc = tmp;
		break;
	}

	if (!segdbDesc)
	{
		/*
		 * 1. for entrydb, it's never be writer.
		 * 2. for first QE, it must be a writer.
		 */
		isWriter = contentId == -1 ? false: (cdbinfo->numIdleQEs == 0 && cdbinfo->numActiveQEs == 0);
		segdbDesc = cdbconn_createSegmentDescriptor(cdbinfo, cdbinfo->cdbs->qeCounter++, isWriter);
	}

	cdbconn_setQEIdentifier(segdbDesc, -1);

	INCR_COUNT(cdbinfo, numActiveQEs);

	MemoryContextSwitchTo(oldContext);

	return segdbDesc;
}

static bool
cleanupQE(SegmentDatabaseDescriptor *segdbDesc)
{
	Assert(segdbDesc != NULL);

#ifdef FAULT_INJECTOR
	if (SIMPLE_FAULT_INJECTOR(CleanupQE) == FaultInjectorTypeSkip)
		return false;
#endif

	/*
	 * if the process is in the middle of blowing up... then we don't do
	 * anything here.  making libpq and other calls can definitely result in
	 * things getting HUNG.
	 */
	if (proc_exit_inprogress)
		return false;

	if (cdbconn_isBadConnection(segdbDesc))
		return false;

	/* if segment is down, the gang can not be reused */
	if (FtsIsSegmentDown(segdbDesc->segment_database_info))
		return false; 

	/* If a reader exceed the cached memory limitation, destroy it */
	if (!segdbDesc->isWriter &&
		(segdbDesc->conn->mop_high_watermark >> 20) > gp_vmem_protect_gang_cache_limit)
		return false;

	/* Note, we cancel all "still running" queries */
	if (!cdbconn_discardResults(segdbDesc, 20))
		elog(FATAL, "cleanup called when a segworker is still busy");

	/* QE is no longer associated with a slice. */
	cdbconn_setQEIdentifier(segdbDesc, /* slice index */ -1);	

	return true;
}

void
cdbcomponent_recycleIdleQE(SegmentDatabaseDescriptor *segdbDesc, bool forceDestroy)
{
	CdbComponentDatabaseInfo	*cdbinfo;
	MemoryContext				oldContext;	
	int							maxLen;

	Assert(cdb_component_dbs);
	Assert(CdbComponentsContext);

	cdbinfo = segdbDesc->segment_database_info;

	/* update num of active QEs */
	DECR_COUNT(cdbinfo, numActiveQEs);

	oldContext = MemoryContextSwitchTo(CdbComponentsContext);

	if (forceDestroy || !cleanupQE(segdbDesc))
		goto destroy_segdb;

	/* If freelist length exceed gp_cached_gang_threshold, destroy it */
	maxLen = segdbDesc->segindex == -1 ?
					MAX_CACHED_1_GANGS : gp_cached_gang_threshold;
	if (!segdbDesc->isWriter && list_length(cdbinfo->freelist) >= maxLen)
		goto destroy_segdb;

	/* Recycle the QE, put it to freelist */
	if (segdbDesc->isWriter)
	{
		/* writer is always the header of freelist */
		segdbDesc->segment_database_info->freelist =
			lcons(segdbDesc, segdbDesc->segment_database_info->freelist);
	}
	else
	{
		/* attatch reader to the tail of freelist */
		segdbDesc->segment_database_info->freelist =
			lappend(segdbDesc->segment_database_info->freelist, segdbDesc);
	}

	INCR_COUNT(cdbinfo, numIdleQEs);

	MemoryContextSwitchTo(oldContext);

	return;

destroy_segdb:

	cdbconn_termSegmentDescriptor(segdbDesc);

	if (segdbDesc->isWriter)
	{
		markCurrentGxactWriterGangLost();
	}

	MemoryContextSwitchTo(oldContext);
}

bool
cdbcomponent_qesExist(void)
{
	return !cdb_component_dbs ? false :
			(cdb_component_dbs->numIdleQEs > 0 || cdb_component_dbs->numActiveQEs > 0);
}

bool
cdbcomponent_activeQEsExist(void)
{
	return !cdb_component_dbs ? false : cdb_component_dbs->numActiveQEs > 0;
}

/*
 * Find CdbComponentDatabaseInfo in the array by segment index.
 */
CdbComponentDatabaseInfo *
cdbcomponent_getComponentInfo(int contentId)
{
	CdbComponentDatabaseInfo *cdbInfo = NULL;
	CdbComponentDatabases *cdbs;

	cdbs = cdbcomponent_getCdbComponents(true);

	if (contentId < -1 || contentId >= cdbs->total_segments)
		ereport(FATAL, (errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("Unexpected content id %d, should be [-1, %d]",
								contentId, cdbs->total_segments - 1)));
	/* entry db */
	if (contentId == -1)
	{
		cdbInfo = &cdbs->entry_db_info[0];	
		return cdbInfo;
	}

	/* no mirror, segment_db_info is sorted by content id */
	if (cdbs->total_segment_dbs == cdbs->total_segments)
	{
		cdbInfo = &cdbs->segment_db_info[contentId];
		return cdbInfo;
	}

	/* with mirror, segment_db_info is sorted by content id */
	if (cdbs->total_segment_dbs != cdbs->total_segments)
	{
		Assert(cdbs->total_segment_dbs == cdbs->total_segments * 2);
		cdbInfo = &cdbs->segment_db_info[2 * contentId];

		if (!SEGMENT_IS_ACTIVE_PRIMARY(cdbInfo))
		{
			cdbInfo = &cdbs->segment_db_info[2 * contentId + 1];
		}

		return cdbInfo;
	}

	return cdbInfo;
}

/*
 * performs all necessary setup required for Greenplum Database mode.
 *
 * This includes cdblink_setup() and initializing the Motion Layer.
 */
void
cdb_setup(void)
{
	elog(DEBUG1, "Initializing Greenplum components...");

	/* If gp_role is UTILITY, skip this call. */
	if (Gp_role != GP_ROLE_UTILITY)
	{
		/* Initialize the Motion Layer IPC subsystem. */
		InitMotionLayerIPC();
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* initialize TM */
		initTM();
	}
}


/*
 * performs all necessary cleanup required when leaving Greenplum
 * Database mode.  This is also called when the process exits.
 *
 * NOTE: the arguments to this function are here only so that we can
 *		 register it with on_proc_exit().  These parameters should not
 *		 be used since there are some callers to this that pass them
 *		 as NULL.
 *
 */
void
cdb_cleanup(int code __attribute__((unused)), Datum arg
						__attribute__((unused)))
{
	elog(DEBUG1, "Cleaning up Greenplum components...");

	DisconnectAndDestroyAllGangs(true);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (cdb_total_plans > 0)
		{
			elog(DEBUG1, "session dispatched %d plans %d slices (%f), largest plan %d",
				 cdb_total_plans, cdb_total_slices,
				 ((double) cdb_total_slices / (double) cdb_total_plans),
				 cdb_max_slices);
		}
	}

	if (Gp_role != GP_ROLE_UTILITY)
	{
		/* shutdown our listener socket */
		CleanUpMotionLayerIPC();
	}
}

/*
 * CdbComponentDatabaseInfoCompare:
 * A compare function for CdbComponentDatabaseInfo structs
 * that compares based on , isprimary desc
 * for use with qsort.
 */
static int
CdbComponentDatabaseInfoCompare(const void *p1, const void *p2)
{
	const CdbComponentDatabaseInfo *obj1 = (CdbComponentDatabaseInfo *) p1;
	const CdbComponentDatabaseInfo *obj2 = (CdbComponentDatabaseInfo *) p2;

	int			cmp = obj1->segindex - obj2->segindex;

	if (cmp == 0)
	{
		int			obj2cmp = 0;
		int			obj1cmp = 0;

		if (SEGMENT_IS_ACTIVE_PRIMARY(obj2))
			obj2cmp = 1;

		if (SEGMENT_IS_ACTIVE_PRIMARY(obj1))
			obj1cmp = 1;

		cmp = obj2cmp - obj1cmp;
	}

	return cmp;
}

/*
 * Maintain a cache of names.
 *
 * The keys are all NAMEDATALEN long.
 */
static char *
getDnsCachedAddress(char *name, int port, int elevel, bool use_cache)
{
	SegIpEntry	   *e = NULL;
	char			hostinfo[NI_MAXHOST];

	if (use_cache)
	{
		if (segment_ip_cache_htab == NULL)
		{
			HASHCTL		hash_ctl;

			MemSet(&hash_ctl, 0, sizeof(hash_ctl));

			hash_ctl.keysize = NAMEDATALEN + 1;
			hash_ctl.entrysize = sizeof(SegIpEntry);

			segment_ip_cache_htab = hash_create("segment_dns_cache",
												256, &hash_ctl, HASH_ELEM);
		}
		else
		{
			e = (SegIpEntry *) hash_search(segment_ip_cache_htab,
										   name, HASH_FIND, NULL);
			if (e != NULL)
				return e->hostinfo;
		}
	}

	/*
	 * The name is either not in our cache, or we've been instructed to not
	 * use the cache. Perform the name lookup.
	 */
	if (!use_cache || (use_cache && e == NULL))
	{
		MemoryContext oldContext = NULL;
		int			ret;
		char		portNumberStr[32];
		char	   *service;
		struct addrinfo *addrs = NULL,
				   *addr;
		struct addrinfo hint;

		/* Initialize hint structure */
		MemSet(&hint, 0, sizeof(hint));
		hint.ai_socktype = SOCK_STREAM;
		hint.ai_family = AF_UNSPEC;

		snprintf(portNumberStr, sizeof(portNumberStr), "%d", port);
		service = portNumberStr;

		ret = pg_getaddrinfo_all(name, service, &hint, &addrs);
		if (ret || !addrs)
		{
			if (addrs)
				pg_freeaddrinfo_all(hint.ai_family, addrs);

			/*
			 * If a host name is unknown, whether it is an error depends on its role:
			 * - if it is a primary then it's an error;
			 * - if it is a mirror then it's just a warning;
			 * but we do not know the role information here, so always treat it as a
			 * warning, the callers should check the role and decide what to do.
			 */
			if (ret != EAI_FAIL && elevel == ERROR)
				elevel = WARNING;

			ereport(elevel,
					(errmsg("could not translate host name \"%s\", port \"%d\" to address: %s",
							name, port, gai_strerror(ret))));

			return NULL;
		}

		/* save in the cache context */
		if (use_cache)
			oldContext = MemoryContextSwitchTo(TopMemoryContext);

		for (addr = addrs; addr; addr = addr->ai_next)
		{
#ifdef HAVE_UNIX_SOCKETS
			/* Ignore AF_UNIX sockets, if any are returned. */
			if (addr->ai_family == AF_UNIX)
				continue;
#endif
			if (addr->ai_family == AF_INET) /* IPv4 address */
			{
				memset(hostinfo, 0, sizeof(hostinfo));
				pg_getnameinfo_all((struct sockaddr_storage *) addr->ai_addr, addr->ai_addrlen,
								   hostinfo, sizeof(hostinfo),
								   NULL, 0,
								   NI_NUMERICHOST);

				if (use_cache)
				{
					/* Insert into our cache htab */
					e = (SegIpEntry *) hash_search(segment_ip_cache_htab,
												   name, HASH_ENTER, NULL);
					memcpy(e->hostinfo, hostinfo, sizeof(hostinfo));
				}

				break;
			}
		}

#ifdef HAVE_IPV6

		/*
		 * IPv6 probably would work fine, we'd just need to make sure all the
		 * data structures are big enough for the IPv6 address.  And on some
		 * broken systems, you can get an IPv6 address, but not be able to
		 * bind to it because IPv6 is disabled or missing in the kernel, so
		 * we'd only want to use the IPv6 address if there isn't an IPv4
		 * address.  All we really need to do is test this.
		 */
		if (((!use_cache && !hostinfo) || (use_cache && e == NULL))
			&& addrs->ai_family == AF_INET6)
		{
			char		hostinfo[NI_MAXHOST];

			addr = addrs;
			/* Get a text representation of the IP address */
			pg_getnameinfo_all((struct sockaddr_storage *) addr->ai_addr, addr->ai_addrlen,
							   hostinfo, sizeof(hostinfo),
							   NULL, 0,
							   NI_NUMERICHOST);

			if (use_cache)
			{
				/* Insert into our cache htab */
				e = (SegIpEntry *) hash_search(segment_ip_cache_htab,
											   name, HASH_ENTER, NULL);
				memcpy(e->hostinfo, hostinfo, sizeof(hostinfo));
			}
		}
#endif

		if (use_cache)
			MemoryContextSwitchTo(oldContext);

		pg_freeaddrinfo_all(hint.ai_family, addrs);
	}

	/* return a pointer to our cache. */
	if (use_cache)
		return e->hostinfo;

	return pstrdup(hostinfo);
}

/*
 * getDnsAddress
 *
 * same as getDnsCachedAddress, but without using the cache. A non-cached
 * version was used inline inside of cdbgang.c, and since it is needed now
 * elsewhere, it is available externally.
 */
char *
getDnsAddress(char *hostname, int port, int elevel)
{
	return getDnsCachedAddress(hostname, port, elevel, false);
}


/*
 * Given a component-db in the system, find the addresses at which it
 * can be reached, appropriately populate the argument-structure, and
 * maintain the ip-lookup-cache.
 */
static void
getAddressesForDBid(CdbComponentDatabaseInfo *c, int elevel)
{
	char	   *name;

	Assert(c != NULL);

	/* Use hostname */
	memset(c->hostaddrs, 0, COMPONENT_DBS_MAX_ADDRS * sizeof(char *));

#ifdef FAULT_INJECTOR
	if (SIMPLE_FAULT_INJECTOR(GetDnsCachedAddress) == FaultInjectorTypeSkip)
	{
		/* inject a dns error for primary of segment 0 */
		if (c->segindex == 0 &&
				c->preferred_role == GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY)
		{
			c->address = pstrdup("dnserrordummyaddress"); 
			c->hostname = pstrdup("dnserrordummyaddress"); 
		}
	}
#endif

	/*
	 * add an entry, using the first the "address" and then the "hostname" as
	 * fallback.
	 */
	name = getDnsCachedAddress(c->address, c->port, elevel, true);

	if (name)
	{
		c->hostaddrs[0] = pstrdup(name);
		return;
	}

	/* now the hostname. */
	name = getDnsCachedAddress(c->hostname, c->port, elevel, true);
	if (name)
	{
		c->hostaddrs[0] = pstrdup(name);
	}
	else
	{
		c->hostaddrs[0] = NULL;
	}

	return;
}

/*
 * hostSegsHashTableInit()
 *    Construct a hash table of HostSegsEntry
 */
static HTAB *
hostSegsHashTableInit(void)
{
	HASHCTL		info;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = INET6_ADDRSTRLEN;
	info.entrysize = sizeof(HostSegsEntry);

	return hash_create("HostSegs", 32, &info, HASH_ELEM);
}

/*
 * Given total number of primary segment databases and a number of
 * segments to "skip" - this routine creates a boolean map (array) the
 * size of total number of segments and randomly selects several
 * entries (total number of total_to_skip) to be marked as
 * "skipped". This is used for external tables with the 'gpfdist'
 * protocol where we want to get a number of *random* segdbs to
 * connect to a gpfdist client.
 *
 * Caller of this function should pfree skip_map when done with it.
 */
bool *
makeRandomSegMap(int total_primaries, int total_to_skip)
{
	int			randint;		/* some random int representing a seg    */
	int			skipped = 0;	/* num segs already marked to be skipped */
	bool	   *skip_map;

	skip_map = (bool *) palloc(total_primaries * sizeof(bool));
	MemSet(skip_map, false, total_primaries * sizeof(bool));

	while (total_to_skip != skipped)
	{
		/*
		 * create a random int between 0 and (total_primaries - 1).
		 *
		 * NOTE that the lower and upper limits in cdb_randint() are inclusive
		 * so we take them into account. In reality the chance of those limits
		 * to get selected by the random generator is extremely small, so we
		 * may want to find a better random generator some time (not critical
		 * though).
		 */
		randint = cdb_randint(0, total_primaries - 1);

		/*
		 * mark this random index 'true' in the skip map (marked to be
		 * skipped) unless it was already marked.
		 */
		if (skip_map[randint] == false)
		{
			skip_map[randint] = true;
			skipped++;
		}
	}

	return skip_map;
}

/*
 * Determine the dbid for the master standby
 */
int16
master_standby_dbid(void)
{
	int16		dbid = 0;
	HeapTuple	tup;
	Relation	rel;
	ScanKeyData scankey[2];
	SysScanDesc scan;

	/*
	 * Can only run on a master node, this restriction is due to the reliance
	 * on the gp_segment_configuration table.
	 */
	if (!IS_QUERY_DISPATCHER())
		elog(ERROR, "master_standby_dbid() executed on execution segment");

	/*
	 * SELECT * FROM gp_segment_configuration WHERE content = -1 AND role =
	 * GP_SEGMENT_CONFIGURATION_ROLE_MIRROR
	 */
	rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);
	ScanKeyInit(&scankey[0],
				Anum_gp_segment_configuration_content,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(-1));
	ScanKeyInit(&scankey[1],
				Anum_gp_segment_configuration_role,
				BTEqualStrategyNumber, F_CHAREQ,
				CharGetDatum(GP_SEGMENT_CONFIGURATION_ROLE_MIRROR));
	/* no index */
	scan = systable_beginscan(rel, InvalidOid, false,
							  NULL, 2, scankey);

	tup = systable_getnext(scan);

	if (HeapTupleIsValid(tup))
	{
		dbid = ((Form_gp_segment_configuration) GETSTRUCT(tup))->dbid;
		/* We expect a single result, assert this */
		Assert(systable_getnext(scan) == NULL);
	}

	systable_endscan(scan);
	/* no need to hold the lock, it's a catalog */
	heap_close(rel, AccessShareLock);

	return dbid;
}

CdbComponentDatabaseInfo *
dbid_get_dbinfo(int16 dbid)
{
	HeapTuple	tuple;
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc scan;
	CdbComponentDatabaseInfo *i = NULL;

	/*
	 * Can only run on a master node, this restriction is due to the reliance
	 * on the gp_segment_configuration table.  This may be able to be relaxed
	 * by switching to a different method of checking.
	 */
	if (!IS_QUERY_DISPATCHER())
		elog(ERROR, "dbid_get_dbinfo() executed on execution segment");

	rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);

	/* SELECT * FROM gp_segment_configuration WHERE dbid = :1 */
	ScanKeyInit(&scankey,
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(dbid));
	scan = systable_beginscan(rel, GpSegmentConfigDbidIndexId, true,
							  NULL, 1, &scankey);

	tuple = systable_getnext(scan);
	if (HeapTupleIsValid(tuple))
	{
		Datum		attr;
		bool		isNull;

		i = palloc(sizeof(CdbComponentDatabaseInfo));

		/*
		 * dbid
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_dbid,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->dbid = DatumGetInt16(attr);

		/*
		 * content
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_content,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->segindex = DatumGetInt16(attr);

		/*
		 * role
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_role,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->role = DatumGetChar(attr);

		/*
		 * preferred-role
		 */
		attr = heap_getattr(tuple,
							Anum_gp_segment_configuration_preferred_role,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->preferred_role = DatumGetChar(attr);

		/*
		 * mode
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_mode,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->mode = DatumGetChar(attr);

		/*
		 * status
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_status,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->status = DatumGetChar(attr);

		/*
		 * hostname
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_hostname,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->hostname = TextDatumGetCString(attr);

		/*
		 * address
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_address,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->address = TextDatumGetCString(attr);

		/*
		 * port
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_port,
							RelationGetDescr(rel), &isNull);
		Assert(!isNull);
		i->port = DatumGetInt32(attr);

		Assert(systable_getnext(scan) == NULL); /* should be only 1 */
	}
	else
	{
		elog(ERROR, "could not find configuration entry for dbid %i", dbid);
	}

	systable_endscan(scan);
	heap_close(rel, NoLock);

	return i;
}

/*
 * Obtain the dbid of a of a segment at a given segment index (i.e., content id)
 * currently fulfilling the role specified. This means that the segment is
 * really performing the role of primary or mirror, irrespective of their
 * preferred role.
 */
int16
contentid_get_dbid(int16 contentid, char role, bool getPreferredRoleNotCurrentRole)
{
	int16		dbid = 0;
	Relation	rel;
	ScanKeyData scankey[2];
	SysScanDesc scan;
	HeapTuple	tup;

	/*
	 * Can only run on a master node, this restriction is due to the reliance
	 * on the gp_segment_configuration table.  This may be able to be relaxed
	 * by switching to a different method of checking.
	 */
	if (!IS_QUERY_DISPATCHER())
		elog(ERROR, "contentid_get_dbid() executed on execution segment");

	rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);

	/* XXX XXX: CHECK THIS  XXX jic 2011/12/09 */
	if (getPreferredRoleNotCurrentRole)
	{
		/*
		 * SELECT * FROM gp_segment_configuration WHERE content = :1 AND
		 * preferred_role = :2
		 */
		ScanKeyInit(&scankey[0],
					Anum_gp_segment_configuration_content,
					BTEqualStrategyNumber, F_INT2EQ,
					Int16GetDatum(contentid));
		ScanKeyInit(&scankey[1],
					Anum_gp_segment_configuration_preferred_role,
					BTEqualStrategyNumber, F_CHAREQ,
					CharGetDatum(role));
		scan = systable_beginscan(rel, GpSegmentConfigContentPreferred_roleIndexId, true,
								  NULL, 2, scankey);
	}
	else
	{
		/*
		 * SELECT * FROM gp_segment_configuration WHERE content = :1 AND role
		 * = :2
		 */
		ScanKeyInit(&scankey[0],
					Anum_gp_segment_configuration_content,
					BTEqualStrategyNumber, F_INT2EQ,
					Int16GetDatum(contentid));
		ScanKeyInit(&scankey[1],
					Anum_gp_segment_configuration_role,
					BTEqualStrategyNumber, F_CHAREQ,
					CharGetDatum(role));
		/* no index */
		scan = systable_beginscan(rel, InvalidOid, false,
								  NULL, 2, scankey);
	}

	tup = systable_getnext(scan);
	if (HeapTupleIsValid(tup))
	{
		dbid = ((Form_gp_segment_configuration) GETSTRUCT(tup))->dbid;
		/* We expect a single result, assert this */
		Assert(systable_getnext(scan) == NULL); /* should be only 1 */
	}

	/* no need to hold the lock, it's a catalog */
	systable_endscan(scan);
	heap_close(rel, AccessShareLock);

	return dbid;
}

List *
cdbcomponent_getCdbComponentsList(void)
{
	CdbComponentDatabases *cdbs;
	List *segments = NIL;
	int i;

	cdbs = cdbcomponent_getCdbComponents(true);

	for (i = 0; i < cdbs->total_segments; i++)
	{
		segments = lappend_int(segments, i);
	}

	return segments;
}

/*
 * return the number of total segments for current snapshot of
 * segments info
 */
int
getgpsegmentCount(void)
{
	int32 numsegments = -1;

	if (Gp_role == GP_ROLE_DISPATCH)
		numsegments = cdbcomponent_getCdbComponents(true)->total_segments;
	else if (Gp_role == GP_ROLE_EXECUTE)
		numsegments = numsegmentsFromQD;

	return numsegments;
}
