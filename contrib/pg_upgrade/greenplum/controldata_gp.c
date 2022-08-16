#include "pg_upgrade_greenplum.h"

#include "postgres_fe.h"

/*
 * Copies of backend xid comparison utilities
 */
#define InvalidTransactionId		((TransactionId) 0)
#define FirstNormalTransactionId	((TransactionId) 3)

#define TransactionIdIsValid(xid)		((xid) != InvalidTransactionId)
#define TransactionIdIsNormal(xid)	((xid) >= FirstNormalTransactionId)

/*
 * TransactionIdPrecedes --- is id1 logically < id2?
 */
static bool
TransactionIdPrecedes(TransactionId id1, TransactionId id2)
{
	/*
	 * If either ID is a permanent XID then we can just do unsigned
	 * comparison.  If both are normal, do a modulo-2^32 comparison.
	 */
	int32		diff;

	if (!TransactionIdIsNormal(id1) || !TransactionIdIsNormal(id2))
		return (id1 < id2);

	diff = (int32) (id1 - id2);
	return (diff < 0);
}

/*
 * Called for GPDB segments only -- since we have copied the master's
 * pg_control file, we need to assign a new system identifier to each segment.
 */
void
reset_system_identifier(void)
{
       struct timeval  tv;
       uint64                  sysidentifier;

       prep_status("Setting database system identifier for new cluster");

       /*
        * Use the same initialization process as BootStrapXLOG():
        *
        * - 32 bits of [current timestamp] seconds
        * - 20 bits of [current timestamp] microseconds
        * - 12 bits of PID
        *
        * This doesn't guarantee uniqueness, but if it's good enough for
        * gpinitsystem it should be good enough for us.
        */
       gettimeofday(&tv, NULL);
       sysidentifier = ((uint64) tv.tv_sec) << 32;
       sysidentifier |= ((uint64) tv.tv_usec) << 12;
       sysidentifier |= getpid() & 0xFFF;

       exec_prog(UTILITY_LOG_FILE, NULL, true, true,
                         "\"%s/pg_resetxlog\" --binary-upgrade --system-identifier " UINT64_FORMAT " \"%s\"",
                         new_cluster.bindir, sysidentifier, new_cluster.pgdata);

       check_ok();
}

/*
 * Greenplum upgrade involves copying the MASTER_DATA_DIRECTORY to
 * each primary segment. We need to freeze the master data *after* the master
 * schema has been restored to allow the data to be visible on the segments.
 * All databases need to be frozen including those where datallowconn is false.
 *
 * Note: No further updates should occur after freezing the master data
 * directory.
 */
void
freeze_master_data(void)
{
       PGconn                  *conn;
       PGconn                  *conn_template1;
       PGresult                *dbres;
       PGresult                *txid_res;
       PGresult                *dbage_res;
       int                             dbnum;
       int                             ntups;
       int                             i_datallowconn;
       int                             i_datname;
       TransactionId   txid_before;
       TransactionId   txid_after;
       int32                   txns_from_freeze;

       prep_status("Freezing all rows in new master after object restore");

       /* Temporarily allow connections to all databases for vacuum freeze */
       conn_template1 = connectToServer(&new_cluster, "template1");

       PQclear(executeQueryOrDie(conn_template1, "set allow_system_table_mods=true"));

       dbres = executeQueryOrDie(conn_template1,
                                 "SELECT datname, datallowconn "
	                             "FROM pg_catalog.pg_database");

       i_datname = PQfnumber(dbres, "datname");
       i_datallowconn = PQfnumber(dbres, "datallowconn");

       ntups = PQntuples(dbres);
       for (dbnum = 0; dbnum < ntups; dbnum++)
       {
               char *datallowconn = PQgetvalue(dbres, dbnum, i_datallowconn);
               char *datname = PQgetvalue(dbres, dbnum, i_datname);
               char *escaped_datname = pg_malloc(strlen(datname) * 2 + 1);
               PQescapeStringConn(conn_template1, escaped_datname, datname, strlen(datname), NULL);

               /* For vacuum freeze, temporarily set datallowconn to true. */
               if (strcmp(datallowconn, "f") == 0)
                       PQclear(executeQueryOrDie(conn_template1,
                                                                         "UPDATE pg_catalog.pg_database "
                                                                         "SET datallowconn = true "
                                                                         "WHERE datname = '%s'", escaped_datname));

               conn = connectToServer(&new_cluster, datname);

               /* Obtain txid_current before vacuum freeze. */
               txid_res = executeQueryOrDie(conn, "SELECT txid_current()");
               txid_before = str2uint(PQgetvalue(txid_res, 0, PQfnumber(txid_res, "txid_current")));
               PQclear(txid_res);

               PQclear(executeQueryOrDie(conn, "VACUUM FREEZE"));

               /*
                * Obtain txid_current and age after vacuum freeze.
                *
                * Note: It is important that this occurs before any other transactions
                * are executed so verification succeeds.
                */
               dbage_res = executeQueryOrDie(conn,
                                             "SELECT txid_current(), age(datfrozenxid) "
	                                         "FROM pg_catalog.pg_database "
	                                         "WHERE datname = '%s'", escaped_datname);
               txid_after = str2uint(PQgetvalue(dbage_res, 0, PQfnumber(dbage_res, "txid_current")));
               uint datfrozenxid_age = str2uint(PQgetvalue(dbage_res, 0, PQfnumber(dbage_res, "age")));
               PQclear(dbage_res);

               /*
                * Verify that the database was frozen by checking that the database age
                * is less than the number of transactions taken by "VACUUM FREEZE".
                * This implies that all transaction ids that are older than the
                * "VACUUM FREEZE" transaction are frozen, and that the oldest
                * transaction in the database is newer than the "VACUUM FREEZE"
                * transaction.
                */
               txns_from_freeze = txid_after - txid_before;
               if (txns_from_freeze < 0)
               {
                       /* Needed if a wrap around occurs between txid after and before. */
                       txns_from_freeze = INT32_MAX - Abs(txns_from_freeze);
               }

               /* Reset datallowconn flag before possibly raising an error. */
               if (strcmp(datallowconn, "f") == 0)
                       PQclear(executeQueryOrDie(conn_template1,
                                                                         "UPDATE pg_catalog.pg_database "
                                                                         "SET datallowconn = false "
                                                                         "WHERE datname = '%s'", escaped_datname));

               pg_free(escaped_datname);
               PQfinish(conn);

               if (datfrozenxid_age > txns_from_freeze)
               {
                       PQfinish(conn_template1);
                       pg_fatal("Error database '%s' was not properly frozen. Database age of %d is older than %d.\n",
                                        datname, datfrozenxid_age, txns_from_freeze);
               }
       }

       /* Freeze the tuples updated from resetting datallowconn flag */
       PQclear(executeQueryOrDie(conn_template1, "VACUUM FREEZE pg_catalog.pg_database"));

       PQclear(dbres);

       PQfinish(conn_template1);

       check_ok();
}

/*
 * GPDB5: Calculate the oldest xid in the old cluster by taking the minimum
 * datfrozenxid across all databases in the old cluster.
 */
void
set_old_cluster_chkpnt_oldstxid()
{
	TransactionId	oldestXid = InvalidTransactionId;

	for (int dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		DbInfo *active_db = &old_cluster.dbarr.dbs[dbnum];
		if (!TransactionIdIsValid(oldestXid) ||
			TransactionIdPrecedes(active_db->datfrozenxid, oldestXid))
			oldestXid = active_db->datfrozenxid;
	}

	old_cluster.controldata.chkpnt_oldstxid = oldestXid;
}
