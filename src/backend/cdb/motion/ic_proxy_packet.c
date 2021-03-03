/*-------------------------------------------------------------------------
 *
 * ic_proxy_message.c
 *
 *    Interconnect Proxy Packet and Message
 *
 * Similar to the ic-udp, in ic-proxy mode we also need to transfer the data as
 * packets, the packet header contains all the necessary information to
 * identify the sender, the receiver, as well the sequence (session id, command
 * id, slice id).
 *
 * A message is a special kind of packet, it contains only the header, no
 * payload.
 *
 * Packets and messages are all allocated from the packet cache, they must be
 * freed with the ic_proxy_pkt_cache_free() function.
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "ic_proxy.h"
#include "ic_proxy_packet.h"
#include "ic_proxy_pkt_cache.h"

/*
 * Get the name string of a message type.
 */
const char *
ic_proxy_message_type_to_str(ICProxyMessageType type)
{
	switch (type)
	{
		case IC_PROXY_MESSAGE_DATA:
			return "DATA";
		case IC_PROXY_MESSAGE_PEER_HELLO:
			return "PEER HELLO";
		case IC_PROXY_MESSAGE_PEER_HELLO_ACK:
			return "PEER HELLO ACK";
		case IC_PROXY_MESSAGE_PEER_QUIT:
			return "PEER QUIT";
		case IC_PROXY_MESSAGE_HELLO:
			return "HELLO";
		case IC_PROXY_MESSAGE_HELLO_ACK:
			return "HELLO ACK";
		case IC_PROXY_MESSAGE_BYE:
			return "BYE";
		case IC_PROXY_MESSAGE_PAUSE:
			return "PAUSE";
		case IC_PROXY_MESSAGE_RESUME:
			return "RESUME";
		default:
			return "UNKNOWN";
	}
}

/*
 * Build a new message from the key.
 *
 * The returned packet must be freed with the ic_proxy_pkt_cache_free()
 * function.
 */
ICProxyPkt *
ic_proxy_message_new(ICProxyMessageType type, const ICProxyKey *key)
{
	ICProxyPkt *pkt = ic_proxy_pkt_cache_alloc(NULL);

	ic_proxy_message_init(pkt, type, key);

	return pkt;
}

/*
 * Initialize a message from the key.
 *
 * The pkt must be large enough to contain a packet header.
 */
void
ic_proxy_message_init(ICProxyPkt *pkt, ICProxyMessageType type,
					  const ICProxyKey *key)
{
	pkt->type = type;
	pkt->len = sizeof(*pkt);

	pkt->sessionId      = key->sessionId;
	pkt->commandId      = key->commandId;
	pkt->sendSliceIndex = key->sendSliceIndex;
	pkt->recvSliceIndex = key->recvSliceIndex;

	pkt->srcContentId   = key->localContentId;
	pkt->srcDbid        = key->localDbid;
	pkt->srcPid         = key->localPid;

	pkt->dstContentId   = key->remoteContentId;
	pkt->dstDbid        = key->remoteDbid;
	pkt->dstPid         = key->remotePid;
}

/*
 * Build a new packet.
 *
 * The data will also be copied to the packet.
 *
 * The returned packet must be freed with the ic_proxy_pkt_cache_free()
 * function.
 */
ICProxyPkt *
ic_proxy_pkt_new(const ICProxyKey *key, const void *data, uint16 size)
{
	ICProxyPkt *pkt;

	Assert(size + sizeof(*pkt) <= IC_PROXY_MAX_PKT_SIZE);

	pkt = ic_proxy_pkt_cache_alloc(NULL);
	ic_proxy_message_init(pkt, IC_PROXY_MESSAGE_DATA, key);

	memcpy(((char *) pkt) + sizeof(*pkt), data, size);
	pkt->len = sizeof(*pkt) + size;

	return pkt;
}

/*
 * Duplicate a packet.
 *
 * The returned packet must be freed with the ic_proxy_pkt_cache_free()
 * function.
 */
ICProxyPkt *
ic_proxy_pkt_dup(const ICProxyPkt *pkt)
{
	ICProxyPkt *newpkt;

	newpkt = ic_proxy_pkt_cache_alloc(NULL);
	memcpy(newpkt, pkt, pkt->len);

	return newpkt;
}

/*
 * Build a describe string of a packet.
 *
 * The string contains all the header information, can be used in log & error
 * messages.
 *
 * Return the string, which must not be freed.  The string is in a static
 * buffer, so a second call to this function will overwrite the result of the
 * previous call.
 */
const char *
ic_proxy_pkt_to_str(const ICProxyPkt *pkt)
{
	static char	buf[256];

	snprintf(buf, sizeof(buf),
			 "%s [con%d,cmd%d,slice[%hd->%hd] %hu bytes seg%hd:dbid%hu:p%d->seg%hd:dbid%hu:p%d]",
			 ic_proxy_message_type_to_str(pkt->type),
			 pkt->sessionId, pkt->commandId,
			 pkt->sendSliceIndex, pkt->recvSliceIndex,
			 pkt->len,
			 pkt->srcContentId, pkt->srcDbid, pkt->srcPid,
			 pkt->dstContentId, pkt->dstDbid, pkt->dstPid);

	return buf;
}

/*
 * Check whether a packet is from a client.
 *
 * The client is identified by the key.
 */
bool
ic_proxy_pkt_is_from_client(const ICProxyPkt *pkt, const ICProxyKey *key)
{
	return pkt->srcDbid        == key->localDbid
		&& pkt->srcPid         == key->localPid
		&& pkt->dstDbid        == key->remoteDbid
		&& pkt->dstPid         == key->remotePid
		&& pkt->sendSliceIndex == key->sendSliceIndex
		&& pkt->recvSliceIndex == key->recvSliceIndex
		;
}

/*
 * Check whether a packet is to a client.
 *
 * The client is identified by the key.
 */
bool
ic_proxy_pkt_is_to_client(const ICProxyPkt *pkt, const ICProxyKey *key)
{
	return pkt->dstDbid        == key->localDbid
		&& pkt->dstPid         == key->localPid
		&& pkt->srcDbid        == key->remoteDbid
		&& pkt->srcPid         == key->remotePid
		&& pkt->sendSliceIndex == key->sendSliceIndex
		&& pkt->recvSliceIndex == key->recvSliceIndex
		;
}

/*
 * Check whether a packet is live to a client.
 *
 * The client is identified by the key.
 *
 * A live packet has the same (sessionId, commandId) with the client.
 */
bool
ic_proxy_pkt_is_live(const ICProxyPkt *pkt, const ICProxyKey *key)
{
	return pkt->sessionId == key->sessionId
		&& pkt->commandId == key->commandId
		;
}

/*
 * Check whether a packet is out-of-date to a client.
 *
 * The client is identified by the key.
 *
 * A packet is out-of-date if
 *
 *     pkt.(sessionId, commandId) < client.(sessionId, commandId)
 */
bool
ic_proxy_pkt_is_out_of_date(const ICProxyPkt *pkt, const ICProxyKey *key)
{
	return ((pkt->sessionId <  key->sessionId) ||
			(pkt->sessionId == key->sessionId &&
			 pkt->commandId <  key->commandId));
}

/*
 * Check whether a packet is in the future to a client.
 *
 * The client is identified by the key.
 *
 * A packet is in the future if
 *
 *     pkt.(sessionId, commandId) > client.(sessionId, commandId)
 */
bool
ic_proxy_pkt_is_in_the_future(const ICProxyPkt *pkt, const ICProxyKey *key)
{
	return ((pkt->sessionId >  key->sessionId) ||
			(pkt->sessionId == key->sessionId &&
			 pkt->commandId >  key->commandId));
}
