/*-------------------------------------------------------------------------
 *
 * cdbutil.c
 *	  Internal utility support functions for Greenplum Database/PostgreSQL.
 *
 * Portions Copyright (c) 2005-2011, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <sys/param.h>			/* for MAXHOSTNAMELEN */

#include "access/genam.h"
#include "catalog/gp_segment_configuration.h"
#include "common/ip.h"
#include "nodes/makefuncs.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "catalog/gp_id.h"
#include "catalog/indexing.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbmotion.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/ml_ipc.h"			/* listener_setup */
#include "cdb/cdbtm.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbfts.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "postmaster/fts.h"
#include "postmaster/postmaster.h"
#include "catalog/namespace.h"
#include "utils/gpexpand.h"
#include "access/xact.h"

#define MAX_CACHED_1_GANGS 1

#define INCR_COUNT(cdbinfo, arg) \
	(cdbinfo)->arg++; \
	(cdbinfo)->cdbs->arg++;

#define DECR_COUNT(cdbinfo, arg) \
	(cdbinfo)->arg--; \
	(cdbinfo)->cdbs->arg--; \
	Assert((cdbinfo)->arg >= 0); \
	Assert((cdbinfo)->cdbs->arg >= 0); \

#define GPSEGCONFIGDUMPFILE "gpsegconfig_dump"
#define GPSEGCONFIGDUMPFILETMP "gpsegconfig_dump_tmp"
#define GPSEGCONFIGNUMATTR 9 

MemoryContext CdbComponentsContext = NULL;
static CdbComponentDatabases *cdb_component_dbs = NULL;

/*
 * Helper Functions
 */
static CdbComponentDatabases *getCdbComponentInfo(void);
static void cleanupComponentIdleQEs(CdbComponentDatabaseInfo *cdi, bool includeWriter);

static int	CdbComponentDatabaseInfoCompare(const void *p1, const void *p2);

static GpSegConfigEntry * readGpSegConfigFromCatalog(int *total_dbs);
static GpSegConfigEntry * readGpSegConfigFromFTSFiles(int *total_dbs);

static void getAddressesForDBid(GpSegConfigEntry *c, int elevel);
static HTAB *hostSegsHashTableInit(void);

static int nextQEIdentifer(CdbComponentDatabases *cdbs);

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
 * Helper functions for fetching latest gp_segment_configuration outside of
 * the transaction.
 *
 * In phase 2 of 2PC, current xact has been marked to TRANS_COMMIT/ABORT, 
 * COMMIT_PREPARED or ABORT_PREPARED DTM are performed, if they failed,
 * dispather disconnect and destroy all gangs and fetch the latest segment
 * configurations to do RETRY_COMMIT_PREPARED or RETRY_ABORT_PREPARED,
 * however, postgres disallow catalog lookups outside of xacts.
 *
 * readGpSegConfigFromFTSFiles() notify FTS to dump the configs from catalog
 * to a flat file and then read configurations from that file.
 */
static GpSegConfigEntry *
readGpSegConfigFromFTSFiles(int *total_dbs)
{
	FILE	*fd;
	int		idx = 0;
	int		array_size = 500;
	GpSegConfigEntry *configs = NULL;
	GpSegConfigEntry *config = NULL;

	char	hostname[MAXHOSTNAMELEN];
	char	address[MAXHOSTNAMELEN];
	char	buf[MAXHOSTNAMELEN * 2 + 32];

	Assert(!IsTransactionState());

	/* notify and wait FTS to finish a probe and update the dump file */
	FtsNotifyProber();	

	fd = AllocateFile(GPSEGCONFIGDUMPFILE, "r");

	if (!fd)
		elog(ERROR, "could not open gp_segment_configutation dump file:%s:%m", GPSEGCONFIGDUMPFILE);

	configs = palloc0(sizeof (GpSegConfigEntry) * array_size); 

	while (fgets(buf, sizeof(buf), fd))
	{ 
		config = &configs[idx];

		if (sscanf(buf, "%d %d %c %c %c %c %d %s %s", (int *)&config->dbid, (int *)&config->segindex,
				   &config->role, &config->preferred_role, &config->mode, &config->status,
				   &config->port, hostname, address) != GPSEGCONFIGNUMATTR)
		{
			FreeFile(fd);
			elog(ERROR, "invalid data in gp_segment_configuration dump file: %s:%m", GPSEGCONFIGDUMPFILE);
		}

		config->hostname = pstrdup(hostname);
		config->address = pstrdup(address);

		idx++;
		/*
		 * Expand CdbComponentDatabaseInfo array if we've used up
		 * currently allocated space
		 */
		if (idx >= array_size)
		{
			array_size = array_size * 2;
			configs = (GpSegConfigEntry *)
				repalloc(configs, sizeof(GpSegConfigEntry) * array_size);
		}
	}

	FreeFile(fd);

	*total_dbs = idx;
	return configs;
}

/*
 * writeGpSegConfigToFTSFiles() dump gp_segment_configuration to the file
 * GPSEGCONFIGDUMPFILE, in $PGDATA, only FTS process can use this function.
 *
 * write contents to GPSEGCONFIGDUMPFILETMP first, then rename it to
 * GPSEGCONFIGDUMPFILE, it makes lockless read and write concurrently.
 */
void
writeGpSegConfigToFTSFiles(void)
{
	FILE	*fd;
	int		idx = 0;
	int		total_dbs = 0;
	GpSegConfigEntry *configs = NULL;
	GpSegConfigEntry *config = NULL;

	Assert(IsTransactionState());
	Assert(am_ftsprobe);

	fd = AllocateFile(GPSEGCONFIGDUMPFILETMP, "w+");

	if (!fd)
		elog(ERROR, "could not create tmp file: %s: %m", GPSEGCONFIGDUMPFILETMP);

	configs = readGpSegConfigFromCatalog(&total_dbs); 

	for (idx = 0; idx < total_dbs; idx++)
	{
		config = &configs[idx];

		if (fprintf(fd, "%d %d %c %c %c %c %d %s %s\n", config->dbid, config->segindex,
					config->role, config->preferred_role, config->mode, config->status,
					config->port, config->hostname, config->address) < 0)
		{
			FreeFile(fd);
			elog(ERROR, "could not dump gp_segment_configuration to file: %s: %m", GPSEGCONFIGDUMPFILE);
		}
	}

	FreeFile(fd);

	/* rename tmp file to permanent file */
	if (rename(GPSEGCONFIGDUMPFILETMP, GPSEGCONFIGDUMPFILE) != 0)
		elog(ERROR, "could not rename file %s to file %s: %m",
			 GPSEGCONFIGDUMPFILETMP, GPSEGCONFIGDUMPFILE);
}

static GpSegConfigEntry *
readGpSegConfigFromCatalog(int *total_dbs)
{
	int					idx = 0;
	int					array_size;
	bool				isNull;
	Datum				attr;
	Relation			gp_seg_config_rel;
	HeapTuple			gp_seg_config_tuple = NULL;
	SysScanDesc			gp_seg_config_scan;
	GpSegConfigEntry	*configs;
	GpSegConfigEntry	*config;

	array_size = 500;
	configs = palloc0(sizeof(GpSegConfigEntry) * array_size);

	gp_seg_config_rel = table_open(GpSegmentConfigRelationId, AccessShareLock);
	gp_seg_config_scan = systable_beginscan(gp_seg_config_rel, InvalidOid, false, NULL,
											0, NULL);

	while (HeapTupleIsValid(gp_seg_config_tuple = systable_getnext(gp_seg_config_scan)))
	{
		config = &configs[idx];

		/* dbid */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_dbid, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->dbid = DatumGetInt16(attr);

		/* content */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_content, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->segindex= DatumGetInt16(attr);

		/* role */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_role, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->role = DatumGetChar(attr);

		/* preferred-role */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_preferred_role, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->preferred_role = DatumGetChar(attr);

		/* mode */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_mode, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->mode = DatumGetChar(attr);

		/* status */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_status, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->status = DatumGetChar(attr);

		/* hostname */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_hostname, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->hostname = TextDatumGetCString(attr);

		/* address */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_address, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->address = TextDatumGetCString(attr);

		/* port */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_port, RelationGetDescr(gp_seg_config_rel), &isNull);
		Assert(!isNull);
		config->port = DatumGetInt32(attr);

		/* datadir is not dumped*/

		idx++;

		/*
		 * Expand CdbComponentDatabaseInfo array if we've used up
		 * currently allocated space
		 */
		if (idx >= array_size)
		{
			array_size = array_size * 2;
			configs = (GpSegConfigEntry *)
				repalloc(configs, sizeof(GpSegConfigEntry) * array_size);
		}
	}

	/*
	 * We're done with the catalog config, clean them up, closing all the
	 * relations we opened.
	 */
	systable_endscan(gp_seg_config_scan);
	table_close(gp_seg_config_rel, AccessShareLock);

	*total_dbs = idx;
	return configs;
}

/*
 *  Internal function to initialize each component info
 */
static CdbComponentDatabases *
getCdbComponentInfo(void)
{
	MemoryContext oldContext;
	CdbComponentDatabaseInfo *cdbInfo;
	CdbComponentDatabases *component_databases = NULL;
	GpSegConfigEntry *configs;

	int			i;
	int			x = 0;
	int			total_dbs = 0;

	bool		found;
	HostSegsEntry *hsEntry;

	if (!CdbComponentsContext)
		CdbComponentsContext = AllocSetContextCreate(TopMemoryContext, "cdb components Context",
								ALLOCSET_DEFAULT_MINSIZE,
								ALLOCSET_DEFAULT_INITSIZE,
								ALLOCSET_DEFAULT_MAXSIZE);

	oldContext = MemoryContextSwitchTo(CdbComponentsContext);

	HTAB	   *hostSegsHash = hostSegsHashTableInit();

	if (IsTransactionState())
		configs = readGpSegConfigFromCatalog(&total_dbs);
	else
		configs = readGpSegConfigFromFTSFiles(&total_dbs);

	component_databases = palloc0(sizeof(CdbComponentDatabases));

	component_databases->numActiveQEs = 0;
	component_databases->numIdleQEs = 0;
	component_databases->qeCounter = 0;
	component_databases->freeCounterList = NIL;

	component_databases->segment_db_info =
		(CdbComponentDatabaseInfo *) palloc0(sizeof(CdbComponentDatabaseInfo) * total_dbs);

	component_databases->entry_db_info =
		(CdbComponentDatabaseInfo *) palloc0(sizeof(CdbComponentDatabaseInfo) * 2);

	for (i = 0; i < total_dbs; i++)
	{
		CdbComponentDatabaseInfo	*pRow;
		GpSegConfigEntry	*config = &configs[i];

		/* lookup hostip/hostaddrs cache */
		config->hostip= NULL;
		getAddressesForDBid(config, !am_ftsprobe? ERROR : LOG);

		/*
		 * We make sure we get a valid hostip for primary here,
		 * if hostip for mirrors can not be get, ignore the error.
		 */
		if (config->hostaddrs[0] == NULL &&
			config->role == GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY)
			ereport(!am_ftsprobe ? ERROR : LOG,
					(errcode(ERRCODE_CONNECTION_FAILURE),
					errmsg("cannot resolve network address for dbid=%d", config->dbid)));

		if (config->hostaddrs[0] != NULL)
			config->hostip = pstrdup(config->hostaddrs[0]);
		AssertImply(config->hostip, strlen(config->hostip) <= INET6_ADDRSTRLEN);

		/*
		 * Determine which array to place this rows data in: entry or segment,
		 * based on the content field.
		 */
		if (config->segindex >= 0)
		{
			pRow = &component_databases->segment_db_info[component_databases->total_segment_dbs];
			component_databases->total_segment_dbs++;
		}
		else
		{
			pRow = &component_databases->entry_db_info[component_databases->total_entry_dbs];
			component_databases->total_entry_dbs++;
		}

		pRow->cdbs = component_databases;
		pRow->config = config;
		pRow->freelist = NIL;
		pRow->numIdleQEs = 0;
		pRow->numActiveQEs = 0;

		if (config->role != GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY || config->hostip == NULL)
			continue;

		hsEntry = (HostSegsEntry *) hash_search(hostSegsHash, config->hostip, HASH_ENTER, &found);
		if (found)
			hsEntry->segmentCount++;
		else
			hsEntry->segmentCount = 1;
	}

	/*
	 * Validate that there exists at least one entry and one segment database
	 * in the configuration
	 */
	if (component_databases->total_segment_dbs == 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_CARDINALITY_VIOLATION),
				 errmsg("number of segment databases cannot be 0")));
	}
	if (component_databases->total_entry_dbs == 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_CARDINALITY_VIOLATION),
				 errmsg("number of entry databases cannot be 0")));
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
			(component_databases->segment_db_info[i].config->segindex != component_databases->segment_db_info[i - 1].config->segindex))
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

		if (cdbInfo->config->dbid == GpIdentity.dbid && cdbInfo->config->segindex == GpIdentity.segindex)
		{
			break;
		}
	}
	if (i == component_databases->total_entry_dbs)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("cannot locate entry database"),
				 errdetail("Entry database represented by this db in gp_segment_configuration: dbid %d content %d",
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
			this_segindex = component_databases->segment_db_info[x].config->segindex;
			if (this_segindex < i)
				x++;
			else if (this_segindex == i)
				break;
			else if (this_segindex > i)
			{
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("content values not valid in %s table",
								GpSegmentConfigRelationName),
						 errdetail("Content values must be in the range 0 to %d inclusive.",
								   component_databases->total_segments - 1)));
			}
		}
		if (this_segindex != i)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("content values not valid in %s table",
							GpSegmentConfigRelationName),
					 errdetail("Content values must be in the range 0 to %d inclusive",
							   component_databases->total_segments - 1)));
		}
	}

	for (i = 0; i < component_databases->total_segment_dbs; i++)
	{
		cdbInfo = &component_databases->segment_db_info[i];

		if (cdbInfo->config->role != GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY || cdbInfo->config->hostip == NULL)
			continue;

		hsEntry = (HostSegsEntry *) hash_search(hostSegsHash, cdbInfo->config->hostip, HASH_FIND, &found);
		Assert(found);
		cdbInfo->hostSegs = hsEntry->segmentCount;
	}

	for (i = 0; i < component_databases->total_entry_dbs; i++)
	{
		cdbInfo = &component_databases->entry_db_info[i];

		if (cdbInfo->config->role != GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY || cdbInfo->config->hostip == NULL)
			continue;

		hsEntry = (HostSegsEntry *) hash_search(hostSegsHash, cdbInfo->config->hostip, HASH_FIND, &found);
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
 * This function is called when a transaction is started and the snapshot of
 * segments info will not changed until the end of transaction
 */
void
cdbcomponent_updateCdbComponents(void)
{
	uint8 ftsVersion= getFtsVersion();
	int expandVersion = GetGpExpandVersion();

	/*
	 * FTS takes responsibility for updating gp_segment_configuration, in each
	 * fts probe cycle, FTS firstly gets a copy of current configuration, then
	 * probe the segments based on it and finally free the copy in the end. In
	 * the probe stage, FTS might start/close transactions many times, so FTS
	 * should not update current copy of gp_segment_configuration when a new
	 * transaction is started.
	 */
	if (am_ftsprobe)
		return;

	PG_TRY();
	{
		if (cdb_component_dbs == NULL)
		{
			cdb_component_dbs = getCdbComponentInfo();
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
				cdb_component_dbs = getCdbComponentInfo();
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
 * structures are allocated from the caller's context.
 */
CdbComponentDatabases *
cdbcomponent_getCdbComponents()
{
	PG_TRY();
	{
		if (cdb_component_dbs == NULL)
		{
			cdb_component_dbs = getCdbComponentInfo();
			cdb_component_dbs->fts_version = getFtsVersion();
			cdb_component_dbs->expand_version = GetGpExpandVersion();
		}
	}
	PG_CATCH();
	{
		if (Gp_role == GP_ROLE_DISPATCH)
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

	/*
	 * Always try to pop from the head.  Make sure to push them back to head
	 * in cdbcomponent_recycleIdleQE().
	 */
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
		segdbDesc = cdbconn_createSegmentDescriptor(cdbinfo, nextQEIdentifer(cdbinfo->cdbs), isWriter);
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
	if (SIMPLE_FAULT_INJECTOR("cleanup_qe") == FaultInjectorTypeSkip)
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
	{
		elog(LOG, "cleaning up seg%d while it is still busy", segdbDesc->segindex);
		return false;
	}

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
		ListCell   *lastWriter = NULL;
		ListCell   *cell;

		/*
		 * In cdbcomponent_allocateIdleQE() readers are always popped from the
		 * head, so to restore the original order we must pushed them back to
		 * the head, and keep in mind readers must be put after the writers.
		 */

		for (cell = list_head(segdbDesc->segment_database_info->freelist);
			 cell && ((SegmentDatabaseDescriptor *) lfirst(cell))->isWriter;
			 lastWriter = cell, cell = lnext(cell)) ;

		if (lastWriter)
			lappend_cell(segdbDesc->segment_database_info->freelist,
						 lastWriter, segdbDesc);
		else
			segdbDesc->segment_database_info->freelist =
				lcons(segdbDesc, segdbDesc->segment_database_info->freelist);
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

static int
nextQEIdentifer(CdbComponentDatabases *cdbs)
{
	int result;

	if (!cdbs->freeCounterList)
		return cdbs->qeCounter++;

	result = linitial_int(cdbs->freeCounterList);
	cdbs->freeCounterList = list_delete_first(cdbs->freeCounterList);
	return result;
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

	cdbs = cdbcomponent_getCdbComponents();

	if (contentId < -1 || contentId >= cdbs->total_segments)
		ereport(FATAL,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("unexpected content id %d, should be [-1, %d]",
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

static void
ensureInterconnectAddress(void)
{
	if (interconnect_address)
		return;

	if (GpIdentity.segindex >= 0)
	{
		Assert(Gp_role == GP_ROLE_EXECUTE);
		Assert(MyProcPort != NULL);
		Assert(MyProcPort->laddr.addr.ss_family == AF_INET
				|| MyProcPort->laddr.addr.ss_family == AF_INET6);
		/*
		 * We assume that the QD, using the address in gp_segment_configuration
		 * as its destination IP address, connects to the segment/QE.
		 * So, the local address in the PORT can be used for interconnect.
		 */
		char local_addr[NI_MAXHOST];
		getnameinfo((const struct sockaddr *)&MyProcPort->laddr.addr,
					MyProcPort->laddr.salen,
					local_addr, sizeof(local_addr),
					NULL, 0, NI_NUMERICHOST);
		interconnect_address = MemoryContextStrdup(TopMemoryContext, local_addr);
	}
	else if (Gp_role == GP_ROLE_DISPATCH)
	{
		/*
		 * Here, we can only retrieve the ADDRESS in gp_segment_configuration
		 * from `cdbcomponent*`. We couldn't get it in a way as the QEs.
		 */
		CdbComponentDatabaseInfo *qdInfo;
		qdInfo = cdbcomponent_getComponentInfo(MASTER_CONTENT_ID);
		interconnect_address = MemoryContextStrdup(TopMemoryContext, qdInfo->config->hostip);
	}
	else if (qdHostname && qdHostname[0] != '\0')
	{
		Assert(Gp_role == GP_ROLE_EXECUTE);
		/*
		 * QE on the master can't get its interconnect address like that on the primary.
		 * The QD connects to its postmaster via the unix domain socket.
		 */
		interconnect_address = qdHostname;
	}
	else
		Assert(false);
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

	if (Gp_role != GP_ROLE_UTILITY)
	{
		ensureInterconnectAddress();
		/* Initialize the Motion Layer IPC subsystem. */
		InitMotionLayerIPC();
	}

	/*
	 * Backend process requires consistent state, it cannot proceed until
	 * dtx recovery process finish up the recovery of distributed transactions.
	 *
	 * Ignore background worker because bgworker_should_start_mpp() already did
	 * the check.
	 */
	if (!IsBackgroundWorker &&
		Gp_role == GP_ROLE_DISPATCH &&
		!*shmDtmStarted)
	{
		ereport(FATAL,
				(errcode(ERRCODE_CANNOT_CONNECT_NOW),
				 errmsg(POSTMASTER_IN_RECOVERY_MSG),
				 errdetail("waiting for distributed transaction recovery to complete")));
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
cdb_cleanup(int code pg_attribute_unused(), Datum arg
						pg_attribute_unused())
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

	int			cmp = obj1->config->segindex - obj2->config->segindex;

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

		hostinfo[0] = '\0';
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
		if (((!use_cache && !hostinfo[0]) || (use_cache && e == NULL))
			&& addrs->ai_family == AF_INET6)
		{
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
getAddressesForDBid(GpSegConfigEntry *c, int elevel)
{
	char	   *name;

	Assert(c != NULL);

	/* Use hostname */
	memset(c->hostaddrs, 0, COMPONENT_DBS_MAX_ADDRS * sizeof(char *));

#ifdef FAULT_INJECTOR
	if (am_ftsprobe &&
		SIMPLE_FAULT_INJECTOR("get_dns_cached_address") == FaultInjectorTypeSkip)
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
		 */
		randint = cdbhashrandomseg(total_primaries);

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
	rel = table_open(GpSegmentConfigRelationId, AccessShareLock);
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
	table_close(rel, AccessShareLock);

	return dbid;
}

GpSegConfigEntry *
dbid_get_dbinfo(int16 dbid)
{
	HeapTuple	tuple;
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc scan;
	GpSegConfigEntry *i = NULL;

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

		i = palloc(sizeof(GpSegConfigEntry));

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

	cdbs = cdbcomponent_getCdbComponents();

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
	/* 1 represents a singleton postgresql in utility mode */
	int32 numsegments = 1;

	if (Gp_role == GP_ROLE_DISPATCH)
		numsegments = cdbcomponent_getCdbComponents()->total_segments;
	else if (Gp_role == GP_ROLE_EXECUTE)
		numsegments = numsegmentsFromQD;
	/*
	 * If we are in 'Utility & Binary Upgrade' mode, it must be launched
	 * by the pg_upgrade, so we give it an correct numsegments to make
	 * sure the pg_upgrade can run normally.
	 * Only Utility QD process have the entire information in the
	 * gp_segment_configuration, so we count the segments count in this
	 * process.
	 */
	else if (Gp_role == GP_ROLE_UTILITY &&
			 IsBinaryUpgrade &&
			 IS_QUERY_DISPATCHER())
	{
		numsegments = cdbcomponent_getCdbComponents()->total_segments;
	}

	return numsegments;
}

/*
 * IsOnConflictUpdate
 * Return true if a plannedstmt is an upsert: insert ... on conflict do update
 */
bool
IsOnConflictUpdate(PlannedStmt *ps)
{
	Plan      *plan;

	if (ps == NULL || ps->commandType != CMD_INSERT)
		return false;

	plan = ps->planTree;

	if (plan && IsA(plan, Motion))
		plan = outerPlan(plan);

	if (plan == NULL || !IsA(plan, ModifyTable))
		return false;

	return ((ModifyTable *)plan)->onConflictAction == ONCONFLICT_UPDATE;
}

/*
 * Avoid core file generation for this PANIC. It helps to avoid
 * filling up disks during tests and also saves time.
 */
void
AvoidCorefileGeneration()
{
#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_CORE)
	struct rlimit lim;
	getrlimit(RLIMIT_CORE, &lim);
	lim.rlim_cur = 0;
	if (setrlimit(RLIMIT_CORE, &lim) != 0)
	{
		int			save_errno = errno;

		elog(NOTICE,
			 "setrlimit failed for RLIMIT_CORE soft limit to zero. errno: %d (%m).",
			 save_errno);
	}
#endif
}
