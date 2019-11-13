/*--------------------------------------------------------------------------
 *
 * cdbcopy.c
 * 	 Provides routines that executed a COPY command on an MPP cluster. These
 * 	 routines are called from the backend COPY command whenever MPP is in the
 * 	 default dispatch mode.
 *
 * Usage:
 *
 * CdbCopy cdbCopy = makeCdbCopy();
 *
 * PG_TRY();
 * {
 *     cdbCopyStart(cdbCopy, ...);
 *
 *     // process each row
 *     while (...)
 *     {
 *         cdbCopyGetData(cdbCopy, ...)
 *         or
 *         cdbCopySendData(cdbCopy, ...)
 *     }
 *     cdbCopyEnd(cdbCopy);
 * }
 * PG_CATCH();
 * {
 *     cdbCopyAbort(cdbCopy);
 * }
 * PG_END_TRY();
 *
 *
 * makeCdbCopy() creates a struct to hold information about the on-going COPY.
 * It does not change the state of the connection yet.
 *
 * cdbCopyStart() puts the connections in the gang into COPY mode. If an error
 * occurs during or after cdbCopyStart(), you must call cdbCopyAbort() to reset
 * the connections to normal state!
 *
 * cdbCopyGetData() and cdbCopySendData() call libpq's PQgetCopyData() and
 * PQputCopyData(), respectively. If an error occurs, it is thrown with ereport().
 *
 * When you're done, call cdbCopyEnd().
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbcopy.c
*
*--------------------------------------------------------------------------
*/

#include "postgres.h"
#include "miscadmin.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbcopy.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "mb/pg_wchar.h"
#include "nodes/makefuncs.h"
#include "storage/pmsignal.h"
#include "tcop/tcopprot.h"
#include "utils/faultinjector.h"
#include "utils/memutils.h"

#include <poll.h>

static void cdbCopyEndInternal(CdbCopy *c, char *abort_msg,
				   int64 *total_rows_completed_p,
				   int64 *total_rows_rejected_p);

static Gang *
getCdbCopyPrimaryGang(CdbCopy *c)
{
	if (!c || !c->dispatcherState)
		return NULL;

	return (Gang *)linitial(c->dispatcherState->allocatedGangs);
}

/*
 * Create a cdbCopy object that includes all the cdb
 * information and state needed by the backend COPY.
 */
CdbCopy *
makeCdbCopy(CopyState cstate, bool is_copy_in)
{
	CdbCopy		*c;
	GpPolicy	*policy;

	policy = cstate->rel->rd_cdbpolicy;
	Assert(policy);

	c = palloc0(sizeof(CdbCopy));

	/* fresh start */
	c->total_segs = 0;
	c->copy_in = is_copy_in;
	c->seglist = NIL;
	c->dispatcherState = NULL;
	initStringInfo(&(c->copy_out_buf));

	/* init total_segs */
	c->aotupcounts = NULL;

	/*
	 * COPY replicated table TO file, pick only one replica, otherwise, duplicate
	 * rows will be copied.
	 */
	if (!is_copy_in && GpPolicyIsReplicated(policy) && !cstate->on_segment)
	{
		c->total_segs = 1;
		c->seglist = list_make1_int(gp_session_id % c->total_segs);
	}
	else
	{
		int			i;

		c->total_segs = policy->numsegments;

		for (i = 0; i < c->total_segs; i++)
			c->seglist = lappend_int(c->seglist, i);
	}

	cstate->cdbCopy = c;

	return c;
}


/*
 * starts a copy command on a specific segment database.
 *
 * may pg_throw via elog/ereport.
 */
void
cdbCopyStart(CdbCopy *c, CopyStmt *stmt,
			 PartitionNode *partitions, List *ao_segnos, int file_encoding)
{
	int			flags;

	stmt = copyObject(stmt);

	/* add in partitions for dispatch */
	stmt->partitions = partitions;

	/* add in AO segno map for dispatch */
	stmt->ao_segnos = ao_segnos;

	/*
	 * If the output needs to be in a different encoding, tell the segment.
	 * Normally, when we run normal queries, we keep the segment connections
	 * in database encoding, and do the encoding conversions in the QD, just
	 * before sending results to the client. But in COPY TO, we don't do
	 * any conversions to the data we receive from the segments, so they
	 * must produce the output in the correct encoding.
	 *
	 * We do this by adding "ENCODING 'xxx'" option to the options list of
	 * the CopyStmt that we dispatch.
	 */
	if (file_encoding != GetDatabaseEncoding())
	{
		bool		found;
		ListCell   *option;

		/*
		 * But first check if the encoding option is already in the options
		 * list (i.e the user specified it explicitly in the COPY command)
		 */
		found = false;
		foreach(option, stmt->options)
		{
			DefElem    *defel = (DefElem *) lfirst(option);

			if (strcmp(defel->defname, "encoding") == 0)
			{
				/*
				 * The 'file_encoding' came from the options, so they should match, but
				 * let's sanity-check.
				 */
				if (pg_char_to_encoding(defGetString(defel)) != file_encoding)
					elog(ERROR, "encoding option in original COPY command does not match encoding being dispatched");
				found = true;
			}
		}

		if (!found)
		{
			const char *encname = pg_encoding_to_char(file_encoding);

			stmt->options = lappend(stmt->options,
									makeDefElem("encoding",
												(Node *) makeString(pstrdup(encname))));
		}
	}

	flags = DF_WITH_SNAPSHOT | DF_CANCEL_ON_ERROR;
	if (c->copy_in)
		flags |= DF_NEED_TWO_PHASE;

	CdbDispatchCopyStart(c, (Node *) stmt, flags);

	SIMPLE_FAULT_INJECTOR("cdb_copy_start_after_dispatch");
}

/*
 * sends data to a copy command on all segments.
 */
void
cdbCopySendDataToAll(CdbCopy *c, const char *buffer, int nbytes)
{
	Gang	   *gp = getCdbCopyPrimaryGang(c);

	Assert(gp);

	for (int i = 0; i < gp->size; ++i)
	{
		int			seg = gp->db_descriptors[i]->segindex;

		cdbCopySendData(c, seg, buffer, nbytes);
	}
}

/*
 * sends data to a copy command on a specific segment (usually
 * the hash result of the data value).
 */
void
cdbCopySendData(CdbCopy *c, int target_seg, const char *buffer,
				int nbytes)
{
	SegmentDatabaseDescriptor *q;
	Gang	   *gp;
	int			result;

	/*
	 * NOTE!! note that another DELIM was added, for the buf_converted in the
	 * code above. I didn't do it because it's broken right now
	 */

	gp = getCdbCopyPrimaryGang(c);
	Assert(gp);
	q = getSegmentDescriptorFromGang(gp, target_seg);

	/* transmit the COPY data */
	result = PQputCopyData(q->conn, buffer, nbytes);

	if (result != 1)
	{
		if (result == 0)
		{
			/* We don't use blocking mode, so this shouldn't happen */
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("could not send COPY data to segment %d, attempt blocked",
							target_seg)));
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("could not send COPY data to segment %d: %s",
							target_seg, PQerrorMessage(q->conn))));
	}
}

/*
 * gets a chunk of rows of data from a copy command.
 * returns boolean true if done. Caller should still
 * empty the leftovers in the outbuf in that case.
 */
bool
cdbCopyGetData(CdbCopy *c, bool copy_cancel, uint64 *rows_processed)
{
	SegmentDatabaseDescriptor *q;
	Gang	   *gp;
	int			nbytes;

	/* clean out buf data */
	resetStringInfo(&c->copy_out_buf);

	gp = getCdbCopyPrimaryGang(c);

	/*
	 * MPP-7712: we used to issue the cancel-requests for each *row* we got
	 * back from each segment -- this is potentially millions of
	 * cancel-requests. Cancel requests consist of an out-of-band connection
	 * to the segment-postmaster, this is *not* a lightweight operation!
	 */
	if (copy_cancel)
	{
		ListCell   *cur;

		/* iterate through all the segments that still have data to give */
		foreach(cur, c->seglist)
		{
			int			source_seg = lfirst_int(cur);

			q = getSegmentDescriptorFromGang(gp, source_seg);

			/* send a query cancel request to that segdb */
			PQrequestCancel(q->conn);
		}
	}

	/*
	 * Collect data rows from the segments that still have rows to give until
	 * chunk minimum size is reached
	 */
	while (c->copy_out_buf.len < COPYOUT_CHUNK_SIZE)
	{
		ListCell   *cur;

		/* iterate through all the segments that still have data to give */
		foreach(cur, c->seglist)
		{
			int			source_seg = lfirst_int(cur);
			char	   *buffer;

			q = getSegmentDescriptorFromGang(gp, source_seg);

			/* get 1 row of COPY data */
			nbytes = PQgetCopyData(q->conn, &buffer, false);

			/*
			 * SUCCESS -- got a row of data
			 */
			if (nbytes > 0 && buffer)
			{
				/* append the data row to the data chunk */
				appendBinaryStringInfo(&(c->copy_out_buf), buffer, nbytes);

				/* increment the rows processed counter for the end tag */
				(*rows_processed)++;

				PQfreemem(buffer);
			}

			/*
			 * DONE -- Got all the data rows from this segment, or a cancel
			 * request.
			 *
			 * Remove the segment that completed sending data, from the list
			 * of in-progress segments.
			 *
			 * Note: After PQgetCopyData() returns -1, you need to call
			 * PGgetResult() to get any possible errors. But we don't do that
			 * here. That's done later, in the call to cdbCopyEnd() (or
			 * cdbCopyAbort(), if something went wrong.)
			 */
			else if (nbytes == -1)
			{
				c->seglist = list_delete_int(c->seglist, source_seg);

				if (list_length(c->seglist) == 0)
					return true;	/* all segments are done */

				/* start over from first seg as we just changed the seg list */
				break;
			}
			/*
			 * ERROR!
			 */
			else
			{
				/*
				 * should never happen since we are blocking. Don't bother to
				 * try again, exit with error.
				 */
				if (nbytes == 0)
					ereport(ERROR,
							(errcode(ERRCODE_IO_ERROR),
							 errmsg("could not send COPY data to segment %d, attempt blocked",
									source_seg)));

				if (nbytes == -2)
					ereport(ERROR,
							(errcode(ERRCODE_IO_ERROR),
							 errmsg("could not receive COPY data from segment %d: %s",
									source_seg, PQerrorMessage(q->conn))));
			}
		}

		if (c->copy_out_buf.len > COPYOUT_CHUNK_SIZE)
			break;
	}

	return false;
}

/*
 * Commands to end the cdbCopy.
 *
 * If an error occurrs, or if an error is reported by one of the segments,
 * cdbCopyEnd() throws it with ereport(), after closing the COPY and cleaning
 * up any resources associated with it.
 *
 * cdbCopyAbort() usually does not throw an error. It is used in error-recovery
 * codepaths, typically in a PG_CATCH() block, and the caller is about to
 * re-throw the original error that caused the abortion.
 */
void
cdbCopyAbort(CdbCopy *c)
{
	cdbCopyEndInternal(c, "aborting COPY in QE due to error in QD",
					   NULL, NULL);
}

/*
 * End the copy command on all segment databases,
 * and fetch the total number of rows completed by all QEs
 */
void
cdbCopyEnd(CdbCopy *c,
		   int64 *total_rows_completed_p,
		   int64 *total_rows_rejected_p)
{
	CHECK_FOR_INTERRUPTS();

	cdbCopyEndInternal(c, NULL,
					   total_rows_completed_p,
					   total_rows_rejected_p);
}

static void
cdbCopyEndInternal(CdbCopy *c, char *abort_msg,
				   int64 *total_rows_completed_p,
				   int64 *total_rows_rejected_p)
{
	Gang	   *gp;
	int			num_bad_connections = 0;
	int64		total_rows_completed = 0;	/* total num rows completed by all
											 * QEs */
	int64		total_rows_rejected = 0;	/* total num rows rejected by all
											 * QEs */
	ErrorData *first_error = NULL;
	int			seg;
	struct pollfd	*pollRead;
	bool		io_errors = false;
	StringInfoData io_err_msg;

	initStringInfo(&io_err_msg);

	/*
	 * Don't try to end a copy that already ended with the destruction of the
	 * writer gang. We know that this has happened if the CdbCopy's
	 * primary_writer is NULL.
	 *
	 * GPDB_91_MERGE_FIXME: ugh, this is nasty. We shouldn't be calling
	 * cdbCopyEnd twice on the same CdbCopy in the first place!
	 */
	gp = getCdbCopyPrimaryGang(c);
	if (!gp)
	{
		if (total_rows_completed_p != NULL)
			*total_rows_completed_p = 0;
		if (total_rows_rejected_p != NULL)
			*total_rows_completed_p = -1;
		return;
	}

	/*
	 * In COPY in mode, call PQputCopyEnd() to tell the segments that we're done.
	 */
	if (c->copy_in)
	{
		for (seg = 0; seg < gp->size; seg++)
		{
			SegmentDatabaseDescriptor *q = gp->db_descriptors[seg];
			int			result;

			elog(DEBUG1, "PQputCopyEnd seg %d    ", q->segindex);
			/* end this COPY command */
			result = PQputCopyEnd(q->conn, abort_msg);

			/* get command end status */
			if (result == -1)
			{
				/* error */
				appendStringInfo(&io_err_msg,
								 "Failed to send end-of-copy to segment %d: %s",
								 seg, PQerrorMessage(q->conn));
				io_errors = true;
			}
			if (result == 0)
			{
				/* attempt blocked */

				/*
				 * CDB TODO: Can this occur?  The libpq documentation says, "this
				 * case is only possible if the connection is in nonblocking
				 * mode... wait for write-ready and try again", i.e., the proper
				 * response would be to retry, not error out.
				 */
				appendStringInfo(&io_err_msg,
								 "primary segment %d, dbid %d, attempt blocked\n",
								 seg, q->segment_database_info->config->dbid);
				io_errors = true;
			}
		}
	}

	pollRead = (struct pollfd *) palloc(sizeof(struct pollfd));
	for (seg = 0; seg < gp->size; seg++)
	{
		SegmentDatabaseDescriptor *q = gp->db_descriptors[seg];
		int			result;
		PGresult   *res;
		int64		segment_rows_completed = 0; /* # of rows completed by this QE */
		int64		segment_rows_rejected = 0;	/* # of rows rejected by this QE */

		pollRead->fd = PQsocket(q->conn);
		pollRead->events = POLLIN;
		pollRead->revents = 0;

		while (PQisBusy(q->conn) && PQstatus(q->conn) == CONNECTION_OK)
		{
			if ((Gp_role == GP_ROLE_DISPATCH) && InterruptPending)
			{
				PQrequestCancel(q->conn);
			}

			if (poll(pollRead, 1, 200) > 0)
			{
				break;
			}
		}

		forwardQENotices();

		/*
		 * Fetch any error status existing on completion of the COPY command.
		 * It is critical that for any connection that had an asynchronous
		 * command sent thru it, we call PQgetResult until it returns NULL.
		 * Otherwise, the next time a command is sent to that connection, it
		 * will return an error that there's a command pending.
		 */
		HOLD_INTERRUPTS();
		while ((res = PQgetResult(q->conn)) != NULL && PQstatus(q->conn) != CONNECTION_BAD)
		{
			elog(DEBUG1, "PQgetResult got status %d seg %d    ",
				 PQresultStatus(res), q->segindex);

			forwardQENotices();

			/* if the COPY command had a data error */
			if (PQresultStatus(res) == PGRES_FATAL_ERROR)
			{
				/*
				 * Always append error from the primary. Append error from
				 * mirror only if its primary didn't have an error.
				 *
				 * For now, we only report the first error we get from the
				 * QE's.
				 *
				 * We get the error message in pieces so that we could append
				 * whoami to the primary error message only.
				 */
				if (!first_error)
					first_error = cdbdisp_get_PQerror(res);
			}

			/*
			 * If we are still in copy mode, tell QE to stop it.  COPY_IN
			 * protocol has a way to say 'end of copy' but COPY_OUT doesn't.
			 * We have no option but sending cancel message and consume the
			 * output until the state transition to non-COPY.
			 */
			if (PQresultStatus(res) == PGRES_COPY_IN)
			{
				elog(LOG, "Segment still in copy in, retrying the putCopyEnd");
				result = PQputCopyEnd(q->conn, NULL);
			}
			else if (PQresultStatus(res) == PGRES_COPY_OUT)
			{
				char	   *buffer = NULL;
				int			ret;

				elog(LOG, "Segment still in copy out, canceling QE");

				/*
				 * I'm a bit worried about sending a cancel, as if this is a
				 * success case the QE gets inconsistent state than QD.  But
				 * this code path is mostly for error handling and in a
				 * success case we wouldn't see COPY_OUT here. It's not clear
				 * what to do if this cancel failed, since this is not a path
				 * we can error out.  FATAL maybe the way, but I leave it for
				 * now.
				 */
				PQrequestCancel(q->conn);

				/*
				 * Need to consume data from the QE until cancellation is
				 * recognized. PQgetCopyData() returns -1 when the COPY is
				 * done, a non-zero result indicates data was returned and in
				 * that case we'll drop it immediately since we aren't
				 * interested in the contents.
				 */
				while ((ret = PQgetCopyData(q->conn, &buffer, false)) != -1)
				{
					if (ret > 0)
					{
						if (buffer)
							PQfreemem(buffer);
						continue;
					}

					/* An error occurred, log the error and break out */
					if (ret == -2)
					{
						ereport(WARNING,
								(errmsg("Error during cancellation: \"%s\"",
										PQerrorMessage(q->conn))));
						break;
					}
				}
			}

			/* in SREH mode, check if this seg rejected (how many) rows */
			if (res->numRejected > 0)
				segment_rows_rejected = res->numRejected;

			/*
			 * When COPY FROM ON SEGMENT, need to calculate the number of this
			 * segment's completed rows
			 */
			if (res->numCompleted > 0)
				segment_rows_completed = res->numCompleted;

			/* Get AO tuple counts */
			c->aotupcounts = PQprocessAoTupCounts(c->aotupcounts,
												  res->aotupcounts, res->naotupcounts);
			/* free the PGresult object */
			PQclear(res);
		}
		RESUME_INTERRUPTS();

		/*
		 * add up the number of rows completed and rejected from this segment
		 * to the totals. Only count from primary segs.
		 */
		if (segment_rows_rejected > 0)
			total_rows_rejected += segment_rows_rejected;
		if (segment_rows_completed > 0)
			total_rows_completed += segment_rows_completed;

		/* Lost the connection? */
		if (PQstatus(q->conn) == CONNECTION_BAD)
		{
			/* command error */
			io_errors = true;
			appendStringInfo(&io_err_msg,
							 "Primary segment %d, dbid %d, with error: %s\n",
							 seg, q->segment_database_info->config->dbid,
							 PQerrorMessage(q->conn));

			/* Free the PGconn object. */
			PQfinish(q->conn);
			q->conn = NULL;

			/* Let FTS deal with it! */
			num_bad_connections++;
		}
	}

	CdbDispatchCopyEnd(c);

	/* If lost contact with segment db, try to reconnect. */
	if (num_bad_connections > 0)
	{
		elog(LOG, "error occurred while ending COPY: %s", io_err_msg.data);
		elog(LOG, "COPY signals FTS to probe segments");

		SendPostmasterSignal(PMSIGNAL_WAKEN_FTS);
		DisconnectAndDestroyAllGangs(true);

		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 (errmsg("MPP detected %d segment failures, system is reconnected",
						 num_bad_connections))));
	}

	/*
	 * Unless we are aborting the COPY, report any errors with ereport()
	 */
	if (!abort_msg)
	{
		/* errors reported by the segments */
		if (first_error)
		{
			FlushErrorState();
			ReThrowError(first_error);
		}

		/* errors that occurred in the COPY itself */
		if (io_errors)
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("could not complete COPY on some segments"),
					 errdetail("%s", io_err_msg.data)));
	}

	if (total_rows_completed_p != NULL)
		*total_rows_completed_p = total_rows_completed;
	if (total_rows_rejected_p != NULL)
		*total_rows_rejected_p = total_rows_rejected;
	return;
}
