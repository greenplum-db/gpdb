/*-------------------------------------------------------------------------
 *
 * ic_proxy_router.h
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_ROUTER_H
#define IC_PROXY_ROUTER_H


#include "postgres.h"

#include <uv.h>


typedef void (* ic_proxy_sent_cb) (void *opaque,
								   const ICProxyPkt *pkt, int status);


extern void ic_proxy_router_init(uv_loop_t *loop);
extern void ic_proxy_router_uninit(void);
extern void ic_proxy_router_route(uv_loop_t *loop, ICProxyPkt *pkt,
								  ic_proxy_sent_cb callback, void *opaque);
extern void ic_proxy_router_write(uv_stream_t *stream,
								  ICProxyPkt *pkt, int32 offset,
								  ic_proxy_sent_cb callback, void *opaque);


#endif   /* IC_PROXY_ROUTER_H */
