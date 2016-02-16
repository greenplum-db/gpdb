/*-------------------------------------------------------------------------
 *
 * cdbgang.h
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#ifndef _CDBGANG_H_
#define _CDBGANG_H_

#include "cdb/cdbutil.h"
#include "executor/execdesc.h"
#include <pthread.h>

struct Port;                    /* #include "libpq/libpq-be.h" */
struct QueryDesc;               /* #include "executor/execdesc.h" */
struct DirectDispatchInfo;

/*
 * GangType enumeration is used in several structures related to CDB
 * slice plan support.
 */
typedef enum GangType
{
	GANGTYPE_UNALLOCATED,       /* a root slice executed by the qDisp */
	GANGTYPE_ENTRYDB_READER,    /* a 1-gang with read access to the entry db */
	GANGTYPE_PRIMARY_READER,    /* a 1-gang or N-gang to read the segment dbs */
	GANGTYPE_PRIMARY_WRITER,    /* the N-gang that can update the segment dbs */
} GangType;

/*
 * A gang represents a single worker on each connected segDB
 */
typedef struct Gang
{
	GangType	type;
	int			gang_id;
	int			size;			/* segment_count or segdb_count ? */

	/* MPP-6253: on *writer* gangs keep track of dispatcher use
	 * (reader gangs already track this properly, since they get
	 * allocated from a list of available gangs.*/
	bool		dispatcherActive; 

	/* the named portal that owns this gang, NULL if none */
	char		*portal_name;

	/* Array of QEs/segDBs that make up this gang */
	struct SegmentDatabaseDescriptor *db_descriptors;	

	/* For debugging purposes only. These do not add any actual functionality. */
	bool		active;
	bool		all_valid_segdbs_connected;
	bool		allocated;

	/* should be destroyed in cleanupGang() if set*/
	bool		noReuse;

	/* MPP-24003: pointer to array of segment database info for each reader and writer gang. */
	struct		CdbComponentDatabaseInfo *segment_database_info;
} Gang;

/*
 * GangMgr
 *
 * GangMgr manage gang's lifecycle.
 *
 */
typedef struct GangMgrMethods
{
	// Construct global slice table by assigning gangs to slices of the slice table.
	// void		(*assign_gangs) (MemoryContext context);

	Gang 	   *(*allocateGang) (GangType type, int size, int content, char *portal_name);
	Gang 	   *(*allocateWriterGang) (void);

	List 	   *(*getAllReaderGangs) (void);
	List	   *(*getAllIdleReaderGangs) (void);
	List	   *(*getAllBusyReaderGangs) (void);
	Gang	   *(*findGangById) (int gang_id);

	// Whether Gang is in good state by checking libpq connection
	bool 		(*gangOK) (Gang *gp);
	bool 		(*gangsExist) (void);
	void 		(*detectFailedConnections) (void);

	void 		(*disconnectAndDestroyAllGangs) (void);

	void 		(*freeGangsForPortal) (char *portal_name);
	void 		(*cleanupPortalGangs) (Portal portal);
	void 		(*resetSessionForPrimaryGangLoss) (void);

	// cleanupIdleReaderGangs and cleanupAllIdleGangs free gang resource when session has been idle
	// for a while. Only call these from an idle session.
	void 		(*cleanupIdleReaderGangs) (void);
	void 		(*cleanupAllIdleGangs) (void);

	int 		(*largestGangsize) (void);

	/*
	 * cdbgang_parse_gpqeid_params
	 *
	 * Called very early in backend initialization, to interpret the "gpqeid"
	 * parameter value that a qExec receives from its qDisp.
	 *
	 * At this point, client authentication has not been done; the backend
	 * command line options have not been processed; GUCs have the settings
	 * inherited from the postmaster; etc; so don't try to do too much in here.
	 */
	void 		(*cdbgang_parse_gpqeid_params) (struct Port *port, const char* gpqeid_value);
	void		(*cdbgang_parse_gpqdid_params) (struct Port *port, const char* gpqdid_value);
	struct SegmentDatabaseDescriptor * (*getSegmentDescriptorFromGang) (const Gang *gp, int seg);
} GangMgrMethods;

// Why this method is in cdbgang.c?
extern void CheckForResetSession(void);

extern int gp_pthread_create(pthread_t *thread, void *(*start_routine)(void *), void *arg, const char *caller);

typedef struct GangMgrContext
{
	NodeTag			type;			/* identifies exact kind of context */
	GangMgrMethods 	methods;		/* virtual function table */
} GangMgrContext;

#endif   /* _CDBGANG_H_ */
