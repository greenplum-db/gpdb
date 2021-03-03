/*-------------------------------------------------------------------------
 *
 * ic_proxy_key.c
 *
 *    Interconnect Proxy Key
 *
 * A key is actually a logical connection identifier, it identifies the sender,
 * the receiver, as well as the logical connection itself.
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "ic_proxy_key.h"

/*
 * Compares whether two keys are identical.
 *
 * Not all the attributes are compared.  Content ids are not compared as the
 * dbids are compared.  Session ids are not compared because pids are compared.
 */
bool
ic_proxy_key_equal(const ICProxyKey *key1, const ICProxyKey *key2)
{
	return key1->localDbid       == key2->localDbid
		&& key1->localPid        == key2->localPid
		&& key1->remoteDbid      == key2->remoteDbid
		&& key1->remotePid       == key2->remotePid
		&& key1->commandId       == key2->commandId
		&& key1->sendSliceIndex  == key2->sendSliceIndex
		&& key1->recvSliceIndex  == key2->recvSliceIndex
		;
}

/*
 * Hash function for keys.
 *
 * The logic is derived from CONN_HASH_VALUE(), however it is possible to build
 * a better with less collision.
 */
uint32
ic_proxy_key_hash(const ICProxyKey *key, Size keysize)
{
	return (key->localPid ^ key->remotePid) + key->remoteDbid + key->commandId;
}

/*
 * Equal function for keys.
 *
 * Return 0 for match, otherwise for no match
 */
int
ic_proxy_key_equal_for_hash(const ICProxyKey *key1,
							const ICProxyKey *key2, Size keysize)
{
	return !ic_proxy_key_equal(key1, key2);
}

/*
 * Initialize a key.
 *
 * All the fields are passed as arguments.
 */
void
ic_proxy_key_init(ICProxyKey *key,
				  int32 sessionId, uint32 commandId,
				  int16 sendSliceIndex, int16 recvSliceIndex,
				  int16 localContentId, uint16 localDbid, int32 localPid,
				  int16 remoteContentId, uint16 remoteDbid, int32 remotePid)
{
	key->sessionId = sessionId;
	key->commandId = commandId;
	key->sendSliceIndex = sendSliceIndex;
	key->recvSliceIndex = recvSliceIndex;

	key->localContentId = localContentId;
	key->localDbid = localDbid;
	key->localPid = localPid;

	key->remoteContentId = remoteContentId;
	key->remoteDbid = remoteDbid;
	key->remotePid = remotePid;
}

/*
 * Initialize a key from a peer-to-client packet.
 *
 * A peer-to-client packet is to a local client, it is usually from a remote
 * peer, but it is also possible to be from a different client on the same
 * segment.
 */
void
ic_proxy_key_from_p2c_pkt(ICProxyKey *key, const ICProxyPkt *pkt)
{
	ic_proxy_key_init(key, pkt->sessionId, pkt->commandId,
					  pkt->sendSliceIndex, pkt->recvSliceIndex,
					  pkt->dstContentId, pkt->dstDbid, pkt->dstPid,
					  pkt->srcContentId, pkt->srcDbid, pkt->srcPid);
}

/*
 * Initialize a key from a client-to-peer packet.
 *
 * A client-to-peer packet is from a local client, it is usually to a remote
 * peer, but it is also possible to be to a different client on the same
 * segment.
 */
void
ic_proxy_key_from_c2p_pkt(ICProxyKey *key, const ICProxyPkt *pkt)
{
	ic_proxy_key_init(key, pkt->sessionId, pkt->commandId,
					  pkt->sendSliceIndex, pkt->recvSliceIndex,
					  pkt->srcContentId, pkt->srcDbid, pkt->srcPid,
					  pkt->dstContentId, pkt->dstDbid, pkt->dstPid);
}

/*
 * Reverse the direction of a key.
 *
 * Convert a peer-to-client packet to a client-to-peer one, or the reverse.
 */
void
ic_proxy_key_reverse(ICProxyKey *key)
{
#define __swap(a, b) do { tmp = (a); (a) = (b); (b) = tmp; } while (0)

	int32		tmp;

	__swap(key->localContentId, key->remoteContentId);
	__swap(key->localDbid,      key->remoteDbid);
	__swap(key->localPid,       key->remotePid);

#undef __swap
}

/*
 * Build a describe string of a key.
 *
 * The string contains all the key information, can be used in log & error
 * messages.
 *
 * Return the string, which must not be freed.  The string is in a static
 * buffer, so a second call to this function will overwrite the result of the
 * previous call.
 */
const char *
ic_proxy_key_to_str(const ICProxyKey *key)
{
	static char	buf[256];

	snprintf(buf, sizeof(buf),
			 "[con%d,cmd%d,slice[%hd->%hd] seg%hd:dbid%hu:p%d->seg%hd:dbid%hu:p%d]",
			 key->sessionId, key->commandId,
			 key->sendSliceIndex, key->recvSliceIndex,
			 key->localContentId, key->localDbid, key->localPid,
			 key->remoteContentId, key->remoteDbid, key->remotePid);

	return buf;
}
