/*-------------------------------------------------------------------------
 *
 * ic_proxy_server.h
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_SERVER_H
#define IC_PROXY_SERVER_H

#include "postgres.h"

#include <uv.h>

#include "ic_proxy.h"
#include "ic_proxy_iobuf.h"
#include "ic_proxy_packet.h"
#include "ic_proxy_router.h"

typedef struct ICProxyPeer ICProxyPeer;
typedef struct ICProxyClient ICProxyClient;
typedef struct ICProxyDelay ICProxyDelay;

/*
 * A peer is a connection to an other proxy.
 *
 * TODO: allocate the name buffer on demand.
 */
struct ICProxyPeer
{
	uv_tcp_t	tcp;			/* the libuv handle */

	int16		content;		/* content, aka segid, only for logging */
	uint16		dbid;			/* dbid is peerid */

	uint32		state;			/* or'ed bits of below ones */
#define IC_PROXY_PEER_STATE_CONNECTING                  0x00000001
#define IC_PROXY_PEER_STATE_CONNECTED                   0x00000002
#define IC_PROXY_PEER_STATE_ACCEPTED                    0x00000004
#define IC_PROXY_PEER_STATE_LEGACY                      0x00000008
#define IC_PROXY_PEER_STATE_SENDING_HELLO               0x00000010
#define IC_PROXY_PEER_STATE_SENT_HELLO                  0x00000020
#define IC_PROXY_PEER_STATE_RECEIVING_HELLO_ACK         0x00000040
#define IC_PROXY_PEER_STATE_RECEIVED_HELLO_ACK          0x00000080
#define IC_PROXY_PEER_STATE_RECEIVING_HELLO             0x00000100
#define IC_PROXY_PEER_STATE_RECEIVED_HELLO              0x00000200
#define IC_PROXY_PEER_STATE_SENDING_HELLO_ACK           0x00000400
#define IC_PROXY_PEER_STATE_SENT_HELLO_ACK              0x00000800
#define IC_PROXY_PEER_STATE_SHUTTING                    0x00001000
#define IC_PROXY_PEER_STATE_SHUTTED                     0x00002000
#define IC_PROXY_PEER_STATE_CLOSING                     0x00004000
#define IC_PROXY_PEER_STATE_CLOSED                      0x00008000

#define IC_PROXY_PEER_STATE_READY_FOR_MESSAGE \
	(IC_PROXY_PEER_STATE_CONNECTED | \
	 IC_PROXY_PEER_STATE_ACCEPTED)

#define IC_PROXY_PEER_STATE_READY_FOR_DATA \
	(IC_PROXY_PEER_STATE_SENT_HELLO_ACK | \
	 IC_PROXY_PEER_STATE_RECEIVED_HELLO_ACK)

	List	   *reqs;			/* outgoing queue for data that can't be sent
								 * immediately */

	ICProxyIBuf	ibuf;			/* ibuf detects the packet boundaries */

	char		name[128];		/* name of the client, only for logging */
};

/*
 * A delay is a c2p packet that cannot be sent immediately.
 *
 * The libuv callbacks are all called directly from the libuv mainloop, no
 * embedding, no nesting, this is important to ensure that all the callbacks
 * are __atomic__, they do not need to worry about that something is modified
 * concurrently.
 *
 * However most callbacks created by us are nested, for example for a pair of
 * loopback clients, they communicate via callbacks directly, if one of them
 * sends a control message to the other, it could cause reentrance of some of
 * the callback functions, which will cause unexpected behavior.  To prevent
 * that, we delay the sending of such kind of packets.
 *
 * A control message usually needs a callback, it is recorded for the delayed
 * packet, so once the packet is sent out the callback can still be triggered.
 *
 * Note that a delay can be transferred from a legacy peer to a new one, so we
 * must record the peer id instead of the peer pointer.
 */
struct ICProxyDelay
{
	int16		content;		/* the content of the peer, only for logging */
	uint16		dbid;			/* the dbid and peerid of the peer */

	ICProxyPkt *pkt;			/* the packet, owned by us */

	ic_proxy_sent_cb callback;	/* the sent callback */
	void	   *opaque;			/* the sent callback data */
};


extern int ic_proxy_server_main(void);
extern void ic_proxy_server_quit(uv_loop_t *loop, bool relaunch);

extern ICProxyClient *ic_proxy_client_new(uv_loop_t *loop, bool placeholder);
extern const char *ic_proxy_client_get_name(ICProxyClient *client);
extern uv_stream_t *ic_proxy_client_get_stream(ICProxyClient *client);
extern int ic_proxy_client_read_hello(ICProxyClient *client);
extern void ic_proxy_client_on_p2c_data(ICProxyClient *client, ICProxyPkt *pkt,
										ic_proxy_sent_cb callback,
										void *opaque);
extern ICProxyClient *ic_proxy_client_blessed_lookup(uv_loop_t *loop,
													 const ICProxyKey *key);
extern void ic_proxy_client_table_init(void);
extern void ic_proxy_client_table_uninit(void);
extern void ic_proxy_client_table_shutdown_by_dbid(uint16 dbid);

extern void ic_proxy_peer_table_init(void);
extern void ic_proxy_peer_table_uninit(void);

extern ICProxyPeer *ic_proxy_peer_new(uv_loop_t *loop,
									  int16 content, uint16 dbid);
extern void ic_proxy_peer_free(ICProxyPeer *peer);
extern void ic_proxy_peer_read_hello(ICProxyPeer *peer);
extern void ic_proxy_peer_connect(ICProxyPeer *peer, struct sockaddr_in *dest);
extern void ic_proxy_peer_route_data(ICProxyPeer *peer, ICProxyPkt *pkt,
									 ic_proxy_sent_cb callback, void *opaque);
extern ICProxyPeer *ic_proxy_peer_lookup(int16 content, uint16 dbid);
extern ICProxyPeer *ic_proxy_peer_blessed_lookup(uv_loop_t *loop,
												 int16 content, uint16 dbid);
extern ICProxyDelay *ic_proxy_peer_build_delay(ICProxyPeer *peer,
											   ICProxyPkt *pkt,
											   ic_proxy_sent_cb callback,
											   void *opaque);

#endif   /* IC_PROXY_SERVER_H */
