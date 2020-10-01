/*-------------------------------------------------------------------------
 *
 * ic_proxy_pkt_cache.c
 *
 *    Interconnect Proxy Packet Cache
 *
 * Libuv needs us to allocate the packet buffer, and it does not reuse the
 * buffer, so it is expansive to repeatedly allocating and freeing the packets.
 *
 * To make it more efficient we save all the freed packets in a free list, and
 * reuse them later.
 *
 * All the allocated packets are of the same size, the max possible packet
 * size, discarding the size requested by libuv, so the packet buffer can be
 * safely reused later.
 *
 * TODO:
 * - many libuv requests, such as uv_write(), needs us to allocate the request
 *   buffer, they are not reused, too, we could consider saving them in a
 *   free list similarly, or even share the same free list with packets;
 * - we need to limit the size of the free list, currently packets are never
 *   freed;
 *
 *
 * Copyright (c) 2020-Present Pivotal Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#define IC_PROXY_LOG_LEVEL WARNING
#include "ic_proxy.h"
#include "ic_proxy_pkt_cache.h"

#include <uv.h>

typedef struct ICProxyPktCache ICProxyPktCache;

/*
 * A simple free list.
 */
struct ICProxyPktCache
{
	ICProxyPktCache *next;
};

static struct 
{
	ICProxyPktCache *freelist;	/* the free list */
	uint32		pkt_size;		/* the packet size for all the packets */
	uint32		n_free;			/* count of packets in the free list */
	uint32		n_total;		/* count of all the allocated packets */
} ic_proxy_pkt_cache;

/*
 * Initialize the packet cache.
 */
void
ic_proxy_pkt_cache_init(uint32 pkt_size)
{
	ic_proxy_pkt_cache.freelist = NULL;
	ic_proxy_pkt_cache.pkt_size = pkt_size;
	ic_proxy_pkt_cache.n_free = 0;
	ic_proxy_pkt_cache.n_total = 0;
}

/*
 * Cleanup the packet cache.
 */
void
ic_proxy_pkt_cache_uninit(void)
{
	while (ic_proxy_pkt_cache.freelist)
	{
		ICProxyPktCache *cpkt = ic_proxy_pkt_cache.freelist;

		ic_proxy_pkt_cache.freelist = cpkt->next;
		ic_proxy_free(cpkt);
	}
}

/*
 * Allocate a packet from the cache.
 *
 * If the free list is empty a new packet is allocated and returned, otherwise
 * one is detached from the free list and returned directly.
 *
 * If pkt_size is not NULL it is set with the actual packet buffer size.
 *
 * Return the packet buffer.
 */
void *
ic_proxy_pkt_cache_alloc(size_t *pkt_size)
{
	ICProxyPktCache *cpkt;

	if (ic_proxy_pkt_cache.freelist)
	{
		cpkt = ic_proxy_pkt_cache.freelist;

		ic_proxy_pkt_cache.freelist = cpkt->next;
		ic_proxy_pkt_cache.n_free--;
	}
	else
	{
		cpkt = ic_proxy_alloc(ic_proxy_pkt_cache.pkt_size);
		ic_proxy_pkt_cache.n_total++;
	}

	if (pkt_size)
		*pkt_size = ic_proxy_pkt_cache.pkt_size;

#if 0
	/* for debug purpose */
	memset(cpkt, 0, ic_proxy_pkt_cache.pkt_size);
#endif

	ic_proxy_log(LOG, "pkt-cache: allocated, %d free, %d total",
				 ic_proxy_pkt_cache.n_free, ic_proxy_pkt_cache.n_total);
	return cpkt;
}

/*
 * Allocate a packet from the cache, as a libuv callback.
 *
 * This is a wrapper of ic_proxy_pkt_cache_alloc(), this function can be used
 * as the libuv uv_alloc_cb callback.
 */
void
ic_proxy_pkt_cache_alloc_buffer(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
	buf->base = ic_proxy_pkt_cache_alloc(&buf->len);
}

/*
 * Return a packet to the free list.
 */
void
ic_proxy_pkt_cache_free(void *pkt)
{
	ICProxyPktCache *cpkt = pkt;

#if 0
	/* for debug purpose */
	memset(cpkt, 0, ic_proxy_pkt_cache.pkt_size);

	for (ICProxyPktCache *iter = ic_proxy_pkt_cache.freelist;
		 iter; iter = iter->next)
		Assert(iter != cpkt);
#endif

	cpkt->next = ic_proxy_pkt_cache.freelist;
	ic_proxy_pkt_cache.freelist = cpkt;
	ic_proxy_pkt_cache.n_free++;

	ic_proxy_log(LOG, "pkt-cache: recycled, %d free, %d total",
				 ic_proxy_pkt_cache.n_free, ic_proxy_pkt_cache.n_total);
}
