/*-------------------------------------------------------------------------
 *
 * ic_proxy_pkt_cache.h
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_PKT_CACHE_H
#define IC_PROXY_PKT_CACHE_H

#include <uv.h>

extern void ic_proxy_pkt_cache_init(uint32 pkt_size);
extern void ic_proxy_pkt_cache_uninit(void);
extern void *ic_proxy_pkt_cache_alloc(size_t *pkt_size);
extern void ic_proxy_pkt_cache_alloc_buffer(uv_handle_t *handle,
											size_t size, uv_buf_t *buf);
extern void ic_proxy_pkt_cache_free(void *pkt);

#endif   /* IC_PROXY_PKT_CACHE_H */
