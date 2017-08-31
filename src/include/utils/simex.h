/*-------------------------------------------------------------------------
 *
 * simex.h
 *		Interface for simulating Exceptional Situations (ES).
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/simex.h
 *
 *-------------------------------------------------------------------------
 */

/*
 * SimExESClass - SimExESSubClass : enums for ES class hierarchy
 *
 * Each ES Class represents a class of ES that can appear at the same
 * ES checkpoint. For example, a write() call (class) can lead to a torn
 * page error (subclass) or a write failure (subclass).
 *
 * Instructions for introducing a new ES class:
 * 1. Add one entry to SimExESClass, before SimExESClass_Max.
 * 2. Add one or more corresponding entries in SimExESSubClass,
 *    before SimExESSubClass_Max.
 * 3. For each new entry in SimExESSubClass, add one entry in table
 *    SimExESClassAssociateTable (simex.c) to associate the subclass
 *    with its class.
 */

#ifndef SIMEX_H_
#define SIMEX_H_

#include "utils/syncbitvector.h"
#include "pg_config.h"

#ifdef USE_TEST_UTILS

/* Enum for Exceptional Situation Class */
typedef enum SimExESClass
{
	SimExESClass_Base = 0,
	SimExESClass_OOM = SimExESClass_Base,	// out-of-memory
	SimExESClass_Cancel,					// cancel
	SimExESClass_CacheInvalidation,         // cache invaldation messages
	SimExESClass_SysError,				    // system call failure
	SimExESClass_ProcKill,					// kill process with SIGKILL
	SimExESClass_Max						// must always be last
} SimExESClass;

#define SimExESClass_IsValid(class) \
	(class >= SimExESClass_Base && class < SimExESClass_Max)

static inline void SimExESClass_CheckValid(int simexclass)
{
	if (!SimExESClass_IsValid(simexclass))
	{
		ereport(ERROR,
	 		    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
	 		     errmsg("SimEx ES class %d is invalid", simexclass)));
	}
}

/* Enum for Exceptional Situation Subclass */
typedef enum SimExESSubClass
{
	SimExESSubClass_OK = 0,
	SimExESSubClass_OOM_ReturnNull,
	SimExESSubClass_Cancel_QueryCancel,
	SimExESSubClass_Cancel_ProcDie,
	SimExESSubClass_CacheInvalidation,
	SimExESSubClass_SysError_Net,
	SimExESSubClass_ProcKill_BgWriter,
	SimExESSubClass_Max						// must always be last
} SimExESSubClass;

#define SimExESSubClass_IsValid(subclass) \
	(subclass >= SimExESSubClass_OK && subclass < SimExESSubClass_Max)

static inline void SimExESSubClass_CheckValid(int subclass)
{
	if (!SimExESSubClass_IsValid(subclass))
	{
		ereport(ERROR,
	 		    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
	 		     errmsg("SimEx ES subclass %d is invalid", subclass)));
	}
}

/* interface */
extern void simex_init(void);
extern void simex_set_sync_bitvector_container(SyncBitvectorContainer container);
extern int32 simex_get_subclass_count(void);
extern SimExESSubClass simex_check(const char *file, int32 line);

/*
 * Macro for ES injection.
 *
 * Checks if there is an ES subclass that has not been injected at the current stack.
 * If all ES subclasses have been examined, it returns SimExESSubClass_OK (no injection).
 * Else, it logs the prominent ES injection and returns the SimExESSubClass to be injected.
 *
 * Note that SimEx does not inject any ES; the caller is responsible for handling the
 * returned value and injecting the appropriate ES.
 */
#define SimEx_CheckInject() (simex_check(__FILE__, __LINE__))

/* GUCs */
extern bool gp_simex_init;            // start-up GUC, controls SimEx initialization
extern int  gp_simex_class;           // start-up GUC, sets ES class to be simulated
extern double  gp_simex_rand;            // start-up GUC, controls randomized ES injection (not implemented)
extern bool gp_simex_run;             // session GUC, starts/stops ES injection

#endif /* USE_TEST_UTILS */


/*
 * Prototypes
 */
struct pollfd;
struct sockaddr;

/*
 * System call wrappers with integrated ES injection
 */

/* networking */
extern int gp_socket(int socket_family, int socket_type, int protocol);
extern int gp_connect(int socket, const struct sockaddr *address, socklen_t address_len);
extern int gp_poll(struct pollfd *fds, uint32 nfds, int timeout);
extern int gp_send(int socket, const void *buffer, size_t length, int flags);
extern int gp_recv(int socket, void *buffer, size_t length, int flags);


#endif /* SIMEX_H_ */
