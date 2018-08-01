/*--------------------------------------------------------------------------
 *
 * cdbcopy.c
 * 	 Provides routines that executed a COPY command on an MPP cluster. These
 * 	 routines are called from the backend COPY command whenever MPP is in the
 * 	 default dispatch mode.
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
#include "storage/pmsignal.h"
#include "tcop/tcopprot.h"
#include "utils/faultinjector.h"
#include "utils/memutils.h"

#include <poll.h>

/*
 * Create a cdbCopy object that includes all the cdb
 * information and state needed by the backend COPY.
 */
CdbCopy *
makeCdbCopy(bool is_copy_in)
{
	CdbCopy    *c;
	int			seg;

	c = palloc(sizeof(CdbCopy));

	/* fresh start */
	c->total_segs = 0;
	c->primary_writer = NULL;
	c->io_errors = false;
	c->copy_in = is_copy_in;
	c->skip_ext_partition = false;
	c->outseglist = NIL;
	c->partitions = NULL;
	c->ao_segnos = NIL;
	c->hasReplicatedTable = false;
	initStringInfo(&(c->err_msg));
	initStringInfo(&(c->err_context));
	initStringInfo(&(c->copy_out_buf));

	/* init total_segs */
	c->total_segs = getgpsegmentCount();
	c->aotupcounts = NULL;

	Assert(c->total_segs >= 0);

	/* Initialize the state of each segment database */
	c->segdb_state = (SegDbState **) palloc((c->total_segs) * sizeof(SegDbState *));

	for (seg = 0; seg < c->total_segs; seg++)
	{
		c->segdb_state[seg] = (SegDbState *) palloc(2 * sizeof(SegDbState));
		c->segdb_state[seg][0] = SEGDB_IDLE;	/* Primary can't be OUT */
	}

	/* init gangs */
	c->primary_writer = AllocateWriterGang();

	/* init seg list for copy out */
	if (!c->copy_in)
	{
		int			i;

		for (i = 0; i < c->total_segs; i++)
			c->outseglist = lappend_int(c->outseglist, i);
	}

	return c;
}


/*
 * starts a copy command on a specific segment database.
 *
 * may pg_throw via elog/ereport.
 */
void
cdbCopyStart(CdbCopy *c, CopyStmt *stmt, struct GpPolicy *policy)
{
	int			seg;

	/* clean err message */
	c->err_msg.len = 0;
	c->err_msg.data[0] = '\0';
	c->err_msg.cursor = 0;

	stmt = copyObject(stmt);

	/* add in partitions for dispatch */
	stmt->partitions = c->partitions;

	/* add in AO segno map for dispatch */
	stmt->ao_segnos = c->ao_segnos;

	stmt->skip_ext_partition = c->skip_ext_partition;

	if (policy)
	{
		stmt->policy = GpPolicyCopy(CurrentMemoryContext, policy);
	}
	else
	{
		stmt->policy = createRandomPartitionedPolicy(NULL);
	}

	CdbDispatchUtilityStatement((Node *) stmt,
								(c->copy_in ? DF_NEED_TWO_PHASE | DF_WITH_SNAPSHOT : DF_WITH_SNAPSHOT) | DF_CANCEL_ON_ERROR,
								NIL,
								NULL);

	SIMPLE_FAULT_INJECTOR(CdbCopyStartAfterDispatch);

	/* fill in CdbCopy structure */
	for (seg = 0; seg < c->total_segs; seg++)
	{
		c->segdb_state[seg][0] = SEGDB_COPY;	/* we be jammin! */
	}
}

/*
 * sends data to a copy command on all segments.
 */
void
cdbCopySendDataToAll(CdbCopy *c, const char *buffer, int nbytes)
{
	Gang	   *gp = c->primary_writer;

	for (int i = 0; i < gp->size; ++i)
	{
		int			seg = gp->db_descriptors[i].segindex;

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

	/* clean err message */
	c->err_msg.len = 0;
	c->err_msg.data[0] = '\0';
	c->err_msg.cursor = 0;

	/*
	 * NOTE!! note that another DELIM was added, for the buf_converted in the
	 * code above. I didn't do it because it's broken right now
	 */

	gp = c->primary_writer;

	q = getSegmentDescriptorFromGang(gp, target_seg);

	/* transmit the COPY data */
	result = PQputCopyData(q->conn, buffer, nbytes);

	if (result != 1)
	{
		if (result == 0)
			appendStringInfo(&(c->err_msg),
							 "Failed to send data to segment %d, attempt blocked\n",
							 target_seg);

		if (result == -1)
			appendStringInfo(&(c->err_msg),
							 "Failed to send data to segment %d: %s\n",
							 target_seg, PQerrorMessage(q->conn));

		c->io_errors = true;
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
	PGresult   *res;
	int			nbytes;
	bool		first_error = true;

	/* clean err message */
	c->err_msg.len = 0;
	c->err_msg.data[0] = '\0';
	c->err_msg.cursor = 0;

	/* clean out buf data */
	c->copy_out_buf.len = 0;
	c->copy_out_buf.data[0] = '\0';
	c->copy_out_buf.cursor = 0;

	gp = c->primary_writer;

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
		foreach(cur, c->outseglist)
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
		foreach(cur, c->outseglist)
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
			 */
			else if (nbytes == -1)
			{
				/*
				 * Fetch any error status existing on completion of the COPY
				 * command.
				 */
				while (NULL != (res = PQgetResult(q->conn)))
				{
					/* if the COPY command had an error */
					if (PQresultStatus(res) == PGRES_FATAL_ERROR && first_error)
					{
						appendStringInfo(&(c->err_msg), "Error from segment %d: %s\n",
										 source_seg, PQresultErrorMessage(res));
						first_error = false;
					}

					if (res->numCompleted > 0)
					{
						*rows_processed += res->numCompleted;
					}

					/* free the PGresult object */
					PQclear(res);
				}

				/*
				 * remove the segment that completed sending data from the
				 * list
				 */
				c->outseglist = list_delete_int(c->outseglist, source_seg);

				/* no more segments with data on the list. we are done */
				if (list_length(c->outseglist) == 0)
					return true;	/* done */

				/* start over from first seg as we just changes the seg list */
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
					appendStringInfo(&(c->err_msg),
									 "Failed to get data from segment %d, attempt blocked\n",
									 source_seg);

				if (nbytes == -2)
				{
					/* GPDB_91_MERGE_FIXME: How should we handle errors here? We used
					 * to append them to err_msg, but that doesn't seem right. Surely
					 * we should ereport()? I put in just a quick elog() for now..
					 */
					elog(ERROR, "could not dispatch COPY: %s", PQerrorMessage(q->conn));
					appendStringInfo(&(c->err_msg),
									 "Failed to get data from segment %d: %s\n",
									 source_seg, PQerrorMessage(q->conn));

					/*
					 * remove the segment that completed sending data from the
					 * list
					 */
					c->outseglist = list_delete_int(c->outseglist, source_seg);

					/* no more segments with data on the list. we are done */
					if (list_length(c->outseglist) == 0)
						return true;	/* done */

					/*
					 * start over from first seg as we just changes the seg
					 * list
					 */
					break;
				}

				c->io_errors = true;
			}
		}

		if (c->copy_out_buf.len > COPYOUT_CHUNK_SIZE)
			break;
	}

	return false;
}

/*
 * Process the results from segments after sending the end of copy command.
 */
static ErrorData *
processCopyEndResults(CdbCopy *c,
					  SegmentDatabaseDescriptor *db_descriptors,
					  int *results,
					  int size,
					  SegmentDatabaseDescriptor **failedSegDBs,
					  bool *err_header,
					  int *failed_count,
					  int *total_rows_rejected,
					  int64 *total_rows_completed)
{
	SegmentDatabaseDescriptor *q;
	int			seg;
	PGresult   *res;
	struct pollfd	*pollRead = (struct pollfd *) palloc(sizeof(struct pollfd));
	int			segment_rows_rejected = 0;	/* num of rows rejected by this QE */
	int			segment_rows_completed = 0; /* num of rows completed by this
											 * QE */
	ErrorData *first_error = NULL;

	for (seg = 0; seg < size; seg++)
	{
		int			result = results[seg];

		q = &db_descriptors[seg];

		/* get command end status */
		if (result == 0)
		{
			/* attempt blocked */

			/*
			 * CDB TODO: Can this occur?  The libpq documentation says, "this
			 * case is only possible if the connection is in nonblocking
			 * mode... wait for write-ready and try again", i.e., the proper
			 * response would be to retry, not error out.
			 */
			if (!(*err_header))
				appendStringInfo(&(c->err_msg),
								 "Failed to complete COPY on the following:\n");
			*err_header = true;

			appendStringInfo(&(c->err_msg), "primary segment %d, dbid %d, attempt blocked\n",
							 seg, q->segment_database_info->dbid);
			c->io_errors = true;
		}

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
			c->aotupcounts = PQprocessAoTupCounts(c->partitions, c->aotupcounts, res->aotupcounts, res->naotupcounts);
			/* free the PGresult object */
			PQclear(res);
		}
		RESUME_INTERRUPTS();

		/* Finished with this segment db. */
		c->segdb_state[seg][0] = SEGDB_DONE;

		/*
		 * add number of rows rejected from this segment to the total of
		 * rejected rows. Only count from primary segs.
		 */
		if (segment_rows_rejected > 0)
			*total_rows_rejected += segment_rows_rejected;

		segment_rows_rejected = 0;

		/*
		 * add number of rows completed from this segment to the total of
		 * completed rows. Only count from primary segs
		 */
		if ((NULL != total_rows_completed) && (segment_rows_completed > 0))
			*total_rows_completed += segment_rows_completed;

		segment_rows_completed = 0;

		/* Lost the connection? */
		if (PQstatus(q->conn) == CONNECTION_BAD)
		{
			if (!*(err_header))
				appendStringInfo(&(c->err_msg),
								 "ERROR - Failed to complete COPY on the following:\n");
			*err_header = true;

			/* command error */
			c->io_errors = true;
			appendStringInfo(&(c->err_msg), "Primary segment %d, dbid %d, with error: %s\n",
							 seg, q->segment_database_info->dbid, PQerrorMessage(q->conn));

			/* Free the PGconn object. */
			PQfinish(q->conn);
			q->conn = NULL;

			/* Let FTS deal with it! */
			failedSegDBs[*failed_count] = q;
			(*failed_count)++;
		}
	}

	return first_error;
}

int
cdbCopyAbort(CdbCopy *c)
{
	return cdbCopyEndAndFetchRejectNum(c, NULL,
									   "aborting COPY in QE due to error in QD");
}

/*
 * End the copy command on all segment databases,
 * and fetch the total number of rows completed by all QEs
 * 
 * GPDB_91_MERGE_FIXME: we allow % value to be specified as segment reject
 * limit, however, the total rejected rows is not allowed to be > INT_MAX.
 */
int
cdbCopyEndAndFetchRejectNum(CdbCopy *c, int64 *total_rows_completed, char *abort_msg)
{
	SegmentDatabaseDescriptor *q;
	SegmentDatabaseDescriptor **failedSegDBs;
	Gang	   *gp;
	int		   *results;		/* final result of COPY command execution */
	int			seg;

	int			failed_count = 0;
	int			total_rows_rejected = 0;	/* total num rows rejected by all
											 * QEs */
	bool		err_header = false;
	struct SegmentDatabaseDescriptor *db_descriptors;
	int			size;
	ErrorData *edata;

	/*
	 * Don't try to end a copy that already ended with the destruction of the
	 * writer gang. We know that this has happened if the CdbCopy's
	 * primary_writer is NULL.
	 *
	 * GPDB_91_MERGE_FIXME: ugh, this is nasty. We shouldn't be calling
	 * cdbCopyEnd twice on the same CdbCopy in the first place!
	 */
	if (!c->primary_writer)
		return -1;

	/* clean err message */
	c->err_msg.len = 0;
	c->err_msg.data[0] = '\0';
	c->err_msg.cursor = 0;

	/* allocate a failed segment database pointer array */
	failedSegDBs = (SegmentDatabaseDescriptor **) palloc(c->total_segs * 2 * sizeof(SegmentDatabaseDescriptor *));

	gp = c->primary_writer;
	db_descriptors = gp->db_descriptors;
	size = gp->size;

	/* results from each segment */
	results = (int *) palloc0(sizeof(int) * size);

	for (seg = 0; seg < size; seg++)
	{
		q = &db_descriptors[seg];
		elog(DEBUG1, "PQputCopyEnd seg %d    ", q->segindex);
		/* end this COPY command */
		results[seg] = PQputCopyEnd(q->conn, abort_msg);
	}

	if (NULL != total_rows_completed)
		*total_rows_completed = 0;

	edata = processCopyEndResults(c, db_descriptors, results, size,
								  failedSegDBs, &err_header,
								  &failed_count, &total_rows_rejected,
								  total_rows_completed);

	/* If lost contact with segment db, try to reconnect. */
	if (failed_count > 0)
	{
		elog(LOG, "%s", c->err_msg.data);
		elog(LOG, "COPY signals FTS to probe segments");
		SendPostmasterSignal(PMSIGNAL_WAKEN_FTS);
		DisconnectAndDestroyAllGangs(true);
		c->primary_writer = NULL;
		ereport(ERROR,
				(errmsg_internal("MPP detected %d segment failures, system is reconnected", failed_count),
				 errSendAlert(true)));
	}

	pfree(results);
	pfree(failedSegDBs);

	/* If we are aborting the COPY, ignore errors sent by the server. */
	if (edata && !abort_msg)
		ReThrowError(edata);

	return total_rows_rejected;
}
