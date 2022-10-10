/*
 *	frozenxids_gp.c
 *
 *	functions for restoring frozenxid from old cluster
 *
 *	Copyright (c) 2016-Present, Pivotal Software Inc
 */
#include "postgres_fe.h"

#include "pg_upgrade_greenplum.h"


/*
 * Segment database contains data, and the tuples should not have
 * any entry with a xmin > relfrozenxid for the table in pg_class.
 * Instead of vacuum freezing the entire data, update the relfrozenxid
 * of the relation in pg_class with the datfrozenxid from the corresponding
 * database in the old cluster. This ensures that the xmin is not > relfrozenxid
 * for any of the tuple.
 * Coordinator contains data entries in GPDB catalog tables like
 * gp_fastsequence, pg_aoseg, pg_aocsseg, pg_aoblkdir, and pg_aovisimap that
 * also need relfrozenxid correction. Also, update the new database with the
 * datfrozenxid from the old cluster as that indicates the lowest xid available.
 *
 * In GPDB5 datminmxid does not exist, so use the chkpnt_nxtmulti to update
 * the value in the GPDB6 cluster.
 */
void
update_db_xids(void)
{
	int			dbnum;
	PGconn	   *conn;

	prep_status("Updating xid's in new cluster databases");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		char *escaped_datname = pg_malloc(strlen(active_db->db_name) * 2 + 1);
		uint32 datfrozenxid = active_db->datfrozenxid;
		uint32 datminmxid = active_db->datminmxid;
		PQescapeString(escaped_datname, active_db->db_name, strlen(active_db->db_name));

		conn = connectToServer(&new_cluster, active_db->db_name);

		PQclear(executeQueryOrDie(conn,
								  "set allow_system_table_mods=true"));

		PQclear(executeQueryOrDie(conn,
								  "UPDATE pg_catalog.pg_database "
								  "SET datfrozenxid = '%u', datminmxid = '%u' "
								  "WHERE datname = '%s'",
								  datfrozenxid,
								  (GET_MAJOR_VERSION(old_cluster.major_version) <= 803) ?
								  old_cluster.controldata.chkpnt_nxtmulti : datminmxid,
								  escaped_datname));

		/*
		 * include heap, materialized view, temporary/toast and AO tables
		 * exclude relations with external storage as well as AO and CO tables
		 * The logic here should keep consistent with function
		 * should_have_valid_relfrozenxid().
		 * Notes: if we ever backport this to Greenplum 5X, remove 'm' first
		 * and then replace 'M' with 'm', because 'm' used to be RELKIND
		 * visimap in 4.3/5X, not matview
		 *
		 */
		PQclear(executeQueryOrDie(conn,
								  "UPDATE	pg_catalog.pg_class "
								  "SET	relfrozenxid = '%u'"
								  "WHERE	(relkind IN ('r', 'm', 't') "
								  "AND NOT relfrozenxid = 0) "
								  "OR (relkind IN ('t', 'o', 'b', 'M'))",
								  datfrozenxid));


		/*
		 * update heap, materialized view, TOAST/temporary and AO tables
		 */
		PQclear(executeQueryOrDie(conn,
								  "UPDATE	pg_catalog.pg_class "
								  "SET	relminmxid = '%u' "
								  "WHERE	relkind IN ('r', 'm', 't', 'o', 'b', 'M')",
								  (GET_MAJOR_VERSION(old_cluster.major_version) <= 803) ?
								  old_cluster.controldata.chkpnt_nxtmulti : datminmxid));
		PQfinish(conn);
	}

	check_ok();

}