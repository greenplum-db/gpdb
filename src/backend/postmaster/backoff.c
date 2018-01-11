/*-------------------------------------------------------------------------
 *
 * backoff.c
 *	  Query Prioritization
 *
 * This file contains functions that implement the Query Prioritization
 * feature. Query prioritization is implemented by employing a
 * 'backing off' technique where each backend sleeps to let some other
 * backend use the CPU. A sweeper process identifies backends that are
 * making active progress and determines what the relative CPU usage
 * should be.
 *
 * BackoffBackendTick() - a CHECK_FOR_INTERRUPTS() call in a backend
 *						  leads to a backend 'tick'. If enough 'ticks'
 *						  elapse, then the backend considers a
 *						  backoff.
 * BackoffSweeper()		- workhorse for the sweeper process
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/postmaster/backoff.c
 *
 *-------------------------------------------------------------------------
 */

#include "postmaster/backoff.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#ifndef HAVE_GETRUSAGE
#include "rusagestub.h"
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include "storage/ipc.h"
#include "storage/smgr.h"
#include "storage/shmem.h"
#include "utils/resowner.h"
#include "cdb/cdbvars.h"
#include "storage/backendid.h"
#include "pgstat.h"
#include "miscadmin.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "libpq-fe.h"
#include <unistd.h>

#include <signal.h>
#include "libpq/pqsignal.h"
#include "utils/ps_status.h"
#include "tcop/tcopprot.h"
#include "storage/pmsignal.h"	/* PostmasterIsAlive */
#include "catalog/pg_database.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_tablespace.h"
#include "catalog/catalog.h"
#include "storage/sinval.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "funcapi.h"
#include "catalog/pg_type.h"
#include "access/tuptoaster.h"
#include "port/atomics.h"
#include "pg_trace.h"

extern bool gp_debug_resqueue_priority;

/* Enable for more debug info to be logged */
/* #define BACKOFF_DEBUG */

/**
 * Difference of two timevals in microsecs
 */
#define TIMEVAL_DIFF_USEC(b, a) ((double) (b.tv_sec - a.tv_sec) * 1000000.0 + (b.tv_usec - a.tv_usec))

/* In ms */
#define MIN_SLEEP_THRESHOLD  5000

/* In ms */
#define DEFAULT_SLEEP_TIME 100.0

/**
 * A statement id consists of a session id and command count.
 */
typedef struct StatementId
{
	int			sessionId;
	int			commandCount;
}	StatementId;

/* Invalid statement id */
static const struct StatementId InvalidStatementId = {0, 0};

/**
 * This is information that only the current backend ever needs to see.
 */
typedef struct BackoffBackendLocalEntry
{
	int			processId;		/* Process Id of backend */
	struct rusage startUsage;	/* Usage when current statement began. To
								 * account for caching of backends. */
	struct rusage lastUsage;	/* Usage statistics when backend process
								 * performed local backoff action */
	double		lastSleepTime;	/* Last sleep time when local backing-off
								 * action was performed */
	int			counter;		/* Local counter is used as an approx measure
								 * of time */
	bool		inTick;			/* Is backend currently performing tick? - to
								 * prevent nested calls */
	bool		groupingTimeExpired;	/* Should backend try to find better
										 * leader? */
}	BackoffBackendLocalEntry;

/**
 * There is a backend entry for every backend with a valid backendid on the master and segments.
 */
typedef struct BackoffBackendSharedEntry
{
	struct StatementId statementId;		/* A statement Id. Can be invalid. */
	int			groupLeaderIndex;		/* Who is my leader? */
	int			groupSize;		/* How many in my group ? */
	int			numFollowers;	/* How many followers do I have? */

	/* These fields are written by backend and read by sweeper process */
	struct timeval lastCheckTime;		/* Last time the backend process
										 * performed local back-off action.
										 * Used to determine inactive
										 * backends. */

	/* These fields are written to by sweeper and read by backend */
	bool		backoff;		/* If set to false, then no backoff to be
								 * performed by this backend */
	double		targetUsage;	/* Current target CPU usage as calculated by
								 * sweeper */
	bool		earlyBackoffExit;		/* Sweeper asking backend to stop
										 * backing off */

	/* These fields are written to and read by sweeper */
	bool		isActive;		/* Sweeper marking backend as active based on
								 * lastCheckTime */
	int			numFollowersActive;		/* If backend is a leader, this
										 * represents number of followers that
										 * are active */

	/* These fields are wrtten by backend during init and by manual adjustment */
	int			weight;			/* Weight of the statement that this backend
								 * belongs to */

}	BackoffBackendSharedEntry;

/**
 * Local entry for backoff.
 */
static BackoffBackendLocalEntry myLocalEntry;

/**
 * This is the global state of the backoff mechanism. It is a singleton structure - one
 * per postmaster. It exists on master and segments. All backends with a valid backendid
 * and entry in the ProcArray have access to this information.
 */
typedef struct BackoffState
{
	BackoffBackendSharedEntry *backendEntries;	/* Indexed by backend ids */
	int			numEntries;

	bool		sweeperInProgress;		/* Is the sweeper process working? */
	int			lastTotalStatementWeight;		/* To keep track of total
												 * weight */
}	BackoffState;

/**
 * Pointer to singleton struct used by the backoff mechanism.
 */
BackoffState *backoffSingleton = NULL;

/* Statement-id related */

static inline void init(StatementId * s, int sessionId, int commandCount);
static inline void setInvalid(StatementId * s);
static inline bool isValid(const StatementId * s);
static inline bool equalStatementId(const StatementId * s1, const StatementId * s2);

/* Main accessor methods for backoff entries */
static inline const BackoffBackendSharedEntry *getBackoffEntryRO(int index);
static inline BackoffBackendSharedEntry *getBackoffEntryRW(int index);

/* Backend uses these */
static inline BackoffBackendLocalEntry *myBackoffLocalEntry(void);
static inline BackoffBackendSharedEntry *myBackoffSharedEntry(void);
static inline void SwitchGroupLeader(int newLeaderIndex);
static inline bool groupingTimeExpired(void);
static inline void findBetterGroupLeader(void);
static inline bool isGroupLeader(int index);
static inline void BackoffBackend(void);

/* Init and exit routines */
static void BackoffStateAtExit(int code, Datum arg);

/* Routines to access global state */
static inline double numProcsPerSegment(void);

/* Sweeper related routines */
static void BackoffSweeper(void);
static void BackoffSweeperLoop(void);
NON_EXEC_STATIC void BackoffSweeperMain(int argc, char *argv[]);
static void BackoffRequestShutdown(SIGNAL_ARGS);
static volatile bool sweeperShutdownRequested = false;
static volatile bool isSweeperProcess = false;

/* Resource queue related routines */
static int	BackoffPriorityValueToInt(const char *priorityVal);
static char *BackoffPriorityIntToValue(int weight);
extern List *GetResqueueCapabilityEntry(Oid queueid);

/*
 * Helper method that verifies setting of default priority guc.
 */
const char *gpvars_assign_gp_resqueue_priority_default_value(const char *newval,
												 bool doit,
								   GucSource source __attribute__((unused)));

/**
 * Primitives on statement id.
 */

static inline void
init(StatementId * s, int sessionId, int commandCount)
{
	Assert(s);
	s->sessionId = sessionId;
	s->commandCount = commandCount;
	return;
}

/**
 * Sets a statemend id to be invalid.
 */
static inline void
setInvalid(StatementId * s)
{
	init(s, InvalidStatementId.sessionId, InvalidStatementId.commandCount);
}

/**
 * Are two statement ids equal?
 */
static inline bool
equalStatementId(const StatementId * s1, const StatementId * s2)
{
	Assert(s1);
	Assert(s2);
	return ((s1->sessionId == s2->sessionId)
			&& (s1->commandCount == s2->commandCount));
}

/**
 * Is a StatementId valid?
 */
static inline bool
isValid(const StatementId * s)
{
	return !equalStatementId(s, &InvalidStatementId);
}

/**
 * Access to the local entry for this backend.
 */

static inline BackoffBackendLocalEntry *
myBackoffLocalEntry()
{
	return &myLocalEntry;
}

/**
 * Access to the shared entry for this backend.
 */
static inline BackoffBackendSharedEntry *
myBackoffSharedEntry()
{
	return getBackoffEntryRW(MyBackendId);
}

/**
 * A backend is a group leader if it is its own leader.
 */
static inline bool
isGroupLeader(int index)
{
	return (getBackoffEntryRO(index)->groupLeaderIndex == index);
}

/**
 * This method is used by a backend to switch the group leader. It is unique
 * in that it modifies the numFollowers field in its current group leader and new leader index.
 * The increments and decrements are done using atomic operations (else we may have race conditions
 * across processes). However, this code is not thread safe. We do not call these code in multi-threaded
 * situations.
 */
static inline void
SwitchGroupLeader(int newLeaderIndex)
{
	BackoffBackendSharedEntry *myEntry = myBackoffSharedEntry();
	BackoffBackendSharedEntry *oldLeaderEntry = NULL;
	BackoffBackendSharedEntry *newLeaderEntry = NULL;

	if (backoffSingleton->sweeperInProgress == true)
		return;

	Assert(newLeaderIndex < myEntry->groupLeaderIndex);
	Assert(newLeaderIndex >= 0 && newLeaderIndex < backoffSingleton->numEntries);

	oldLeaderEntry = &backoffSingleton->backendEntries[myEntry->groupLeaderIndex];
	newLeaderEntry = &backoffSingleton->backendEntries[newLeaderIndex];

	pg_atomic_sub_fetch_u32((pg_atomic_uint32 *) &oldLeaderEntry->numFollowers, 1);
	pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &newLeaderEntry->numFollowers, 1);
	myEntry->groupLeaderIndex = newLeaderIndex;
}

/*
 * Should this backend stop finding a better leader? If the backend has spent enough time working
 * on the current statement (measured in elapsedTimeForStatement), it marks grouping time expired.
 */
static inline bool
groupingTimeExpired()
{
	BackoffBackendLocalEntry *le = myBackoffLocalEntry();

	if (le->groupingTimeExpired)
	{
		return true;
	}
	else
	{
		double		elapsedTimeForStatement =
		TIMEVAL_DIFF_USEC(le->lastUsage.ru_utime, le->startUsage.ru_utime)
		+ TIMEVAL_DIFF_USEC(le->lastUsage.ru_stime, le->startUsage.ru_stime);

		if (elapsedTimeForStatement > gp_resqueue_priority_grouping_timeout * 1000.0)
		{
			le->groupingTimeExpired = true;
			return true;
		}
		else
		{
			return false;
		}
	}
}

/**
 * Executed by a backend to find a better group leader (i.e. one with a lower index), if possible.
 * This is the only method that can write to groupLeaderIndex.
 */
static inline void
findBetterGroupLeader()
{
	int			leadersLeaderIndex = -1;
	BackoffBackendSharedEntry *myEntry = myBackoffSharedEntry();
	const BackoffBackendSharedEntry *leaderEntry = getBackoffEntryRO(myEntry->groupLeaderIndex);

	Assert(myEntry);
	leadersLeaderIndex = leaderEntry->groupLeaderIndex;

	if (backoffSingleton->sweeperInProgress == true)
		return;

	/* If my leader has a different leader, then jump pointer */
	if (myEntry->groupLeaderIndex != leadersLeaderIndex)
	{
		SwitchGroupLeader(leadersLeaderIndex);
	}
	else
	{
		int			i = 0;

		for (i = 0; i < myEntry->groupLeaderIndex; i++)
		{
			const BackoffBackendSharedEntry *other = getBackoffEntryRO(i);

			if (equalStatementId(&other->statementId, &myEntry->statementId))
			{
				/* Found a better leader! */
				break;
			}
		}
		if (i < myEntry->groupLeaderIndex)
		{
			SwitchGroupLeader(i);
		}
	}
	return;
}


/**
 * Read only access to a backend entry.
 */
static inline const BackoffBackendSharedEntry *
getBackoffEntryRO(int index)
{
	return (const BackoffBackendSharedEntry *) getBackoffEntryRW(index);
}

/**
 * Gives write access to a backend entry.
 */
static inline BackoffBackendSharedEntry *
getBackoffEntryRW(int index)
{
	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE || isSweeperProcess);
	Assert(index >= 0 && index < backoffSingleton->numEntries);
	return &backoffSingleton->backendEntries[index];
}


/**
 * This method is called by the backend when it begins working on a new statement.
 * This initializes the backend entry corresponding to this backend.
 * After initialization, the backend entry immediately finds its group leader, which
 * is the first backend entry that has the same statement id with itself.
 */
void
BackoffBackendEntryInit(int sessionid, int commandcount, int weight)
{
	BackoffBackendSharedEntry *mySharedEntry = NULL;
	BackoffBackendLocalEntry *myLocalEntry = NULL;

	Assert(sessionid > -1);
	Assert(commandcount > -1);
	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE);
	Assert(!isSweeperProcess);
	Assert(weight > 0);

	/* Shared information */
	mySharedEntry = myBackoffSharedEntry();

	mySharedEntry->targetUsage = 1.0 / numProcsPerSegment();	/* Initially, do not
																 * perform any backoffs */
	mySharedEntry->isActive = false;
	mySharedEntry->backoff = true;
	mySharedEntry->earlyBackoffExit = false;

	if (gettimeofday(&mySharedEntry->lastCheckTime, NULL) < 0)
	{
		elog(ERROR, "Unable to execute gettimeofday(). Please disable query prioritization.");
	}

	mySharedEntry->groupLeaderIndex = MyBackendId;
	mySharedEntry->weight = weight;
	mySharedEntry->groupSize = 0;
	mySharedEntry->numFollowers = 1;

	/* this should happen last or the sweeper may pick up a non-complete entry */
	init(&mySharedEntry->statementId, sessionid, commandcount);
	Assert(isValid(&mySharedEntry->statementId));

	/* Local information */
	myLocalEntry = myBackoffLocalEntry();

	myLocalEntry->processId = MyProcPid;
	myLocalEntry->lastSleepTime = DEFAULT_SLEEP_TIME;
	myLocalEntry->groupingTimeExpired = false;
	if (getrusage(RUSAGE_SELF, &myLocalEntry->lastUsage) < 0)
	{
		elog(ERROR, "Unable to execute getrusage(). Please disable query prioritization.");
	}
	memcpy(&myLocalEntry->startUsage, &myLocalEntry->lastUsage, sizeof(myLocalEntry->lastUsage));
	myLocalEntry->counter = 1;
	myLocalEntry->inTick = false;

	/* Try to find a better leader for my group */
	findBetterGroupLeader();

	return;
}

/**
 * Accessing the number of procs per segment.
 */
static inline double
numProcsPerSegment()
{
	Assert(gp_enable_resqueue_priority);
	Assert(backoffSingleton);
	Assert(gp_resqueue_priority_cpucores_per_segment > 0);
	return gp_resqueue_priority_cpucores_per_segment;
}


/**
 * This method is called once in a while by a backend to determine if it needs
 * to backoff per its current usage and target usage.
 */
static inline void
BackoffBackend()
{
	BackoffBackendLocalEntry *le = NULL;
	BackoffBackendSharedEntry *se = NULL;

	/* Try to achieve target usage! */
	struct timeval currentTime;
	struct rusage currentUsage;
	double		thisProcessTime = 0.0;
	double		totalTime = 0.0;
	double		cpuRatio = 0.0;
	double		changeFactor = 1.0;

	le = myBackoffLocalEntry();
	Assert(le);
	se = myBackoffSharedEntry();
	Assert(se);
	Assert(se->weight > 0);

	/* Provide tracing information */
	TRACE_POSTGRESQL_BACKOFF_LOCALCHECK(MyBackendId);

	if (gettimeofday(&currentTime, NULL) < 0)
	{
		elog(ERROR, "Unable to execute gettimeofday(). Please disable query prioritization.");
	}

	if (getrusage(RUSAGE_SELF, &currentUsage) < 0)
	{
		elog(ERROR, "Unable to execute getrusage(). Please disable query prioritization.");
	}

	/* If backoff can be performed by this process */
	if (se->backoff)
	{
		/*
		 * How much did the cpu work on behalf of this process - incl user and
		 * sys time
		 */
		thisProcessTime = TIMEVAL_DIFF_USEC(currentUsage.ru_utime, le->lastUsage.ru_utime)
			+ TIMEVAL_DIFF_USEC(currentUsage.ru_stime, le->lastUsage.ru_stime);

		/*
		 * Absolute cpu time since the last check. This accounts for multiple
		 * procs per segment
		 */
		totalTime = TIMEVAL_DIFF_USEC(currentTime, se->lastCheckTime);

		cpuRatio = thisProcessTime / totalTime;

		cpuRatio = Min(cpuRatio, 1.0);

		changeFactor = cpuRatio / se->targetUsage;

		le->lastSleepTime *= changeFactor;

		if (le->lastSleepTime < DEFAULT_SLEEP_TIME)
			le->lastSleepTime = DEFAULT_SLEEP_TIME;

		if (gp_debug_resqueue_priority)
		{
			elog(LOG, "thissession = %d, thisProcTime = %f, totalTime = %f, targetusage = %f, cpuRatio = %f, change factor = %f, sleeptime = %f",
				 se->statementId.sessionId, thisProcessTime, totalTime, se->targetUsage, cpuRatio, changeFactor, (double) le->lastSleepTime);
		}

		memcpy(&le->lastUsage, &currentUsage, sizeof(currentUsage));
		memcpy(&se->lastCheckTime, &currentTime, sizeof(currentTime));

		if (le->lastSleepTime > MIN_SLEEP_THRESHOLD)
		{
			/*
			 * Sleeping happens in chunks so that the backend may exit early
			 * from its sleep if the sweeper requests it to.
			 */
			int			j = 0;
			long		sleepInterval = ((long) gp_resqueue_priority_sweeper_interval) * 1000L;
			int			numIterations = (int) (le->lastSleepTime / sleepInterval);
			double		leftOver = (double) ((long) le->lastSleepTime % sleepInterval);

			for (j = 0; j < numIterations; j++)
			{
				/* Sleep a chunk */
				pg_usleep(sleepInterval);
				/* Check for early backoff exit */
				if (se->earlyBackoffExit)
				{
					le->lastSleepTime = DEFAULT_SLEEP_TIME;		/* Minimize sleep time
																 * since we may need to
																 * recompute from
																 * scratch */
					break;
				}
			}
			if (j == numIterations)
				pg_usleep(leftOver);
		}
	}
	else
	{
		/*
		 * Even if this backend did not backoff, it should record current
		 * usage and current time so that subsequent calculations are
		 * accurate.
		 */
		memcpy(&le->lastUsage, &currentUsage, sizeof(currentUsage));
		memcpy(&se->lastCheckTime, &currentTime, sizeof(currentTime));
	}

	/* Consider finding a better leader for better grouping */
	if (!groupingTimeExpired())
	{
		findBetterGroupLeader();
	}
}

/**
 * The backend performs a 'tick' on its local counter as a record of the number of times
 * it called CHECK_FOR_INTERRUPTS, which we use as a loose measure of progress.
 * If the counter is sufficiently large, it performs a backoff action (see BackoffBackend()).
 */
void
BackoffBackendTick()
{
	BackoffBackendLocalEntry *le = NULL;
	BackoffBackendSharedEntry *se = NULL;
	StatementId currentStatementId = {gp_session_id, gp_command_count};

	Assert(gp_enable_resqueue_priority);

	if (!(Gp_role == GP_ROLE_DISPATCH
		  || Gp_role == GP_ROLE_EXECUTE)
		|| !IsUnderPostmaster
		|| (MyBackendId == InvalidBackendId)
		|| proc_exit_inprogress
		|| ProcDiePending		/* Proc is dying */
		|| QueryCancelPending	/* Statement cancellation */
		|| QueryFinishPending	/* Statement finish requested */
		|| InterruptHoldoffCount != 0	/* We're holding off on handling
										 * interrupts */
		|| CritSectionCount != 0	/* In critical section */
		)
	{
		/* Do nothing under these circumstances */
		return;
	}

	if (!backoffSingleton)
	{
		/* Not initialized yet. Do nothing */
		return;
	}

	Assert(backoffSingleton);

	le = myBackoffLocalEntry();
	se = myBackoffSharedEntry();

	if (!equalStatementId(&se->statementId, &currentStatementId))
	{
		/* This backend's entry has not yet been initialized. Do nothing yet. */
		return;
	}

	if (le->inTick)
	{
		/* No nested calls allowed. This may happen during elog calls :( */
		return;
	}

	Assert(le->inTick == false);
	Assert(le->counter < gp_resqueue_priority_local_interval);

	le->inTick = true;

	le->counter++;
	if (le->counter == gp_resqueue_priority_local_interval)
	{
		le->counter = 0;
		/* Enough time has passed. Perform backoff. */
		BackoffBackend();
		se->earlyBackoffExit = false;
	}

	le->inTick = false;

	Assert(le->counter < gp_resqueue_priority_local_interval);
	Assert(le->inTick == false);
}

/**
 * BackoffSweeper() looks at all the backend structures to determine if any
 * backends are not making progress. This is done by inspecting the lastchecked
 * time.  It also calculates the total weight of all 'active' backends to
 * re-calculate the target CPU usage per backend process. If it finds that a
 * backend is trying to request more CPU resources than the maximum CPU that it
 * can get (such a backend is called a 'pegger'), it assigns maxCPU to it.
 *
 * For example:
 * Let Qi be the ith query statement, Ri be the target CPU usage for Qi,
 * Wi be the statement weight for Qi, W be the total statements weight.
 * For simplicity, let's assume every statement only has 1 backend per segment.
 *
 * Let there be 4 active queries with weights {1,100,10,1000} with K=3 CPUs
 * available per segment to share. The maximum CPU that a backend can get is
 * maxCPU = 1.0. The total active statements weight is
 * W (activeWeight) = 1 + 100 + 10 + 1000 = 1111.
 * The following algorithm determines that Q4 is pegger, because
 * K * W4 / W > maxCPU, which is 3000/1111 > 1.0, so we assign R4 = 1.0.
 * Now K becomes 2.0, W becomes 111.
 * It restarts from the beginning and determines that Q2 is now a pegger as
 * well, because K * W2 / W > maxCPU, which is 200/111 > 1.0, we assign
 * R2 = 1.0. Now there is only 1 CPU left and no peggers left. We continue
 * to distribute the left 1 CPU to other backends according to their weight,
 * so we assign the target CPU ratio of R1=1/11 and R3=10/11. The final
 * target CPU assignments are {0.09,1.0,0.91,1.0}.
 *
 * If there are multiple backends within a segment running for the query Qi,
 * the target CPU ratio Ri for query Qi is divided equally among all the
 * active backends belonging to the query.
 */
void
BackoffSweeper()
{
	int			i = 0;

	/* The overall weight of active statements */
	volatile double activeWeight = 0.0;
	int			numActiveBackends = 0;
	int			numActiveStatements = 0;

	/* The overall weight of active and inactive statements */
	int			totalStatementWeight = 0;
	int			numValidBackends = 0;
	int			numStatements = 0;

	struct timeval currentTime;

	if (gettimeofday(&currentTime, NULL) < 0)
	{
		elog(ERROR, "Unable to execute gettimeofday(). Please disable query prioritization.");
	}

	Assert(backoffSingleton->sweeperInProgress == false);

	backoffSingleton->sweeperInProgress = true;

	TRACE_POSTGRESQL_BACKOFF_GLOBALCHECK();

	/* Reset status for all the backend entries */
	for (i = 0; i < backoffSingleton->numEntries; i++)
	{
		BackoffBackendSharedEntry *se = getBackoffEntryRW(i);

		se->isActive = false;
		se->numFollowersActive = 0;
		se->backoff = true;
	}

	/*
	 * Mark backends that are active. Count of active group members is
	 * maintained at their group leader.
	 */
	for (i = 0; i < backoffSingleton->numEntries; i++)
	{
		BackoffBackendSharedEntry *se = getBackoffEntryRW(i);

		if (isValid(&se->statementId))
		{
			Assert(se->weight > 0);
			if (TIMEVAL_DIFF_USEC(currentTime, se->lastCheckTime)
				< gp_resqueue_priority_inactivity_timeout * 1000.0)
			{
				/*
				 * This is an active backend. Need to maintain count at group
				 * leader
				 */
				BackoffBackendSharedEntry *gl = getBackoffEntryRW(se->groupLeaderIndex);

				if (gl->numFollowersActive == 0)
				{
					activeWeight += se->weight;
					numActiveStatements++;
				}
				gl->numFollowersActive++;
				numActiveBackends++;
				se->isActive = true;
			}
			if (isGroupLeader(i))
			{
				totalStatementWeight += se->weight;
				numStatements++;
			}
			numValidBackends++;
		}
	}

	/* Sanity checks */
	Assert(numActiveBackends <= numValidBackends);
	Assert(numValidBackends >= numStatements);

	/**
	 * Under certain conditions, we want to avoid backoff. Cases are:
	 * 1. A statement just entered or exited
	 * 2. A statement's weight changed due to user intervention via gp_adjust_priority()
	 * 3. There is no active backend
	 * 4. There is exactly one statement
	 * 5. Total number valid of backends <= number of procs per segment
	 * Case 1 and 2 are approximated by checking if total statement weight changed since last sweeper loop.
	 */
	if (backoffSingleton->lastTotalStatementWeight != totalStatementWeight
		|| numActiveBackends == 0
		|| numStatements == 1
		|| numValidBackends <= numProcsPerSegment())
	{
		/* Write to targets */
		for (i = 0; i < backoffSingleton->numEntries; i++)
		{
			BackoffBackendSharedEntry *se = getBackoffEntryRW(i);

			se->backoff = false;
			se->earlyBackoffExit = true;
			se->targetUsage = 1.0;
		}
	}
	else
	{
		/**
		 * There are multiple statements with active backends.
		 *
		 * Let 'found' be true if we find a backend is trying to
		 * request more CPU resources than the maximum CPU that it can
		 * get. No matter how high the priority of a query process, it
		 * can utilize at most a single CPU at a time.
		 */
		bool		found = true;
		int			numIterations = 0;
		double		CPUAvailable = numProcsPerSegment();
		double		maxCPU = Min(1.0, numProcsPerSegment());	/* Maximum CPU that a
																 * backend can get */

		Assert(maxCPU > 0.0);

		if (gp_debug_resqueue_priority)
		{
			elog(LOG, "before allocation: active backends = %d, active weight = %f, cpu available = %f", numActiveBackends, activeWeight, CPUAvailable);
		}

		while (found)
		{
			found = false;

			/**
			 * We try to find one or more backends that deserve maxCPU.
			 */
			for (i = 0; i < backoffSingleton->numEntries; i++)
			{
				BackoffBackendSharedEntry *se = getBackoffEntryRW(i);

				if (se->isActive
					&& se->backoff)
				{
					double		targetCPU = 0.0;
					const BackoffBackendSharedEntry *gl = getBackoffEntryRO(se->groupLeaderIndex);

					Assert(gl->numFollowersActive > 0);

					if (activeWeight <= 0.0)
					{
						/*
						 * There is a race condition here:
						 * Backend A,B,C are belong to same statement and have weight of
						 * 100000.
						 *
						 * Timestamp1: backend A's leader is A, backend B's leader is B
						 * backend C's leader is also B.
						 *
						 * Timestamp2: Sweeper calculates the activeWeight to 200000.
						 *
						 * Timestamp3: backend B changes it's leader to A.
						 *
						 * Timestamp4: Sweeper try to find the backends who deserve maxCPU,
						 * if backend A, B, C all deserve maxCPU, then activeWeight = 
						 * 200000 - 100000/1 - 100000/1 - 100000/2 which is less than zero.
						 *
						 * We can stop sweeping for such race condition because current
						 * backoff mechanism dose not ask for accurate control.
						 */
						backoffSingleton->sweeperInProgress = false;
						elog(LOG, "activeWeight underflow!");
						return;
					}

					Assert(activeWeight > 0.0);
					Assert(se->weight > 0.0);

					targetCPU = (CPUAvailable) * (se->weight) / activeWeight / gl->numFollowersActive;

					/**
					 * Some statements may be weighed so heavily that they are allocated the maximum cpu ratio.
					 */
					if (targetCPU >= maxCPU)
					{
						Assert(numProcsPerSegment() >= 1.0);	/* This can only happen
																 * when there is more
																 * than one proc */
						se->targetUsage = maxCPU;
						se->backoff = false;
						activeWeight -= (se->weight / gl->numFollowersActive);

						CPUAvailable -= maxCPU;
						found = true;
					}
				}
			}
			numIterations++;
			AssertImply(found, (numIterations <= floor(numProcsPerSegment())));
			Assert(numIterations <= ceil(numProcsPerSegment()));
		}

		if (gp_debug_resqueue_priority)
		{
			elog(LOG, "after heavy backends: active backends = %d, active weight = %f, cpu available = %f", numActiveBackends, activeWeight, CPUAvailable);
		}

		/**
		 * Distribute whatever is the CPU available among the rest.
		 */
		for (i = 0; i < backoffSingleton->numEntries; i++)
		{
			BackoffBackendSharedEntry *se = getBackoffEntryRW(i);

			if (se->isActive
				&& se->backoff)
			{
				const BackoffBackendSharedEntry *gl = getBackoffEntryRO(se->groupLeaderIndex);

				Assert(activeWeight > 0.0);
				Assert(gl->numFollowersActive > 0);
				Assert(se->weight > 0.0);
				se->targetUsage = (CPUAvailable) * (se->weight) / activeWeight / gl->numFollowersActive;
			}
		}
	}


	backoffSingleton->lastTotalStatementWeight = totalStatementWeight;
	backoffSingleton->sweeperInProgress = false;

	if (gp_debug_resqueue_priority)
	{
		StringInfoData str;

		initStringInfo(&str);
		appendStringInfo(&str, "num active statements: %d ", numActiveStatements);
		appendStringInfo(&str, "num active backends: %d ", numActiveBackends);
		appendStringInfo(&str, "targetusages: ");
		for (i = 0; i < MaxBackends; i++)
		{
			const BackoffBackendSharedEntry *se = getBackoffEntryRO(i);

			if (se->isActive)
				appendStringInfo(&str, "(%d,%f)", i, se->targetUsage);
		}
		elog(LOG, "%s", (const char *) str.data);
		pfree(str.data);
	}

}

/**
 * Initialize global sate of backoff scheduler. This is called during creation
 * of shared memory and semaphores.
 */
void
BackoffStateInit()
{
	bool		found = false;

	/* Create or attach to the shared array */
	backoffSingleton = (BackoffState *) ShmemInitStruct("Backoff Global State", sizeof(BackoffState), &found);

	if (!found)
	{
		bool		ret = false;

		/*
		 * We're the first - initialize.
		 */
		MemSet(backoffSingleton, 0, sizeof(BackoffState));
		backoffSingleton->numEntries = MaxBackends;
		backoffSingleton->backendEntries = (BackoffBackendSharedEntry *) ShmemInitStruct("Backoff Backend Entries", mul_size(sizeof(BackoffBackendSharedEntry), backoffSingleton->numEntries), &ret);
		backoffSingleton->sweeperInProgress = false;
		Assert(!ret);
	}

	on_shmem_exit(BackoffStateAtExit, 0);
}

/**
 * This backend is done working on a statement.
 */
void
BackoffBackendEntryExit()
{
	if (MyBackendId >= 0
		&& (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE))
	{
		BackoffBackendSharedEntry *se = myBackoffSharedEntry();

		Assert(se);
		setInvalid(&se->statementId);
	}
	return;
}

/**
 * Invalidate the statement id corresponding to this backend so that it may
 * be eliminated from consideration by the sweeper early.
 */
static void
BackoffStateAtExit(int code, Datum arg)
{
	BackoffBackendEntryExit();
}


/**
 * An interface to re-weigh an existing session on the master and all backends.
 * Input:
 *	session id - what session is statement on?
 *	command count - what is the command count of statement.
 *	priority value - text, what should be the new priority of this statement.
 * Output:
 *	number of backends whose weights were changed by this call.
 */
Datum
gp_adjust_priority_value(PG_FUNCTION_ARGS)
{
	int32		session_id = PG_GETARG_INT32(0);
	int32		command_count = PG_GETARG_INT32(1);
	Datum		dVal = PG_GETARG_DATUM(2);
	char	   *priorityVal;
	int			wt;

	priorityVal = TextDatumGetCString(dVal);

	if (!priorityVal)
		elog(ERROR, "Invalid priority value specified.");

	wt = BackoffPriorityValueToInt(priorityVal);

	Assert(wt > 0);

	pfree(priorityVal);

	return DirectFunctionCall3(gp_adjust_priority_int, Int32GetDatum(session_id),
							Int32GetDatum(command_count), Int32GetDatum(wt));

}

/**
 * An interface to re-weigh an existing session on the master and all backends.
 * Input:
 *	session id - what session is statement on?
 *	command count - what is the command count of statement.
 *	weight - int, what should be the new priority of this statement.
 * Output:
 *	number of backends whose weights were changed by this call.
 */
Datum
gp_adjust_priority_int(PG_FUNCTION_ARGS)
{
	int32		session_id = PG_GETARG_INT32(0);
	int32		command_count = PG_GETARG_INT32(1);
	int32		wt = PG_GETARG_INT32(2);
	int			numfound = 0;
	StatementId sid;

	if (!gp_enable_resqueue_priority)
		elog(ERROR, "Query prioritization is disabled.");

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("Only superuser can re-prioritize a query after it has begun execution."))));

	if (Gp_role == GP_ROLE_UTILITY)
		elog(ERROR, "Query prioritization does not work in utility mode.");

	if (wt <= 0)
		elog(ERROR, "Weight of statement must be greater than 0.");

	init(&sid, session_id, command_count);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		int			i = 0;
		CdbPgResults cdb_pgresults = {NULL, 0};
		char		cmd[255];

		/*
		 * Make sure the session exists before dispatching
		 */
		for (i = 0; i < backoffSingleton->numEntries; i++)
		{
			BackoffBackendSharedEntry *se = getBackoffEntryRW(i);

			if (equalStatementId(&se->statementId, &sid))
			{
				if (gp_debug_resqueue_priority)
				{
					elog(LOG, "changing weight of (%d:%d) from %d to %d", se->statementId.sessionId, se->statementId.commandCount, se->weight, wt);
				}

				se->weight = wt;
				numfound++;
			}
		}

		if (numfound == 0)
			elog(ERROR, "Did not find any backend entries for session %d, command count %d.", session_id, command_count);

		/*
		 * Ok, it exists, dispatch the command to the segDBs.
		 */
		sprintf(cmd, "select gp_adjust_priority(%d,%d,%d)", session_id, command_count, wt);

		CdbDispatchCommand(cmd, DF_WITH_SNAPSHOT, &cdb_pgresults);

		for (i = 0; i < cdb_pgresults.numResults; i++)
		{
			struct pg_result *pgresult = cdb_pgresults.pg_results[i];

			if (PQresultStatus(pgresult) != PGRES_TUPLES_OK)
			{
				cdbdisp_clearCdbPgResults(&cdb_pgresults);
				elog(ERROR, "gp_adjust_priority: resultStatus not tuples_Ok");
			}
			else
			{

				int			j;

				for (j = 0; j < PQntuples(pgresult); j++)
				{
					int			retvalue = 0;

					retvalue = atoi(PQgetvalue(pgresult, j, 0));
					numfound += retvalue;
				}
			}
		}

		cdbdisp_clearCdbPgResults(&cdb_pgresults);

	}
	else	/* Gp_role == EXECUTE */
	{
		/*
		 * Find number of backends working on behalf of this session and
		 * distribute the weight evenly.
		 */
		int			i = 0;

		Assert(Gp_role == GP_ROLE_EXECUTE);
		for (i = 0; i < backoffSingleton->numEntries; i++)
		{
			BackoffBackendSharedEntry *se = getBackoffEntryRW(i);

			if (equalStatementId(&se->statementId, &sid))
			{
				if (gp_debug_resqueue_priority)
				{
					elog(LOG, "changing weight of (%d:%d) from %d to %d", se->statementId.sessionId, se->statementId.commandCount, se->weight, wt);
				}
				se->weight = wt;
				numfound++;
			}
		}

		if (gp_debug_resqueue_priority && numfound == 0)
		{
			elog(LOG, "did not find any matching backends on segments");
		}
	}

	PG_RETURN_INT32(numfound);
}

/*
 * Main entry point for backoff process. This forks off a sweeper process
 * and calls BackoffSweeperMain(), which does all the setup.
 *
 * This code is heavily based on pgarch.c, q.v.
 */
int
backoff_start(void)
{
	pid_t		backoffId = -1;

	switch ((backoffId = fork_process()))
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork sweeper process: %m")));
			return 0;

		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			BackoffSweeperMain(0, NULL);
			break;
		default:
			return (int) backoffId;
	}

	/* shouldn't get here */
	Assert(false);
	return 0;
}


/**
 * This method is called after fork of the sweeper process. It sets up signal
 * handlers and does initialization that is required by a postgres backend.
 */
NON_EXEC_STATIC void
BackoffSweeperMain(int argc, char *argv[])
{
	sigjmp_buf	local_sigjmp_buf;

	IsUnderPostmaster = true;
	isSweeperProcess = true;

	/* Stay away from PMChildSlot */
	MyPMChildSlot = -1;

	/* reset MyProcPid */
	MyProcPid = getpid();

	/* Lose the postmaster's on-exit routines */
	on_exit_reset();

	/* Identify myself via ps */
	init_ps_display("sweeper process", "", "", "");

	SetProcessingMode(InitProcessing);

	/*
	 * Set up signal handlers.  We operate on databases much like a regular
	 * backend, so we use the same signal handling.  See equivalent code in
	 * tcop/postgres.c.
	 */
	pqsignal(SIGHUP, SIG_IGN);
	pqsignal(SIGINT, SIG_IGN);
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, SIG_IGN);

	pqsignal(SIGTERM, die);
	pqsignal(SIGQUIT, quickdie);
	pqsignal(SIGUSR2, BackoffRequestShutdown);

	pqsignal(SIGFPE, FloatExceptionHandler);
	pqsignal(SIGCHLD, SIG_DFL);

	/*
	 * Copied from bgwriter
	 */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "Sweeper process");

	/* Early initialization */
	BaseInit();

	/* See InitPostgres()... */
	InitProcess();

	SetProcessingMode(NormalProcessing);

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * See notes in postgres.c about the design of this coding.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Prevents interrupts while cleaning up */
		HOLD_INTERRUPTS();

		/* Report the error to the server log */
		EmitErrorReport();

		/*
		 * We can now go away.  Note that because we'll call InitProcess, a
		 * callback will be registered to do ProcKill, which will clean up
		 * necessary state.
		 */
		proc_exit(0);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	PG_SETMASK(&UnBlockSig);

	MyBackendId = InvalidBackendId;

	/* main loop */
	BackoffSweeperLoop();

	/* One iteration done, go away */
	proc_exit(0);
}

/**
 * Main loop of the sweeper process. It wakes up once in a while, marks backends as active
 * or not and re-calculates CPU usage among active backends.
 */
void
BackoffSweeperLoop(void)
{
	for (;;)
	{
		CHECK_FOR_INTERRUPTS();

		if (sweeperShutdownRequested)
			break;

		/* no need to live on if postmaster has died */
		if (!PostmasterIsAlive(true))
			exit(1);

		if (gp_enable_resqueue_priority)
			BackoffSweeper();

		Assert(gp_resqueue_priority_sweeper_interval > 0.0);
		/* Sleep a while. */
		pg_usleep(gp_resqueue_priority_sweeper_interval * 1000.0);
	}							/* end server loop */

	return;
}

/**
 * Note the request to shut down.
 */
static void
BackoffRequestShutdown(SIGNAL_ARGS)
{
	sweeperShutdownRequested = true;
}

/**
 * Set returning function to inspect current state of query prioritization.
 * Input:
 *	none
 * Output:
 *	Set of (session_id, command_count, priority, weight) for all backends (on the current segment).
 *	This function is used by jetpack views gp_statement_priorities.
 */
Datum
gp_list_backend_priorities(PG_FUNCTION_ARGS)
{
	typedef struct Context
	{
		int			currentIndex;
	} Context;

	FuncCallContext *funcctx = NULL;
	Context    *context = NULL;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		/* this had better match gp_distributed_xacts view in system_views.sql */
		tupdesc = CreateTemplateTupleDesc(4, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "session_id",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "command_count",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "priority",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "weight",
						   INT4OID, -1, 0);


		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc(sizeof(Context));
		funcctx->user_fctx = (void *) context;
		context->currentIndex = 0;
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	Assert(funcctx);
	context = (Context *) funcctx->user_fctx;
	Assert(context);

	if (!IsResQueueEnabled() || !gp_enable_resqueue_priority)
		SRF_RETURN_DONE(funcctx);

	while (context->currentIndex < backoffSingleton->numEntries)
	{
		Datum		values[4];
		bool		nulls[4];
		HeapTuple	tuple = NULL;
		Datum		result;
		char	   *priorityVal = NULL;

		const BackoffBackendSharedEntry *se = NULL;

		se = getBackoffEntryRO(context->currentIndex);

		Assert(se);

		if (!isValid(&se->statementId))
		{
			context->currentIndex++;
			continue;
		}

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = Int32GetDatum((int32) se->statementId.sessionId);
		values[1] = Int32GetDatum((int32) se->statementId.commandCount);

		priorityVal = BackoffPriorityIntToValue(se->weight);

		Assert(priorityVal);

		values[2] = CStringGetTextDatum(priorityVal);
		Assert(se->weight > 0);
		values[3] = Int32GetDatum((int32) se->weight);
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		Assert(tuple);
		result = HeapTupleGetDatum(tuple);

		context->currentIndex++;

		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

/**
 * What is the weight assigned to superuser issued queries?
 */
int
BackoffSuperuserStatementWeight()
{
	int			wt = -1;

	Assert(superuser());
	wt = BackoffPriorityValueToInt("MAX");
	Assert(wt > 0);
	return wt;
}

/**
 * Integer value for default weight.
 */
int
BackoffDefaultWeight()
{
	int			wt = BackoffPriorityValueToInt(gp_resqueue_priority_default_value);

	Assert(wt > 0);
	return wt;
}

/**
 * Get weight associated with queue. See queue.c.
 * TODO: tidy up interface.
 */
int
ResourceQueueGetPriorityWeight(Oid queueId)
{
	List	   *capabilitiesList = NULL;
	List	   *entry = NULL;
	ListCell   *le = NULL;
	int			weight = BackoffDefaultWeight();

	if (queueId == InvalidOid)
		return weight;

	capabilitiesList = GetResqueueCapabilityEntry(queueId);		/* This is a list of
																 * lists */

	if (!capabilitiesList)
		return weight;

	foreach(le, capabilitiesList)
	{
		Value	   *key = NULL;

		entry = (List *) lfirst(le);
		Assert(entry);
		key = (Value *) linitial(entry);
		Assert(key->type == T_Integer); /* This is resource type id */
		if (intVal(key) == PG_RESRCTYPE_PRIORITY)
		{
			Value	   *val = lsecond(entry);

			Assert(val->type == T_String);
			weight = BackoffPriorityValueToInt(strVal(val));
		}
	}
	list_free(capabilitiesList);
	return weight;
}

typedef struct PriorityMapping
{
	const char *priorityVal;
	int			weight;
} PriorityMapping;

const struct PriorityMapping priority_map[] = {
	{"MAX", 1000000},
	{"HIGH", 1000},
	{"MEDIUM", 500},
	{"LOW", 200},
	{"MIN", 100},
	/* End of list marker */
	{NULL, 0}
};


/**
 * Resource queues are associated with priority values which are stored
 * as text. This method maps them to double values that will be used for
 * cpu target usage computations by the sweeper. Keep this method in sync
 * with its dual BackoffPriorityIntToValue().
 */
static int
BackoffPriorityValueToInt(const char *priorityVal)
{
	const PriorityMapping *p = priority_map;

	Assert(p);
	while (p->priorityVal != NULL && (pg_strcasecmp(priorityVal, p->priorityVal) != 0))
	{
		p++;
		Assert((char *) p < (const char *) priority_map + sizeof(priority_map));
	}

	if (p->priorityVal == NULL)
	{
		/* No match found, throw an error */
		elog(ERROR, "Invalid priority value.");
	}

	Assert(p->weight > 0);

	return p->weight;
}

/**
 * Dual of the method BackoffPriorityValueToInt(). Given a weight, this
 * method maps it to a text value corresponding to this weight. Caller is
 * responsible for deallocating the return pointer.
 */
static char *
BackoffPriorityIntToValue(int weight)
{
	const PriorityMapping *p = priority_map;

	Assert(p);

	while (p->priorityVal != NULL && (p->weight != weight))
	{
		p = p + 1;
		Assert((char *) p < (const char *) priority_map + sizeof(priority_map));
	}

	if (p->priorityVal != NULL)
	{
		Assert(p->weight == weight);
		return pstrdup(p->priorityVal);
	}

	return pstrdup("NON-STANDARD");
}

/*
 * Helper method that verifies setting of default priority guc.
 */
const char *
gpvars_assign_gp_resqueue_priority_default_value(const char *newval,
												 bool doit,
									GucSource source __attribute__((unused)))
{
	if (doit)
	{
		int			wt;

		wt = BackoffPriorityValueToInt(newval); /* This will throw an error if
												 * bad value is specified */
		Assert(wt > 0);
	}

	return newval;
}
