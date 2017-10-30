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
#include "catalog/gp_segment_config.h"
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

/*
 * Helper Functions
 */
static int	CdbComponentDatabaseInfoCompare(const void *p1, const void *p2);
static void freeCdbComponentDatabaseInfo(CdbComponentDatabaseInfo *cdi);

static void getAddressesForDBid(CdbComponentDatabaseInfo *c, int elevel);
static HTAB *hostSegsHashTableInit(void);

static HTAB *segment_ip_cache_htab = NULL;

struct segment_ip_cache_entry
{
	char		key[NAMEDATALEN];
	char		hostinfo[NI_MAXHOST];
};

typedef struct HostSegsEntry
{
	char		hostip[INET6_ADDRSTRLEN];
	int			segmentCount;
} HostSegsEntry;

/*
 * getCdbComponentDatabases
 *
 *
 * Storage for the SegmentInstances block and all subsidiary
 * strucures are allocated from the caller's context.
 */
CdbComponentDatabases *
getCdbComponentInfo(bool DNSLookupAsError)
{
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
	HTAB	   *hostSegsHash = hostSegsHashTableInit();

	/*
	 * Allocate component_databases return structure and
	 * component_databases->segment_db_info array with an initial size of 128,
	 * and component_databases->entry_db_info with an initial size of 4.  If
	 * necessary during row fetching, we grow these by doubling each time we
	 * run out.
	 */
	component_databases = palloc0(sizeof(CdbComponentDatabases));

	component_databases->segment_db_info =
		(CdbComponentDatabaseInfo *) palloc0(sizeof(CdbComponentDatabaseInfo) * segment_array_size);

	component_databases->entry_db_info =
		(CdbComponentDatabaseInfo *) palloc0(sizeof(CdbComponentDatabaseInfo) * entry_array_size);

	gp_seg_config_rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);

	gp_seg_config_scan = heap_beginscan(gp_seg_config_rel, SnapshotNow, 0, NULL);

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

		pRow->dbid = dbid;
		pRow->segindex = content;
		pRow->role = role;
		pRow->preferred_role = preferred_role;
		pRow->mode = mode;
		pRow->status = status;

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

		/*
		 * Filerep_port
		 */
		attr = heap_getattr(gp_seg_config_tuple, Anum_gp_segment_configuration_replication_port, RelationGetDescr(gp_seg_config_rel), &isNull);
		if (!isNull)
			pRow->filerep_port = DatumGetInt32(attr);
		else
			pRow->filerep_port = -1;

		getAddressesForDBid(pRow, DNSLookupAsError ? ERROR : LOG);

		/* We make sure we get a valid hostip here */
		if (pRow->hostaddrs[0] == NULL)
			elog(ERROR, "Cannot resolve network address for dbid=%d", dbid);
		pRow->hostip = pstrdup(pRow->hostaddrs[0]);
		Assert(strlen(pRow->hostip) <= INET6_ADDRSTRLEN);

		if (pRow->role != SEGMENT_ROLE_PRIMARY)
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
	 * Validate that gp_numsegments == segment_databases->total_segment_dbs
	 */
	if (getgpsegmentCount() != component_databases->total_segments)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("Greenplum Database number of segments inconsistency: count is %d from pg_catalog.%s table, but %d from getCdbComponentDatabases()",
						getgpsegmentCount(), GpIdRelationName, component_databases->total_segments)));
	}

	/*
	 * Now validate that our identity is present in the entry databases
	 */
	for (i = 0; i < component_databases->total_entry_dbs; i++)
	{
		cdbInfo = &component_databases->entry_db_info[i];

		if (cdbInfo->dbid == GpIdentity.dbid && cdbInfo->segindex == Gp_segment)
		{
			break;
		}
	}
	if (i == component_databases->total_entry_dbs)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("Cannot locate entry database represented by this db in gp_segment_configuration: dbid %d content %d",
						GpIdentity.dbid, Gp_segment)));
	}

	/*
	 * Now validate that the segindexes for the segment databases are between
	 * 0 and (GpIdentity.numsegments - 1) inclusive, and that we hit them all.
	 * Since it's sorted, this is relatively easy.
	 */
	x = 0;
	for (i = 0; i < getgpsegmentCount(); i++)
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
								GpSegmentConfigRelationName, getgpsegmentCount() - 1)));
			}
		}
		if (this_segindex != i)
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("Content values not valid in %s table.  They must be in the range 0 to %d inclusive",
							GpSegmentConfigRelationName, getgpsegmentCount() - 1)));
		}
	}

	for (i = 0; i < component_databases->total_segment_dbs; i++)
	{
		cdbInfo = &component_databases->segment_db_info[i];

		if (cdbInfo->role != SEGMENT_ROLE_PRIMARY)
			continue;

		hsEntry = (HostSegsEntry *) hash_search(hostSegsHash, cdbInfo->hostip, HASH_FIND, &found);
		Assert(found);
		cdbInfo->hostSegs = hsEntry->segmentCount;
	}

	for (i = 0; i < component_databases->total_entry_dbs; i++)
	{
		cdbInfo = &component_databases->entry_db_info[i];

		if (cdbInfo->role != SEGMENT_ROLE_PRIMARY)
			continue;

		hsEntry = (HostSegsEntry *) hash_search(hostSegsHash, cdbInfo->hostip, HASH_FIND, &found);
		Assert(found);
		cdbInfo->hostSegs = hsEntry->segmentCount;
	}

	hash_destroy(hostSegsHash);

	return component_databases;
}

/*
 * getCdbComponentDatabases
 *
 *
 * Storage for the SegmentInstances block and all subsidiary
 * strucures are allocated from the caller's context.
 */
CdbComponentDatabases *
getCdbComponentDatabases(void)
{
	return getCdbComponentInfo(true);
}


/*
 * freeCdbComponentDatabases
 *
 * Releases the storage occupied by the CdbComponentDatabases
 * struct pointed to by the argument.
 */
void
freeCdbComponentDatabases(CdbComponentDatabases *pDBs)
{
	int			i;

	if (pDBs == NULL)
		return;

	if (pDBs->segment_db_info != NULL)
	{
		for (i = 0; i < pDBs->total_segment_dbs; i++)
		{
			CdbComponentDatabaseInfo *cdi = &pDBs->segment_db_info[i];

			freeCdbComponentDatabaseInfo(cdi);
		}

		pfree(pDBs->segment_db_info);
	}

	if (pDBs->entry_db_info != NULL)
	{
		for (i = 0; i < pDBs->total_entry_dbs; i++)
		{
			CdbComponentDatabaseInfo *cdi = &pDBs->entry_db_info[i];

			freeCdbComponentDatabaseInfo(cdi);
		}

		pfree(pDBs->entry_db_info);
	}

	pfree(pDBs);
}

/*
 * freeCdbComponentDatabaseInfo:
 * Releases any storage allocated for members variables of a CdbComponentDatabaseInfo struct.
 */
void
freeCdbComponentDatabaseInfo(CdbComponentDatabaseInfo *cdi)
{
	int			i;

	if (cdi == NULL)
		return;

	if (cdi->hostname != NULL)
		pfree(cdi->hostname);

	if (cdi->address != NULL)
		pfree(cdi->address);

	if (cdi->hostip != NULL)
		pfree(cdi->hostip);

	for (i = 0; i < COMPONENT_DBS_MAX_ADDRS; i++)
	{
		if (cdi->hostaddrs[i] != NULL)
		{
			pfree(cdi->hostaddrs[i]);
			cdi->hostaddrs[i] = NULL;
		}
	}
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
getDnsCachedAddress(char *name, int port, int elevel)
{
	struct segment_ip_cache_entry *e;

	if (segment_ip_cache_htab == NULL)
	{
		HASHCTL		hash_ctl;

		MemSet(&hash_ctl, 0, sizeof(hash_ctl));

		hash_ctl.keysize = NAMEDATALEN + 1;
		hash_ctl.entrysize = sizeof(struct segment_ip_cache_entry);

		segment_ip_cache_htab = hash_create("segment_dns_cache",
											256,
											&hash_ctl,
											HASH_ELEM);
		Assert(segment_ip_cache_htab != NULL);
	}

	e = (struct segment_ip_cache_entry *) hash_search(segment_ip_cache_htab,
													  name, HASH_FIND, NULL);

	/* not in our cache, we've got to actually do the name lookup. */
	if (e == NULL)
	{
		MemoryContext oldContext;
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

			ereport(elevel,
					(errmsg("could not translate host name \"%s\", port \"%d\" to address: %s",
							name, port, gai_strerror(ret))));

			return NULL;
		}

		/* save in the cache context */
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
				char		hostinfo[NI_MAXHOST];

				pg_getnameinfo_all((struct sockaddr_storage *) addr->ai_addr, addr->ai_addrlen,
								   hostinfo, sizeof(hostinfo),
								   NULL, 0,
								   NI_NUMERICHOST);

				/* INSERT INTO OUR CACHE HTAB HERE */

				e = (struct segment_ip_cache_entry *) hash_search(segment_ip_cache_htab,
																  name,
																  HASH_ENTER,
																  NULL);
				Assert(e != NULL);
				memcpy(e->hostinfo, hostinfo, sizeof(hostinfo));

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
		if (e == NULL && addrs->ai_family == AF_INET6)
		{
			char		hostinfo[NI_MAXHOST];

			addr = addrs;



			pg_getnameinfo_all((struct sockaddr_storage *) addr->ai_addr, addr->ai_addrlen,
							   hostinfo, sizeof(hostinfo),
							   NULL, 0,
							   NI_NUMERICHOST);

			/* INSERT INTO OUR CACHE HTAB HERE */

			e = (struct segment_ip_cache_entry *) hash_search(segment_ip_cache_htab,
															  name,
															  HASH_ENTER,
															  NULL);
			Assert(e != NULL);
			memcpy(e->hostinfo, hostinfo, sizeof(hostinfo));

		}
#endif

		MemoryContextSwitchTo(oldContext);

		pg_freeaddrinfo_all(hint.ai_family, addrs);
	}

	/* return a pointer to our cache. */
	return e->hostinfo;
}

/*
 * getDnsAddress
 *
 * same as getDnsCachedAddress, but without caching. Looks like the
 * non-cached version was used inline inside of cdbgang.c, and since
 * it is needed now elsewhere, it is factored out to this routine.
 */
char *
getDnsAddress(char *hostname, int port, int elevel)
{
	int			ret;
	char		portNumberStr[32];
	char	   *service;
	char	   *result = NULL;
	struct addrinfo *addrs = NULL,
			   *addr;
	struct addrinfo hint;

	/* Initialize hint structure */
	MemSet(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_family = AF_UNSPEC;

	snprintf(portNumberStr, sizeof(portNumberStr), "%d", port);
	service = portNumberStr;

	ret = pg_getaddrinfo_all(hostname, service, &hint, &addrs);
	if (ret || !addrs)
	{
		if (addrs)
			pg_freeaddrinfo_all(hint.ai_family, addrs);
		ereport(elevel,
				(errmsg("could not translate host name \"%s\", port \"%d\" to address: %s",
						hostname, port, gai_strerror(ret))));
	}
	for (addr = addrs; addr; addr = addr->ai_next)
	{
#ifdef HAVE_UNIX_SOCKETS
		/* Ignore AF_UNIX sockets, if any are returned. */
		if (addr->ai_family == AF_UNIX)
			continue;
#endif
		if (addr->ai_family == AF_INET) /* IPv4 address */
		{
			char		hostinfo[NI_MAXHOST];

			/* Get a text representation of the IP address */
			pg_getnameinfo_all((struct sockaddr_storage *) addr->ai_addr, addr->ai_addrlen,
							   hostinfo, sizeof(hostinfo),
							   NULL, 0,
							   NI_NUMERICHOST);
			result = pstrdup(hostinfo);
			break;
		}
	}

#ifdef HAVE_IPV6

	/*
	 * IPv6 should would work fine, we'd just need to make sure all the data
	 * structures are big enough for the IPv6 address.  And on some broken
	 * systems, you can get an IPv6 address, but not be able to bind to it
	 * because IPv6 is disabled or missing in the kernel, so we'd only want to
	 * use the IPv6 address if there isn't an IPv4 address.  All we really
	 * need to do is test this.
	 */
	if (result == NULL && addrs->ai_family == AF_INET6)
	{
		char		hostinfo[NI_MAXHOST];

		addr = addrs;
		/* Get a text representation of the IP address */
		pg_getnameinfo_all((struct sockaddr_storage *) addr->ai_addr, addr->ai_addrlen,
						   hostinfo, sizeof(hostinfo),
						   NULL, 0,
						   NI_NUMERICHOST);
		result = pstrdup(hostinfo);
	}
#endif

	pg_freeaddrinfo_all(hint.ai_family, addrs);

	return result;
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

	/*
	 * add an entry, using the first the "address" and then the "hostname" as
	 * fallback.
	 */
	name = getDnsCachedAddress(c->address, c->port, elevel);

	if (name)
	{
		c->hostaddrs[0] = pstrdup(name);
		return;
	}

	/* now the hostname. */
	name = getDnsCachedAddress(c->hostname, c->port, elevel);
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
	if (GpIdentity.segindex != MASTER_CONTENT_ID)
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
							  SnapshotNow, 2, scankey);

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
	if (GpIdentity.segindex != MASTER_CONTENT_ID)
		elog(ERROR, "dbid_get_dbinfo() executed on execution segment");

	rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);

	/* SELECT * FROM gp_segment_configuration WHERE dbid = :1 */
	ScanKeyInit(&scankey,
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(dbid));
	scan = systable_beginscan(rel, GpSegmentConfigDbidIndexId, true,
							  SnapshotNow, 1, &scankey);

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

		/*
		 * Filerep_port
		 */
		attr = heap_getattr(tuple, Anum_gp_segment_configuration_replication_port,
							RelationGetDescr(rel), &isNull);
		if (!isNull)
			i->filerep_port = DatumGetInt32(attr);
		else
			i->filerep_port = -1;

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
	if (GpIdentity.segindex != MASTER_CONTENT_ID)
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
								  SnapshotNow, 2, scankey);
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
								  SnapshotNow, 2, scankey);
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

/*
 * Returns the number of segments
 *
 * N.B.  Gp_role must be either dispatch or execute, since
 * when utiliy	no GP catalog tables are read.  An Assert is
 * thrown if Gp_role = utility.
 */
int
getgpsegmentCount(void)
{
	if (Gp_role == GP_ROLE_UTILITY)
	{
		if (GpIdentity.numsegments <= 0)
		{
			elog(DEBUG5, "getgpsegmentCount called when Gp_role == utility. returning zero segments.");
			return 0;
		}

		elog(DEBUG1, "getgpsegmentCount called when Gp_role == utility, but is relying on gp_id info");

	}

	verifyGpIdentityIsSet();
	Assert(GpIdentity.numsegments > 0);
	return GpIdentity.numsegments;
}

bool
isSockAlive(int sock)
{
	int			ret;
	char		buf;
	int			i = 0;

	for (i = 0; i < 10; i++)
	{
#ifndef WIN32
		ret = recv(sock, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
#else
		ret = recv(sock, &buf, 1, MSG_PEEK | MSG_PARTIAL);
#endif

		if (ret == 0)			/* socket has been closed. EOF */
			return false;

		if (ret > 0)			/* data waiting on socket, it must be OK. */
			return true;

		if (ret == -1)			/* error, or would be block. */
		{
			if (errno == EAGAIN || errno == EINPROGRESS)
				return true;	/* connection intact, no data available */
			else if (errno == EINTR)
				continue;		/* interrupted by signal, retry at most 10
								 * times */
			else
				return false;
		}
	}

	return true;
}
