/*-------------------------------------------------------------------------
 *
 * cdbutil.h
 *	  Header file for routines in cdbutil.c and results returned by
 *	  those routines.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbutil.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBUTIL_H
#define CDBUTIL_H

#include "catalog/gp_segment_configuration.h"
#include "nodes/pg_list.h"
#include "nodes/plannodes.h"

struct SegmentDatabaseDescriptor;

extern MemoryContext CdbComponentsContext;

typedef struct CdbComponentDatabaseInfo CdbComponentDatabaseInfo;
typedef struct CdbComponentDatabases CdbComponentDatabases;

/* --------------------------------------------------------------------------------------------------
 * Structure for MPP 2.0 database information
 *
 * The information contained in this structure represents logically a row from
 * gp_configuration.  It is used to describe either an entry
 * database or a segment database.
 *
 * Storage for instances of this structure are palloc'd.  Storage for the values
 * pointed to by non-NULL char*'s are also palloc'd.
 *
 */
#define COMPONENT_DBS_MAX_ADDRS (8)
typedef struct GpSegConfigEntry 
{
	/* copy of entry in gp_segment_configuration */
	int16		dbid;			/* the dbid of this database */
	int16		segindex;		/* content indicator: -1 for entry database,
								 * 0, ..., n-1 for segment database */

	char		role;			/* primary, master, mirror, master-standby */
	char		preferred_role; /* what role would we "like" to have this segment in ? */
	char		mode;
	char		status;
	int32		port;			/* port that instance is listening on */
	char		*hostname;		/* name or ip address of host machine */
	char		*address;		/* ip address of host machine */
	char		*datadir;		/* absolute path to data directory on the host. */

	/* additional cached info */
	char		*hostip;	/* cached lookup of name */
	char		*hostaddrs[COMPONENT_DBS_MAX_ADDRS];	/* cached lookup of names */	
} GpSegConfigEntry;

struct CdbComponentDatabaseInfo
{
	struct GpSegConfigEntry	*config;

	CdbComponentDatabases	*cdbs; /* point to owners */

	int16		hostSegs;	/* number of primary segments on the same hosts */
	List		*freelist;	/* list of idle segment dbs */
	int			numIdleQEs;
	int			numActiveQEs;
};


#define SEGMENT_IS_ACTIVE_MIRROR(p) \
	((p)->config->role == GP_SEGMENT_CONFIGURATION_ROLE_MIRROR ? true : false)
#define SEGMENT_IS_ACTIVE_PRIMARY(p) \
	((p)->config->role == GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY ? true : false)

#define SEGMENT_IS_ALIVE(p) \
	((p)->config->status == GP_SEGMENT_CONFIGURATION_STATUS_UP ? true : false)

#define SEGMENT_IS_IN_SYNC(p) \
	((p)->config->mode == GP_SEGMENT_CONFIGURATION_MODE_INSYNC ? true : false)
#define SEGMENT_IS_NOT_INSYNC(p) \
	((p)->config->mode == GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC ? true : false)

/* --------------------------------------------------------------------------------------------------
 * Structure for return value from getCdbSegmentDatabases()
 *
 * The storage for instances of this structure returned by getCdbSegmentInstances() is palloc'd.
 * freeCdbSegmentDatabases() can be used to release the structure.
 */
struct CdbComponentDatabases
{
	CdbComponentDatabaseInfo *segment_db_info;	/* array of
												 * SegmentDatabaseInfo's for
												 * segment databases */
	int			total_segment_dbs;		/* count of the array  */
	CdbComponentDatabaseInfo *entry_db_info;	/* array of
												 * SegmentDatabaseInfo's for
												 * entry databases */
	int			total_entry_dbs;	/* count of the array  */
	int			total_segments; /* count of distinct segindexes */
	uint8		fts_version;	/* the version of fts */
	int			expand_version;
	int			numActiveQEs;
	int			numIdleQEs;
	int			qeCounter;
	List		*freeCounterList;
};

//
typedef enum SegmentType
{
	SEGMENTTYPE_EXPLICT_WRITER = 1,
	SEGMENTTYPE_EXPLICT_READER,
	SEGMENTTYPE_ANY
}SegmentType;

/*
 * performs all necessary setup required for initializing Greenplum Database components.
 *
 * This includes cdblink_setup() and initializing the Motion Layer.
 *
 */
extern void cdb_setup(void);


/*
 * performs all necessary cleanup required when cleaning up Greenplum Database components
 * when disabling Greenplum Database functionality.
 *
 */
extern void cdb_cleanup(int code, Datum arg  pg_attribute_unused() );


/*
 * cdbcomponent_getCdbComponents() returns a pointer to a CdbComponentDatabases
 * structure. Both the segment_db_info array and the entry_db_info_array are ordered by segindex,
 * isprimary desc.
 *
 * The CdbComponentDatabases structure, and the segment_db_info array and the entry_db_info_array
 * are contained in palloc'd storage allocated from the current storage context.
 * The same is true for pointer-based values in CdbComponentDatabaseInfo.  The caller is responsible
 * for setting the current storage context and releasing the storage occupied the returned values.
 */
CdbComponentDatabases * cdbcomponent_getCdbComponents(void);
void cdbcomponent_destroyCdbComponents(void);

void cdbcomponent_updateCdbComponents(void);
/*
 * cdbcomponent_cleanupIdleQEs()
 *
 * This routine is used when a session has been idle for a while (waiting for the
 * client to send us SQL to execute). The idea is to consume less resources while sitting idle.
 *
 * The expectation is that if the session is logged on, but nobody is sending us work to do,
 * we want to free up whatever resources we can. Usually it means there is a human being at the
 * other end of the connection, and that person has walked away from their terminal, or just hasn't
 * decided what to do next. We could be idle for a very long time (many hours).
 *
 * Of course, freeing QEs means that the next time the user does send in an SQL statement,
 * we need to allocate QEs (at least the writer QEs) to do anything. This entails extra work,
 * so we don't want to do this if we don't think the session has gone idle.
 *
 * Only call these routines from an idle session.
 *
 * This routine is also called from the sigalarm signal handler (hopefully that is safe to do).
 */
void cdbcomponent_cleanupIdleQEs(bool includeWriter);

CdbComponentDatabaseInfo * cdbcomponent_getComponentInfo(int contentId);

struct SegmentDatabaseDescriptor * cdbcomponent_allocateIdleQE(int contentId, SegmentType segmentType);

void cdbcomponent_recycleIdleQE(struct SegmentDatabaseDescriptor *segdbDesc, bool forceDestroy);

bool cdbcomponent_qesExist(void);
bool cdbcomponent_activeQEsExist(void);

List *cdbcomponent_getCdbComponentsList(void);

extern void writeGpSegConfigToFTSFiles(void);

/*
 * Given total number of primary segment databases and a number of segments
 * to "skip" - this routine creates a boolean map (array) the size of total
 * number of segments and randomly selects several entries (total number of
 * total_to_skip) to be marked as "skipped". This is used for external tables
 * with the 'gpfdist' protocol where we want to get a number of *random* segdbs
 * to connect to a gpfdist client.
 */
extern bool *makeRandomSegMap(int total_primaries, int total_to_skip);

/*
 * do a host:port to IP lookup, bypass caching.
 */
extern char *getDnsAddress(char *name, int port, int elevel);

extern int16 master_standby_dbid(void);
extern GpSegConfigEntry *dbid_get_dbinfo(int16 dbid);
extern int16 contentid_get_dbid(int16 contentid, char role, bool getPreferredRoleNotCurrentRole);

extern int numsegmentsFromQD;
/*
 * Returns the number of segments
 */
extern int getgpsegmentCount(void);

extern bool IsOnConflictUpdate(PlannedStmt *ps);

extern void AvoidCorefileGeneration(void);

#define ELOG_DISPATCHER_DEBUG(...) do { \
       if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG) elog(LOG, __VA_ARGS__); \
    } while(false);

#endif   /* CDBUTIL_H */
