#include <linux/kernel.h>		/* Needed for KERN_INFO */
#include <linux/init.h>			/* Needed for the macros */
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/version.h>
#include <linux/slab.h>

#include "connectionTable.h"

extern volatile int counter;

/*
 * Initialize the connection hash table
 */
int
initHT(struct connHashTable *ht)
{
	int			i;

	ht->size = DEFAULT_MAX_HT_SIZE;
	/* GFP_ATOMIC|__GFP_ZERO does nit work in 2.6.18?? */
	ht->table = (struct connectionInfo **) kmalloc(ht->size * sizeof(struct connectionInfo *), GFP_ATOMIC);

	if (ht->table == NULL)
	{
		return 0;
	}
	for (i = 0; i < ht->size; i++)
		ht->table[i] = NULL;
	return 1;
}

/*
 * Initialize one connectionInfo
 */
/* pkt_type is from control_type */
struct connectionInfo *
initConn(struct icpkthdr *header, short drop_times, int pkt_type)
{
	int			i,
				j;

	struct connectionInfo *oneConn;

	if (header == NULL)
	{
		return NULL;
	}

	/*
	 * struct icpkthdr * temp=(struct icpkthdr *)kmalloc(sizeof(struct
	 * icpkthdr),GFP_ATOMIC);
	 */
	oneConn = (struct connectionInfo *) kmalloc(sizeof(struct connectionInfo), GFP_ATOMIC);
	if (oneConn == NULL)
	{
		return NULL;
	}
	memcpy(&(oneConn->icheader), header, sizeof(struct icpkthdr));

	oneConn->next = NULL;
	oneConn->eos_droptime = 0;

	for (i = 0; i < PACKET_TYPE_NUMBER; i++)
	{
		oneConn->pkt_control_info[i] = NULL;
		if ((pkt_type & DATA_PACKET) == i + 1 || (pkt_type & ACK_PACKET) == i + 1)
		{
			oneConn->pkt_control_info[i] = kmalloc(sizeof(struct pkt_control), GFP_ATOMIC);
			if (oneConn->pkt_control_info[i] == NULL)
			{
				return NULL;
			}
			for (j = 0; j < MAX_SEQUENCE_NUMBER; j++)
			{
				oneConn->pkt_control_info[i]->droptime[j] = drop_times;
			}
		}
	}
	if ((pkt_type & EOS_PACKET) > 0)
	{
		oneConn->eos_droptime = (int) drop_times;
	}

	return oneConn;
}



/*
 * Insert one connection to hash table
 * return value: 1 if insert successfully;
 * 		 0 if the connection already exist
 */
int
insertToHT(struct connectionInfo *oneConn, struct connHashTable *ht)
{

	int			hashcode = 0;
	struct connectionInfo *iter;

	hashcode = CONN_HASH_VALUE(&(oneConn->icheader)) % ht->size;
	iter = ht->table[hashcode];

	/* the hash node is empty, just fill the node with the connection */
	if (NULL == iter)
	{
		ht->table[hashcode] = oneConn;
		counter++;
/* 		printk("Successfully insert to position without conflict: %d\n",hashcode); */
		return 1;
	}

	/* the hash node is same as the connection */
	if (CONN_HASH_MATCH(&(oneConn->icheader), &(iter->icheader)))
	{
/* 		printk("The connection is already in the hash table: %d\n", hashcode); */
		return 0;
	}

	/*
	 * the hash node is not empty, follow the next pointer to find the last
	 * node
	 */
	while (iter->next != NULL)
	{
		/* the same connection already exists */
		if (CONN_HASH_MATCH(&(oneConn->icheader), &(iter->icheader)))
		{
/* 			printk("The connection is already in the hash table: %d\n", hashcode); */
			return 0;
		}
		else
			iter = iter->next;
	}

	/* put the connection info to the last position of the list */
	iter->next = oneConn;
	counter++;
	/* oneConn->next = NULL; */

/* 	printk("Successfully insert to position with conflict: %d\n",hashcode); */
	return 1;
}


/*
 * Try to find one connection in hash table
 * return value: the pointer to the connection in the hash table if found;
 * 		 NULL if not found
 */
struct connectionInfo *
findInHT(struct icpkthdr *header, struct connHashTable *ht)
{
	struct connectionInfo *iter = NULL;
	int			hashcode = 0;

	if (header == NULL)
	{
		return NULL;
	}

	hashcode = CONN_HASH_VALUE(header) % (ht->size);



	if (ht->table[hashcode] == NULL)
	{
/*  	    printk(KERN_INFO"hash value %d list is empty\n",hashcode); */
		return NULL;
	}

/*     printk(KERN_INFO"hash value %d list has at least one element\n",hashcode); */
	for (iter = ht->table[hashcode]; iter != NULL; iter = iter->next)
	{
/*         printk(KERN_INFO"search in list\n"); */
		if (CONN_HASH_MATCH(header, &(iter->icheader)))
		{
/* 			printk(KERN_INFO"and the  connection has existed\n"); */
			return iter;
		}
	}

/* 	printk(KERN_INFO"but the connection didn't exist\n"); */
	return NULL;
}



/*
 * Destroy the hash table
 */
void
destroyHT(struct connHashTable *ht)
{
	int			i,
				j;

	for (i = 0; i < ht->size; i++)
	{
		struct connectionInfo *trash;

		while (ht->table[i] != NULL)
		{
			trash = ht->table[i];
			ht->table[i] = trash->next;
			for (j = 0; j < PACKET_TYPE_NUMBER; j++)
			{
				if (trash->pkt_control_info[j] != NULL)
				{
					kfree(trash->pkt_control_info[j]);
				}
			}
			kfree(trash);
			/* printk("Destroy the hash node: %d\n",i); */
		}
	}
	kfree(ht->table);
	/* printk("Destroy finished.\n"); */
}
