/*-------------------------------------------------------------------------
 *
 * cdbutil.h
 *	  Header file for routines in cdbutil.c and results returned by
 *	  those routines.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbutil.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBUTIL_H
#define CDBUTIL_H

/*
 * Copied from geqo_random.h (deprecated)
 */

#include <math.h>
#include "catalog/gp_segment_config.h"

/* cdb_rand returns a random float value between 0 and 1 inclusive */
#define cdb_rand() ((double) random() / (double) MAX_RANDOM_VALUE)

/* cdb_randint returns integer value between lower and upper inclusive */
#define cdb_randint(upper,lower) \
	( (int) floor( cdb_rand()*(((upper)-(lower))+0.999999) ) + (lower) )

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
typedef struct CdbComponentDatabaseInfo
{
	int16		dbid;			/* the dbid of this database */
	int16		segindex;		/* content indicator: -1 for entry database,
								 * 0, ..., n-1 for segment database */

	char		role;			/* primary, master, mirror, master-standby */
	char		preferred_role; /* what role would we "like" to have this segment in ? */
	char		mode;
	char		status;

	char	   *hostname;		/* name or ip address of host machine */
	char	   *address;		/* ip address of host machine */

	char	   *datadir;		/* absolute path to data directory on the host. */

	char	   *hostip;			/* cached lookup of name */
	int32		port;			/* port that instance is listening on */

	char	   *hostaddrs[COMPONENT_DBS_MAX_ADDRS];	/* cached lookup of names */	
	int16		hostSegs;		/* number of primary segments on the same hosts */
} CdbComponentDatabaseInfo;

#define SEGMENT_IS_ACTIVE_MIRROR(p) \
	((p)->role == GP_SEGMENT_CONFIGURATION_ROLE_MIRROR ? true : false)
#define SEGMENT_IS_ACTIVE_PRIMARY(p) \
	((p)->role == GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY ? true : false)

#define SEGMENT_IS_ALIVE(p) \
	((p)->status == GP_SEGMENT_CONFIGURATION_STATUS_UP ? true : false)

#define SEGMENT_IS_IN_SYNC(p) \
	((p)->mode == GP_SEGMENT_CONFIGURATION_MODE_INSYNC ? true : false)
#define SEGMENT_IS_NOT_INSYNC(p) \
	((p)->mode == GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC ? true : false)

/* --------------------------------------------------------------------------------------------------
 * Structure for return value from getCdbSegmentDatabases()
 *
 * The storage for instances of this structure returned by getCdbSegmentInstances() is palloc'd.
 * freeCdbSegmentDatabases() can be used to release the structure.
 */
typedef struct CdbComponentDatabases
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
	int			my_dbid;		/* the dbid of this database */
	int			my_segindex;	/* the content of this database */
	bool		my_isprimary;	/* the isprimary flag of this database */
	int		fts_version;	/* the version of fts */
} CdbComponentDatabases;

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
extern void cdb_cleanup(int code, Datum arg  __attribute__((unused)) );


/*
 * getCdbComponentDatabases() returns a pointer to a CdbComponentDatabases
 * structure. Both the segment_db_info array and the entry_db_info_array are ordered by segindex,
 * isprimary desc.
 *
 * The CdbComponentDatabases structure, and the segment_db_info array and the entry_db_info_array
 * are contained in palloc'd storage allocated from the current storage context.
 * The same is true for pointer-based values in CdbComponentDatabaseInfo.  The caller is responsible
 * for setting the current storage context and releasing the storage occupied the returned values.
 *
 * The function freeCdbComponentDatabases() is used to release the structure
 * returned by getCdbComponentDatabases().
 *
 */
extern CdbComponentDatabases *getCdbComponentDatabases(void);

/*
 * freeCdbComponentDatabases() releases the palloc'd storage returned by
 * getCdbComponentDatabases().
 */
extern void freeCdbComponentDatabases(CdbComponentDatabases *pDBs);

// UNDONE: This was a private procedure... are there any issues in making it public???
/*
 * getCdbComponentInfo
 *
 *
 * Storage for the SegmentInstances block and all subsidiary
 * structures are allocated from the caller's context.
 */
extern CdbComponentDatabases *getCdbComponentInfo(bool DnsLookupFailureIsError);

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
extern CdbComponentDatabaseInfo *dbid_get_dbinfo(int16 dbid);
extern int16 contentid_get_dbid(int16 contentid, char role, bool getPreferredRoleNotCurrentRole);

/*
 * Returns the number of segments
 *
 * N.B.  Gp_role must be either dispatch or execute, since
 * when utiliy	no mpp catalog tables are read.  An Assert is
 * thrown if Gp_role = utility.
 */
extern int	getgpsegmentCount(void);

extern bool isSockAlive(int sock);

#define ELOG_DISPATCHER_DEBUG(...) do { \
       if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG) elog(LOG, __VA_ARGS__); \
    } while(false);

#define WRITE_LOG_DISPATCHER_DEBUG(...) do { \
	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG) write_log(__VA_ARGS__); \
    } while(false);

#endif   /* CDBUTIL_H */
