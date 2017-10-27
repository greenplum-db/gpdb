/*-------------------------------------------------------------------------
* udp_client.c
*       A sample program to send an interconnect packet.
*       Usage: 1) gcc -o udp_client.o udp_client.c 2) ./udp_client.o 127.0.0.1
*       Created: Oct.29 2012
*       Last modified:
* Copyright (c) 2006-2012, Greenplum inc
*-------------------------------------------------------------------------
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
/*
#include <linux/in.h>
#include <linux/types.h>
#include <linux/socket.h>
*/
#include "ict.h"

int
main(int argc, char **argv)
{
	int			sockfd;
	char	   *sendline = (char *) malloc(sizeof(icpkthdr));
	icpkthdr	icheader;

	icheader.motNodeId = 11;
	icheader.srcPid = 12;
	icheader.srcListenerPort = 13;
	icheader.dstPid = 14;
	icheader.dstListenerPort = 15;
	icheader.sessionId = 1090;
	icheader.icId = 106;
	icheader.recvSliceIndex = 107;
	icheader.sendSliceIndex = 108;
	icheader.srcContentId = 18989;
	icheader.dstContentId = 1971497;
	icheader.crc = 726476;
	icheader.flags = 791084;
	icheader.len = 980271;
	icheader.seq = 893747;
	icheader.extraSeq = 921441;

	memcpy(sendline, &icheader, sizeof(icpkthdr));

	struct sockaddr_in servaddr;

	if (argc != 2)
	{
		printf("usage: .o <IPaddress>\n");
		return 0;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(1056);
	inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	icpkthdr   *x = (struct icpkthdr *) sendline;

	printf("crc:%d\n", x->crc);
	printf("seg:%d\n", x->seq);

	sendto(sockfd, sendline, sizeof(icpkthdr), 0, (struct sockaddr *) &servaddr, sizeof(servaddr));

	return 0;
}
