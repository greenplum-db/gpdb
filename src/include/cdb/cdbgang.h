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
#include "utils/faultinjector.h"

struct Port;
struct QueryDesc;
struct DirectDispatchInfo;
struct EState;
struct PQExpBufferData;

/*
 * A gang represents a single group of workers on each connected segDB
 */
typedef struct Gang
{
	GangType type;
	int	gang_id;
	int	size;

	/*
	 * Keep track of dispatcher use for writer gang. (reader gangs already track
	 * this properly, since they get allocated from a list of available gangs.)
	 */
	bool dispatcherActive;

	/* the named portal that owns this gang, NULL if none */
	char *portal_name;

	/*
	 * Array of QEs/segDBs that make up this gang. Sorted by segment index.
	 */
	struct SegmentDatabaseDescriptor *db_descriptors;	

	/* For debugging purposes only. These do not add any actual functionality. */
	bool allocated;

	/* should be destroyed if set */
	bool noReuse;

	/* memory context */
	MemoryContext perGangContext;
} Gang;

extern int qe_gang_id;

extern int host_segments;

extern MemoryContext GangContext;
extern Gang *CurrentGangCreating;

extern Gang *AllocateReaderGang(GangType type, char *portal_name);

extern Gang *AllocateWriterGang(void);

extern List *getCdbProcessList(Gang *gang, int sliceIndex, struct DirectDispatchInfo *directDispatch);

extern bool GangOK(Gang *gp);

extern List *getCdbProcessesForQD(int isPrimary);

extern void freeGangsForPortal(char *portal_name);

extern void DisconnectAndDestroyGang(Gang *gp);
extern void DisconnectAndDestroyAllGangs(bool resetSession);

extern void CheckForResetSession(void);

extern List *getAllIdleReaderGangs(void);

extern List *getAllAllocatedReaderGangs(void);

extern CdbComponentDatabases *getComponentDatabases(void);

extern bool GangsExist(void);

extern struct SegmentDatabaseDescriptor *getSegmentDescriptorFromGang(const Gang *gp, int seg);

bool isPrimaryWriterGangAlive(void);

Gang *buildGangDefinition(GangType type, int gang_id, int size, int content);
void build_gpqeid_param(char *buf, int bufsz, int segIndex, bool is_writer, int gangId, int hostSegs);
char *makeOptions(void);
bool segment_failure_due_to_recovery(struct PQExpBufferData *segdbDesc);

/*
 * disconnectAndDestroyIdleReaderGangs()
 *
 * This routine is used when a session has been idle for a while (waiting for the
 * client to send us SQL to execute). The idea is to consume less resources while sitting idle.
 *
 * The expectation is that if the session is logged on, but nobody is sending us work to do,
 * we want to free up whatever resources we can. Usually it means there is a human being at the
 * other end of the connection, and that person has walked away from their terminal, or just hasn't
 * decided what to do next. We could be idle for a very long time (many hours).
 *
 * Of course, freeing gangs means that the next time the user does send in an SQL statement,
 * we need to allocate gangs (at least the writer gang) to do anything. This entails extra work,
 * so we don't want to do this if we don't think the session has gone idle.
 *
 * Only call these routines from an idle session.
 *
 * This routine is also called from the sigalarm signal handler (hopefully that is safe to do).
 */
extern void disconnectAndDestroyIdleReaderGangs(void);

extern void cleanupPortalGangs(Portal portal);

extern int largestGangsize(void);
extern void setLargestGangsize(int size);

extern int gp_pthread_create(pthread_t *thread, void *(*start_routine)(void *), void *arg, const char *caller);

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
extern void cdbgang_parse_gpqeid_params(struct Port *port, const char *gpqeid_value);

/*
 * MPP Worker Process information
 *
 * This structure represents the global information about a worker process.
 * It is constructed on the entry process (QD) and transmitted as part of
 * the global slice table to the involved QEs. Note that this is an
 * immutable, fixed-size structure so it can be held in a contiguous
 * array. In the Slice node, however, it is held in a List.
 */
typedef struct CdbProcess
{
	NodeTag type;

	/*
	 * These fields are established at connection (libpq) time and are
	 * available to the QD in PGconn structure associated with connected
	 * QE. It needs to be explicitly transmitted to QE's
	 */
	char *listenerAddr; /* Interconnect listener IPv4 address, a C-string */

	int listenerPort; /* Interconnect listener port */
	int pid; /* Backend PID of the process. */

	int contentid;
} CdbProcess;

typedef Gang *(*CreateGangFunc)(GangType type, int gang_id, int size, int content);

extern void cdbgang_setAsync(bool async);

#endif   /* _CDBGANG_H_ */
