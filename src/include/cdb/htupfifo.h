/*-------------------------------------------------------------------------
 *
 * htupfifo.h
 *	   A FIFO queue for HeapTuples.
 *
 * NOTES:
 *	   This FIFO doesn't do any tuple-copying right now.  It may be that we
 *	   need to take memory-contexts into account in the future, and that
 *	   would mean we would possibly need to make copies of tuples when they
 *	   are added and/or when they are retrieved.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/htupfifo.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef HTUPFIFO_H
#define HTUPFIFO_H

#include "access/htup.h"

/* An entry in the HeapTuple FIFO.	Entries are formed into queues. */
typedef struct htf_entry_data
{
	/* The tuple itself. */
	GenericTuple tup;

	/* The next entry in the FIFO. */
	struct htf_entry_data *p_next;

}	htf_entry_data, *htf_entry;


/* A HeapTuple FIFO.  The FIFO is dynamically sizable, since there is no
 * specified upper bound on the number of entries in the FIFO.
 */
typedef struct htup_fifo_state
{
	htf_entry	p_first;
	htf_entry	p_last;

	htf_entry	freelist;
	int			freelist_count;

	/* A count of HeapTuples in the FIFO. */
	int			tup_count;

	/*
	 * An estimate of the current in-memory size of the FIFO's contents, in
	 * bytes.
	 */
	int			curr_mem_size;

	/*
	 * The maximum size that the FIFO is allowed to grow to, in bytes, before
	 * it will cause an error to be reported.
	 */
	uint32		max_mem_size;

}	htup_fifo_state, *htup_fifo;


#define MIN_HTUPFIFO_KB 8


extern htup_fifo htfifo_create(int max_mem_kb);
extern void htfifo_init(htup_fifo htf, int max_mem_kb);
extern void htfifo_destroy(htup_fifo htf);

extern void htfifo_addtuple(htup_fifo htf, GenericTuple htup);
extern GenericTuple htfifo_gettuple(htup_fifo htf);

#endif   /* HTUPFIFO_H */
