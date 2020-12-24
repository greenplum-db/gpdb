/*-------------------------------------------------------------------------
 *
 * ic_proxy_backend.h
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_BACKEND_H
#define IC_PROXY_BACKEND_H

#include "postgres.h"

#include "cdb/cdbinterconnect.h"

#include <uv.h>

/*
 * ICProxyBackendContext represents the common state of ic_proxy backend for each
 * ChunkTransportState.
 */
typedef struct ICProxyBackendContext
{
	uv_loop_t	loop;

	/* timer for interrupt */
	uv_timer_t	interruptTimer;
	uv_timer_t	cancelFromQDTimer;

	/* timer and queue for connect and reconnect */
	uv_timer_t	connectTimer;
	List	   *connectQueue;

	/* Pointer to ChunkTransportState * */
	ChunkTransportState   *transportState;
} ICProxyBackendContext;

extern void ic_proxy_backend_connect(ICProxyBackendContext *context,
									 ChunkTransportStateEntry *pEntry,
									 MotionConn *conn, bool isSender);

extern void ic_proxy_backend_init_context(ChunkTransportState *state);
extern void ic_proxy_backend_close_context(ChunkTransportState *state);
extern void ic_proxy_backend_run_loop(ICProxyBackendContext *context);

#endif   /* IC_PROXY_BACKEND_H */
