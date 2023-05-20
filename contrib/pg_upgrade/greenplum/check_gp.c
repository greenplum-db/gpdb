/*
 *	check_gp.c
 *
 *	Greenplum specific server checks and output routines
 *
 *	Any compatibility checks which are version dependent (testing for issues in
 *	specific versions of Greenplum) should be placed in their respective
 *	version_old_gpdb{MAJORVERSION}.c file.  The checks in this file supplement
 *	the checks present in check.c, which is the upstream file for performing
 *	checks against a PostgreSQL cluster.
 *
 *	Copyright (c) 2010, PostgreSQL Global Development Group
 *	Copyright (c) 2017-Present Pivotal Software, Inc
 *	contrib/pg_upgrade/check_gp.c
 */

#include "pg_upgrade_greenplum.h"
#include "check_gp.h"

#include <sys/wait.h>

static int check_greenplum_parallel_jobs;
typedef void (*check_function)(void);

static void check_covering_aoindex(void);
static void check_parent_partitions_with_seg_entries(void);
static void check_partition_indexes(void);
static void check_orphaned_toastrels(void);
static void check_online_expansion(void);
static void check_gphdfs_external_tables(void);
static void check_gphdfs_user_roles(void);
static void check_unique_primary_constraint(void);
static void check_for_array_of_partition_table_types(void);
static void check_large_objects(void);
static void check_invalid_indexes(void);
static void check_foreign_key_constraints_on_root_partition(void);
static void check_distributed_on_duplicate_columns(void);
static void check_views_with_unsupported_lag_lead_function(void);
static void check_views_with_fabricated_anyarray_casts(void);
static void check_views_with_fabricated_unknown_casts(void);
static void check_views_referencing_deprecated_tables(void);
static void check_views_referencing_deprecated_columns(void);
static void parallel_check_greenplum(check_function check_func);
static bool parallel_checks_reap_child(bool wait_for_child);

/*
 *	check_greenplum
 *
 *	Rather than exporting all checks, we export a single API function which in
 *	turn is responsible for running Greenplum checks. This function should be
 *	executed after all PostgreSQL checks. The order of the checks should not
 *	matter.
 */
void
check_greenplum(void)
{
	int i = 0;
	check_function check_functions[64] = {
		check_online_expansion,
		check_covering_aoindex,
		check_parent_partitions_with_seg_entries,
		check_heterogeneous_partition,
		check_partition_indexes,
		check_foreign_key_constraints_on_root_partition,
		check_orphaned_toastrels,
		check_gphdfs_external_tables,
		check_gphdfs_user_roles,
		check_unique_primary_constraint,
		check_for_array_of_partition_table_types,
		check_large_objects,
		check_invalid_indexes,
		check_distributed_on_duplicate_columns,
		check_views_with_unsupported_lag_lead_function,
		check_views_with_fabricated_anyarray_casts,
		check_views_with_fabricated_unknown_casts,
		check_views_referencing_deprecated_tables,
		check_views_referencing_deprecated_columns,
		NULL /* indicator for end of check functions */
	};

	pg_log(PG_REPORT, "\nStarting Parallel Greenplum Checks\n");
	pg_log(PG_REPORT, "==================================\n");

	while (check_functions[i])
	{
		parallel_check_greenplum(check_functions[i]);
		i++;
	}

	/* wait for all children to finish */
	while (parallel_checks_reap_child(true) == true);
	pg_log(PG_REPORT, "==================================\n\n");
}

/*
 *	parallel_check_greenplum
 *
 *	Do given check in parallel execution.
 */
static void
parallel_check_greenplum(check_function check_func)
{
	pid_t		child;

	if (user_opts.jobs <= 1)
		check_func();
	else /* parallel */
	{
		/* clear any finished children */
		while (parallel_checks_reap_child(false) == true);

		/* must we wait for a finished child? */
		if (check_greenplum_parallel_jobs >= user_opts.jobs)
			parallel_checks_reap_child(true);

		/* set this before we start the job */
		check_greenplum_parallel_jobs++;

		/* Ensure stdio state is quiesced before forking */
		fflush(NULL);

		child = fork();
		if (child == 0)
		{
			/* do the parallel work */
			check_func();

			/* if we take another exit path, it will be non-zero */
			/* use _exit to skip atexit() functions */
			_exit(get_check_fatal_occurred() ? 1 : 0);
		}
		else if (child < 0)
			/* fork failed */
			pg_fatal("could not create worker process: %s\n", strerror(errno));
	}

	return;
}

/*
 *	collect status from a completed worker child
 */
static bool
parallel_checks_reap_child(bool wait_for_child)
{
	int			work_status;
	pid_t		child;

	if (user_opts.jobs <= 1 || check_greenplum_parallel_jobs == 0)
		return false;

	child = waitpid(-1, &work_status, wait_for_child ? 0 : WNOHANG);
	if (child == (pid_t) -1)
		pg_fatal("waitpid() failed: %s\n", strerror(errno));
	if (child == 0)
		return false; /* no children, or no finished children */
	if (work_status != 0)
		set_check_fatal_occured();

	/* do this after job has been removed */
	check_greenplum_parallel_jobs--;

	return true;
}

/*
 *	check_online_expansion
 *
 *	Check for online expansion status and refuse the upgrade if online
 *	expansion is in progress.
 */
static void
check_online_expansion(void)
{
	char	   *check_name = "Checking for online expansion status";
	bool		expansion = false;
	int			dbnum;

	/*
	 * Only need to check cluster expansion status in gpdb6 or later.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) < 804)
		return;

	/*
	 * We only need to check the cluster expansion status on master.
	 * On the other hand the status can not be detected correctly on segments.
	 */
	if (!is_greenplum_dispatcher_mode())
		return;

	start_parallel_check(check_name);

	/* Check if the cluster is in expansion status */
	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
								"SELECT true AS expansion "
								"FROM pg_catalog.gp_distribution_policy d "
								"JOIN (SELECT count(*) segcount "
								"      FROM pg_catalog.gp_segment_configuration "
								"      WHERE content >= 0 and role = 'p') s "
								"ON d.numsegments <> s.segcount "
								"LIMIT 1;");

		ntups = PQntuples(res);

		if (ntups > 0)
			expansion = true;

		PQclear(res);
		PQfinish(conn);

		if (expansion)
			break;
	}

	if (expansion)
	{
		parallel_gp_fatal_log(
				check_name,
				"| Your installation is in progress of online expansion,\n"
				"| must complete that job before the upgrade.\n\n");
	}
	else
		parallel_check_ok(check_name);
}

/*
 *	check_unique_primary_constraint
 *
 *  For unique or primary key constraint, the index name is auto generated,
 *  and if the default index name is already taken by other objects, an incremental
 *  number is appended to the index name.
 *  This means that pg_upgrade cannot upgrade a cluster containing indexes of such
 *  type, they must be handled manually before/after the upgrade. Although, the issue
 *  exists only with such indexes, but we wholesale ban upgrading of unique or
 *  primary key constraints. Such, constraints must be dropped before the upgrade,
 *  and can be recreated after the upgrade.
 *
 *	Check for the existence of unique or primary key constraint and refuse the upgrade if
 *	found.
 */
static void
check_unique_primary_constraint(void)
{
	char	   *check_name = "Checking for unique or primary key constraints";
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "unique_primary_key_constraint.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
			"SELECT conname constraint_name, c.relname index_name, objsubid "
			"FROM pg_constraint con "
			"    JOIN pg_depend dep ON (refclassid, classid, objsubid) = "
			"                               ('pg_constraint'::regclass, 'pg_class'::regclass, 0) "
			"    AND refobjid = con.oid AND deptype = 'i' AND "
			"                               contype IN ('u', 'p') "
			"    JOIN pg_class c ON objid = c.oid AND relkind = 'i' "
			"WHERE conname <> relname;");

		ntups = PQntuples(res);

		if (ntups > 0)
		{
			found = true;

			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary upgrade report file:  %s\n",
					   output_path);

			for (rowno = 0; rowno < ntups; rowno++)
			{
				if (!db_used)
				{
					fprintf(script, "Database:  %s\n", active_db->db_name);
					db_used = true;
				}
				fprintf(script, "Constraint name \"%s\" does not match index name \"%s\"\n",
						PQgetvalue(res, rowno, PQfnumber(res, "constraint_name")),
						PQgetvalue(res, rowno, PQfnumber(res, "index_name")));
			}
		}

		PQclear(res);
		PQfinish(conn);
	}
	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains unique or primary key constraints\n"
				"| on tables.  These constraints need to be removed\n"
				"| from the tables before the upgrade.  A list of\n"
				"| constraints to remove is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

/*
 *	check_covering_aoindex
 *
 *	A partitioned AO table which had an index created on the parent relation,
 *	and an AO partition exchanged into the hierarchy without any indexes will
 *	break upgrades due to the way pg_dump generates DDL.
 *
 *	create table t (a integer, b text, c integer)
 *		with (appendonly=true)
 *		distributed by (a)
 *		partition by range(c) (start(1) end(3) every(1));
 *	create index t_idx on t (b);
 *
 *	At this point, the t_idx index has created AO blockdir relations for all
 *	partitions. We now exchange a new table into the hierarchy which has no
 *	index defined:
 *
 *	create table t_exch (a integer, b text, c integer)
 *		with (appendonly=true)
 *		distributed by (a);
 *	alter table t exchange partition for (rank(1)) with table t_exch;
 *
 *	The partition which was swapped into the hierarchy with EXCHANGE does not
 *	have any indexes and thus no AO blockdir relation. This is in itself not
 *	a problem, but when pg_dump generates DDL for the above situation it will
 *	create the index in such a way that it covers the entire hierarchy, as in
 *	its original state. The below snippet illustrates the dumped DDL:
 *
 *	create table t ( ... )
 *		...
 *		partition by (... );
 *	create index t_idx on t ( ... );
 *
 *	This creates a problem for the Oid synchronization in pg_upgrade since it
 *	expects to find a preassigned Oid for the AO blockdir relations for each
 *	partition. A longer term solution would be to generate DDL in pg_dump which
 *	creates the current state, but for the time being we disallow upgrades on
 *	cluster which exhibits this.
 */
static void
check_covering_aoindex(void)
{
	char		   *check_name = "Checking for non-covering indexes on partitioned AO tables";
	char			output_path[MAXPGPATH];
	FILE		   *script = NULL;
	bool			found = false;
	int				dbnum;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "mismatched_aopartition_indexes.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		PGconn	   *conn;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
			 "SELECT DISTINCT ao.relid, inh.inhrelid "
			 "FROM   pg_catalog.pg_appendonly ao "
			 "       JOIN pg_catalog.pg_inherits inh "
			 "         ON (inh.inhparent = ao.relid) "
			 "       JOIN pg_catalog.pg_appendonly aop "
			 "         ON (inh.inhrelid = aop.relid AND aop.blkdirrelid = 0) "
			 "       JOIN pg_catalog.pg_index i "
			 "         ON (i.indrelid = ao.relid) "
			 "WHERE  ao.blkdirrelid <> 0;");

		ntups = PQntuples(res);

		if (ntups > 0)
		{
			found = true;

			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
					   output_path);

			for (rowno = 0; rowno < ntups; rowno++)
			{
				fprintf(script, "Mismatched index on partition %s in relation %s\n",
						PQgetvalue(res, rowno, PQfnumber(res, "inhrelid")),
						PQgetvalue(res, rowno, PQfnumber(res, "relid")));
			}
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains partitioned append-only tables\n"
				"| with an index defined on the partition parent which isn't\n"
				"| present on all partition members.  These indexes must be\n"
				"| dropped before the upgrade.  A list of relations, and the\n"
				"| partitions in question is in the file:\n"
				"| \t%s\n\n", output_path);

	}
	else
		parallel_check_ok(check_name);
}

static void
check_orphaned_toastrels(void)
{
	char		   *check_name = "Checking for orphaned TOAST relations";
	bool			found = false;
	int				dbnum;
	char			output_path[MAXPGPATH];
	FILE		   *script = NULL;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "orphaned_toast_tables.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		PGconn	   *conn;
		int			ntups;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
								"WITH orphan_toast AS ( "
								"    SELECT c.oid AS reloid, "
								"           c.relname, t.oid AS toastoid, "
								"           t.relname AS toastrelname "
								"    FROM pg_catalog.pg_class t "
								"         LEFT OUTER JOIN pg_catalog.pg_class c ON (c.reltoastrelid = t.oid) "
								"    WHERE t.relname ~ '^pg_toast' AND "
								"          t.relkind = 't') "
								"SELECT reloid "
								"FROM   orphan_toast "
								"WHERE  reloid IS NULL");

		ntups = PQntuples(res);
		if (ntups > 0)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n", output_path);

			fprintf(script, "Database \"%s\" has %d orphaned toast tables\n", active_db->db_name, ntups);
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains orphaned toast tables which\n"
				"| must be dropped before upgrade.\n"
				"| A list of the problem databases is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);

}

/*
 * check_heterogeneous_partition
 *
 * Detect if heterogeneous partition tables exists in the GPDB cluster. A
 * heterogeneous partition table is defined as:
 *
 * 1. The root partition has no dropped column reference but at least one of
 *    its child partitions has dropped column references.
 * 2. The root partition has dropped column references but at least one of its
 *    child partitions does not.
 * 3. The root partition and all of its child partitions have dropped column
 *    references but the columns are misaligned.
 *
 * Valid homogeneous partition tables are defined as:
 *
 * 1. The root partition and all of its child partitions have no dropped
 *    column references.
 * 2. The root partition and all of its child partitions have the same dropped
 *    column references and the columns are aligned.
 * 3. The root partition has a dropped column reference but none of its child
 *    partitions do.
 *
 * Note: For homogeneous partition table definition (3), we assume that
 * pg_dump --binary-upgrade will NOT output the dropped column reference in
 * the partition table DDL schema dump by suppressing it. There is currently a
 * GPDB hack that does this which is dependent on the logic in this function.
 * If anything is to change here, please review if anything needs to be
 * changed in the related GPDB hack for pg_dump --binary-upgrade.
 */
void
check_heterogeneous_partition(void)
{
	char		   *check_name = "Checking for heterogeneous partitioned tables";
	int				dbnum;
	FILE		   *script = NULL;
	bool			found = false;
	char			output_path[MAXPGPATH];

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "heterogeneous_partitioned_tables.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);
		bool		db_used = false;

		/* Scenario 1: Check for dropped column references and number of attributes mismatch */
		res = executeQueryOrDie(conn, CHECK_PARTITION_TABLE_DROPPED_COLUMN_REFERENCES);
		ntups = PQntuples(res);
		if (ntups != 0)
		{
			found = true;
			if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
				pg_fatal("Could not open file \"%s\": %s\n",
						 output_path, getErrorText());

			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}

			fprintf(script, "  Partitions with invalid dropped column references:\n");
			for (rowno = 0; rowno < ntups; rowno++)
			{
				fprintf(script, "    %s.%s\n",
						PQgetvalue(res, rowno, PQfnumber(res, "childnamespace")),
						PQgetvalue(res, rowno, PQfnumber(res, "childrelname")));
			}
		}

		PQclear(res);

		/*
		 * Scenario 2: Compare root and child partition dropped column
		 * attributes for name, type, length, and alignment.
		 */
		res = executeQueryOrDie(conn, CHECK_PARTITION_TABLE_MATCHES_DROPPED_COLUMN_ATTRIBUTES);
		ntups = PQntuples(res);
		if (ntups != 0)
		{
			found = true;
			if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
				pg_fatal("Could not open file \"%s\": %s\n",
						 output_path, getErrorText());

			if (!db_used)
				fprintf(script, "Database: %s\n", active_db->db_name);

			fprintf(script, "  Partitions with misaligned dropped column references:\n");
			for (rowno = 0; rowno < ntups; rowno++)
				fprintf(script, "    %s\n", PQgetvalue(res, rowno, PQfnumber(res, "parchildrelid")));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (script)
		fclose(script);

	if (found)
	{
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains heterogeneous partition tables. Either one or more\n"
				"| child partitions have invalid dropped column references or the columns are\n"
				"| misaligned compared to the root partition. Upgrade cannot output partition\n"
				"| table DDL to preserve the dropped columns for the detected child partitions\n"
				"| since ALTER statements can only be applied from the root partition (which will\n"
				"| cascade down the partition hierarchy). Preservation of these columns is\n"
				"| necessary for on-disk compatibility of the child partitions. In order to\n"
				"| correct the child partitions, create a new staging table with the same schema\n"
				"| as the child partition, insert the old data into the staging table, exchange\n"
				"| the child partition with the staging table, and drop the staging table.\n"
				"| Alternatively, the entire partition table can be recreated.\n"
				"| A list of the problem tables is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

/*
 *	check_partition_indexes
 *
 *	There are numerous pitfalls surrounding indexes on partition hierarchies,
 *	so rather than trying to cover all the cornercases we disallow indexes on
 *	partitioned tables altogether during the upgrade.  Since we in any case
 *	invalidate the indexes forcing a REINDEX, there is little to be gained by
 *	handling them for the end-user.
 */
static void
check_partition_indexes(void)
{
	char		   *check_name = "Checking for indexes on partitioned tables";
	int				dbnum;
	FILE		   *script = NULL;
	bool			found = false;
	char			output_path[MAXPGPATH];

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "partitioned_tables_indexes.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname;
		int			i_relname;
		int			i_indexes;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		res = executeQueryOrDie(conn,
								"WITH partitions AS ("
								"    SELECT DISTINCT n.nspname, "
								"           c.relname "
								"    FROM pg_catalog.pg_partition p "
								"         JOIN pg_catalog.pg_class c ON (p.parrelid = c.oid) "
								"         JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace) "
								"    UNION "
								"    SELECT n.nspname, "
								"           partitiontablename AS relname "
								"    FROM pg_catalog.pg_partitions p "
								"         JOIN pg_catalog.pg_class c ON (p.partitiontablename = c.relname) "
								"         JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace) "
								") "
								"SELECT nspname, "
								"       relname, "
								"       count(indexname) AS indexes "
								"FROM partitions "
								"     JOIN pg_catalog.pg_indexes ON (relname = tablename AND "
								"                                    nspname = schemaname) "
								"GROUP BY nspname, relname "
								"ORDER BY relname");

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_relname = PQfnumber(res, "relname");
		i_indexes = PQfnumber(res, "indexes");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database:  %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s.%s has %s index(es)\n",
					PQgetvalue(res, rowno, i_nspname),
					PQgetvalue(res, rowno, i_relname),
					PQgetvalue(res, rowno, i_indexes));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains partitioned tables with\n"
				"| indexes defined on them.  Indexes on partition parents,\n"
				"| as well as children, must be dropped before upgrade.\n"
				"| A list of the problem tables is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

/*
 * check_gphdfs_external_tables
 *
 * Check if there are any remaining gphdfs external tables in the database.
 * We error if any gphdfs external tables remain and let the users know that,
 * any remaining gphdfs external tables have to be removed.
 */
static void
check_gphdfs_external_tables(void)
{
	char	   *check_name = "Checking for gphdfs external tables";
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	/* GPDB only supported gphdfs in this version range */
	if (!(old_cluster.major_version >= 80215 && old_cluster.major_version < 80400))
		return;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "gphdfs_external_tables.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
			 "SELECT d.objid::regclass as tablename "
			 "FROM pg_catalog.pg_depend d "
			 "       JOIN pg_catalog.pg_exttable x ON ( d.objid = x.reloid ) "
			 "       JOIN pg_catalog.pg_extprotocol p ON ( p.oid = d.refobjid ) "
			 "       JOIN pg_catalog.pg_class c ON ( c.oid = d.objid ) "
			 "       WHERE d.refclassid = 'pg_extprotocol'::regclass "
			 "       AND p.ptcname = 'gphdfs';");

		ntups = PQntuples(res);

		if (ntups > 0)
		{
			found = true;

			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
					   output_path);

			for (rowno = 0; rowno < ntups; rowno++)
			{
				fprintf(script, "gphdfs external table \"%s\" in database \"%s\"\n",
						PQgetvalue(res, rowno, PQfnumber(res, "tablename")),
						active_db->db_name);
			}
		}

		PQclear(res);
		PQfinish(conn);
	}
	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains gphdfs external tables.  These \n"
				"| tables need to be dropped before upgrade.  A list of\n"
				"| external gphdfs tables to remove is provided in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

/*
 * check_gphdfs_user_roles
 *
 * Check if there are any remaining users with gphdfs roles.
 * We error if this is the case and let the users know how to proceed.
 */
static void
check_gphdfs_user_roles(void)
{
	char	   *check_name = "Checking for users assigned the gphdfs role";
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	PGresult   *res;
	int			ntups;
	int			rowno;
	int			i_hdfs_read;
	int			i_hdfs_write;
	PGconn	   *conn;

	/* GPDB only supported gphdfs in this version range */
	if (!(old_cluster.major_version >= 80215 && old_cluster.major_version < 80400))
		return;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "gphdfs_user_roles.txt");

	conn = connectToServer(&old_cluster, "template1");
	res = executeQueryOrDie(conn,
							"SELECT rolname as role, "
							"       rolcreaterexthdfs as hdfs_read, "
							"       rolcreatewexthdfs as hdfs_write "
							"FROM pg_catalog.pg_roles"
							"       WHERE rolcreaterexthdfs OR rolcreatewexthdfs");

	ntups = PQntuples(res);

	if (ntups > 0)
	{
		if ((script = fopen(output_path, "w")) == NULL)
			pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
					output_path);

		i_hdfs_read = PQfnumber(res, "hdfs_read");
		i_hdfs_write = PQfnumber(res, "hdfs_write");

		for (rowno = 0; rowno < ntups; rowno++)
		{
			bool hasReadRole = (PQgetvalue(res, rowno, i_hdfs_read)[0] == 't');
			bool hasWriteRole =(PQgetvalue(res, rowno, i_hdfs_write)[0] == 't');

			fprintf(script, "role \"%s\" has the gphdfs privileges:",
					PQgetvalue(res, rowno, PQfnumber(res, "role")));
			if (hasReadRole)
				fprintf(script, " read(rolcreaterexthdfs)");
			if (hasWriteRole)
				fprintf(script, " write(rolcreatewexthdfs)");
			fprintf(script, " \n");
		}
	}

	PQclear(res);
	PQfinish(conn);

	if (ntups > 0)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains roles that have gphdfs privileges.\n"
				"| These privileges need to be revoked before upgrade.  A list\n"
				"| of roles and their corresponding gphdfs privileges that\n"
				"| must be revoked is provided in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_for_array_of_partition_table_types(void)
{
	char	   *check_name = "Checking array types derived from partitions";
	const char *const SEPARATOR = "\n";
	int			dbnum;
	char	   *dependee_partition_report = palloc0(1);

	start_parallel_check(check_name);

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			n_tables_to_check;
		int			i;

		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		/* Find the arraytypes derived from partitions of partitioned tables */
		res = executeQueryOrDie(conn,
		                        "SELECT td.typarray, ns.nspname || '.' || td.typname AS dependee_partition_qname "
		                        "FROM (SELECT typarray, typname, typnamespace "
		                        "FROM (SELECT pg_c.reltype AS rt "
		                        "FROM pg_class AS pg_c JOIN pg_partitions AS pg_p ON pg_c.relname = pg_p.partitiontablename) "
		                        "AS check_types JOIN pg_type AS pg_t ON check_types.rt = pg_t.oid WHERE pg_t.typarray != 0) "
		                        "AS td JOIN pg_namespace AS ns ON td.typnamespace = ns.oid "
		                        "ORDER BY td.typarray;");

		n_tables_to_check = PQntuples(res);
		for (i = 0; i < n_tables_to_check; i++)
		{
			char	   *array_type_oid_to_check = PQgetvalue(res, i, 0);
			char	   *dependee_partition_qname = PQgetvalue(res, i, 1);
			PGresult   *res2 = executeQueryOrDie(conn, "SELECT 1 FROM pg_depend WHERE refobjid = %s;", array_type_oid_to_check);

			if (PQntuples(res2) > 0)
			{
				dependee_partition_report = repalloc(
					dependee_partition_report,
					strlen(dependee_partition_report) + strlen(array_type_oid_to_check) + 1 + strlen(dependee_partition_qname) + strlen(SEPARATOR) + 1
				);
				sprintf(
					&(dependee_partition_report[strlen(dependee_partition_report)]),
					"%s %s%s",
					array_type_oid_to_check, dependee_partition_qname, SEPARATOR
				);
			}
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (strlen(dependee_partition_report))
	{
		parallel_gp_fatal_log(
				check_name,
				"| Array types derived from partitions of a partitioned table must not have dependants.\n"
				"| OIDs of such types found and their original partitions:\n%s\n\n",
				dependee_partition_report);
	}
	pfree(dependee_partition_report);

	parallel_check_ok(check_name);
}

/*
 * Greenplum 6 does not support large objects, but 5 does.
 */
static void
check_large_objects(void)
{
	char	   *check_name = "Checking for large objects";
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "pg_largeobject.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		/* find if there are any large objects */
		res = executeQueryOrDie(conn,
								"SELECT count(*) > 0 as large_object_exists"
								" FROM	pg_catalog.pg_largeobject ");

		if (PQgetvalue(res, 0, PQfnumber(res, "large_object_exists"))[0] == 't')
		{
			found = true;
			if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
				pg_fatal("could not open file \"%s\": %s\n", output_path, getErrorText());

			/* We do not list out the objects (like other checks) since we only
			 * have oids and finding associated tables could be time consuming.
			 */
			fprintf(script, "Database %s contains large objects\n", active_db->db_name);
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (script)
		fclose(script);

	if (found)
	{
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains large objects.  These objects are not supported\n"
				"| by the new cluster and must be dropped.\n"
				"| A list of databases which contains large objects is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

/*
 * check_invalid_indexes
 *
 * Check if there are any invalid indexes and let the users know that.
 * In greenplum, upgrade for bitmap and bpchar_pattern_ops indexes is not supported,
 * these indexes are marked invalid during the upgrade of the master database, and are reset
 * to valid state at the start of segment upgrade. Since, there is no way to identify what
 * indexes are marked invalid by pg_upgrade vs what were already invalid in the source cluster,
 * there is an expectation that the old cluster does not have any invalid index.
 *
 * Note: Indexes are marked invalid with CREATE INDEX CONCURRENTLY statement, however, its
 * not supported in Greenplum, so we shouldn't have any invalid indexes in the source cluster
 * to start with.
 */
static void
check_invalid_indexes(void)
{
	char	   *check_name = "Checking for invalid indexes";
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;
	int			i_indexname;
	int			i_relname;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "invalid_indexes.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;
		bool		db_used = false;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
								"SELECT indexrelid::pg_catalog.regclass indexname, indrelid::pg_catalog.regclass relname "
								"FROM pg_catalog.pg_index i "
								"WHERE i.indisvalid = false;");

		ntups = PQntuples(res);

		i_indexname = PQfnumber(res, "indexname");
		i_relname = PQfnumber(res, "relname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_fatal("Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s on relation %s\n",
					PQgetvalue(res, rowno, i_indexname),
					PQgetvalue(res, rowno, i_relname));
		}

		PQclear(res);
		PQfinish(conn);
	}
	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains invalid indexes.  These indexes either \n"
				"| need to be dropped or reindexed before proceeding to upgrade.\n"
				"| A list of invalid indexes is provided in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_foreign_key_constraints_on_root_partition(void)
{
	char	   *check_name = "Checking for foreign key constraints on root partitions";
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;
	int			i_relname;
	int			i_constraintname;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "foreign_key_constraints.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;
		bool		db_used = false;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
								"SELECT oid::regclass as relname, conname  "
								"FROM pg_constraint cc "
								"JOIN "
								"(SELECT DISTINCT c.oid, c.relname "
									"FROM pg_catalog.pg_partition p "
									"JOIN pg_catalog.pg_class c ON (p.parrelid = c.oid)) as sub ON sub.oid=cc.conrelid "
         						"WHERE cc.contype IN ('f');");

		ntups = PQntuples(res);

		i_relname = PQfnumber(res, "relname");
		i_constraintname = PQfnumber(res, "conname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_fatal("Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s on relation %s\n",
					PQgetvalue(res, rowno, i_constraintname),
					PQgetvalue(res, rowno, i_relname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains foreign key constraint on root \n"
				"| partition tables. These constraints need to be dropped before \n"
				"| proceeding to upgrade. A list of foreign key constraints is \n"
				"| in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_views_with_unsupported_lag_lead_function(void)
{
	/*
	 * Only need to check for versions prior to GPDB6
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 804)
		return;

	char		*check_name = "Checking for views with lead/lag functions using bigint";
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;
	int			i_viewname;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "view_lead_lag_functions.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;
		bool		db_used = false;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
								"SELECT ev_class::regclass::text viewname  "
								"FROM pg_rewrite pgr "
								"WHERE ev_action ~ "
								"(SELECT $$:winfnoid ($$||string_agg(oid::text,'|')||$$) :$$ "
								"	FROM (SELECT DISTINCT oid FROM pg_catalog.pg_proc WHERE (proname, pronamespace) in "
								"			(('lag', 11), ('lead', 11))AND proargtypes[1]=20)s1);");

		ntups = PQntuples(res);

		i_viewname = PQfnumber(res, "viewname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_fatal("Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s \n",
					PQgetvalue(res, rowno, i_viewname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains views using lag or lead \n"
				"| functions with the second parameter as bigint. These views \n"
				"| need to be dropped before proceeding to upgrade. \n"
				"| A list of views is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_views_with_fabricated_anyarray_casts()
{
	char		*check_name = "Checking for non-dumpable views with anyarray casts";
	char		output_path[MAXPGPATH];
	FILE		*script = NULL;
	bool		found = false;
	int			dbnum;
	int			i_viewname;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "view_anyarray_casts.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;
		bool		db_used = false;

		conn = connectToServer(&old_cluster, active_db->db_name);
		PQclear(executeQueryOrDie(conn, "SET search_path TO 'public';"));

		/* Install check support function */
		PQclear(executeQueryOrDie(conn,
									 "CREATE OR REPLACE FUNCTION "
									 "view_has_anyarray_casts(OID) "
									 "RETURNS BOOL "
									 "AS '$libdir/pg_upgrade_support' "
									 "LANGUAGE C STRICT;"));
		res = executeQueryOrDie(conn,
								"SELECT quote_ident(n.nspname) || '.' || quote_ident(c.relname) AS badviewname "
								"FROM pg_class c JOIN pg_namespace n on c.relnamespace=n.oid "
								"WHERE c.relkind = 'v' AND "
								"view_has_anyarray_casts(c.oid) = TRUE;");

		PQclear(executeQueryOrDie(conn, "DROP FUNCTION view_has_anyarray_casts(OID);"));
		PQclear(executeQueryOrDie(conn, "SET search_path to 'pg_catalog';"));

		ntups = PQntuples(res);
		i_viewname = PQfnumber(res, "badviewname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_fatal("Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s \n",
					PQgetvalue(res, rowno, i_viewname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains views having anyarray\n"
				"| casts. Drop the view or recreate the view without explicit \n"
				"| array-type type casts before running the upgrade. Alternatively, drop the view \n"
				"| before the upgrade and recreate the view after the upgrade. \n"
				"| A list of views is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_views_with_fabricated_unknown_casts()
{
	char		*check_name = "Checking for non-dumpable views with unknown casts";
	char		output_path[MAXPGPATH];
	FILE		*script = NULL;
	bool		found = false;
	int			dbnum;
	int			i_viewname;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "view_unknown_casts.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;
		bool		db_used = false;

		conn = connectToServer(&old_cluster, active_db->db_name);
		PQclear(executeQueryOrDie(conn, "SET search_path TO 'public';"));

		/* Install check support function */
		PQclear(executeQueryOrDie(conn,
									 "CREATE OR REPLACE FUNCTION "
									 "view_has_unknown_casts(OID) "
									 "RETURNS BOOL "
									 "AS '$libdir/pg_upgrade_support' "
									 "LANGUAGE C STRICT;"));
		res = executeQueryOrDie(conn,
								"SELECT quote_ident(n.nspname) || '.' || quote_ident(c.relname) AS badviewname "
								"FROM pg_class c JOIN pg_namespace n on c.relnamespace=n.oid "
								"WHERE c.relkind = 'v' AND "
								"view_has_unknown_casts(c.oid) = TRUE;");

		PQclear(executeQueryOrDie(conn, "DROP FUNCTION view_has_unknown_casts(OID);"));
		PQclear(executeQueryOrDie(conn, "SET search_path to 'pg_catalog';"));

		ntups = PQntuples(res);
		i_viewname = PQfnumber(res, "badviewname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_fatal("Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s \n",
					PQgetvalue(res, rowno, i_viewname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains views having unknown\n"
				"| casts. Drop the view or recreate the view without explicit \n"
				"| unknown::cstring type casts before running the upgrade.\n"
				"| A list of views is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_views_referencing_deprecated_tables()
{
	char		*check_name = "Checking for views referencing deprecated tables";
	char		output_path[MAXPGPATH];
	FILE		*script = NULL;
	bool		found = false;
	int			dbnum;
	int			i_viewname;

	/*
	 * An upgrade check for deprecated objects only applies to a major version
	 * upgrade.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) ==
		GET_MAJOR_VERSION(new_cluster.major_version))
		return;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "view_deprecated_tables.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;
		bool		db_used = false;

		conn = connectToServer(&old_cluster, active_db->db_name);
		PQclear(executeQueryOrDie(conn, "SET search_path TO 'public';"));

		/* Install check support function */
		PQclear(executeQueryOrDie(conn,
								  "CREATE OR REPLACE FUNCTION "
								  "view_references_deprecated_tables(OID) "
								  "RETURNS BOOL "
								  "AS '$libdir/pg_upgrade_support' "
								  "LANGUAGE C STRICT;"));
		res = executeQueryOrDie(conn,
								"SELECT quote_ident(n.nspname) || '.' || quote_ident(c.relname) AS badviewname "
								"FROM pg_class c JOIN pg_namespace n on c.relnamespace=n.oid "
								"WHERE c.relkind = 'v' AND "
								"view_references_deprecated_tables(c.oid) = TRUE AND "
								"n.nspname NOT IN ('pg_catalog', 'gp_toolkit');");

		PQclear(executeQueryOrDie(conn, "DROP FUNCTION view_references_deprecated_tables(OID);"));
		PQclear(executeQueryOrDie(conn, "SET search_path to 'pg_catalog';"));

		ntups = PQntuples(res);
		i_viewname = PQfnumber(res, "badviewname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_fatal("Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s \n",
					PQgetvalue(res, rowno, i_viewname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains views referencing catalog\n"
				"| tables that no longer exist in the target cluster.\n"
				"| Drop these views before running the upgrade. Please refer to\n"
				"| the documentation for a complete list of deprecated tables.\n"
				"| A list of such views is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_views_referencing_deprecated_columns()
{
	char		*check_name = "Checking for views referencing deprecated columns";
	char		output_path[MAXPGPATH];
	FILE		*script = NULL;
	bool		found = false;
	int			dbnum;
	int			i_viewname;

	/*
	 * An upgrade check for deprecated objects only applies to a major version
	 * upgrade.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) ==
		GET_MAJOR_VERSION(new_cluster.major_version))
		return;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "view_deprecated_columns.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;
		bool		db_used = false;

		conn = connectToServer(&old_cluster, active_db->db_name);
		PQclear(executeQueryOrDie(conn, "SET search_path TO 'public';"));

		/* Install check support function */
		PQclear(executeQueryOrDie(conn,
								  "CREATE OR REPLACE FUNCTION "
								  "view_references_deprecated_columns(OID) "
								  "RETURNS BOOL "
								  "AS '$libdir/pg_upgrade_support' "
								  "LANGUAGE C STRICT;"));
		res = executeQueryOrDie(conn,
								"SELECT quote_ident(n.nspname) || '.' || quote_ident(c.relname) AS badviewname "
								"FROM pg_class c JOIN pg_namespace n on c.relnamespace=n.oid "
								"WHERE c.relkind = 'v' AND "
								"view_references_deprecated_columns(c.oid) = TRUE AND "
								"n.nspname NOT IN ('pg_catalog', 'gp_toolkit', 'information_schema');");

		PQclear(executeQueryOrDie(conn, "DROP FUNCTION view_references_deprecated_columns(OID);"));
		PQclear(executeQueryOrDie(conn, "SET search_path to 'pg_catalog';"));

		ntups = PQntuples(res);
		i_viewname = PQfnumber(res, "badviewname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_fatal("Could not create necessary file:  %s\n", output_path);
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s \n",
					PQgetvalue(res, rowno, i_viewname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains views referencing columns\n"
				"| in catalog tables that no longer exist in the target cluster.\n"
				"| Drop these views before running the upgrade. Please refer to\n"
				"| the documentation for a complete list of deprecated columns.\n"
				"| A list of such views is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_parent_partitions_with_seg_entries(void)
{
	char	   *check_name = "Checking AO/CO parent partitions with pg_aoseg entries";
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "parent_partitions_with_seg_entries.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult    *res;
		int			ntups;
		DbInfo	    *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	    *conn;
		bool		db_used = false;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
							"SELECT relid::regclass AS ao_root_relname, segrelid::regclass AS ao_root_segrelname\n"
								"FROM  pg_appendonly a JOIN pg_class c ON a.relid = c.oid\n"
								"WHERE c.oid IN (SELECT parrelid FROM pg_partition\n"
								"                 UNION SELECT parchildrelid\n"
								"                 FROM pg_partition_rule)\n"
								"      AND c.relhassubclass = true\n"
								"      AND a.relid IS NOT NULL\n"
								"      AND a.segrelid IS NOT NULL\n"
								"ORDER BY 1;");

        ntups = PQntuples(res);

        for (int rowno = 0; rowno < ntups; rowno++)
        {
			char	   *ao_parent_relname = PQgetvalue(res, rowno, 0);
			char	   *ao_parent_segrelname = PQgetvalue(res, rowno, 1);
			PGresult   *ao_parent_segrel_result = executeQueryOrDie(conn, "SELECT 1 FROM %s;", ao_parent_segrelname);

			if (PQntuples(ao_parent_segrel_result) > 0)
			{
				found = true;
				if (script == NULL && (script = fopen(output_path, "w")) == NULL)
					pg_fatal("Could not create necessary file:  %s\n", output_path);
				if (!db_used)
				{
					fprintf(script, "Database: %s\n", active_db->db_name);
					db_used = true;
                }
				fprintf(script, "  %s has non empty segrel %s\n", ao_parent_relname, ao_parent_segrelname);
            }

			PQclear(ao_parent_segrel_result);
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains append-only or column-oriented\n"
				"| parent partitions that contain entries in their pg_aoseg or pg_aocsseg\n"
				"| tables respectively. Delete all rows from these pg_aoseg or pg_aocsseg \n"
				"| tables before upgrading. A list of the problem tables is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);
}

static void
check_distributed_on_duplicate_columns(void)
{
	/*
	 * Distribution on duplicate columns is already fixed in GPDB6+. Also, the
	 * column attrnums in gp_distribution_policy does not exist on GPDB6+.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 804)
		return;

	char	   *check_name = "Checking for tables distributed on duplicated columns";
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	start_parallel_check(check_name);

	snprintf(output_path, sizeof(output_path), "duplicate_column_distribution.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_tablename,
					i_attrnums;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		res = executeQueryOrDie(conn,
								"WITH subquery1 AS ("
								"    SELECT localoid::regclass AS tablename, "
								"           unnest(attrnums) AS attrnum "
								"    FROM gp_distribution_policy dp "
								"    GROUP BY localoid, attrnum "
								"    HAVING count(*) > 1 "
								"), subquery2 AS ( "
								"    SELECT tablename, array_agg(attrnum ORDER BY attrnum) AS sorted_attrnums "
								"    FROM subquery1 "
								"    GROUP BY tablename "
								"    ORDER BY tablename "
								")"
								"SELECT tablename, replace(array_to_string(sorted_attrnums, ', '), '{', '') AS attrnums "
								"FROM subquery2;");

		ntups = PQntuples(res);
		i_tablename = PQfnumber(res, "tablename");
		i_attrnums = PQfnumber(res, "attrnums");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
				pg_fatal("Could not open file \"%s\": %s\n",
						 output_path, getErrorText());
			if (!db_used)
			{
				fprintf(script, "Database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s: %s\n",
					PQgetvalue(res, rowno, i_tablename),
					PQgetvalue(res, rowno, i_attrnums));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		parallel_gp_fatal_log(
				check_name,
				"| Your installation contains tables distributed on duplicated columns.\n"
				"| Update the distribution policy of the tables so there are no duplicated\n"
				"| columns and restart the upgrade. A list of the problem tables and its\n"
				"| duplicated columns is in the file:\n"
				"| \t%s\n\n", output_path);
	}
	else
		parallel_check_ok(check_name);

}
