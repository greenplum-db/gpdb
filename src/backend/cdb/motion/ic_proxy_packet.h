/*-------------------------------------------------------------------------
 *
 * ic_proxy_packet.h
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_PACKET_H
#define IC_PROXY_PACKET_H

#include "postgres.h"

#include "cdb/cdbvars.h"

typedef struct ICProxyPkt ICProxyPkt;

/* we have to include it after the declaration of ICProxyPkt */
#include "ic_proxy_key.h"

#define IC_PROXY_MAX_PKT_SIZE (Gp_max_packet_size + sizeof(ICProxyPkt))
#define IC_PROXY_PKT_MAGIC_NUMBER (0xBADDCAFE)

typedef enum
{
	IC_PROXY_MESSAGE_DATA = 0,

	/* these are peer messages */
	IC_PROXY_MESSAGE_PEER_HELLO,
	IC_PROXY_MESSAGE_PEER_HELLO_ACK,
	IC_PROXY_MESSAGE_PEER_QUIT,

	/* these are client messages */
	IC_PROXY_MESSAGE_HELLO,
	IC_PROXY_MESSAGE_HELLO_ACK,
	IC_PROXY_MESSAGE_BYE,
	IC_PROXY_MESSAGE_DATA_ACK,
} ICProxyMessageType;

struct ICProxyPkt
{
	/* for the sanity check of package */
	uint32		magicNumber;

	uint16		len;
	uint16		type;

	int16		srcContentId;
	uint16		srcDbid;
	int32		srcPid;

	int16		dstContentId;
	uint16		dstDbid;
	int32		dstPid;

	int32		sessionId;
	uint32		commandId;
	int16		sendSliceIndex;
	int16		recvSliceIndex;
};

const char *ic_proxy_message_type_to_str(ICProxyMessageType type);

extern ICProxyPkt *ic_proxy_message_new(ICProxyMessageType type,
										const ICProxyKey *key);
extern void ic_proxy_message_init(ICProxyPkt *pkt,
								  ICProxyMessageType type,
								  const ICProxyKey *key);

extern ICProxyPkt *ic_proxy_pkt_new(const ICProxyKey *key,
									const void *data, uint16 size);
extern ICProxyPkt *ic_proxy_pkt_dup(const ICProxyPkt *pkt);
extern const char *ic_proxy_pkt_to_str(const ICProxyPkt *pkt);
extern bool ic_proxy_pkt_is_from_client(const ICProxyPkt *pkt,
										const ICProxyKey *key);
extern bool ic_proxy_pkt_is_to_client(const ICProxyPkt *pkt,
									  const ICProxyKey *key);
extern bool ic_proxy_pkt_is_live(const ICProxyPkt *pkt,
								 const ICProxyKey *key);
extern bool ic_proxy_pkt_is_out_of_date(const ICProxyPkt *pkt,
										const ICProxyKey *key);
extern bool ic_proxy_pkt_is_in_the_future(const ICProxyPkt *pkt,
										  const ICProxyKey *key);

static inline ICProxyMessageType
ic_proxy_message_get_type(const ICProxyPkt *pkt)
{
	return pkt->type;
}

static inline bool
ic_proxy_pkt_is(const ICProxyPkt *pkt, ICProxyMessageType type)
{
	Assert(pkt);
	Assert(pkt->len >= sizeof(*pkt));

	/* then check the message type on demand */
	return type == pkt->type;
}

/* check the validity of a package by magicNumber */
static inline bool
ic_proxy_pkt_is_valid(const ICProxyPkt *pkt)
{
	Assert(pkt);
	return pkt->magicNumber == IC_PROXY_PKT_MAGIC_NUMBER;
}

#endif   /* IC_PROXY_PACKET_H */
