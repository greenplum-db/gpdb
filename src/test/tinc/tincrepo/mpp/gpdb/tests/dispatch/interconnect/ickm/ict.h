/*-------------------------------------------------------------------------
* ict.h
*       defines structure that is used in interconnect kernel module.
*       The structure is subject to change if structure with the same name
*       is changed in cdb2.
*       Refer to cdbinterconnect.h, ic_udp.c, ic_udpifc.c
*
* Copyright (c) 2012-Present Pivotal Software, Inc.
*
*-------------------------------------------------------------------------
*/
#ifndef ICT_H
#define ICT_H

#define DEBUG 1

#define UDPIC_FLAGS_RECEIVER_TO_SENDER  (1)
#define UDPIC_FLAGS_ACK					(2)
#define UDPIC_FLAGS_STOP				(4)
#define UDPIC_FLAGS_EOS					(8)
#define UDPIC_FLAGS_NAK					(16)
#define UDPIC_FLAGS_DISORDER    		(32)
#define UDPIC_FLAGS_DUPLICATE   		(64)
#define UDPIC_FLAGS_CAPACITY    		(128)

/* define different test function */
/*
#define DROP_DATA_PACKET  (1)
#define DROP_ACK_PACKET  (2)
#define DUPLICATE_PACKET (4)
#define OUT_OF_ORDER_PACKET (8)
*/
/* define packet type */
/*
#define	TYPE_DATA_PACKET (1)
#define	TYPE_NORMAL_ACK  (2)
#define	TYPE_MESSAGE_DUPLICATE (4)
#define	TYPE_ACK_DISORDER  (8)
#define	TYPE_MESSAGE_STOP  (16)
#define	TYPE_DATA_EOS_PACKET  (32)
#define	TYPE_DATA_STATUQUERY  (64)
*/

/* define different test scenarios */
#define DO_NOTHING  (0x0000)
#define DROP_PACKET  (0x0100)
#define DUPLICATE_PACKET (0x0200)
#define OUT_OF_ORDER_PACKET (0x0400)

#define DATA_PACKET (0x0001)
#define ACK_PACKET (0x0002)
#define EOS_PACKET (0x0008)
#define UNKNOWN_PACKET (0x0010)


#define FUNCTION_MASK 0xff00
#define PACKET_MASK 0x00ff
#define LOG_MAX_SIZE 300000
#define MAX_CONNECTION_SIZE 400
typedef int int32;
typedef unsigned int uint32;

struct icpkthdr
{
	int32		motNodeId;

	/*
	 * three pairs which seem useful for identifying packets.
	 *
	 * MPP-4194: It turns out that these can cause collisions; but the high
	 * bit (1<<31) of the dstListener port is now used for disambiguation with
	 * mirrors.
	 */

	int32		srcPid;
	int32		srcListenerPort;

	int32		dstPid;
	int32		dstListenerPort;

	int32		sessionId;
			  //for one psql session
	uint32		icId; //for one query

	int32		recvSliceIndex;
	int32		sendSliceIndex;
	int32		srcContentId; //segment ID
	int32		dstContentId; //segment ID

	/* MPP-6042: add CRC field */
	uint32		crc; //control msg and data

	/* packet specific info */
	int32		flags;
	int32		len; //include data and header

	/*
	 * The usage of seq and extraSeq field a) In a normal DATA packet seq
	 * -> the data packet sequence number extraSeq -> not used b) In a normal
	 * ACK message (UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY) seq      -> the
	 * largest seq of the continuously cached packets sometimes, it is
	 * special, for exampke, conn req ack, mismatch ack. extraSeq -> the
	 * largest seq of the consumed packets c) In a start race NAK message
	 * (UPDIC_FLAGS_NAK) seq      -> the seq from the pkt extraSeq -> the
	 * extraSeq from the pkt d) In a DISORDER message (UDPIC_FLAGS_DISORDER)
	 * seq      -> packet sequence number that triggers the disorder message
	 * extraSeq -> the largest seq of the received packets e) In a DUPLICATE
	 * message (UDPIC_FLAGS_DUPLICATE) seq      -> packet sequence number that
	 * triggers the duplicate message extraSeq -> the largest seq of the
	 * continuously cached packets f) In a stop messege (UDPIC_FLAGS_STOP |
	 * UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY) seq      -> the largest seq of
	 * the continuously cached packets extraSeq -> the largest seq of the
	 * continuously cached packets
	 *
	 *
	 * NOTE that: EOS/STOP flags are often saved in conn_info structure of a
	 * connection. It is possible for them to be sent together with other
	 * flags.
	 *
	 */
	uint32		seq;
	uint32		extraSeq;
};


#endif							/* ICT_H  */
