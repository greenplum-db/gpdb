/*-------------------------------------------------------------------------
 *
 * ic_proxy_router.c
 *
 *    Interconnect Proxy Router
 *
 * A router routes a packet to the correct target, a client or a peer.
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */


#include "ic_proxy.h"
#include "ic_proxy_server.h"
#include "ic_proxy_router.h"
#include "ic_proxy_packet.h"
#include "ic_proxy_pkt_cache.h"
#include "ic_proxy_server.h"


typedef struct ICProxyWriteReq ICProxyWriteReq;
typedef struct ICProxyLoopback ICProxyLoopback;


/*
 * A router write request.
 *
 * It is similar to the libuv write request, however the router will take care
 * of the common part, such as the freeing of the packet and the request, so
 * the caller can focus on the real business.
 */
struct ICProxyWriteReq
{
	uv_write_t	req;			/* the libuv write request */

	ic_proxy_sent_cb callback;	/* the callback */
	void	   *opaque;			/* the callback data */
};

/*
 * The loopback packet queue.
 *
 * Loopback packets can not be routed immediately, refer to ICProxyDelay for
 * details.  They are first put in a queue, and are actually routed in a libuv
 * check callback, it is triggered after all the current I/O events are
 * handled, so there will be no misordered packets, and no reentrance to some
 * critical functions.
 */
struct ICProxyLoopback
{
	uv_check_t	check;					/* the libuv check handle */

	List	   *queue;					/* List<ICProxyWriteReq *> */
};


static ICProxyLoopback ic_proxy_router_loopback;


/*
 * The loopback check is triggered.
 */
static void
ic_proxy_router_loopback_on_check(uv_check_t *handle)
{
	List	   *queue;
	ListCell   *cell;

	/*
	 * Stop the check callback, all the queued loopback packets will be routed
	 * in this round.  This must happen before routing the packets, so if new
	 * loopback packets are queued, the check callback can be re-turned on,
	 * those packets will be handled next round.
	 *
	 * TODO: the new loopback packets can be handled in this round, too.
	 * Queueing them to next round means it will not be triggered until some
	 * I/O events happen.  In current logic there is the per second timer, so
	 * in worst case the new loopback packets are delayed for 1 second.  If the
	 * timer is paused in the future, as an optimization, then in worst case
	 * the new packets may not get a chance to be routed.  The only concern on
	 * handling them now is that if a infinite ping-pong happens, this function
	 * would never return to the mainloop.  It's unlikely to happen, though,
	 * and we could prevent that by adding a round count limit.
	 */
	uv_check_stop(&ic_proxy_router_loopback.check);

	/*
	 * We must detach the queue before handling it, in case some new packets
	 * are queued during the process
	 */
	queue = ic_proxy_router_loopback.queue;
	ic_proxy_router_loopback.queue = NIL;

	foreach(cell, queue)
	{
		ICProxyDelay *delay = lfirst(cell);
		ICProxyClient *client;
		ICProxyKey	key;

		/* Loopback packets are always to a loopback client */
		ic_proxy_key_from_p2c_pkt(&key, delay->pkt);
		client = ic_proxy_client_blessed_lookup(handle->loop, &key);

		elogif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG5,
			   "ic-proxy: router: looped back %s to %s",
					 ic_proxy_pkt_to_str(delay->pkt),
					 ic_proxy_client_get_name(client));

		ic_proxy_client_on_p2c_data(client, delay->pkt,
									delay->callback, delay->opaque);

		/* do not forget to call the callback */
		if (delay->callback)
			delay->callback(delay->opaque, delay->pkt, 0);

		/* and do not forget to free the memory */
		ic_proxy_free(delay);
	}

	list_free(queue);
}

/*
 * Push a loopback packet to the queue.
 */
static void
ic_proxy_router_loopback_push(ICProxyPkt *pkt,
							  ic_proxy_sent_cb callback, void *opaque)
{
	ICProxyDelay *delay;

	elogif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG5,
		   "ic-proxy: router: looping back %s",
				 ic_proxy_pkt_to_str(pkt));

	/*
	 * Enable the libuv check callback if not yet.
	 */
	if (ic_proxy_router_loopback.queue == NIL)
		uv_check_start(&ic_proxy_router_loopback.check,
					   ic_proxy_router_loopback_on_check);

	/*
	 * Loopback packets are always to a loopback client, so it's safe to pass
	 * NULL as the peer.
	 */
	delay = ic_proxy_peer_build_delay(NULL, pkt, callback, opaque);
	ic_proxy_router_loopback.queue = lappend(ic_proxy_router_loopback.queue, delay);
}

/*
 * Initialize the router.
 */
void
ic_proxy_router_init(uv_loop_t *loop)
{
	uv_check_init(loop, &ic_proxy_router_loopback.check);
	ic_proxy_router_loopback.queue = NIL;
}

/*
 * Cleanup the router.
 */
void
ic_proxy_router_uninit(void)
{
	List	   *queue;
	ListCell   *cell;

	queue = ic_proxy_router_loopback.queue;
	ic_proxy_router_loopback.queue = NIL;

	uv_check_stop(&ic_proxy_router_loopback.check);

	foreach(cell, queue)
	{
		ICProxyDelay *delay = lfirst(cell);

		/*
		 * TODO: this function is only called on exiting, so it's better to
		 * drop the callbacks silently, right?
		 */
#if 0
		if (delay->callback)
			delay->callback(delay->opaque, pkt, UV_ECANCELED);
#endif

		ic_proxy_pkt_cache_free(delay->pkt);
		ic_proxy_free(delay);
	}

	list_free(queue);
}

/*
 * Route a packet.
 */
void
ic_proxy_router_route(uv_loop_t *loop, ICProxyPkt *pkt,
					  ic_proxy_sent_cb callback, void *opaque)
{
	Assert(ic_proxy_pkt_is_valid(pkt));
	if (pkt->dstDbid == pkt->srcDbid)
	{
		/*
		 * For a loopback target, we do not need to send the packet via a peer,
		 * we could pass the packet to the target client via
		 * ic_proxy_client_on_p2c_data(), however that function is not
		 * reentrantable, which happens on PAUSE & RESUME messages, so we must
		 * schedule it in a libuv check callback.
		 *
		 * TODO: when callback is NULL, we could pass the packet immediately.
		 */
		ic_proxy_router_loopback_push(pkt, callback, opaque);
	}
	else if (pkt->dstDbid == GpIdentity.dbid)
	{
		ICProxyClient *client;
		ICProxyKey	key;

		ic_proxy_key_from_p2c_pkt(&key, pkt);
		client = ic_proxy_client_blessed_lookup(loop, &key);

		elogif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG5,
			   "ic-proxy: router: routing %s to %s",
					 ic_proxy_pkt_to_str(pkt),
					 ic_proxy_client_get_name(client));

		ic_proxy_client_on_p2c_data(client, pkt, callback, opaque);
	}
	else
	{
		ICProxyPeer *peer;

		peer = ic_proxy_peer_blessed_lookup(loop,
											pkt->dstContentId, pkt->dstDbid);

		elogif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG5,
			   "ic-proxy: router: routing %s to %s",
					 ic_proxy_pkt_to_str(pkt), peer->name);

		ic_proxy_peer_route_data(peer, pkt, callback, opaque);
	}
}

/*
 * The packet is written.
 */
static void
ic_proxy_router_on_write(uv_write_t *req, int status)
{
	ICProxyWriteReq *wreq = (ICProxyWriteReq *) req;
	ICProxyPkt *pkt = req->data;

	Assert(ic_proxy_pkt_is_valid(pkt));

	if (status < 0)
	{
		elogif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG5,
			   "ic-proxy: router: fail to send %s: %s",
					 ic_proxy_pkt_to_str(pkt), uv_strerror(status));
	}
	else
	{
		elogif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG5,
			   "ic-proxy: router: sent %s",
					 ic_proxy_pkt_to_str(pkt));
	}

	if (wreq->callback)
		wreq->callback(wreq->opaque, pkt, status);

	ic_proxy_pkt_cache_free(pkt);
	ic_proxy_free(req);
}

/*
 * Write a packet to a libuv stream.
 *
 * This is a simple wrapper for the uv_write() function.  The boring parts,
 * like buffer & request management, are handled by this wrapper, so the caller
 * can focus on the real business.
 *
 * It can write the packet at a specific offset, this is useful when writing
 * data from the client to the backend, the backend wants headless data, so the
 * client can specify sizeof(ICProxyPkt) as the offset.
 *
 * - stream: the target stream, usually a peer or a client;
 * - pkt: the data to write, the ownership is taken;
 * - offset: the data offset to write from, usually 0 when writing to a peer,
 *   or sizeof(ICProxyPkt) when writing to a client;
 * - callback: the callback function;
 * - opaque: the callback data;
 */
void
ic_proxy_router_write(uv_stream_t *stream, ICProxyPkt *pkt, int32 offset,
					  ic_proxy_sent_cb callback, void *opaque)
{
	ICProxyWriteReq *wreq;
	uv_buf_t	wbuf;

	elogif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG5,
		   "ic-proxy: router: sending %s", ic_proxy_pkt_to_str(pkt));

	wreq = ic_proxy_new(ICProxyWriteReq);

	wreq->req.data = pkt;
	wreq->callback = callback;
	wreq->opaque = opaque;

	wbuf.base = ((char *) pkt) + offset;
	wbuf.len = pkt->len - offset;

	uv_write(&wreq->req, stream, &wbuf, 1, ic_proxy_router_on_write);
}
