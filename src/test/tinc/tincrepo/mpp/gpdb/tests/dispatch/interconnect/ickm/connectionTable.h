
#ifndef CONNECTIONTABLE_H
#define CONNECTIONTABLE_H

#include "ict.h"
#include <linux/spinlock.h>

#define DEFAULT_MAX_HT_SIZE 128
#define MAX_SEQUENCE_NUMBER 512

#define CONN_HASH_VALUE(connInfo) ((int)((((connInfo)->srcPid ^ (connInfo)->dstPid)) + (connInfo)->dstContentId))
#define CONN_HASH_MATCH(a, b) (((a)->motNodeId == (b)->motNodeId && \
				(a)->dstContentId == (b)->dstContentId && \
				(a)->srcContentId == (b)->srcContentId && \
				(a)->recvSliceIndex == (b)->recvSliceIndex && \
				(a)->sendSliceIndex == (b)->sendSliceIndex && \
				(a)->srcPid == (b)->srcPid &&			\
				(a)->dstPid == (b)->dstPid && (a)->icId == (b)->icId))


#define PACKET_TYPE_NUMBER (4)

struct pkt_control
{
	short		droptime[MAX_SEQUENCE_NUMBER];
};

/*The struct to hold the connection between the sender and receiver*/
struct connectionInfo
{
	struct icpkthdr icheader;
	/* fields used for kernel mode */
	/* short droptime[MAX_SEQUENCE_NUMBER]; */
	struct pkt_control *pkt_control_info[PACKET_TYPE_NUMBER];
	int			eos_droptime;

	/* next pointer if hash conflict */
	struct connectionInfo *next;
};

/*The data struct of the connection hash table*/
struct connHashTable
{
	struct connectionInfo **table;
	int			size;
};




int			initHT(struct connHashTable *ht);
void		destroyHT(struct connHashTable *ht);

struct connectionInfo *initConn(struct icpkthdr *header, short drop_times, int pkt_type);

int			insertToHT(struct connectionInfo *oneConn, struct connHashTable *ht);
struct connectionInfo *findInHT(struct icpkthdr *header, struct connHashTable *ht);
int			decreaseDroptime(struct connectionInfo *oneConn, struct connHashTable *ht, int seqnum);


#endif
