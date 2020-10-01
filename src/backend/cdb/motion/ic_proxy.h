/*-------------------------------------------------------------------------
 *
 * ic_proxy.h
 *
 *
 * Copyright (c) 2020-Present Pivotal Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_H
#define IC_PROXY_H

#include "postgres.h"

#include "cdb/cdbinterconnect.h"
#include "cdb/cdbvars.h"
#include "nodes/pg_list.h"
#include "postmaster/postmaster.h"

#define IC_PROXY_BACKLOG 1024
#define IC_PROXY_INVALID_CONTENT ((uint16) -2)
#define IC_PROXY_INVALID_DBID ((int16) 0)
#define IC_PROXY_TRESHOLD_PAUSE 4
#define IC_PROXY_TRESHOLD_RESUME 2

#ifndef IC_PROXY_LOG_LEVEL
#define IC_PROXY_LOG_LEVEL WARNING
#endif

#define ic_proxy_alloc(size) palloc(size)
#define ic_proxy_free(ptr) pfree(ptr)
#define ic_proxy_new(type) ((type *) ic_proxy_alloc(sizeof(type)))

#define ic_proxy_log(elevel, msg...) do { \
	if (elevel >= IC_PROXY_LOG_LEVEL) \
	{ \
		elog(elevel, msg); \
	} \
} while (0)

/*
 * Build the domain socket path.
 *
 * Every proxy on the same host must use a different path, this is important to
 * let proxies from different segments or even different clusters to coexist.
 *
 * This is ensured by including the postmaster port & pid in the path.
 */
static inline void
ic_proxy_build_server_sock_path(char *buf, size_t bufsize)
{
	snprintf(buf, bufsize, "/tmp/.s.PGSQL.ic_proxy.%d.%d",
			 PostPortNumber, PostmasterPid);
}

#endif   /* IC_PROXY_H */
