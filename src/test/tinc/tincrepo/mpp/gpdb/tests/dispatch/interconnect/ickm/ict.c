/*-------------------------------------------------------------------------
* ict.c
*       Implement kernel module to test interconnect with packet control.
*       Created: Oct.29 2012
*       Last modified:
*
* Copyright (c) 2012-Present Pivotal Software, Inc.
*
*-------------------------------------------------------------------------
*/

#include <linux/module.h>		/* Needed by all modules */
#include <linux/moduleparam.h>
#include <linux/kernel.h>		/* Needed for KERN_INFO */
#include <linux/init.h>			/* Needed for the macros */
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <net/ip.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <linux/random.h>

#include "ict.h"
#include "connectionTable.h"


MODULE_LICENSE("GPL");

/* following are parameters for kernel module */
/* usage: insmod ickm.ko ict_type=0x0101 seq_array=1,2,3,4,5 drop_times=3 */
static int	ict_type = 0x0;
static int	seq_array[MAX_SEQUENCE_NUMBER] = {0};	/* TODO */
static int	seq_argc = 0;		/* parameter count */
static short drop_times = 0x1;
static int	local_seq_arr[MAX_SEQUENCE_NUMBER];
short		debug = 0x0;
short		percent = 0;
static int	timeout = 0x0;
static short drop_all = 0;

module_param(ict_type, int, 0);
MODULE_PARM_DESC(ict_type, "Test type");
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout, "TIME OUT");
module_param(drop_times, short, 0);
MODULE_PARM_DESC(drop_times, "Packet drop times");
module_param_array(seq_array, int, &seq_argc, 0000);
MODULE_PARM_DESC(seq_array, "Packet sequence");
module_param(debug, short, 0);
MODULE_PARM_DESC(debug, "for debug");
module_param(percent, short, 0);
MODULE_PARM_DESC(percent, "drop percent");
module_param(drop_all, short, 0);
MODULE_PARM_DESC(drop_all, "drop all by percent");
/* end of parameters  */

static struct nf_hook_ops nfho_in;	/* local in hook  */
static struct nf_hook_ops nfho_out; /* local out hook  */

struct connHashTable HT;
static DEFINE_SPINLOCK(connectionTable_lock);
volatile int counter = 0;
char		log_buff[LOG_MAX_SIZE];
volatile long log_offset = 0;

static struct proc_dir_entry *entry;
extern struct file_operations my_proc_ops;
extern void init_seq_ops(void);
extern void init_proc_ops(void);

extern struct icpkthdr *get_ic_pkt_hdr(struct sk_buff *skb, int hook_num);
extern int	get_packet_type(struct icpkthdr *header, int hooknum);
extern unsigned int handle_drop_packet(struct icpkthdr *header, int droptimes, struct connHashTable *table, int control_type, int pkt_type);
extern void show_packet_info(struct icpkthdr *ic_header);
extern void show_time(void);
extern int	show_info(const char *type, const char *action, struct icpkthdr *header);

/*verify the seq in the array we input */
extern int	handle_out_of_order_packet(struct sk_buff *buff, int (*okfn) (struct sk_buff *), int timeout);
extern int	init_disorder_env(void);
extern int	destroy_disorder_env(void);
int			write_log(const char *type, const char *action, short drop_times, struct icpkthdr *header);

unsigned int
verify_seq(uint32 index)
{
	if (index > MAX_SEQUENCE_NUMBER - 1)
	{
		return 0;
	}
	if (1 == local_seq_arr[index])
	{
		return 1;
	}
	else
	{

		return 0;
	}

}


/* Please check definition of nfhook_fn in kernel, since it is different
   with kernel version, in skb is sk_buff ** or sk_buff *
*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static unsigned int
ic_hook_in(unsigned int hooknum,
		   struct sk_buff **skb,
		   const struct net_device *in,
		   const struct net_device *out,
		   int (*okfn) (struct sk_buff *))
#else
static unsigned int
ic_hook_in(unsigned int hooknum,
		   struct sk_buff *skb,
		   const struct net_device *in,
		   const struct net_device *out,
		   int (*okfn) (struct sk_buff *))

#endif
{

	struct icpkthdr *ic_header;
	int			pkt_type = 0;
	unsigned int rnum;

	/* struct connectionInfo * conn; */

	/* printk(KERN_INFO"\n\n\n"); */
	/* printk(KERN_INFO"************ LOCAL_IN ***************\n "); */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
	ic_header = get_ic_pkt_hdr(*skb, hooknum);
#else
	ic_header = get_ic_pkt_hdr(skb, hooknum);
#endif

	/**************
    ***************
    **************/
	if (NULL == ic_header)
	{

		return NF_ACCEPT;
	}

	if (drop_all == 1)
	{
		get_random_bytes(&rnum, sizeof(unsigned int));
		rnum = rnum % 10000;
		if (rnum < percent)
		{
			return NF_DROP;
		}
		else
		{
			return NF_ACCEPT;
		}
	}


	if ((debug == 1) && ((ict_type & FUNCTION_MASK) == OUT_OF_ORDER_PACKET) && (ic_header->seq >= 1) && (1 == local_seq_arr[ic_header->seq - 1]))
	{
		show_info("DBG", "SKIP", ic_header);

	}
	/* packet type match? */
	pkt_type = get_packet_type(ic_header, hooknum);
	if ((pkt_type & (ict_type & PACKET_MASK)) == 0)
	{
		if ((debug == 1) && ((ict_type & FUNCTION_MASK) == DROP_PACKET))
		{
			show_info("DBG", "SKIP", ic_header);
		}
		return NF_ACCEPT;
	}

	/* seq match? */
	if (((pkt_type & 0x0003) > 0) && percent == 0 && !verify_seq(ic_header->seq))
	{
		if ((debug == 1) && ((ict_type & FUNCTION_MASK) == DROP_PACKET))
		{
			show_info("DBG", "SKIP", ic_header);
		}
		return NF_ACCEPT;
	}

	switch (ict_type & FUNCTION_MASK)
	{

		case DROP_PACKET:
			if (counter >= MAX_CONNECTION_SIZE)
			{
				return NF_ACCEPT;

			}
			spin_lock_bh(&connectionTable_lock);
			if (handle_drop_packet(ic_header, drop_times, &HT, ict_type & PACKET_MASK, pkt_type) == NF_DROP)
			{
				spin_unlock_bh(&connectionTable_lock);
				return NF_DROP;
			}
			else
			{
				spin_unlock_bh(&connectionTable_lock);
				return NF_ACCEPT;
			}
			break;

		case DUPLICATE_PACKET:
			break;

		case OUT_OF_ORDER_PACKET:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
			write_log("OUT", "PUSE", 0, ic_header);
			return handle_out_of_order_packet(*skb, okfn, timeout);
#else
			write_log("OUT", "PUSE", 0, ic_header);
			return handle_out_of_order_packet(skb, okfn, timeout);
#endif
			break;

		case DO_NOTHING:
			return NF_ACCEPT;
			break;
	}
	/**************
***************
**************/

	return NF_ACCEPT;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static unsigned int
ic_hook_out(unsigned int hooknum,
			struct sk_buff **skb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn) (struct sk_buff *))
#else
static unsigned int
ic_hook_out(unsigned int hooknum,
			struct sk_buff *skb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn) (struct sk_buff *))
#endif
{

	struct icpkthdr *ic_header;

	/* printk(KERN_INFO"\n\n\n"); */
	/* printk(KERN_INFO"************ LOCAL_OUT ***************\n "); */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
	ic_header = get_ic_pkt_hdr(*skb, hooknum);
#else
	ic_header = get_ic_pkt_hdr(skb, hooknum);
#endif
	if (NULL == ic_header)
	{
		return NF_ACCEPT;
	}


	return NF_ACCEPT;
}

static int	__init
ic_init_module(void)
{

	int			i = seq_argc;

	memset(local_seq_arr, 0, sizeof(int) * MAX_SEQUENCE_NUMBER);
	for (i = 0; i < seq_argc; i++)
	{
		if (seq_array[i] >= MAX_SEQUENCE_NUMBER || seq_array[i] < 1)
		{
			return -1;
		}
		local_seq_arr[seq_array[i]] = 1;
	}
	if (percent < 0 || percent > 10000)
	{
		return -1;
	}

	if (drop_all < 0)
	{
		return -1;
	}
	if (drop_times < 1)
	{
		return -1;
	}

	memset(log_buff, 0, LOG_MAX_SIZE);
	log_offset += sprintf(log_buff, "Successfully inserted ic hacking module into kernel\n");
	log_offset += sprintf(log_buff + log_offset, "seq_argc:%d \n", seq_argc);
	log_offset += sprintf(log_buff + log_offset, "ict_type=0x%x drop_times=%d\n", ict_type, drop_times);

	/* init global hashtable for interconnect */
	if (!initHT(&HT))
	{
		return -1;
	}

	entry = create_proc_entry("ickmlog", 0, NULL);
	if (entry == NULL)
	{
		printk(KERN_INFO "Unable to create proc entry for module ickm");
		return -ENODEV;
	}
	init_seq_ops();
	init_proc_ops();
	if (entry)
		entry->proc_fops = &my_proc_ops;

	if ((ict_type & FUNCTION_MASK) == OUT_OF_ORDER_PACKET)
	{
		init_disorder_env();
	}

	nfho_in.hook = (nf_hookfn *) ic_hook_in;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	nfho_in.hooknum = NF_IP_LOCAL_IN;
#else
	nfho_in.hooknum = NF_INET_LOCAL_IN;
#endif
	nfho_in.pf = PF_INET;
	nfho_in.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_in);

	nfho_out.hook = (nf_hookfn *) ic_hook_out;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	nfho_out.hooknum = NF_IP_LOCAL_OUT; /* local out */
#else
	nfho_out.hooknum = NF_INET_LOCAL_OUT;	/* local out */
#endif
	nfho_out.pf = PF_INET;
	nfho_out.priority = NF_IP_PRI_FIRST;
	/* nf_register_hook(&nfho_out); */

	printk(KERN_INFO "Successfully inserted the ic hacking module.\n");

	return 0;
}

static void __exit
ic_cleanup_module(void)
{
	nf_unregister_hook(&nfho_in);
	/* nf_unregister_hook(&nfho_out); */
	if ((ict_type & FUNCTION_MASK) == OUT_OF_ORDER_PACKET)
	{
		destroy_disorder_env();
	}

	printk(KERN_INFO "Trying to unload the ic hacking module.\n");
	remove_proc_entry("ickmlog", NULL);
	destroyHT(&HT);
	printk(KERN_INFO "used connection:%d\n", counter);

	printk(KERN_INFO "Successfully unloaded the ic hacking module.\n");
}

module_init(ic_init_module);
module_exit(ic_cleanup_module);
