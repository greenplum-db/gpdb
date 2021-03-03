/*-------------------------------------------------------------------------
 *
 * ic_proxy_key.h
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_KEY_H
#define IC_PROXY_KEY_H

#include "postgres.h"

typedef struct ICProxyKey ICProxyKey;

/* we have to include it after the declaration of ICProxyKey */
#include "ic_proxy_packet.h"

/*
 * XXX: sessionId and {local,remote}ContentId are only for debugging purpose,
 * they are not actually part of the key.
 *
 * dbid and segindex are both defined as int32 in type GpId, however we define
 * them as int16 to reduce the size of ICProxyKey, we also mark dbids as
 * unsigned in hope that the compiler could help to catch the errors if we pass
 * them in the wrong order.
 */
struct ICProxyKey
{
	int16		localContentId;
	uint16		localDbid;
	int32		localPid;

	int16		remoteContentId;
	uint16		remoteDbid;
	int32		remotePid;

	int32		sessionId;
	uint32		commandId;
	int16		sendSliceIndex;
	int16		recvSliceIndex;
};

extern uint32 ic_proxy_key_hash(const ICProxyKey *key, Size keysize);
extern bool ic_proxy_key_equal(const ICProxyKey *key1, const ICProxyKey *key2);
extern int ic_proxy_key_equal_for_hash(const ICProxyKey *key1,
									   const ICProxyKey *key2,
									   Size keysize);
extern void ic_proxy_key_init(ICProxyKey *key,
							  int32 sessionId, uint32 commandId,
							  int16 sendSliceIndex, int16 recvSliceIndex,
							  int16 localContentId, uint16 localDbid, int32 localPid,
							  int16 remoteContentId, uint16 remoteDbid, int32 remotePid);
extern void ic_proxy_key_from_p2c_pkt(ICProxyKey *key, const ICProxyPkt *pkt);
extern void ic_proxy_key_from_c2p_pkt(ICProxyKey *key, const ICProxyPkt *pkt);
extern void ic_proxy_key_reverse(ICProxyKey *key);
extern const char *ic_proxy_key_to_str(const ICProxyKey *key);

#endif   /* IC_PROXY_KEY_H */
