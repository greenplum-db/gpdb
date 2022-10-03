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
/* pause the sender when the unack packet increase to this threshold */
#define IC_PROXY_TRESHOLD_UNACK_PACKET_PAUSE 100
/* resume the sender when the unack packet reduce to this threshold */
#define IC_PROXY_TRESHOLD_UNACK_PACKET_RESUME 30
/* send a ack message after a batch of packets*/
#define IC_PROXY_ACK_INTERVAL 10

#define ic_proxy_alloc(size) palloc(size)
#define ic_proxy_free(ptr) pfree(ptr)
#define ic_proxy_new(type) ((type *) ic_proxy_alloc(sizeof(type)))

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
