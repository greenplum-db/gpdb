/*-------------------------------------------------------------------------
 *
 * htupfifo.c
 *	   A FIFO queue for HeapTuples.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
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

#define HTF_FREELIST_MAX_LEN (10)

static void htfifo_cleanup(htup_fifo htf);

/*
 * Create and initialize a HeapTuple FIFO. The FIFO state is allocated
 * by this function, then initialized and returned.
 *
 * The HeapTuple FIFO must be given at least MIN_HTUPFIFO_KB bytes of
 * space to work with.  Anything less will be trapped as an
 * assert-failure.
 *
 * Parameters:
 *		max_mem_kb - The maximum memory size the FIFO can grow to, in
 *		kilobytes.  If the FIFO grows larger than this, an error is
 *		logged.
 *
 * XXX: THIS MEMORY PARAMETER IS COMPLETELY IGNORED!
 */
htup_fifo
htfifo_create(int max_mem_kb)
{
	htup_fifo	htf;

	htf = (htup_fifo) palloc(sizeof(htup_fifo_state));
	htfifo_init(htf, max_mem_kb);

	return htf;
}


/*
 * Initialize a HeapTuple FIFO.  The FIFO state is already allocated before
 * this function is called, but it is assumed that its contents are
 * uninitialized.
 *
 * The HeapTuple FIFO must be given at least MIN_HTUPFIFO_KB bytes of space
 * to work with.  Anything less will be trapped as an assert-failure.
 *
 * Parameters:
 *
 *	   htf - An uninitialized pointer to a htup_fifo_state structure.
 *
 * XXX: THIS MEMORY PARAMETER IS COMPLETELY IGNORED!
 *	   max_mem_kb - The maximum memory size the FIFO can grow to, in
 *		   kilobytes.  If the FIFO grows larger than this, an error is logged.
 */
void
htfifo_init(htup_fifo htf, int max_mem_kb)
{
	AssertArg(htf != NULL);
	AssertArg(max_mem_kb > MIN_HTUPFIFO_KB);

	htf->p_first = NULL;
	htf->p_last = NULL;

	htf->freelist = NULL;
	htf->freelist_count = 0;

	htf->tup_count = 0;
	htf->curr_mem_size = 0;
	htf->max_mem_size = max_mem_kb * 1024;
}


/*
 * Clean up a HeapTuple FIFO.  This frees all dynamically-allocated
 * contents of the FIFO, but not the FIFO state itself.
 */
static void
htfifo_cleanup(htup_fifo htf)
{
	GenericTuple tup;

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
	htf->freelist_count = 0;

	htf->p_first = NULL;
	htf->p_last = NULL;
	htf->tup_count = 0;
	htf->curr_mem_size = 0;
	htf->max_mem_size = 0;
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
htfifo_addtuple(htup_fifo htf, GenericTuple tup)
{
	htf_entry	p_ent;

	AssertArg(htf != NULL);
	AssertArg(tup != NULL);

	/* Populate the new entry. */
	if (htf->freelist != NULL)
	{
		p_ent = htf->freelist;
		htf->freelist = p_ent->p_next;
		htf->freelist_count = htf->freelist_count - 1;
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

	/* Update the FIFO state. */

	htf->tup_count++;
	htf->curr_mem_size += GetMemoryChunkSpace(p_ent) + GetMemoryChunkSpace(tup);
}


/*
 * Retrieve the next HeapTuple from the start of the FIFO. If the FIFO
 * is empty then NULL is returned.
 */
GenericTuple
htfifo_gettuple(htup_fifo htf)
{
	htf_entry	p_ent;
	GenericTuple tup;

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

		/* Update the FIFO state. */

		AssertState(htf->tup_count > 0);
		AssertState(htf->curr_mem_size > 0);

		htf->tup_count--;
		htf->curr_mem_size -=
			(GetMemoryChunkSpace(p_ent) + GetMemoryChunkSpace(tup));

		AssertState(htf->tup_count >= 0);
		AssertState(htf->curr_mem_size >= 0);

		/* Free the FIFO entry. */
		if (htf->freelist_count > HTF_FREELIST_MAX_LEN)
		{
			pfree(p_ent);
		}
		else
		{
			p_ent->p_next = htf->freelist;
			htf->freelist = p_ent;
			htf->freelist_count = htf->freelist_count + 1;
		}
	}
	else
	{
		/*
		 * No entries in FIFO.	Just do some sanity checks, but NULL is the
		 * result.
		 */
		AssertState(htf->tup_count == 0);
		AssertState(htf->curr_mem_size == 0);
		tup = NULL;
	}

	return tup;
}
