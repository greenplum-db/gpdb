/*-------------------------------------------------------------------------
* pkt_control.c
*
* Copyright (c) 2012-Present Pivotal Software, Inc.
*
*-------------------------------------------------------------------------
*/
#include <linux/kernel.h>		/* Needed for KERN_INFO */
#include <linux/init.h>			/* Needed for the macros */
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/version.h>
#include <linux/slab.h>

#include "connectionTable.h"
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/random.h>


extern char log_buff[];
extern long log_offset;
extern int	write_log(const char *, const char *, short, struct icpkthdr *);
extern short percent;

void
show_time(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);
	printk("%ld : %ld\n", tv.tv_sec, tv.tv_usec);
}


void
show_packet_info(struct icpkthdr *ic_header)
{
	if (NULL == ic_header)
	{
		return;
	}
/* #if DEBUG > 0  */
/*     show_time(); */
	printk(KERN_INFO "**src contentID**  %d  **dst contentId**  %d\n", ic_header->srcContentId, ic_header->dstContentId);
	printk(KERN_INFO "**nodeId**  %d \n**srcPid** %d \n**dstPid** %d\n", ic_header->motNodeId, ic_header->srcPid, ic_header->dstPid);
	printk(KERN_INFO "**srcPort** %d \n**dstPort** %d \n**icid**%d \n", ic_header->srcListenerPort, ic_header->dstListenerPort, ic_header->icId);

	printk(KERN_INFO "**rcvSlice** %d \n**sendSlice** %d \n**flags** %d \n**len** %d\n", ic_header->recvSliceIndex, ic_header->sendSliceIndex, ic_header->flags, ic_header->len);
	printk(KERN_INFO "**seq** %d \n**extraSeq** %d \n", ic_header->seq, ic_header->extraSeq);
/* #endif */

}

/* get interconnect packet header */
struct icpkthdr *
get_ic_pkt_hdr(struct sk_buff *skb, int hook_num)
{
	struct udphdr *udp_header;
	struct icpkthdr *ic_header;
	struct iphdr *ip_header;

	if (!skb)
	{
		/* printk(KERN_INFO "skb_buff is null \n"); */
		return NULL;
	}

	/* skb->head + skb->network_header; not avail in 2.6.18 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	ip_header = (struct iphdr *) (skb->nh.iph);
#else
	ip_header = (struct iphdr *) skb_network_header(skb);
#endif
	if (ip_header == NULL)
	{
		/* printk(KERN_INFO"ip header is null\n"); */
		/* printk(KERN_INFO"*****************END******************\n"); */
		return NULL;
	}

	if (ip_header->protocol != IPPROTO_UDP)
	{
		if (DEBUG)
		{
			/* printk(KERN_INFO "**OTHER PROTOCOL**\n"); */
			/* printk(KERN_INFO"*****************END******************\n"); */
		}
		return NULL;
	}

	/* check interconnect packet from packet length */
	if (htons(ip_header->tot_len) < sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct icpkthdr))
	{
		if (DEBUG)
		{
			/* printk(KERN_INFO"**PACKET LENGTH <92**\n"); */
			/* printk(KERN_INFO"*****************END******************\n"); */
		}
		return NULL;
	}

	/* printk(KERN_INFO "**PACKET IP ID**	%d\n", ip_header->id); */

	udp_header = (struct udphdr *) (ip_header + 1);
	ic_header = (struct icpkthdr *) (udp_header + 1);



	/* check interconnect packet from the valid of fileds */
	if (ic_header->motNodeId >= 256 ||
		ic_header->dstListenerPort >= 65536 || ic_header->srcListenerPort >= 65536 ||
		ic_header->recvSliceIndex >= 256 || ic_header->sendSliceIndex >= 256 ||
		ic_header->flags >= 256
		)
	{

		return NULL;
	}

	/* printk(KERN_INFO " <  hook num  >   %d\n ", hook_num); */

	/*
	 * printk(KERN_INFO"< flag >   %d   <  seqs>
	 * %d\n",ic_header->flags,ic_header->seq);
	 */

/*  printk(KERN_INFO "**PACKET IP ID**	%d\n", ip_header->id); */

	return ic_header;
}


 /* function to get the type of the packet */
int
get_packet_type(struct icpkthdr *header, int hooknum)
{

	int			flags;

	if (NULL == header)
	{
		return NF_ACCEPT;
	}

	flags = header->flags;
	if (0 == flags)
	{
		return DATA_PACKET;
	}

	if (!(flags & UDPIC_FLAGS_RECEIVER_TO_SENDER))
	{							/* sender to receiver */

		if (flags & UDPIC_FLAGS_EOS)
		{

			return EOS_PACKET;

		}
		else if (flags & UDPIC_FLAGS_CAPACITY)
		{

			return UNKNOWN_PACKET;

		}

	}
	else
	{							/* receiver to sender */


		if (flags & UDPIC_FLAGS_STOP)
		{
			write_log("STO", "SKIP", 0, header);
			return UNKNOWN_PACKET;

		}
		else if (flags & UDPIC_FLAGS_DISORDER)
		{
			write_log("DIS", "SKIP", 0, header);
			return UNKNOWN_PACKET;

		}
		else if (flags & UDPIC_FLAGS_DUPLICATE)
		{
			write_log("DUP", "SKIP", 0, header);
			return UNKNOWN_PACKET;

		}
		else if (flags & UDPIC_FLAGS_EOS)
		{

			return UNKNOWN_PACKET;
		}
		else
		{
			return ACK_PACKET;
		}
	}
	return 0;
}


/*function to drop packet*/
unsigned int
handle_drop_packet(struct icpkthdr *header, short droptimes, struct connHashTable *table, int control_type, int pkt_type)
{

	struct connectionInfo *Conn;
	struct pkt_control *pcon = NULL;
	unsigned int rnum;

	if (NULL == header)
	{
		return NF_ACCEPT;
	}
	/* if drop packets in a percentage, we do not need create connection */
	if (percent > 0)
	{
		get_random_bytes(&rnum, sizeof(unsigned int));
		if (rnum % 10000 < percent)
			return NF_DROP;
		return NF_ACCEPT;
	}
	/* find the packet in HashTable */
	Conn = findInHT(header, table);


	if (Conn)
	{							/* connection aready exsit */
		switch (pkt_type)
		{
			case DATA_PACKET:
			case ACK_PACKET:
				pcon = Conn->pkt_control_info[pkt_type - 1];
				if (pcon != NULL && (pcon->droptime[header->seq] > 0))
				{				/* decrease the drop times */
					write_log("D/A", "DROP", droptimes - pcon->droptime[header->seq], header);
					pcon->droptime[header->seq]--;
					return NF_DROP;
				}
				else
				{
					write_log("D/A", "SKIP", droptimes - pcon->droptime[header->seq], header);
					return NF_ACCEPT;
				}
				break;
			case EOS_PACKET:
				if (Conn->eos_droptime > 0)
				{
					Conn->eos_droptime--;
					return NF_DROP;
				}
				else
				{
					return NF_ACCEPT;
				}
				break;


			default:
				return NF_ACCEPT;
				break;
		}


	}
	else
	{							/* connection not exist */

		/* use control type to init */
		Conn = initConn(header, droptimes, control_type);	/* create new connection */

		if (NULL == Conn)
		{
			return NF_ACCEPT;
		}
		/* insert to the hash table */
		if (0 == insertToHT(Conn, table))
		{
			return NF_ACCEPT;
		}

		switch (pkt_type)
		{
			case DATA_PACKET:
			case ACK_PACKET:
				pcon = Conn->pkt_control_info[pkt_type - 1];
				if (pcon != NULL && (pcon->droptime[header->seq] > 0))
				{				/* decrease the drop times */
					write_log("D/A", "DROP", droptimes - pcon->droptime[header->seq], header);
					pcon->droptime[header->seq]--;
					return NF_DROP;
				}
				else
				{
					write_log("D/A", "SKIP", droptimes - pcon->droptime[header->seq], header);
					return NF_ACCEPT;
				}
				break;
			case EOS_PACKET:
				if (Conn->eos_droptime > 0)
				{
					Conn->eos_droptime--;
					return NF_DROP;
				}
				else
				{
					return NF_ACCEPT;
				}
				break;

			default:
				return NF_ACCEPT;
				break;
		}

	}
}
