/*-------------------------------------------------------------------------
 *
 * ic_proxy_server_peer.c
 *
 *    Interconnect Proxy Peer
 *
 * A peer lives in the proxy bgworker and connects to a proxy on an other
 * segment.  When there are N segments, including the master, a proxy bgworker
 * needs to connect to all the other (N - 1) segments, the same amount of peers
 * are needed, too.
 *
 * A peer is identified with the dbid, so two different peers are used to
 * connect to a remote segment's primary and mirror.  The proxy bgworker is not
 * launched on a mirror until it is promoted, so most of time there is only the
 * peer to the segment's primary, but there is a chance for the peer to the
 * mirror to live together with the primary one, this happends during the
 * mirror promotion.
 *
 * There are only one proxy connection between two proxies, a rule is put here
 * that the proxy on segment X connects to the one on segment Y iff X > Y, not
 * the reverse.  This rule is true even if X or Y crashes and relaunches the
 * proxy bgworker.
 *
 * Peers always communicate to each other via ICProxyPkt, a connection must
 * begin with the hand shaking messages.  A hand shaking is needed for a pair
 * of peers to know the information of each other, such as the dbids.
 *
 * Clients can send packets before the peer hand shaking is finished, in such a
 * case a placeholder is registered to hold the early outgoing packets.  Once
 * the peer finishes the hand shaking it replaces the placeholder and handles
 * these early packets in the arriving order.
 *
 * Incoming packets, the one received from a remote peer, is never cached in
 * the peer, they are routed to the target clients, or their placeholders,
 * immediately.
 *
 *
 * Copyright (c) 2020-Present Pivotal Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "ic_proxy_server.h"
#include "ic_proxy_pkt_cache.h"

#include <uv.h>


/*
 * The peer register table, the peer with dbid is stored in [dbid].
 *
 * TODO: not using a fixed length array.
 */
static ICProxyPeer *ic_proxy_peers[65536];


static void ic_proxy_peer_shutdown(ICProxyPeer *peer);
static void ic_proxy_peer_handle_out_cache(ICProxyPeer *peer);
static void ic_proxy_peer_on_data_pkt(void *opaque,
									  const void *data, uint16 size);
static void ic_proxy_peer_send_message(ICProxyPeer *peer,
									   ICProxyMessageType mtype,
									   const ICProxyKey *key,
									   ic_proxy_sent_cb callback);


/*
 * Build a delayed packet.
 *
 * We'll take the packet's ownership.
 */
ICProxyDelay *
ic_proxy_peer_build_delay(ICProxyPeer *peer, ICProxyPkt *pkt,
						  ic_proxy_sent_cb callback, void *opaque)
{
	ICProxyDelay *delay;

	delay = ic_proxy_new(ICProxyDelay);
	delay->content = peer ? peer->content : IC_PROXY_INVALID_CONTENT;
	delay->dbid = peer ? peer->dbid : IC_PROXY_INVALID_DBID;
	delay->pkt = pkt;
	delay->callback = callback;
	delay->opaque = opaque;

	return delay;
}

/*
 * Initialize the peer register table.
 */
void
ic_proxy_peer_table_init(void)
{
	memset(ic_proxy_peers, 0, sizeof(ic_proxy_peers));
}

void
ic_proxy_peer_table_uninit(void)
{
	/*
	 * nothing to do for the peers table:
	 * - no need to clear the peers table, we will do that in init();
	 * - no need to free the peers, they should already freed themselves;
	 */
}

/*
 * Update the peer name from the state bits.
 */
static void
ic_proxy_peer_update_name(ICProxyPeer *peer)
{
	struct sockaddr_storage peeraddr;
	int			addrlen = sizeof(peeraddr);
	char		sockname[HOST_NAME_MAX] = "";
	char		peername[HOST_NAME_MAX] = "";
	int			sockport = 0;
	int			peerport = 0;

	/*
	 * Show the tcp level connection information in the name, they are not very
	 * useful, though.
	 */
	uv_tcp_getsockname(&peer->tcp, (struct sockaddr *) &peeraddr, &addrlen);
	if (peeraddr.ss_family == AF_INET)
	{
		struct sockaddr_in *peeraddr4 = (struct sockaddr_in *) &peeraddr;

		uv_ip4_name(peeraddr4, sockname, sizeof(sockname));
		sockport = ntohs(peeraddr4->sin_port);
	}
	else if (peeraddr.ss_family == AF_INET6)
	{
		struct sockaddr_in6 *peeraddr6 = (struct sockaddr_in6 *) &peeraddr;

		uv_ip6_name(peeraddr6, sockname, sizeof(sockname));
		sockport =  ntohs(peeraddr6->sin6_port);
	}

	uv_tcp_getpeername(&peer->tcp, (struct sockaddr *) &peeraddr, &addrlen);
	if (peeraddr.ss_family == AF_INET)
	{
		struct sockaddr_in *peeraddr4 = (struct sockaddr_in *) &peeraddr;

		uv_ip4_name(peeraddr4, peername, sizeof(peername));
		peerport = ntohs(peeraddr4->sin_port);
	}
	else if (peeraddr.ss_family == AF_INET6)
	{
		struct sockaddr_in6 *peeraddr6 = (struct sockaddr_in6 *) &peeraddr;

		uv_ip6_name(peeraddr6, peername, sizeof(peername));
		peerport =  ntohs(peeraddr6->sin6_port);
	}

	snprintf(peer->name, sizeof(peer->name), "peer%s[seg%hd,dbid%hu %s:%d->%s:%d]",
			 (peer->state & IC_PROXY_PEER_STATE_LEGACY) ? ".legacy" : "",
			 peer->content, peer->dbid, sockname, sockport, peername, peerport);
}

/*
 * Unregister a peer.
 */
static void
ic_proxy_peer_unregister(ICProxyPeer *peer)
{
	Assert(peer->dbid > 0);

	if (ic_proxy_peers[peer->dbid] == peer)
	{
		/* keep the peer as a placeholder */

		ic_proxy_log(LOG, "%s: unregistered", peer->name);

		/* reset the state */
		peer->state = 0;
		ic_proxy_peer_update_name(peer);
	}
	else if (ic_proxy_peers[peer->dbid])
	{
		/*
		 * if there is already a placeholder, transfer my cached packets to it
		 */
		ICProxyPeer *placeholder = ic_proxy_peers[peer->dbid];

		placeholder->reqs = list_concat(placeholder->reqs, peer->reqs);
		peer->reqs = NIL;

		/* then free the peer */
		ic_proxy_peer_free(peer);
	}
}

/*
 * Register a peer.
 */
static void
ic_proxy_peer_register(ICProxyPeer *peer)
{
	ICProxyPeer *placeholder = ic_proxy_peers[peer->dbid];

	Assert(peer->dbid > 0);

	if (placeholder)
	{
		/*
		 * FIXME: is it possible for a new peer to come before the legacy one
		 * is ready for message?
		 */

		if (placeholder->state & IC_PROXY_PEER_STATE_READY_FOR_MESSAGE)
		{
			/*
			 * This is not actually a placeholder, but a legacy peer, this
			 * happens due to network problem, etc..
			 */
			ic_proxy_log(WARNING, "%s(state=0x%08x): found a legacy peer %s(state=0x%08x)",
						 peer->name, peer->state,
						 placeholder->name, placeholder->state);

			placeholder->state |= IC_PROXY_PEER_STATE_LEGACY;
			ic_proxy_peer_update_name(placeholder);

			ic_proxy_peer_shutdown(placeholder);
		}
		else
		{
			/* This is an actual placeholder */
			ic_proxy_log(LOG, "%s(state=0x%08x): found my placeholder %s(state=0x%08x)",
						 peer->name, peer->state,
						 placeholder->name, placeholder->state);

			if (placeholder->ibuf.len > 0)
				ic_proxy_log(WARNING, "%s(state=0x%08x): my placeholder %s(state=0x%08x) has %d bytes in ibuf",
							 peer->name, peer->state,
							 placeholder->name, placeholder->state,
							 placeholder->ibuf.len);

			/* TODO: verify that it's really a placeholder */

			/* transfer the cached pkts */
			peer->reqs = list_concat(peer->reqs, placeholder->reqs);
			placeholder->reqs = NIL;

			/* finally free the placeholder */
			ic_proxy_peer_free(placeholder);
		}
	}

	ic_proxy_peers[peer->dbid] = peer;

	ic_proxy_log(LOG, "%s: registered", peer->name);
}

/*
 * Lookup a peer with peerid.
 *
 * We require to pass both content and dbid as arguments, but only dbid is
 * used.
 */
ICProxyPeer *
ic_proxy_peer_lookup(int16 content, uint16 dbid)
{
	Assert(dbid > 0);

	return ic_proxy_peers[dbid];
}

/*
 * Lookup a peer with peerid, create a placeholder if not found.
 */
ICProxyPeer *
ic_proxy_peer_blessed_lookup(uv_loop_t *loop, int16 content, uint16 dbid)
{
	Assert(dbid > 0);

	if (!ic_proxy_peers[dbid])
	{
		ICProxyPeer *peer = ic_proxy_peer_new(loop, content, dbid);

		/* register as a placeholder */
		ic_proxy_peer_register(peer);
	}

	return ic_proxy_peers[dbid];
}

/*
 * Received a complete DATA or MESSAGE packet from a remote peer.
 */
static void
ic_proxy_peer_on_data_pkt(void *opaque, const void *data, uint16 size)
{
	const ICProxyPkt *pkt = data;
	ICProxyPeer *peer = opaque;

	ic_proxy_log(LOG, "%s: received %s", peer->name, ic_proxy_pkt_to_str(pkt));

	if (!(peer->state & IC_PROXY_PEER_STATE_READY_FOR_DATA))
	{
		ic_proxy_log(WARNING, "%s: not ready to receive DATA yet: %s",
					 peer->name, ic_proxy_pkt_to_str(pkt));
		return;
	}

	ic_proxy_router_route(peer->tcp.loop, ic_proxy_pkt_dup(pkt), NULL, NULL);
}

/*
 * Received bytes from a remote peer.
 */
static void
ic_proxy_peer_on_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	ICProxyPeer *peer = CONTAINER_OF((void *) stream, ICProxyPeer, tcp);

	if (unlikely(nread < 0))
	{
		if (nread != UV_EOF)
			ic_proxy_log(WARNING, "%s: fail to receive DATA: %s",
						 peer->name, uv_strerror(nread));
		else
			ic_proxy_log(LOG, "%s: received EOF while waiting for DATA",
						 peer->name);

		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		ic_proxy_peer_shutdown(peer);
		return;
	}
	else if (unlikely(nread == 0))
	{
		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		/* EAGAIN or EWOULDBLOCK, retry */
		return;
	}

	ic_proxy_ibuf_push(&peer->ibuf, buf->base, nread,
					   ic_proxy_peer_on_data_pkt, peer);
	ic_proxy_pkt_cache_free(buf->base);
}

/*
 * Create a peer.
 */
ICProxyPeer *
ic_proxy_peer_new(uv_loop_t *loop, int16 content, uint16 dbid)
{
	ICProxyPeer *peer;

	peer = ic_proxy_new(ICProxyPeer);
	peer->content = content;
	peer->dbid = dbid;
	peer->state = 0;
	peer->reqs = NIL;

	ic_proxy_ibuf_init_p2p(&peer->ibuf);

	uv_tcp_init(loop, &peer->tcp);
	uv_tcp_nodelay(&peer->tcp, true);

	ic_proxy_peer_update_name(peer);

	return peer;
}

/*
 * Free a peer.
 *
 * A peer should only be used if it is really unused.  Most of the time a
 * closed peer is converted to a placeholder, so it should not be freed.  Only
 * a replaced placeholder should be freed.
 */
void
ic_proxy_peer_free(ICProxyPeer *peer)
{
	ListCell   *cell;

	ic_proxy_log(LOG, "%s: freeing", peer->name);

	foreach(cell, peer->reqs)
	{
		ICProxyPkt *pkt = lfirst(cell);

		ic_proxy_log(WARNING, "%s: unhandled outgoing %s, dropping it",
					 peer->name, ic_proxy_pkt_to_str(pkt));

		ic_proxy_pkt_cache_free(pkt);
	}

	list_free(peer->reqs);

	ic_proxy_ibuf_uninit(&peer->ibuf);
	ic_proxy_free(peer);

	/*
	 * TODO: if a peer disconnected, should we also disconnect all the relative
	 * clients?  The concern is that some packets might already be lost.
	 *
	 * Anyway, future packets should not be cached inside the peer.
	 */
}

/*
 * The peer is closed.
 */
static void
ic_proxy_peer_on_close(uv_handle_t *handle)
{
	ICProxyPeer *peer = CONTAINER_OF((void *) handle, ICProxyPeer, tcp);

	ic_proxy_log(LOG, "%s: closed", peer->name);

	/* reset the state */
	peer->state = 0;

	/* it's unlikely that the ibuf is non-empty, but clear it for sure */
	ic_proxy_ibuf_clear(&peer->ibuf);

	ic_proxy_peer_unregister(peer);
}

/*
 * Close a peer.
 *
 * A peer could only be closed after its shutdown.
 */
static void
ic_proxy_peer_close(ICProxyPeer *peer)
{
	if (peer->state & IC_PROXY_PEER_STATE_CLOSING)
		return;

	ic_proxy_log(LOG, "%s: closing", peer->name);

	peer->state |= IC_PROXY_PEER_STATE_CLOSING;

	uv_close((uv_handle_t *) &peer->tcp, ic_proxy_peer_on_close);
}

/*
 * The peer is shutted down.
 */
static void
ic_proxy_peer_on_shutdown(uv_shutdown_t *req, int status)
{
	ICProxyPeer *peer = CONTAINER_OF((void *) req->handle, ICProxyPeer, tcp);

	ic_proxy_free(req);

	if (status < 0)
		ic_proxy_log(WARNING, "%s: fail to shutdown: %s",
					 peer->name, uv_strerror(status));

	ic_proxy_log(LOG, "%s: shutted down", peer->name);

	peer->state |= IC_PROXY_PEER_STATE_SHUTTED;

	ic_proxy_peer_close(peer);
}

/*
 * Shutdown a peer.
 */
static void
ic_proxy_peer_shutdown(ICProxyPeer *peer)
{
	uv_shutdown_t *req;

	if (peer->state & IC_PROXY_PEER_STATE_SHUTTING)
		return;

	ic_proxy_log(LOG, "%s: shutting down", peer->name);

	peer->state |= IC_PROXY_PEER_STATE_SHUTTING;

	/* disconnect all the clients */
	ic_proxy_client_table_shutdown_by_dbid(peer->dbid);

	req = ic_proxy_new(uv_shutdown_t);

	uv_shutdown(req, (uv_stream_t *) &peer->tcp, ic_proxy_peer_on_shutdown);
}

/*
 * Sent the HELLO ACK message.
 */
static void
ic_proxy_peer_on_sent_hello_ack(void *opaque, const ICProxyPkt *pkt, int status)
{
	ICProxyPeer *peer = opaque;

	if (status < 0)
	{
		ic_proxy_peer_shutdown(peer);
		return;
	}

	peer->state |= IC_PROXY_PEER_STATE_SENT_HELLO_ACK;

	ic_proxy_log(LOG, "%s: start receiving DATA", peer->name);

	/* it's unlikely that the ibuf is non-empty, but clear it for sure */
	ic_proxy_ibuf_clear(&peer->ibuf);

	/*
	 * If there are early coming packets, make sure to route them before
	 * receiving new data, we must ensure that packets are routed in the same
	 * order as they arrive.
	 */
	ic_proxy_peer_handle_out_cache(peer);

	/* now it's time to receive the normal data */
	uv_read_start((uv_stream_t *) &peer->tcp,
				  ic_proxy_pkt_cache_alloc_buffer, ic_proxy_peer_on_data);
}

/*
 * Received the complete HELLO message.
 */
static void
ic_proxy_peer_on_hello_pkt(void *opaque, const void *data, uint16 size)
{
	const ICProxyPkt *pkt = data;
	ICProxyPeer *peer = opaque;
	ICProxyKey	key;

	/* we only expect one hello message */
	uv_read_stop((uv_stream_t *) &peer->tcp);

	ic_proxy_key_from_p2c_pkt(&key, pkt);

	/* TODO: verify that old dbid and content are both set or invalid */
	peer->content = key.remoteContentId;
	peer->dbid = key.remoteDbid;

	ic_proxy_peer_update_name(peer);

	/*
	 * A peer could be registered as long as it knows the peer information from
	 * the HELLO message, the client packets will still be cached until the
	 * HELLO ACK is sent out.
	 */
	ic_proxy_peer_register(peer);

	ic_proxy_log(LOG, "%s: received %s, sending HELLO ACK",
				 peer->name, ic_proxy_pkt_to_str(pkt));

	/*
	 * below two state bits can be merged into one, but it is harmless to keep
	 * them as two.
	 */
	peer->state |= IC_PROXY_PEER_STATE_RECEIVED_HELLO;

	peer->state |= IC_PROXY_PEER_STATE_SENDING_HELLO_ACK;

	ic_proxy_key_reverse(&key);
	key.localPid = MyProcPid;

	ic_proxy_peer_send_message(peer, IC_PROXY_MESSAGE_PEER_HELLO_ACK, &key,
							   ic_proxy_peer_on_sent_hello_ack);
}

/*
 * Received some HELLO bytes.
 */
static void
ic_proxy_peer_on_hello_data(uv_stream_t *stream,
							ssize_t nread, const uv_buf_t *buf)
{
	ICProxyPeer *peer = CONTAINER_OF((void *) stream, ICProxyPeer, tcp);

	if (unlikely(nread < 0))
	{
		if (nread != UV_EOF)
			ic_proxy_log(WARNING, "%s: fail to receive HELLO: %s",
						 peer->name, uv_strerror(nread));
		else
			ic_proxy_log(LOG, "%s: received EOF while waiting for HELLO",
						 peer->name);

		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		ic_proxy_peer_shutdown(peer);
		return;
	}
	else if (unlikely(nread == 0))
	{
		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		/* EAGAIN or EWOULDBLOCK, retry */
		return;
	}

	ic_proxy_ibuf_push(&peer->ibuf, buf->base, nread,
					   ic_proxy_peer_on_hello_pkt, peer);
	ic_proxy_pkt_cache_free(buf->base);
}

/*
 * Start reading the HELLO message.
 */
void
ic_proxy_peer_read_hello(ICProxyPeer *peer)
{
	if (peer->state & IC_PROXY_PEER_STATE_RECEIVING_HELLO)
		return;

	ic_proxy_log(LOG, "%s: waiting for HELLO", peer->name);

	peer->state |= IC_PROXY_PEER_STATE_RECEIVING_HELLO;

	uv_read_start((uv_stream_t *) &peer->tcp,
				  ic_proxy_pkt_cache_alloc_buffer, ic_proxy_peer_on_hello_data);
}

/*
 * Received the complete HELLO ACK message.
 */
static void
ic_proxy_peer_on_hello_ack_pkt(void *opaque, const void *data, uint16 size)
{
	const ICProxyPkt *pkt = data;
	ICProxyPeer *peer = opaque;

	if (size < sizeof(*pkt) || size != pkt->len)
		ic_proxy_log(ERROR, "%s: received incomplete HELLO ACK: size = %d",
					 peer->name, size);

	if (peer->state & IC_PROXY_PEER_STATE_RECEIVED_HELLO_ACK)
	{
		/*
		 * A DATA packet is sent together with the HELLO, so the ibuf push the
		 * DATA here.  I still don't know how would this happen, but this does
		 * happen on the pipeline, so at least let it work.
		 *
		 * TODO: as we can't draw a clear line between handshake and data, it
		 * would be better to merge on_hello* and on_data into one.
		 */
		ic_proxy_log(WARNING, "%s: early DATA: %s",
					 peer->name, ic_proxy_pkt_to_str(pkt));

		ic_proxy_peer_on_data_pkt(opaque, data, size);
		return;
	}

	if (!ic_proxy_pkt_is(pkt, IC_PROXY_MESSAGE_PEER_HELLO_ACK))
		ic_proxy_log(ERROR, "%s: received invalid HELLO ACK: %s",
					 peer->name, ic_proxy_pkt_to_str(pkt));

	if (pkt->dstDbid != peer->dbid)
		ic_proxy_log(ERROR, "%s: received invalid HELLO ACK: %s",
					 peer->name, ic_proxy_pkt_to_str(pkt));

	/* we only expect one hello ack message */
	uv_read_stop((uv_stream_t *) &peer->tcp);

	ic_proxy_log(LOG, "%s: received %s", peer->name, ic_proxy_pkt_to_str(pkt));

	peer->state |= IC_PROXY_PEER_STATE_RECEIVED_HELLO_ACK;

	/* do not clear the ibuf, it could already contain incoming DATA */

	/*
	 * If there are early coming packets, make sure to route them before
	 * receiving new data, we must ensure that packets are routed in the same
	 * order as they arrive.
	 */
	ic_proxy_peer_handle_out_cache(peer);

	ic_proxy_log(LOG, "%s: start receiving DATA", peer->name);

	/* now it's time to receive the normal data */
	uv_read_start((uv_stream_t *) &peer->tcp,
				  ic_proxy_pkt_cache_alloc_buffer, ic_proxy_peer_on_data);
}

/*
 * Received HELLO ACK bytes.
 */
static void
ic_proxy_peer_on_hello_ack_data(uv_stream_t *stream,
								ssize_t nread, const uv_buf_t *buf)
{
	ICProxyPeer *peer = CONTAINER_OF((void *) stream, ICProxyPeer, tcp);

	if (unlikely(nread < 0))
	{
		if (nread != UV_EOF)
			ic_proxy_log(WARNING, "%s: fail to recv HELLO ACK: %s",
						 peer->name, uv_strerror(nread));
		else
			ic_proxy_log(LOG, "%s: received EOF while waiting for HELLO ACK",
						 peer->name);

		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		ic_proxy_peer_shutdown(peer);
		return;
	}
	else if (unlikely(nread == 0))
	{
		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		/* EAGAIN or EWOULDBLOCK, retry */
		return;
	}

	ic_proxy_ibuf_push(&peer->ibuf, buf->base, nread,
					   ic_proxy_peer_on_hello_ack_pkt, peer);
	ic_proxy_pkt_cache_free(buf->base);
}

/*
 * Sent the HELLO message.
 */
static void
ic_proxy_peer_on_sent_hello(void *opaque, const ICProxyPkt *pkt, int status)
{
	ICProxyPeer *peer = opaque;

	if (status < 0)
	{
		ic_proxy_peer_shutdown(peer);
		return;
	}

	ic_proxy_log(LOG, "%s: waiting for HELLO ACK", peer->name);

	peer->state |= IC_PROXY_PEER_STATE_SENT_HELLO;

	peer->state |= IC_PROXY_PEER_STATE_RECEIVING_HELLO_ACK;

	/* wait for hello ack */
	uv_read_start((uv_stream_t *) &peer->tcp,
				  ic_proxy_pkt_cache_alloc_buffer,
				  ic_proxy_peer_on_hello_ack_data);
}

/*
 * Connected to a peer.
 */
static void
ic_proxy_peer_on_connected(uv_connect_t *conn, int status)
{
	ICProxyPeer *peer = CONTAINER_OF((void *) conn->handle, ICProxyPeer, tcp);
	ICProxyKey	key;

	ic_proxy_free(conn);

	if (status < 0)
	{
		/* the peer might just not get ready yet, retry later */
		ic_proxy_log(LOG, "%s: fail to connect: %s",
					 peer->name, uv_strerror(status));
		ic_proxy_peer_close(peer);
		return;
	}

	ic_proxy_log(LOG, "%s: connected, sending HELLO", peer->name);

	peer->state |= IC_PROXY_PEER_STATE_CONNECTED;

	/* TODO: increase ic_proxy_peer_contents[peer->content] */

	/* hello packet must be the first one from a client */

	/*
	 * For a peer HELLO message, the only meaningful field is localDbid,
	 * but we also set the content and pid for debugging purpose.
	 */
	ic_proxy_key_init(&key,
					  0						/* sessionId */,
					  0						/* commandId */,
					  0						/* sendSliceIndex */,
					  0						/* recvSliceIndex */,
					  GpIdentity.segindex	/* localContentId */,
					  GpIdentity.dbid		/* localDbid */,
					  MyProcPid				/* localPid */,
					  peer->content			/* remoteContentId */,
					  peer->dbid			/* remoteDbid */,
					  0						/* remotePid */);

	peer->state |= IC_PROXY_PEER_STATE_SENDING_HELLO;

	ic_proxy_peer_update_name(peer);
	ic_proxy_peer_send_message(peer, IC_PROXY_MESSAGE_PEER_HELLO, &key,
							   ic_proxy_peer_on_sent_hello);
}

/*
 * Connect to a remote peer.
 */
void
ic_proxy_peer_connect(ICProxyPeer *peer, struct sockaddr_in *dest)
{
	uv_connect_t *conn;
	char		name[HOST_NAME_MAX];

	if (peer->state & IC_PROXY_PEER_STATE_CONNECTING)
		return;

	peer->state |= IC_PROXY_PEER_STATE_CONNECTING;

	uv_ip4_name(dest, name, sizeof(name));
	ic_proxy_log(LOG, "%s: connecting to %s:%d",
				 peer->name, name, ntohs(dest->sin_port));

	/* reinit the tcp handle */
	uv_tcp_init(peer->tcp.loop, &peer->tcp);
	uv_tcp_nodelay(&peer->tcp, true);

	conn = ic_proxy_new(uv_connect_t);

	uv_tcp_connect(conn, &peer->tcp, (struct sockaddr *) dest,
				   ic_proxy_peer_on_connected);
}

/*
 * Send a packet to a remote peer.
 */
void
ic_proxy_peer_route_data(ICProxyPeer *peer, ICProxyPkt *pkt,
						 ic_proxy_sent_cb callback, void *opaque)
{
	if (!(peer->state & IC_PROXY_PEER_STATE_READY_FOR_DATA))
	{
		ICProxyDelay *delay;

		ic_proxy_log(LOG, "%s: caching outgoing %s",
					 peer->name, ic_proxy_pkt_to_str(pkt));

		delay = ic_proxy_peer_build_delay(peer, pkt, callback, opaque);
		peer->reqs = lappend(peer->reqs, delay);

		return;
	}

	ic_proxy_router_write((uv_stream_t *) &peer->tcp, pkt, 0, callback, opaque);
}

/*
 * Send the peer control message, HELLO and HELLO ACK.  The client control
 * message should be sent with ic_proxy_peer_route_data().
 *
 * TODO: it's better to separate the peer messages from the client messages
 * completely.
 */
static void
ic_proxy_peer_send_message(ICProxyPeer *peer, ICProxyMessageType mtype,
						   const ICProxyKey *key, ic_proxy_sent_cb callback)
{
	ICProxyPkt *pkt;

	if (!(peer->state & IC_PROXY_PEER_STATE_READY_FOR_MESSAGE))
		ic_proxy_log(ERROR,
					 "%s: not ready to send or receive messages",
					 peer->name);

	pkt = ic_proxy_message_new(mtype, key);

	ic_proxy_router_write((uv_stream_t *) &peer->tcp, pkt, 0, callback, peer);
}

/*
 * This function is only called on a new peer, so it is not so expansive to
 * rebuild the cache list.
 */
static void
ic_proxy_peer_handle_out_cache(ICProxyPeer *peer)
{
	List	   *reqs;
	ListCell   *cell;

	if (!(peer->state & IC_PROXY_PEER_STATE_READY_FOR_DATA))
		return;

	if (peer->reqs == NIL)
		return;

	ic_proxy_log(LOG, "%s: trying to consume the %d cached outgoing pkts",
				 peer->name, list_length(peer->reqs));

	/* First detach all the pkts */
	reqs = peer->reqs;
	peer->reqs = NIL;

	/* Then re-handle them one by one */
	foreach(cell, reqs)
	{
		ICProxyDelay *delay = lfirst(cell);

		/* TODO: can we pass the delay directly? */
		ic_proxy_peer_route_data(peer, delay->pkt,
								 delay->callback, delay->opaque);

		ic_proxy_free(delay);
	}

	ic_proxy_log(LOG, "%s: consumed %d cached pkts",
				 peer->name, list_length(reqs) - list_length(peer->reqs));

	/*
	 * the pkts ownership were transfered during ic_proxy_peer_route_data(),
	 * only need to free the list itself.
	 */
	list_free(reqs);
}
