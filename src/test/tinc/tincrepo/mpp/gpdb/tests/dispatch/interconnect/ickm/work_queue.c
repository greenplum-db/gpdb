#include<linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

/* disorder move the phase 2. We can try use timer, and check
 * if a work_struct can be reused
 */

static atomic_t counter = ATOMIC_INIT(0);
static wait_queue_head_t my_waitqueue;
static struct workqueue_struct *my_workqueue;
int			i = 0;

typedef int (*resend_function) (struct sk_buff *);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
struct my_work_t
{
	struct delayed_work work;
	struct sk_buff *copy;
	resend_function func;

};
#else
struct my_work_t
{
	struct work_struct work;
	struct sk_buff *copy;
	resend_function func;

};
#endif

struct my_work_t ickm_work;
rwlock_t	wq_rwlock;
spinlock_t	ic_spin_lock;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void
work_func(struct work_struct *work)
{
	struct my_work_t *mywork;

	mywork = container_of((struct delayed_work *) work, struct my_work_t, work);
	if (mywork != NULL)
	{
		/* mywork->func(mywork->copy);  */
		/* kfree(mywork); */
	}
	if (atomic_dec_and_test(&counter))
	{

		wake_up(&my_waitqueue);

	}

}
#else
void
work_func(void *data)
{
	struct my_work_t *mywork;

	mywork = (struct my_work_t *) data;
	if (mywork != NULL)
	{
		/* mywork->func(mywork->copy);  */
		/* kfree(mywork); */
	}
	if (atomic_dec_and_test(&counter))
	{

		wake_up(&my_waitqueue);

	}

}
#endif



int			handle_out_of_order_packet(struct sk_buff *buff, int (*okfn) (struct sk_buff *), int timeout)
{

	struct sk_buff *new_skb = NULL;

	/*
	 * struct my_work_t * mywork; mywork=(struct
	 * my_work_t*)kmalloc(sizeof(struct my_work_t),GFP_ATOMIC); if (mywork ==
	 * NULL) { return NF_ACCEPT; } mywork->copy=buff; mywork->func=okfn; #if
	 * LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	 * INIT_DELAYED_WORK(&(mywork->work),work_func); #else
	 * INIT_WORK(&(mywork->work),work_func,mywork); #endif
	 * queue_delayed_work(my_workqueue,&(mywork->work),msecs_to_jiffies(timeout));
	 */

	if (!spin_trylock_bh(&ic_spin_lock))
	{
		return NF_ACCEPT;
	}
	/* write_lock(&wq_rwlock); */
	if (atomic_read(&counter))
	{
		/* write_unlock(&wq_rwlock); */
		spin_unlock_bh(&ic_spin_lock);
		return NF_ACCEPT;
	}
	new_skb = skb_copy(buff, GFP_KERNEL);
	/* Need flush work? */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	INIT_DELAYED_WORK(&(ickm_work.work), work_func);
#else
	INIT_WORK(&(ickm_work.work), work_func, &ickm_work);
#endif
	ickm_work.copy = new_skb;
	ickm_work.func = okfn;
	queue_delayed_work(my_workqueue, &(ickm_work.work), msecs_to_jiffies(timeout));
	atomic_inc(&counter);
	spin_unlock_bh(&ic_spin_lock);
	/* write_unlock(&wq_rwlock); */
	return NF_DROP;
}

int
init_disorder_env(void)
{

	init_waitqueue_head(&my_waitqueue);

	my_workqueue = create_workqueue("resend queue");
	/* rwlock_init(&wq_rwlock); */
	spin_lock_init(&ic_spin_lock);

	return 0;

}

int
destroy_disorder_env(void)
{
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(&my_waitqueue, &wait);

	if (atomic_read(&counter))
	{
		wait_event(my_waitqueue, 0 == atomic_read(&counter));
	}

	destroy_workqueue(my_workqueue);
	return 0;
}
