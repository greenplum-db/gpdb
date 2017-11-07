/*-------------------------------------------------------------------------
* log.c
* 	Functions to log debug messages.
*
* Copyright (c) 2012-Present Pivotal Software, Inc.
*
*-------------------------------------------------------------------------
*/

#include "ict.h"
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>

extern char log_buff[LOG_MAX_SIZE];
extern volatile long log_offset;
struct seq_operations my_seq_ops;
struct file_operations my_proc_ops;
static DEFINE_SPINLOCK(log_lock);



static void *
my_seq_start(struct seq_file *s, loff_t * pos)
{

	/* printk("my_seq_start\n");  */
	/* printk(KERN_INFO"enter return  %d\n",*pos); */
	if ((*pos) <= log_offset / 1024)
	{
		return (void *) ((unsigned long) *pos + 1);;
	}
	else
		return NULL;

}

void *
my_seq_next(struct seq_file *s, void *v, loff_t * pos)
{
	/* printk(KERN_INFO"next \n"); */
	return NULL;

}
void
my_seq_stop(struct seq_file *s, void *v)
{
	/* printk(KERN_INFO"==scull_seq_stop() enter\n");  */
	return;

}
int
my_seq_show(struct seq_file *s, void *v)
{
	unsigned long i,
				temp;

	i = (unsigned long) v - 1;
	/* printk(KERN_INFO"my_seq_show() enter *d\n",i); */
	if (i == 0)
	{
		seq_printf(s, "Type  Action  Seq  Drop  MoteId  D/S_Segment D/S_Worker\n");
	}
	for (temp = i * 1024; (temp < log_offset && temp < (i + 1) * 1024); temp++)
	{
		if (temp < LOG_MAX_SIZE)
		{
			seq_putc(s, *(log_buff + temp));
		}
	}
	return 0;
}


int
my_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &my_seq_ops);
}

void
init_seq_ops(void)
{
	my_seq_ops.start = my_seq_start;
	my_seq_ops.next = my_seq_next;
	my_seq_ops.stop = my_seq_stop;
	my_seq_ops.show = my_seq_show;
}

void
init_proc_ops(void)
{
	my_proc_ops.open = my_proc_open;
	my_proc_ops.read = seq_read;
	my_proc_ops.llseek = seq_lseek;
	my_proc_ops.release = seq_release;
};

int
write_log(const char *type, const char *action, short drop_times, struct icpkthdr *header)
{
	int			i = 0;

	if (header == NULL)
	{
		return 0;
	}
	spin_lock(&log_lock);
	i = snprintf(log_buff + log_offset, LOG_MAX_SIZE - log_offset, "%3s  %4s   % 4d  % 4d  % 4d     <% 5d,% 5d>   <% 6d,% 6d>\n", type, action, header->seq, drop_times, \
				 header->motNodeId, header->dstContentId, header->srcContentId, \
				 header->dstPid, header->srcPid);
	log_offset += i;
	/* printk(KERN_INFO"log size :%d\n",i); */

	spin_unlock(&log_lock);
	return 0;
}

/* type: "DBG", "DUP", "DIS", "D/A", "STO"
   action: "DROP", "SKIP"
*/
int
show_info(const char *type, const char *action, struct icpkthdr *header)
{
	int			i = 0;

	if (header == NULL)
	{
		return 0;
	}
	spin_lock(&log_lock);
	i = snprintf(log_buff + log_offset, LOG_MAX_SIZE - log_offset, "%3s  %4s   % 4d  % 4d  % 4d     <% 5d,% 5d>   <% 6d,% 6d>\n", type, action, header->seq, header->flags, \
				 header->motNodeId, header->dstContentId, header->srcContentId, \
				 header->dstPid, header->srcPid);
	log_offset += i;

	spin_unlock(&log_lock);
	return 0;
}
