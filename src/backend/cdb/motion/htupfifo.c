/*-------------------------------------------------------------------------
 *
 * htupfifo.c
 *	   A FIFO queue for HeapTuples.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/motion/htupfifo.c
 *
 * Reviewers: tkordas
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "utils/memutils.h"
#include "cdb/htupfifo.h"

static void htfifo_cleanup(htup_fifo htf);

/*
 * Create and initialize a HeapTuple FIFO. The FIFO state is allocated
 * by this function, then initialized and returned.
 */
htup_fifo
htfifo_create(void)
{
	htup_fifo	htf;

	htf = (htup_fifo) palloc(sizeof(htup_fifo_state));
	htf->p_first = NULL;
	htf->p_last = NULL;

	htf->freelist = NULL;

	return htf;
}

/*
 * Clean up a HeapTuple FIFO.  This frees all dynamically-allocated
 * contents of the FIFO, but not the FIFO state itself.
 */
static void
htfifo_cleanup(htup_fifo htf)
{
	MinimalTuple tup;

	AssertArg(htf != NULL);

	/* TODO:  This can be faster if we didn't reuse code, but this will work. */
	while ((tup = htfifo_gettuple(htf)) != NULL)
		pfree(tup);

	while (htf->freelist)
	{
		htf_entry	trash = htf->freelist;

		htf->freelist = trash->p_next;

		pfree(trash);
	}

	htf->p_first = NULL;
	htf->p_last = NULL;
}

/*
 * Clean up and free a HeapTuple FIFO. This frees both the
 * dynamically- allocated contents of the FIFO, and the FIFO's state.
 */
void
htfifo_destroy(htup_fifo htf)
{
	/*
	 * MPP-3910: race with cancel -- if we haven't been initialized, there is
	 * nothing to do.
	 */
	if (htf == NULL)
		return;

	htfifo_cleanup(htf);
	pfree(htf);
}


/*
 * Append the specified HeapTuple to the end of a HeapTuple FIFO.
 *
 * The HeapTuple must NOT be NULL.
 *
 * If the current memory usage of the FIFO exceeds the maximum specified at
 * init-time, then an error is flagged.
 */
void
htfifo_addtuple(htup_fifo htf, MinimalTuple tup)
{
	htf_entry	p_ent;

	AssertArg(htf != NULL);
	AssertArg(tup != NULL);

	/* Serialized tuple should never have external attribute */
	Assert(!(tup->t_infomask & HEAP_HASEXTERNAL));

	/* Populate the new entry. */
	if (htf->freelist != NULL)
	{
		p_ent = htf->freelist;
		htf->freelist = p_ent->p_next;
	}
	else
	{
		p_ent = (htf_entry) palloc(sizeof(htf_entry_data));
	}
	p_ent->tup = tup;
	p_ent->p_next = NULL;

	/* Put the new entry at the end of the FIFO. */
	if (htf->p_last != NULL)
	{
		AssertState(htf->p_first != NULL);
		htf->p_last->p_next = p_ent;
	}
	else
		/* htf->p_last == NULL */
	{
		AssertState(htf->p_first == NULL);
		htf->p_first = p_ent;
	}
	htf->p_last = p_ent;
}


/*
 * Retrieve the next tuple from the start of the FIFO. If the FIFO
 * is empty then NULL is returned.
 */
MinimalTuple
htfifo_gettuple(htup_fifo htf)
{
	htf_entry	p_ent;
	MinimalTuple tup;

	AssertArg(htf != NULL);

	/* Pull the first entry from the FIFO. */

	p_ent = htf->p_first;
	if (p_ent != NULL)
	{
		/* Got something.  Unhook the first entry from the list. */

		htf->p_first = p_ent->p_next;
		if (htf->p_first == NULL)
			htf->p_last = NULL;

		p_ent->p_next = NULL;	/* Just for the sake of completeness... */

		tup = p_ent->tup;
		AssertState(tup != NULL);

		/* Free the FIFO entry. */
		p_ent->p_next = htf->freelist;
		htf->freelist = p_ent;
	}
	else
	{
		/* No entries in FIFO. */
		tup = NULL;
	}

	return tup;
}
