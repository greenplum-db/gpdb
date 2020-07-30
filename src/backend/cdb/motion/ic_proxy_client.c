/*-------------------------------------------------------------------------
 *
 * ic_proxy_client.c
 *
 *    Interconnect Proxy Client
 *
 * A client lives in the proxy bgworker and connects to a backend, each logical
 * connection needs a client, so there could be multiple connections between
 * one backend process and the proxy bgworker, the same amount of clients are
 * needed, too.
 *
 * A local client communicates to exact one remote client, a special case is
 * that both clients are on the same segment, in which case both are called
 * loopback clients.
 *
 * A client is created on a new backend connection, it is registered after the
 * hand shaking.  The hand shaking is made between the backend and the client,
 * this is different with the ic-tcp or ic-udp modes, they do the hand shaking
 * between the local and remote backends.
 *
 * A client is identified with a key, every packet also contains such a key, so
 * when a packet is received we could know which client to route it to.
 *
 * Packets can arrive before the client registration, in such a case a
 * placeholder is registered to hold the early coming packets.  Once the client
 * finishes the hand shaking it replaces the placeholder and handles these
 * early packets in the arriving order.
 *
 * The interconnect treats the motion sender and the receiver differently,
 * however in ic-proxy we do not distinguish the sender client or the receiver
 * client.  Sometimes we will mention the words sender or receiver, they only
 * have logical meanings, and are not marked in the code.
 *
 * Sometimes a backend receives slower than the sender, so more and more
 * packets are put in the writing queue.  To prevent it consuming too many
 * memory we have a simple flow control.  The receiver client sends a PAUSE
 * request to the sender, the sender pauses reading from its backend, so the
 * data flow is paused.  Once the receiver consumes some packets and the
 * writing queue is empty enough the receiver sends a RESUME request to the
 * sender, the sender resumes reading from its backend, so the data flow is
 * resumed.
 *
 * Packets are routed in 2 directions, c2p and p2c:
 * - c2p packets are routed from a client to a peer;
 * - p2c packets are routed from a peer to a client;
 * - there is no term "c2c" because loopback packets are also handled as c2p
 *   by the sender and p2c by the receiver;
 *
 * The directions are not named as incoming and outgoing because the they can
 * lead to confusing descriptions: "a client receives an outgoing packet from
 * its backend", or, "a client sends an incoming packet to its backend".
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
#include "ic_proxy_router.h"

#include <uv.h>


typedef struct ICProxyClientEntry ICProxyClientEntry;


/*
 * TODO: a client only has one target, it's either a peer, or a loopback
 * client, there is no need to lookup it every time, the loopback client lookup
 * is expansive.
 *
 * We used to save the target since the very beginning, however it was buggy on
 * placeholders, so we disabled it.  If we want to reenable it be careful on
 * below notes:
 * - a peer can be a placeholder;
 * - a loopback client can be a placeholder;
 * - ibuf/obuf functions are not reentrantable, delayed queue is necessary;
 */
struct ICProxyClient
{
	uv_pipe_t	pipe;			/* the libuv handle */

	ICProxyKey	key;			/* the key to identify a client */

	uint32		state;			/* or'ed bits of below ones */
#define IC_PROXY_CLIENT_STATE_RECEIVED_HELLO  0x00000001
#define IC_PROXY_CLIENT_STATE_SENT_HELLO_ACK  0x00000002
#define IC_PROXY_CLIENT_STATE_REGISTERED      0x00000004
#define IC_PROXY_CLIENT_STATE_PLACEHOLDER     0x00000008
#define IC_PROXY_CLIENT_STATE_C2P_SHUTTING    0x00000010
#define IC_PROXY_CLIENT_STATE_C2P_SHUTTED     0x00000020
#define IC_PROXY_CLIENT_STATE_P2C_SHUTTING    0x00000040
#define IC_PROXY_CLIENT_STATE_P2C_SHUTTED     0x00000080
#define IC_PROXY_CLIENT_STATE_CLOSING         0x00000100
#define IC_PROXY_CLIENT_STATE_CLOSED          0x00000200
#define IC_PROXY_CLIENT_STATE_PAUSING         0x00000400
#define IC_PROXY_CLIENT_STATE_PAUSED          0x00000800
#define IC_PROXY_CLIENT_STATE_REMOTE_PAUSE    0x00001000

	int			unconsumed;		/* count of the packets that are unconsumed by
								 * the backend */
	int			sending;		/* count of the packets being sent, both c2p &
								 * p2c, both data & message */

	ICProxyOBuf	obuf;			/* obuf merges small outgoing packets into
								 * large ones to reduce network overhead */
	ICProxyIBuf	ibuf;			/* ibuf detects the packet boundaries */

	ICProxyClient *successor;	/* if a client is registered while a previous
								 * one with the same key is still alive, the
								 * registration is delayed, the new one marks
								 * itself as the successor of the previous one,
								 * when the previous one is deregistered the
								 * new one gets actually registered. */

	List	   *pkts;			/* early coming packets */

	char	   *name;			/* name of the client, only for logging */
#define IC_PROXY_CLIENT_NAME_SIZE 256

	/* TODO: statistics */
};

/*
 * Client hash table entry.
 */
struct ICProxyClientEntry
{
	ICProxyKey	key;			/* the hash key */

	ICProxyClient *client;		/* the client */
};


static void ic_proxy_client_clear_name(ICProxyClient *client);
static void ic_proxy_client_shutdown_c2p(ICProxyClient *client);
static void ic_proxy_client_shutdown_p2c(ICProxyClient *client);
static void ic_proxy_client_close(ICProxyClient *client);
static void ic_proxy_client_read_data(ICProxyClient *client);
static void ic_proxy_client_cache_p2c_pkt(ICProxyClient *client,
										  ICProxyPkt *pkt);
static void ic_proxy_client_cache_p2c_pkts(ICProxyClient *client, List *pkts);
static void ic_proxy_client_drop_p2c_cache(ICProxyClient *client);
static void ic_proxy_client_handle_p2c_cache(ICProxyClient *client);
static void ic_proxy_client_maybe_start_read_data(ICProxyClient *client);
static void ic_proxy_client_maybe_request_pause(ICProxyClient *client);
static void ic_proxy_client_maybe_request_resume(ICProxyClient *client);
static void ic_proxy_client_maybe_pause(ICProxyClient *client);
static void ic_proxy_client_maybe_resume(ICProxyClient *client);


/*
 * The client register table.
 */
static HTAB		   *ic_proxy_clients = NULL;


void
ic_proxy_client_table_init(void)
{
	HASHCTL		hashctl;

	hashctl.hash = (HashValueFunc) ic_proxy_key_hash;
	hashctl.match = (HashCompareFunc) ic_proxy_key_equal_for_hash;
	hashctl.keycopy = memcpy;

	hashctl.keysize = sizeof(ICProxyKey);
	hashctl.entrysize = sizeof(ICProxyClientEntry);

	ic_proxy_clients = hash_create("ic-proxy clients",
								   MaxConnections /* nelem */,
								   &hashctl,
								   HASH_ELEM | HASH_FUNCTION |
								   HASH_COMPARE | HASH_KEYCOPY);
}

void
ic_proxy_client_table_uninit(void)
{
	if (ic_proxy_clients)
	{
		hash_destroy(ic_proxy_clients);
		ic_proxy_clients = NULL;
	}
}

/*
 * Shutdown all the clients who communicate with dbid.
 *
 * When a proxy-proxy connection to dbid is lost, all the logical connections
 * from or to it must be dropped, too.
 */
void
ic_proxy_client_table_shutdown_by_dbid(uint16 dbid)
{
	ICProxyClientEntry *entry;
	HASH_SEQ_STATUS seq;

	ic_proxy_log(LOG,
				 "ic-proxy-clients: shutting down all the clients by dbid %hu",
				 dbid);

	hash_seq_init(&seq, ic_proxy_clients);

	while ((entry = hash_seq_search(&seq)))
	{
		ICProxyClient *client = entry->client;

		/* cached pkts must also be dropped */
		ic_proxy_client_drop_p2c_cache(client);

		if (client->state & IC_PROXY_CLIENT_STATE_PLACEHOLDER)
			continue;

		if (client->key.remoteDbid != dbid)
			continue;

		ic_proxy_client_shutdown_c2p(client);
		ic_proxy_client_shutdown_p2c(client);
	}
}

/*
 * Register a client with its key.
 *
 * - if there was a placeholder, replace it;
 * - if there was a legacy but still alive one, make the client as its
 *   successor and delay the registration;
 */
static void
ic_proxy_client_register(ICProxyClient *client)
{
	ICProxyClientEntry *entry;
	bool		found;

	/* this should never happen */
	if (client->state & IC_PROXY_CLIENT_STATE_REGISTERED)
	{
		ic_proxy_log(WARNING, "%s: already registered",
					 ic_proxy_client_get_name(client));
		return;
	}

	entry = hash_search(ic_proxy_clients, &client->key, HASH_ENTER, &found);

	if (found)
	{
		/* someone with the same identifier comes earlier than me */
		ICProxyClient *placeholder = entry->client;

		/* this should never happen, if it does, sth. serious is wrong */
		if (placeholder == client)
		{
			ic_proxy_log(ERROR,
						 "%s: unregistered client found in the client table",
						 ic_proxy_client_get_name(client));
			return;
		}

		if (!(placeholder->state & IC_PROXY_CLIENT_STATE_PLACEHOLDER))
		{
			/*
			 * it's the client of last statement, for example in the query
			 * "alter table set distributed by", it contains multiple
			 * statements, some of them share the same command id.
			 *
			 * TODO: we believe the old one is shutting down, but what if not?
			 */
			ic_proxy_log(WARNING,
						 "%s: delay the register as the previous client is still shutting down",
						 ic_proxy_client_get_name(client));

			/* this should never happen, if it does, sth. serious is wrong */
			if (placeholder->successor != NULL)
				ic_proxy_log(ERROR,
							 "%s: the previous client already has a successor",
							 ic_proxy_client_get_name(client));

			/*
			 * register client as a successor, it will be actually registered
			 * when the placeholder unregister.
			 */
			placeholder->successor = client;
			return;
		}

		/* it's a placeholder, this happens if the pkts arrived before me */
		ic_proxy_log(LOG, "%s: replace my placeholder %s",
					 ic_proxy_client_get_name(client),
					 ic_proxy_client_get_name(placeholder));

		/*
		 * the only purpose of a placeholder is to save the early coming
		 * pkts, there should be no chance for it to be empty; but in case
		 * it really happens, don't panic, nothing serious.
		 */
		if (placeholder->pkts == NIL)
			ic_proxy_log(LOG, "%s: no cached pkts in the placeholder %s",
						 ic_proxy_client_get_name(client),
						 ic_proxy_client_get_name(placeholder));

		/* replace the placeholder / legacy client */
		entry->client = client;

		/* transfer the cached pkts */
		ic_proxy_client_cache_p2c_pkts(client, placeholder->pkts);
		placeholder->pkts = NIL;

		if (placeholder->state & IC_PROXY_CLIENT_STATE_PLACEHOLDER)
		{
			/*
			 * need to free the placeholder, but no need to unregister it as we
			 * replaced it already.  The placeholder has nothing more than
			 * itself, so free it directly.
			 */
			ic_proxy_free(placeholder);
			ic_proxy_log(LOG, "%s: freed my placeholder",
						 ic_proxy_client_get_name(client));
		}
	}
	else
	{
		entry->client = client;
	}

	client->state |= IC_PROXY_CLIENT_STATE_REGISTERED;

	/* clear the name so we could show the new name */
	ic_proxy_client_clear_name(client);

	ic_proxy_log(LOG, "%s: registered, %ld in total",
				 ic_proxy_client_get_name(client),
				 hash_get_num_entries(ic_proxy_clients));
}

/*
 * Unregister a client.
 *
 * If it has a successor, trigger its registration.
 *
 * If there are still unhandled pkts, transfer them to the successor or the
 * placeholder.  This happens on loopback connections, due to the limitation of
 * the delayed delivery, pkts can be pushed to a p2c shutting/shutted client,
 * these pkts belong to the next subplan in the same query.
 */
static void
ic_proxy_client_unregister(ICProxyClient *client)
{
	if (!(client->state & IC_PROXY_CLIENT_STATE_REGISTERED))
		return;

	hash_search(ic_proxy_clients, &client->key, HASH_REMOVE, NULL);

	client->state &= ~IC_PROXY_CLIENT_STATE_REGISTERED;

	ic_proxy_log(LOG, "%s: unregistered, %ld in total",
				 ic_proxy_client_get_name(client),
				 hash_get_num_entries(ic_proxy_clients));

	if (client->successor)
	{
		ICProxyClient *successor;

		successor = client->successor;
		client->successor = NULL;

		if (client->pkts)
		{
			ic_proxy_log(LOG, "%s: transfer %d unhandled pkts to my successor",
						 ic_proxy_client_get_name(client),
						 list_length(client->pkts));

			ic_proxy_client_cache_p2c_pkts(successor, client->pkts);
			client->pkts = NIL;
		}

		ic_proxy_log(LOG, "%s: re-register my successor",
					 ic_proxy_client_get_name(client));

		/* the successor must have not registered */
		Assert(!(successor->state & IC_PROXY_CLIENT_STATE_REGISTERED));

		ic_proxy_client_register(successor);

		/* the successor must have successfully registered */
		Assert(successor->state & IC_PROXY_CLIENT_STATE_REGISTERED);

		ic_proxy_client_maybe_start_read_data(successor);
	}
	else if (client->pkts)
	{
		ICProxyClient *placeholder;

		ic_proxy_log(LOG, "%s: transfer %d unhandled pkts to my placeholder",
					 ic_proxy_client_get_name(client),
					 list_length(client->pkts));

		placeholder = ic_proxy_client_new(client->pipe.loop,
										  true /* placeholder */);
		placeholder->key = client->key;
		ic_proxy_client_register(placeholder);

		ic_proxy_client_cache_p2c_pkts(placeholder, client->pkts);
		client->pkts = NIL;
	}
}

/*
 * Look up a client with a key.
 */
static ICProxyClient *
ic_proxy_client_lookup(const ICProxyKey *key)
{
	ICProxyClientEntry *entry;
	bool		found;

	entry = hash_search(ic_proxy_clients, key, HASH_FIND, &found);

	return found ? entry->client : NULL;
}

/*
 * Look up a client with a key, create a placeholder if not found.
 */
ICProxyClient *
ic_proxy_client_blessed_lookup(uv_loop_t *loop, const ICProxyKey *key)
{
	ICProxyClient *client = ic_proxy_client_lookup(key);

	if (!client)
	{
		client = ic_proxy_client_new(loop, true);
		client->key = *key;
		ic_proxy_client_register(client);

		ic_proxy_log(LOG, "%s: registered as a placeholder",
					 ic_proxy_client_get_name(client));
	}

	return client;
}

/*
 * Pass a c2p packet to the router.
 */
static void
ic_proxy_client_route_c2p_data(void *opaque, const void *data, uint16 size)
{
	const ICProxyPkt *pkt = data;
	ICProxyClient *client = opaque;

	Assert(ic_proxy_pkt_is_from_client(pkt, &client->key));
	Assert(ic_proxy_pkt_is_live(pkt, &client->key));

	ic_proxy_router_route(client->pipe.loop, ic_proxy_pkt_dup(pkt), NULL, NULL);
}

/*
 * Received a complete c2p packet.
 */
static void
ic_proxy_client_on_c2p_data_pkt(void *opaque, const void *data, uint16 size)
{
	ICProxyClient *client = opaque;

	ic_proxy_log(LOG, "%s: received B2C PKT [%d bytes] from the backend",
				 ic_proxy_client_get_name(client), size);

	/*
	 * Send it out, but maybe not immediately.  The obuf helps to merge small
	 * packets into large ones, which reduces the network overhead
	 * significantly.
	 */
	ic_proxy_obuf_push(&client->obuf, data, size,
					   ic_proxy_client_route_c2p_data, client);
}

/*
 * Received c2p data, or events (error, eof).
 */
static void
ic_proxy_client_on_c2p_data(uv_stream_t *stream,
							ssize_t nread, const uv_buf_t *buf)
{
	ICProxyClient *client = CONTAINER_OF((void *) stream, ICProxyClient, pipe);

	if (unlikely(nread < 0))
	{
		if (nread != UV_EOF)
			ic_proxy_log(WARNING, "%s: fail to receive c2p DATA: %s",
						 ic_proxy_client_get_name(client), uv_strerror(nread));
		else
			ic_proxy_log(LOG, "%s: received EOF while waiting for c2p DATA",
						 ic_proxy_client_get_name(client));

		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		/*
		 * If a backend shuts down normally the ibuf should be empty, or
		 * contains exactly 1 byte, the STOP message.  However it's possible
		 * that the backend shuts down due to exception, in such a case there
		 * can be any amount of data left in the ibuf.
		 */
		if (client->ibuf.len > 0)
		{
			ic_proxy_log(LOG,
						 "%s: the ibuf still contains %d bytes,"
						 " flush before shutting down",
						 ic_proxy_client_get_name(client), client->ibuf.len);

			ic_proxy_ibuf_push(&client->ibuf, NULL, 0,
							   ic_proxy_client_on_c2p_data_pkt, client);
		}

		/* flush unsent data */
		ic_proxy_obuf_push(&client->obuf, NULL, 0,
						   ic_proxy_client_route_c2p_data, client);

		/* stop reading from the backend */
		uv_read_stop((uv_stream_t *) &client->pipe);

		/* inform the other side of the logical connection to close, too */
		ic_proxy_client_shutdown_c2p(client);
		return;
	}
	else if (unlikely(nread == 0))
	{
		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		/* EAGAIN or EWOULDBLOCK, retry */
		return;
	}

	/* TODO: Will this really happen? */
	if (client->state & IC_PROXY_CLIENT_STATE_PAUSED)
		ic_proxy_log(ERROR,
					 "%s: paused already, but still received DATA[%zd bytes] from the backend, state is 0x%08x",
					 ic_proxy_client_get_name(client), nread, client->state);

	ic_proxy_log(LOG, "%s: received DATA[%zd bytes] from the backend",
				 ic_proxy_client_get_name(client), nread);

	/*
	 * The c2p data needs to be handled as packets, the ibuf helps to find the
	 * boundaries.
	 */
	ic_proxy_ibuf_push(&client->ibuf, buf->base, nread,
					   ic_proxy_client_on_c2p_data_pkt, client);
	ic_proxy_pkt_cache_free(buf->base);

	/* Handle pending PAUSE request */
	ic_proxy_client_maybe_pause(client);
}

/*
 * Start reading from backend if the client is successfully registered.
 */
static void
ic_proxy_client_maybe_start_read_data(ICProxyClient *client)
{
	if (!(client->state & IC_PROXY_CLIENT_STATE_REGISTERED))
		/*
		 * the register is delayed as the previous client is still shutting
		 * down, do not send the ACK until that's done.
		 */
		return;

	ic_proxy_log(LOG, "%s: start receiving DATA",
				 ic_proxy_client_get_name(client));

	/* since now on, the client and backend communicate only via b2c packets */
	Assert(ic_proxy_ibuf_empty(&client->ibuf));
	ic_proxy_ibuf_uninit(&client->ibuf);
	ic_proxy_ibuf_init_b2c(&client->ibuf);

	/* and do not forget to init the obuf header */
	ic_proxy_message_init(ic_proxy_obuf_ensure_buffer(&client->obuf),
						  0, &client->key);

#ifdef USE_ASSERT_CHECKING
	/*
	 * this should never happen because no p2c pkt is handled yet, the early
	 * coming pkts are in the cache list, we are just going to handle them.
	 */
	if (client->state &
		(IC_PROXY_CLIENT_STATE_C2P_SHUTTING |
		 IC_PROXY_CLIENT_STATE_P2C_SHUTTING))
	{
		ic_proxy_log(WARNING,
					 "%s: state=0x%08x: unexpected shutting down in progress",
					 ic_proxy_client_get_name(client), client->state);
		return;
	}
#endif /* USE_ASSERT_CHECKING */

	/*
	 * now it's time to receive the normal data, it is important to start the
	 * reading before handling the cached pkts, so if the cached pkts contain
	 * the BYE we could stop the reading in time.
	 */
	ic_proxy_client_read_data(client);

	ic_proxy_client_handle_p2c_cache(client);
}

/*
 * The HELLO ACK is sent out.
 */
static void
ic_proxy_client_on_sent_hello_ack(void *opaque,
								  const ICProxyPkt *pkt, int status)
{
	ICProxyClient *client = opaque;

	if (status < 0)
	{
		ic_proxy_client_shutdown_p2c(client);
		return;
	}

	client->state |= IC_PROXY_CLIENT_STATE_SENT_HELLO_ACK;

	ic_proxy_client_register(client);

	ic_proxy_client_maybe_start_read_data(client);
}

/*
 * Received the complete HELLO message.
 */
static void
ic_proxy_client_on_hello_pkt(void *opaque, const void *data, uint16 size)
{
	const ICProxyPkt *pkt = data;
	ICProxyClient *client = opaque;
	ICProxyKey	key;
	ICProxyPkt *ackpkt;

	/* we only expect one HELLO message */
	uv_read_stop((uv_stream_t *) &client->pipe);

	if (!ic_proxy_pkt_is(pkt, IC_PROXY_MESSAGE_HELLO))
	{
		ic_proxy_log(WARNING, "%s: invalid %s",
					 ic_proxy_client_get_name(client),
					 ic_proxy_pkt_to_str(pkt));

		ic_proxy_client_shutdown_p2c(client);
		return;
	}

	ic_proxy_log(LOG, "%s: received %s from the backend",
				 ic_proxy_client_get_name(client), ic_proxy_pkt_to_str(pkt));

	client->state |= IC_PROXY_CLIENT_STATE_RECEIVED_HELLO;

	ic_proxy_key_from_c2p_pkt(&key, pkt);

	/*
	 * If we register before the HELLO ACK is sent, the peer has a chance to
	 * route data before HELLO ACK.  So we must register in the write callback.
	 */

	client->key = key;

	/* clear the name so we could show the new name */
	ic_proxy_client_clear_name(client);

	/* build a HELLO ACK */
	ic_proxy_key_reverse(&key);
	ackpkt = ic_proxy_message_new(IC_PROXY_MESSAGE_HELLO_ACK, &key);

	ic_proxy_router_write((uv_stream_t *) &client->pipe, ackpkt, 0,
						  ic_proxy_client_on_sent_hello_ack, client);
}

/*
 * Received the HELLO message, maybe only part of.
 */
static void
ic_proxy_client_on_hello_data(uv_stream_t *stream,
							  ssize_t nread, const uv_buf_t *buf)
{
	ICProxyClient *client = CONTAINER_OF((void *) stream, ICProxyClient, pipe);

	if (unlikely(nread < 0))
	{
		if (nread != UV_EOF)
			ic_proxy_log(WARNING, "%s: fail to receive HELLO: %s",
						 ic_proxy_client_get_name(client), uv_strerror(nread));
		else
			ic_proxy_log(LOG, "%s: received EOF while waiting for HELLO",
						 ic_proxy_client_get_name(client));

		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		ic_proxy_log(WARNING, "%s, TODO: fail to receive HELLO",
					 ic_proxy_client_get_name(client));
		/* 
		 * Pending data in a failed handshake is useless,
		 * no need to call shutdown() explicitly.
		 */
		ic_proxy_client_close(client);
		return;
	}
	else if (unlikely(nread == 0))
	{
		if (buf->base)
			ic_proxy_pkt_cache_free(buf->base);

		/* EAGAIN or EWOULDBLOCK, retry */
		return;
	}

	/* Hand shaking is done via p2p packets */
	ic_proxy_ibuf_push(&client->ibuf, buf->base, nread,
					   ic_proxy_client_on_hello_pkt, client);
	ic_proxy_pkt_cache_free(buf->base);
}

/*
 * Start reading the HELLO message.
 */
int
ic_proxy_client_read_hello(ICProxyClient *client)
{
	/* start reading the HELLO message */
	return uv_read_start((uv_stream_t *) &client->pipe,
						 ic_proxy_pkt_cache_alloc_buffer,
						 ic_proxy_client_on_hello_data);
}

/*
 * Start reading the DATA.
 */
static void
ic_proxy_client_read_data(ICProxyClient *client)
{
	int			ret;

	ret = uv_read_start((uv_stream_t *) &client->pipe,
						ic_proxy_pkt_cache_alloc_buffer,
						ic_proxy_client_on_c2p_data);

	if (ret < 0)
	{
		ic_proxy_log(WARNING, "%s: state=0x%08x: fail to start reading data: %s",
					 ic_proxy_client_get_name(client),
					 client->state, uv_strerror(ret));

		ic_proxy_client_shutdown_c2p(client);
	}
}

/*
 * Get the client name.
 *
 * The name is constructed from the state lazily.  When the state gets changed,
 * call ic_proxy_client_clear_name() to clear the name, so we could reconstruct
 * the name from the new state.
 */
const char *
ic_proxy_client_get_name(ICProxyClient *client)
{
	if (unlikely(client->name == NULL))
	{
		uint32		namesize = IC_PROXY_CLIENT_NAME_SIZE;
		const char *keystr = "";
		const char *suffix = "";

		if (client->state & IC_PROXY_CLIENT_STATE_REGISTERED)
			keystr = ic_proxy_key_to_str(&client->key);

		if (client->state & IC_PROXY_CLIENT_STATE_PLACEHOLDER)
			suffix = ".placeholder";

		client->name = ic_proxy_alloc(namesize);
		snprintf(client->name, namesize, "client%s%s", suffix, keystr);
	}

	return client->name;
}

static void
ic_proxy_client_clear_name(ICProxyClient *client)
{
	if (client->name)
	{
		ic_proxy_free(client->name);
		client->name = NULL;
	}
}

/*
 * Return the libuv stream handle of a client.
 */
uv_stream_t *
ic_proxy_client_get_stream(ICProxyClient *client)
{
	return (uv_stream_t *) &client->pipe;
}

ICProxyClient *
ic_proxy_client_new(uv_loop_t *loop, bool placeholder)
{
	ICProxyClient *client;

	client = ic_proxy_new(ICProxyClient);
	uv_pipe_init(loop, &client->pipe, false);

	client->pkts = NIL;
	client->state = 0;
	client->unconsumed = 0;
	client->sending = 0;
	client->successor = NULL;
	client->name = NULL;

	ic_proxy_obuf_init_p2p(&client->obuf);

	/* hand shaking messages are in the p2p format */
	ic_proxy_ibuf_init_p2p(&client->ibuf);

	if (placeholder)
		client->state |= IC_PROXY_CLIENT_STATE_PLACEHOLDER;

	return client;
}

/*
 * Free a non-placeholder client.
 *
 * The placeholders are free()'ed directly in ic_proxy_client_register(), only
 * the non-placeholder clients will need this function.
 */
static void
ic_proxy_client_free(ICProxyClient *client)
{
	ic_proxy_log(LOG, "%s: freeing", ic_proxy_client_get_name(client));

	/*
	 * This should not happen, but in case it happens we want to leave a
	 * message in the logs to help us understand why.
	 */
	ic_proxy_client_drop_p2c_cache(client);

	ic_proxy_obuf_uninit(&client->obuf);
	ic_proxy_ibuf_uninit(&client->ibuf);

	Assert(client->successor == NULL);

	ic_proxy_client_clear_name(client);
	ic_proxy_free(client);
}

/*
 * The client is closed.
 */
static void
ic_proxy_client_on_close(uv_handle_t *handle)
{
	ICProxyClient *client = CONTAINER_OF((void *) handle, ICProxyClient, pipe);

	if (client->state & IC_PROXY_CLIENT_STATE_CLOSED)
		ic_proxy_log(ERROR, "%s: double close",
					 ic_proxy_client_get_name(client));

	client->state |= IC_PROXY_CLIENT_STATE_CLOSED;

	ic_proxy_log(LOG, "%s: closed", ic_proxy_client_get_name(client));

	ic_proxy_client_free(client);
}

/*
 * Close a client.
 */
static void
ic_proxy_client_close(ICProxyClient *client)
{
	if (client->state & IC_PROXY_CLIENT_STATE_CLOSING)
		return;

	client->state |= IC_PROXY_CLIENT_STATE_CLOSING;

	ic_proxy_client_unregister(client);

	ic_proxy_log(LOG, "%s: closing", ic_proxy_client_get_name(client));
	uv_close((uv_handle_t *) &client->pipe, ic_proxy_client_on_close);
}

/*
 * Close a client if it is ready to close.
 *
 * Where ready means below conditions are both met:
 * - no data or message pkts being sent;
 * - both c2p and p2c are shutted down;
 *
 * Return true if it is ready to close, false otherwise.
 */
static bool
ic_proxy_client_maybe_close(ICProxyClient *client)
{
	uint32		bits = 0;

	bits |= IC_PROXY_CLIENT_STATE_C2P_SHUTTED;
	bits |= IC_PROXY_CLIENT_STATE_P2C_SHUTTED;

	if (client->sending == 0 && ((client->state & bits) == bits))
	{
		ic_proxy_client_close(client);
		return true;
	}

	return false;
}

/*
 * Sent the c2p BYE message.
 */
static void
ic_proxy_client_on_sent_c2p_bye(void *opaque, const ICProxyPkt *pkt, int status)
{
	ICProxyClient *client = opaque;

	if (status < 0)
	{
		/*
		 * Failed to send the c2p BYE, maybe the peer connection is already
		 * lost.  This can be safely ignored, we could just behave as it is
		 * sent out.
		 */
		ic_proxy_log(LOG, "%s: fail to shutdown c2p",
					 ic_proxy_client_get_name(client));

		/*
		 * When we fail to send the BYE, should we trigger the shutdown_p2c
		 * process immediately?
		 */
	}

	ic_proxy_log(LOG, "%s: shutted down c2p", ic_proxy_client_get_name(client));

	client->sending--;

	client->state |= IC_PROXY_CLIENT_STATE_C2P_SHUTTED;
	ic_proxy_client_maybe_close(client);
}

/*
 * Shutdown in c2p direction.
 */
static void
ic_proxy_client_shutdown_c2p(ICProxyClient *client)
{
	/* Do not re-shutdown if we are already doing so or even have done */
	if (client->state & IC_PROXY_CLIENT_STATE_C2P_SHUTTING)
	{
		ic_proxy_client_maybe_close(client);
		return;
	}

	ic_proxy_log(LOG, "%s: shutting down c2p",
				 ic_proxy_client_get_name(client));

	client->state |= IC_PROXY_CLIENT_STATE_C2P_SHUTTING;

	if (client->state & IC_PROXY_CLIENT_STATE_REGISTERED)
	{
		/*
		 * For a registered client, try to send a c2p BYE message to the
		 * remote.
		 */

		ICProxyPkt *pkt = ic_proxy_message_new(IC_PROXY_MESSAGE_BYE,
											   &client->key);

		client->sending++;

		ic_proxy_router_route(client->pipe.loop, pkt,
							  ic_proxy_client_on_sent_c2p_bye, client);
	}
	else
	{
		/*
		 * For a non-registered client, simply behave as if there is a remote
		 * and it has received the BYE and sent back a P2C BYE.
		 */

		ic_proxy_log(LOG, "%s: shutted down c2p",
					 ic_proxy_client_get_name(client));

		client->state |= IC_PROXY_CLIENT_STATE_C2P_SHUTTED;

		ic_proxy_client_shutdown_p2c(client);
	}
}

/*
 * Shutted down in p2c direction.
 */
static void
ic_proxy_client_on_shutdown_p2c(uv_shutdown_t *req, int status)
{
	ICProxyClient *client = CONTAINER_OF((void *) req->handle, ICProxyClient, pipe);

	ic_proxy_free(req);

	if (status < 0)
		ic_proxy_log(WARNING, "%s: fail to shutdown p2c: %s",
					 ic_proxy_client_get_name(client), uv_strerror(status));
	else
		ic_proxy_log(LOG, "%s: shutted down p2c",
					 ic_proxy_client_get_name(client));

	client->state |= IC_PROXY_CLIENT_STATE_P2C_SHUTTED;
	ic_proxy_client_maybe_close(client);
}

/*
 * Shutdown in p2c direction.
 */
static void
ic_proxy_client_shutdown_p2c(ICProxyClient *client)
{
	uv_shutdown_t *req;

	if (client->state & IC_PROXY_CLIENT_STATE_P2C_SHUTTING)
	{
		ic_proxy_client_maybe_close(client);
		return;
	}

	ic_proxy_log(LOG, "%s: shutting down p2c",
				 ic_proxy_client_get_name(client));

	client->state |= IC_PROXY_CLIENT_STATE_P2C_SHUTTING;

	req = ic_proxy_new(uv_shutdown_t);
	uv_shutdown(req, (uv_stream_t *) &client->pipe,
				ic_proxy_client_on_shutdown_p2c);
}

/*
 * Sent c2p RESUME message.
 */
static void
ic_proxy_client_on_sent_c2p_resume(void *opaque,
								   const ICProxyPkt *pkt, int status)
{
	ICProxyClient *client = opaque;

	if (status < 0)
	{
		/*
		 * TODO: Fail to send the RESUME, should we retry instead of shutting
		 * down?
		 */
		ic_proxy_client_shutdown_p2c(client);
	}

	client->sending--;

	ic_proxy_client_maybe_close(client);
}

/*
 * Sent c2p PAUSE message.
 */
static void
ic_proxy_client_on_sent_c2p_pause(void *opaque,
								  const ICProxyPkt *pkt, int status)
{
	ICProxyClient *client = opaque;

	if (status < 0)
	{
		/*
		 * TODO: Fail to send the PAUSE, should we retry instead of shutting
		 * down?
		 */
		ic_proxy_client_shutdown_p2c(client);
	}

	client->sending--;

	ic_proxy_client_maybe_close(client);
}

/*
 * Routed p2c DATA to the backend.
 */
static void
ic_proxy_client_on_sent_p2c_data(void *opaque,
								 const ICProxyPkt *pkt, int status)
{
	ICProxyClient *client = opaque;

	/* this could happen if the backend errored out */
	if (status < 0)
		ic_proxy_client_shutdown_c2p(client);

	client->unconsumed--;
	client->sending--;

	ic_proxy_client_maybe_request_resume(client);
}

/*
 * Received a complete p2c message.
 *
 * This should not be called by a placeholder.
 */
static void
ic_proxy_client_on_p2c_message(ICProxyClient *client, const ICProxyPkt *pkt,
							   ic_proxy_sent_cb callback, void *opaque)
{
	if (ic_proxy_pkt_is(pkt, IC_PROXY_MESSAGE_BYE))
	{
		ic_proxy_log(LOG, "%s: received %s",
					 ic_proxy_client_get_name(client),
					 ic_proxy_pkt_to_str(pkt));

		ic_proxy_client_maybe_resume(client);

		ic_proxy_client_shutdown_p2c(client);
	}
	else if (ic_proxy_pkt_is(pkt, IC_PROXY_MESSAGE_PAUSE))
	{
		ic_proxy_log(LOG, "%s: received %s",
					 ic_proxy_client_get_name(client),
					 ic_proxy_pkt_to_str(pkt));

		/*
		 * the PAUSE message should only be handled when the b2c ibuf
		 * is empty, so we mark the PAUSING flag, on next c2p data we will
		 * actuall stop reading if the b2c ibuf is empty.
		 */
		client->state |= IC_PROXY_CLIENT_STATE_PAUSING;

		/*
		 * if the b2c ibuf is already empty, we can stop reading immediately
		 */
		ic_proxy_client_maybe_pause(client);
	}
	else if (ic_proxy_pkt_is(pkt, IC_PROXY_MESSAGE_RESUME))
	{
		ic_proxy_log(LOG, "%s: received %s",
					 ic_proxy_client_get_name(client),
					 ic_proxy_pkt_to_str(pkt));

		ic_proxy_client_maybe_resume(client);
	}
	else
	{
		ic_proxy_log(ERROR, "%s: unsupported message",
					 ic_proxy_client_get_name(client));
	}
}

/*
 * Received a complete p2c DATA / MESSAGE packet.
 *
 * The client will takes the ownership of the pkt.
 */
void
ic_proxy_client_on_p2c_data(ICProxyClient *client, ICProxyPkt *pkt,
							ic_proxy_sent_cb callback, void *opaque)
{
	/* A placeholder does not send any packets, it always cache them */
	if (client->state & IC_PROXY_CLIENT_STATE_PLACEHOLDER)
	{
		/* FIXME: also save the callback */
		ic_proxy_client_cache_p2c_pkt(client, pkt);
	}
#ifdef USE_ASSERT_CHECKING
	else if (!ic_proxy_pkt_is_to_client(pkt, &client->key))
	{
		ic_proxy_log(WARNING, "%s: the %s is not to me",
					 ic_proxy_client_get_name(client),
					 ic_proxy_pkt_to_str(pkt));
		/* TODO: callback? */
		ic_proxy_pkt_cache_free(pkt);
	}
#endif /* USE_ASSERT_CHECKING */
	else if (!ic_proxy_pkt_is_live(pkt, &client->key))
	{
		if (ic_proxy_pkt_is_out_of_date(pkt, &client->key))
		{
			ic_proxy_log(LOG, "%s: drop out-of-date %s",
						 ic_proxy_client_get_name(client),
						 ic_proxy_pkt_to_str(pkt));
			/* TODO: callback? */
			ic_proxy_pkt_cache_free(pkt);
		}
		else
		{
			Assert(ic_proxy_pkt_is_in_the_future(pkt, &client->key));

			ic_proxy_log(LOG, "%s: future %s",
						 ic_proxy_client_get_name(client),
						 ic_proxy_pkt_to_str(pkt));
			/* FIXME: also save the callback */
			ic_proxy_client_cache_p2c_pkt(client, pkt);
		}
	}
	/* the pkt is valid and live, send it to the backend */
	else if (ic_proxy_pkt_is(pkt, IC_PROXY_MESSAGE_DATA))
	{
		if (client->state & IC_PROXY_CLIENT_STATE_P2C_SHUTTING)
		{
			/*
			 * a special kind of future packet happens between different stats
			 * of a query, the client and its successor have the same keys,
			 * such a future packet can not be detected in previous checks,
			 * we could only tell accoding to the client state.
			 */
			ic_proxy_client_cache_p2c_pkt(client, pkt);
			return;
		}

		client->unconsumed++;
		client->sending++;

		Assert(callback == NULL);
		ic_proxy_router_write((uv_stream_t *) &client->pipe,
							  pkt, sizeof(*pkt),
							  ic_proxy_client_on_sent_p2c_data, client);

		ic_proxy_client_maybe_request_pause(client);
	}
	else
	{
		ic_proxy_client_on_p2c_message(client, pkt, callback, opaque);
		ic_proxy_pkt_cache_free(pkt);
	}
}

/*
 * Save a packet for later handling.
 *
 * The client will takes the ownership of the pkt.
 */
static void
ic_proxy_client_cache_p2c_pkt(ICProxyClient *client, ICProxyPkt *pkt)
{
	/* TODO: drop out-of-date pkt directly */
	/* TODO: verify the pkt is to client */

	client->pkts = lappend(client->pkts, pkt);

	ic_proxy_log(LOG, "%s: cached a %s for future use, %d in the list",
				 ic_proxy_client_get_name(client), ic_proxy_pkt_to_str(pkt),
				 list_length(client->pkts));
}

/*
 * Save a list of packets for later handling.
 *
 * The client will takes the ownership of the pkts.
 */
static void
ic_proxy_client_cache_p2c_pkts(ICProxyClient *client, List *pkts)
{
	/* TODO: verify the pkts is to client */

	client->pkts = list_concat(client->pkts, pkts);

	ic_proxy_log(LOG,
				 "%s: cached %d pkts for future use, %d in the list",
				 ic_proxy_client_get_name(client),
				 list_length(pkts), list_length(client->pkts));
}

/*
 * Drop all the cached p2c packets.
 */
static void
ic_proxy_client_drop_p2c_cache(ICProxyClient *client)
{
	ListCell   *cell;

	if (!client->pkts)
		return;

	foreach(cell, client->pkts)
	{
		ICProxyPkt *pkt = lfirst(cell);

		ic_proxy_log(WARNING, "%s: unhandled cached %s, dropping it",
					 ic_proxy_client_get_name(client),
					 ic_proxy_pkt_to_str(pkt));

		ic_proxy_pkt_cache_free(pkt);
	}

	list_free(client->pkts);
	client->pkts = NIL;
}

/*
 * Handle all the cached p2c packets.
 *
 * Out-of-date ones will be droped, future ones will still be cached, only the
 * live ones are handled.
 *
 * This function is only called on a new client, so it is not so expansive
 * to rebuild the cache list.
 */
static void
ic_proxy_client_handle_p2c_cache(ICProxyClient *client)
{
	int			count = 0;

	/* A placeholder does not handle any packets */
	if (client->state & IC_PROXY_CLIENT_STATE_PLACEHOLDER)
		return;

	if (client->pkts == NIL)
		return;

	ic_proxy_log(LOG, "%s: trying to consume the %d cached pkts",
				 ic_proxy_client_get_name(client), list_length(client->pkts));

	/*
	 * Consume the pkts one by one, stop immediately if the client begin to
	 * shutdown in the p2c direction.
	 */
	while (client->pkts != NIL &&
		   !(client->state & IC_PROXY_CLIENT_STATE_P2C_SHUTTING))
	{
		ICProxyPkt *pkt = linitial(client->pkts);

		count++;

		/*
		 * The pkt's ownership is taken by ic_proxy_client_on_p2c_data(), so
		 * only need to free the list cell itself.
		 */
		client->pkts = list_delete_first(client->pkts);

		/* FIXME: callback */
		ic_proxy_client_on_p2c_data(client, pkt, NULL, NULL);
	}

	ic_proxy_log(LOG, "%s: consumed %d cached pkts",
				 ic_proxy_client_get_name(client), count);
}

/*
 * Request the sender side to PAUSE if the writing queue is too full.
 */
static void
ic_proxy_client_maybe_request_pause(ICProxyClient *client)
{
	ic_proxy_log(LOG, "%s: %d unconsumed packets to the backend",
				 ic_proxy_client_get_name(client), client->unconsumed);

	if (client->unconsumed >= IC_PROXY_TRESHOLD_PAUSE &&
		!(client->state & IC_PROXY_CLIENT_STATE_REMOTE_PAUSE))
	{
		ICProxyPkt *pkt = ic_proxy_message_new(IC_PROXY_MESSAGE_PAUSE,
											   &client->key);

		/*
		 * We could also clear this flag in the write callback, however
		 * that might cause us to resend the message if the sender sends
		 * fast enough.
		 */
		client->state |= IC_PROXY_CLIENT_STATE_REMOTE_PAUSE;

		client->sending++;

		ic_proxy_router_route(client->pipe.loop, pkt,
							  ic_proxy_client_on_sent_c2p_pause, client);
	}
}

/*
 * Request the sender side to RESUME if the writing queue is empty enough.
 */
static void
ic_proxy_client_maybe_request_resume(ICProxyClient *client)
{
	ic_proxy_log(LOG, "%s: %d unconsumed packets to the backend",
				 ic_proxy_client_get_name(client), client->unconsumed);

	if (client->unconsumed <= IC_PROXY_TRESHOLD_RESUME &&
		(client->state & IC_PROXY_CLIENT_STATE_REMOTE_PAUSE))
	{
		ICProxyPkt *pkt = ic_proxy_message_new(IC_PROXY_MESSAGE_RESUME,
											   &client->key);

		/*
		 * We could also clear this flag in the write callback, however that
		 * might cause us to resend the message if the backend receives fast
		 * enough.
		 */
		client->state &= ~IC_PROXY_CLIENT_STATE_REMOTE_PAUSE;

		client->sending++;

		ic_proxy_router_route(client->pipe.loop, pkt,
							  ic_proxy_client_on_sent_c2p_resume, client);
	}
}

/*
 * PAUSE if being requested.
 */
static void
ic_proxy_client_maybe_pause(ICProxyClient *client)
{
	/* Handle pending PAUSE request */
	if (((client->state & (IC_PROXY_CLIENT_STATE_PAUSING |
						   IC_PROXY_CLIENT_STATE_PAUSED)) ==
		 IC_PROXY_CLIENT_STATE_PAUSING) &&
		ic_proxy_ibuf_empty(&client->ibuf))
	{
		client->state |= IC_PROXY_CLIENT_STATE_PAUSED;

		uv_read_stop((uv_stream_t *) &client->pipe);

		/* flush unsent data */
		ic_proxy_obuf_push(&client->obuf, NULL, 0,
						   ic_proxy_client_route_c2p_data, client);

		ic_proxy_log(LOG, "%s: paused", ic_proxy_client_get_name(client));
	}
}

/*
 * RESUME if being requested.
 */
static void
ic_proxy_client_maybe_resume(ICProxyClient *client)
{
	if (client->state & IC_PROXY_CLIENT_STATE_PAUSING)
	{
		if (client->state & IC_PROXY_CLIENT_STATE_PAUSED)
			ic_proxy_client_read_data(client);

		client->state &= ~(IC_PROXY_CLIENT_STATE_PAUSING |
						   IC_PROXY_CLIENT_STATE_PAUSED);

		ic_proxy_log(LOG, "%s: resumed", ic_proxy_client_get_name(client));
	}
}
