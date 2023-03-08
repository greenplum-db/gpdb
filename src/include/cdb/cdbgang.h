/*-------------------------------------------------------------------------
 *
 * cdbgang.h
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbgang.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _CDBGANG_H_
#define _CDBGANG_H_

#include "cdb/cdbutil.h"
#include "executor/execdesc.h"
#include "utils/faultinjector.h"
#include "utils/portal.h"
#include "storage/latch.h"

struct Port;
struct QueryDesc;
struct DirectDispatchInfo;
struct EState;
struct PQExpBufferData;
struct CdbDispatcherState;

/*
 * A gang represents a single group of workers on each connected segDB
 */
typedef struct Gang
{
	GangType type;

	int	size;

	/*
	 * Array of QEs/segDBs that make up this gang. Sorted by segment index.
	 */
	struct SegmentDatabaseDescriptor **db_descriptors;	

	/* For debugging purposes only. These do not add any actual functionality. */
	bool allocated;
} Gang;

extern int qe_identifier;

extern int host_primary_segment_count;
extern int ic_htab_size;

extern MemoryContext GangContext;
extern Gang *CurrentGangCreating;

extern WaitEventSet *DispWaitSet;

/*
 * cdbgang_createGang:
 *
 * Creates a new gang by logging on a session to each segDB involved.
 *
 * call this function in GangContext memory context.
 * elog ERROR or return a non-NULL gang.
 */
extern Gang *
cdbgang_createGang(List *segments, SegmentType segmentType);

extern const char *gangTypeToString(GangType type);

extern void setupCdbProcessList(ExecSlice *slice);

extern List *getCdbProcessesForQD(int isPrimary);

extern Gang *AllocateGang(struct CdbDispatcherState *ds, enum GangType type, List *segments);
extern void RecycleGang(Gang *gp, bool forceDestroy);
extern void DisconnectAndDestroyAllGangs(bool resetSession);
extern void DisconnectAndDestroyUnusedQEs(void);

extern void GpDropTempTables(void);
extern void ResetAllGangs(void);

extern struct SegmentDatabaseDescriptor *getSegmentDescriptorFromGang(const Gang *gp, int seg);

Gang *buildGangDefinition(List *segments, SegmentType segmentType);
bool build_gpqeid_param(char *buf, int bufsz, bool is_writer, int identifier, int hostSegs, int icHtabSize);

extern void makeOptions(char **options, char **diff_options);
extern bool segment_failure_due_to_recovery(const char *error_message);
extern bool segment_failure_due_to_missing_writer(const char *error_message);

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

extern void resetSessionForPrimaryGangLoss(void);

/*
 * MPP Worker Process information
 *
 * This structure represents the global information about a worker process.
 * It is constructed on the entry process (QD) and transmitted as part of
 * the global slice table to the involved QEs. Note that this is an
 * immutable, fixed-size structure so it can be held in a contiguous
 * array. In the ExecSlice node, however, it is held in a List.
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
	int dbid;
} CdbProcess;

typedef Gang *(*CreateGangFunc)(List *segments, SegmentType segmentType);

extern Datum gp_backend_info(PG_FUNCTION_ARGS);
extern void printCreateGangTime(int sliceId, Gang *gang);

#endif   /* _CDBGANG_H_ */
