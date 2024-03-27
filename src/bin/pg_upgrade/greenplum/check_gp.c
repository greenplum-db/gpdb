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
 *	Copyright (c) 2017-Present VMware, Inc. or its affiliates
 *	contrib/pg_upgrade/check_gp.c
 */

#include "pg_upgrade_greenplum.h"
#include "catalog/pg_class_d.h"

#define RELSTORAGE_EXTERNAL	'x'

static void check_external_partition(void);
static void check_covering_aoindex(void);
static void check_partition_indexes(void);
static void check_orphaned_toastrels(void);
static void check_online_expansion(void);
static void check_for_array_of_partition_table_types(ClusterInfo *cluster);
static void check_multi_column_list_partition_keys(ClusterInfo *cluster);
static void check_for_plpython2_dependent_functions(ClusterInfo *cluster);
static void check_views_with_removed_operators(void);
static void check_views_with_removed_functions(void);
static void check_views_with_removed_types(void);
static void check_for_disallowed_pg_operator(void);

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
	if (!check_not_in_place())
	{
		check_online_expansion();
		check_orphaned_toastrels();
	}
	check_external_partition();
	check_covering_aoindex();
	check_partition_indexes();
	check_for_array_of_partition_table_types(&old_cluster);
	check_multi_column_list_partition_keys(&old_cluster);
	check_for_plpython2_dependent_functions(&old_cluster);
	check_views_with_removed_operators();
	check_views_with_removed_functions();
	check_views_with_removed_types();
	check_for_disallowed_pg_operator();
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
	bool		expansion = false;
	int			dbnum;

	/*
	 * We only need to check the cluster expansion status on master.
	 * On the other hand the status can not be detected correctly on segments.
	 */
	if (!is_greenplum_dispatcher_mode())
		return;

	prep_status("Checking for online expansion status");

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
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation is in progress of online expansion,\n"
			   "| must complete that job before the upgrade.\n\n");
	}
	else
		check_ok();
}

/*
 *	check_external_partition
 *
 *	External tables cannot be included in the partitioning hierarchy during the
 *	initial definition with CREATE TABLE, they must be defined separately and
 *	injected via ALTER TABLE EXCHANGE. The partitioning system catalogs are
 *	however not replicated onto the segments which means ALTER TABLE EXCHANGE
 *	is prohibited in utility mode. This means that pg_upgrade cannot upgrade a
 *	cluster containing external partitions, they must be handled manually
 *	before/after the upgrade.
 *
 *	Check for the existence of external partitions and refuse the upgrade if
 *	found.
 */
static void
check_external_partition(void)
{
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	/*
	 * This was only a problem with GPDB 6 and below.
	 *
	 * GPDB_UPGRADE_FIXME: Could we support upgrading these to GPDB 7,
	 * even though it wasn't possible before? The upstream syntax used in
	 * GPDB 7 to recreate the partition hierarchy is more flexible, and
	 * could possibly handle this. If so, we could remove this check
	 * entirely.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 1000)
		return;

	prep_status("Checking for external tables used in partitioning");

	snprintf(output_path, sizeof(output_path), "%s/%s",
			 log_opts.basedir, "external_partitions.txt");

	/*
	 * We need to query the inheritance catalog rather than the partitioning
	 * catalogs since they are not available on the segments.
	 */

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn;

		conn = connectToServer(&old_cluster, active_db->db_name);
		res = executeQueryOrDie(conn,
			 "SELECT cc.relname, c.relname AS partname, c.relnamespace "
			 "FROM   pg_inherits i "
			 "       JOIN pg_class c ON (i.inhrelid = c.oid AND c.relstorage = '%c') "
			 "       JOIN pg_class cc ON (i.inhparent = cc.oid);",
			 RELSTORAGE_EXTERNAL);

		ntups = PQntuples(res);

		if (ntups > 0)
		{
			found = true;

			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_log(PG_FATAL, "Could not create necessary file:  %s\n",
					   output_path);

			for (rowno = 0; rowno < ntups; rowno++)
			{
				fprintf(script, "External partition \"%s\" in relation \"%s\"\n",
						PQgetvalue(res, rowno, PQfnumber(res, "partname")),
						PQgetvalue(res, rowno, PQfnumber(res, "relname")));
			}
		}

		PQclear(res);
		PQfinish(conn);
	}
	if (found)
	{
		fclose(script);
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation contains partitioned tables with external\n"
			   "| tables as partitions.  These partitions need to be removed\n"
			   "| from the partition hierarchy before the upgrade.  A list of\n"
			   "| external partitions to remove is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
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
	char			output_path[MAXPGPATH];
	FILE		   *script = NULL;
	bool			found = false;
	int				dbnum;

	prep_status("Checking for non-covering indexes on partitioned AO tables");

	snprintf(output_path, sizeof(output_path), "%s/%s",
			 log_opts.basedir, "mismatched_aopartition_indexes.txt");

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
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation contains partitioned append-only tables\n"
			   "| with an index defined on the partition parent which isn't\n"
			   "| present on all partition members.  These indexes must be\n"
			   "| dropped before the upgrade.  A list of relations, and the\n"
			   "| partitions in question is in the file:\n"
			   "| \t%s\n\n", output_path);

	}
	else
		check_ok();
}

static void
check_orphaned_toastrels(void)
{
	bool			found = false;
	int				dbnum;
	char			output_path[MAXPGPATH];
	FILE		   *script = NULL;

	prep_status("Checking for orphaned TOAST relations");

	snprintf(output_path, sizeof(output_path), "%s/%s",
			 log_opts.basedir, "orphaned_toast_tables.txt");

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
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation contains orphaned toast tables which\n"
			   "| must be dropped before upgrade.\n"
			   "| A list of the problem databases is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();

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
	int				dbnum;
	FILE		   *script = NULL;
	bool			found = false;
	char			output_path[MAXPGPATH];

	/*
	 * This was only a problem with GPDB 6 and below.
	 *
	 * GPDB_UPGRADE_FIXME: Could we support upgrading these to GPDB 7,
	 * even though it wasn't possible before? The upstream syntax used in
	 * GPDB 7 to recreate the partition hierarchy is more flexible, and
	 * could possibly handle this. If so, we could remove this check
	 * entirely.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 1000)
		return;

	prep_status("Checking for indexes on partitioned tables");

	snprintf(output_path, sizeof(output_path), "%s/%s",
			 log_opts.basedir, "partitioned_tables_indexes.txt");

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
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation contains partitioned tables with\n"
			   "| indexes defined on them.  Indexes on partition parents,\n"
			   "| as well as children, must be dropped before upgrade.\n"
			   "| A list of the problem tables is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
}

static void
check_for_array_of_partition_table_types(ClusterInfo *cluster)
{
	const char *const SEPARATOR = "\n";
	int			dbnum;
	char	   *dependee_partition_report = palloc0(1);

	/*
	 * This was only a problem with GPDB 6 and below.
	 *
	 * GPDB_UPGRADE_FIXME: Could we support upgrading these to GPDB 7,
	 * even though it wasn't possible before? The upstream syntax used in
	 * GPDB 7 to recreate the partition hierarchy is more flexible, and
	 * could possibly handle this. If so, we could remove this check
	 * entirely.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 1000)
		return;

	prep_status("Checking array types derived from partitions");

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			n_tables_to_check;
		int			i;

		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(cluster, active_db->db_name);

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
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			"| Array types derived from partitions of a partitioned table must not have dependants.\n"
			"| OIDs of such types found and their original partitions:\n%s", dependee_partition_report);
	}
	pfree(dependee_partition_report);

	check_ok();
}

static void
check_multi_column_list_partition_keys(ClusterInfo *cluster)
{
	char			output_path[MAXPGPATH];
	FILE		   *script = NULL;
	bool			found = false;
	int				dbnum;

	if (GET_MAJOR_VERSION(cluster->major_version) > 904)
		return;

	prep_status("Checking for multi-column LIST partition keys");

	snprintf(output_path, sizeof(output_path), "%s/%s",
			 log_opts.basedir, "multi_column_list_partitions.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname,
					i_relname;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		res = executeQueryOrDie(conn,
			 "SELECT n.nspname, c.relname "
			 "FROM pg_class c "
			 "JOIN pg_namespace n on n.oid=c.relnamespace "
			 "JOIN pg_partition p on p.parrelid=c.oid "
			 "WHERE parkind = 'l' and parnatts > 1");

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_relname = PQfnumber(res, "relname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
				pg_fatal("could not open file \"%s\": %s\n",
						 output_path, strerror(errno));
			if (!db_used)
			{
				fprintf(script, "In database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s.%s\n",
					PQgetvalue(res, rowno, i_nspname),
					PQgetvalue(res, rowno, i_relname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		fclose(script);
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation contains partitioned tables\n"
			   "| with a LIST partition key containing multiple\n"
			   "| columns, which is not supported anymore. Consider\n"
			   "| modifying the partition key to use a single column\n"
			   "| or dropping the tables. A list of the problem tables\n"
			   "| is in the file:\n\t%s\n\n", output_path);
	}
	else
		check_ok();
}

static void
check_for_plpython2_dependent_functions(ClusterInfo *cluster)
{

	FILE		*script = NULL;
	char		output_path[MAXPGPATH];
	bool		found = false;

	prep_status("Checking for functions dependent on plpython2");

	snprintf(output_path, sizeof(output_path), "%s/%s",
			 log_opts.basedir,
			 "plpython2_dependent_functions.txt");

	for (int dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname,
					i_proname;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(cluster, active_db->db_name);

		/* Find any functions dependent on $libdir/plpython2' */
		res = executeQueryOrDie(conn,
								"SELECT n.nspname, p.proname "
								"FROM pg_catalog.pg_proc p "
								"JOIN pg_namespace n on n.oid=p.pronamespace "
								"JOIN pg_language l on l.oid=p.prolang "
								"JOIN pg_pltemplate t on t.tmplname = l.lanname "
								"WHERE t.tmpllibrary = '$libdir/plpython2'");

		ntups = PQntuples(res);
		i_nspname = PQfnumber(res, "nspname");
		i_proname = PQfnumber(res, "proname");
		for (rowno = 0; rowno < ntups; rowno++)
		{
			found = true;
			if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
				pg_fatal("could not open file \"%s\": %s\n",
						 output_path, strerror(errno));
			if (!db_used)
			{
				fprintf(script, "In database: %s\n", active_db->db_name);
				db_used = true;
			}
			fprintf(script, "  %s.%s\n",
					PQgetvalue(res, rowno, i_nspname),
					PQgetvalue(res, rowno, i_proname));
		}

		PQclear(res);

		PQfinish(conn);
	}

	if (script)
		fclose(script);

	if (found)
	{
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
				"| Your installation contains \"plpython\" functions which rely\n"
				"| on Python 2. These functions must be either be updated to use\n"
				"| Python 3 or dropped before upgrade. A list of the problem functions\n"
				"| is in the file:\n"
				"|     %s\n\n", output_path);
	}
	else
		check_ok();
}

void
setup_GPDB6_data_type_checks(ClusterInfo *cluster)
{
	if (GET_MAJOR_VERSION(cluster->major_version) > 904)
		return;

	for (int dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		DbInfo *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn *conn = connectToServer(cluster, active_db->db_name);
		PGresult *res = executeQueryOrDie(conn,
										  "SET CLIENT_MIN_MESSAGES = WARNING; "
										  "DROP SCHEMA IF EXISTS __gpupgrade_tmp CASCADE; "
										  "CREATE SCHEMA __gpupgrade_tmp; "
										  "CREATE FUNCTION __gpupgrade_tmp.data_type_checks(base_query TEXT) "
										  "RETURNS TABLE ( "
										  "    nspname NAME, "
										  "    relname NAME, "
										  "    attname NAME "
										  ") "
										  "AS $$ "
										  "DECLARE "
										  "    result_oids REGTYPE[]; "
										  "    base_oids REGTYPE[]; "
										  "    dependent_oids REGTYPE[]; "
										  "BEGIN "
										  "    EXECUTE base_query INTO base_oids; "
										  "    dependent_oids = base_oids; "
										  "    result_oids = base_oids; "
										  "    WHILE array_length(dependent_oids, 1) IS NOT NULL LOOP "
										  "        dependent_oids := ARRAY( "
										  "            SELECT t.oid "
										  "            FROM ( "
										  "                SELECT t.oid "
										  "                FROM pg_catalog.pg_type t, unnest(dependent_oids) AS x(oid) "
										  "                WHERE typbasetype = x.oid AND typtype = 'd' "
										  "                UNION ALL "
										  "                SELECT t.oid "
										  "                FROM pg_catalog.pg_type t, unnest(dependent_oids) AS x(oid) "
										  "                WHERE typelem = x.oid AND typtype = 'b' "
										  "                UNION ALL "
										  "                SELECT t.oid "
										  "                FROM pg_catalog.pg_type t "
										  "                JOIN pg_catalog.pg_class c ON t.oid = c.reltype "
										  "                JOIN pg_catalog.pg_attribute a ON c.oid = a.attrelid "
										  "                WHERE t.typtype = 'c' "
										  "                    AND NOT a.attisdropped "
										  "                    AND a.atttypid = ANY(dependent_oids) "
										  "                UNION ALL "
										  "                SELECT t.oid "
										  "                FROM pg_catalog.pg_type t, pg_catalog.pg_range r, unnest(dependent_oids) AS x(oid) "
										  "                WHERE t.typtype = 'r' "
										  "                    AND r.rngtypid = t.oid "
										  "                    AND r.rngsubtype = x.oid "
										  "            ) AS t "
										  "        ); "
										  "        result_oids := result_oids || dependent_oids; "
										  "    END LOOP; "
										  "    RETURN QUERY "
										  "    SELECT n.nspname, c.relname, a.attname "
										  "    FROM pg_catalog.pg_class c "
										  "    JOIN pg_catalog.pg_namespace n ON c.relnamespace = n.oid "
										  "    JOIN pg_catalog.pg_attribute a ON c.oid = a.attrelid "
										  "    WHERE NOT a.attisdropped "
										  "        AND a.atttypid = ANY(result_oids) "
										  "        AND c.relkind IN ( "
													   CppAsString2(RELKIND_RELATION) ", "
													   CppAsString2(RELKIND_MATVIEW) ", "
													   CppAsString2(RELKIND_INDEX)
										  "        ) "
										  "        AND n.nspname !~ '^pg_temp_' "
										  "        AND n.nspname !~ '^pg_toast_temp_' "
										  "        AND n.nspname NOT IN ('pg_catalog', 'information_schema'); "
										  "END; "
										  "$$ LANGUAGE plpgsql; "
										  "RESET CLIENT_MIN_MESSAGES;");

		PQclear(res);
		PQfinish(conn);
	}
}

void
teardown_GPDB6_data_type_checks(ClusterInfo *cluster)
{
	if (GET_MAJOR_VERSION(cluster->major_version) > 904)
		return;

	for (int dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		DbInfo *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn *conn = connectToServer(cluster, active_db->db_name);
		PGresult *res = executeQueryOrDie(conn,
										  "SET CLIENT_MIN_MESSAGES = WARNING; "
										  "DROP SCHEMA __gpupgrade_tmp CASCADE; "
										  "RESET CLIENT_MIN_MESSAGES;");

		PQclear(res);
		PQfinish(conn);
	}

}

static void
check_views_with_removed_operators()
{
	if (GET_MAJOR_VERSION(old_cluster.major_version) > 904)
		return;

	char  output_path[MAXPGPATH];
	FILE *script = NULL;
	bool  found = false;
	int   dbnum;
	int   i_viewname;

	prep_status("Checking for views with removed operators");

	snprintf(output_path, sizeof(output_path), "views_with_removed_operators.txt");

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

		/*
		 * Disabling track_counts results in a large performance improvement of
		 * several orders of magnitude when walking the views. This is because
		 * calling try_relation_open to get a handle of the view calls
		 * pgstat_initstats which has been profiled to be very expensive. For
		 * our purposes, this is not needed and disabled for performance.
		 */
		PQclear(executeQueryOrDie(conn, "SET track_counts TO off;"));

		/* Install check support function */
		PQclear(executeQueryOrDie(conn,
								  "CREATE OR REPLACE FUNCTION "
								  "view_has_removed_operators(OID) "
								  "RETURNS BOOL "
								  "AS '$libdir/pg_upgrade_support' "
								  "LANGUAGE C STRICT;"));
		res = executeQueryOrDie(conn,
								"SELECT quote_ident(n.nspname) || '.' || quote_ident(c.relname) AS badviewname "
								"FROM pg_class c JOIN pg_namespace n on c.relnamespace=n.oid "
								"WHERE c.relkind = 'v' AND "
								"view_has_removed_operators(c.oid) = TRUE;");

		PQclear(executeQueryOrDie(conn, "DROP FUNCTION view_has_removed_operators(OID);"));
		PQclear(executeQueryOrDie(conn, "SET search_path to 'pg_catalog';"));
		PQclear(executeQueryOrDie(conn, "RESET track_counts;"));

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
			fprintf(script, "  %s\n", PQgetvalue(res, rowno, i_viewname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation contains views using removed operators.\n"
			   "| These operators are no longer present on the target version.\n"
			   "| These views must be updated to use operators supported in the\n"
			   "| target version or removed before upgrade can continue. A list\n"
			   "| of the problem views is in the file:\n\t%s\n\n", output_path);
	}
	else
		check_ok();
}

static void
check_views_with_removed_functions()
{
	if (GET_MAJOR_VERSION(old_cluster.major_version) > 904)
		return;

	char  output_path[MAXPGPATH];
	FILE *script = NULL;
	bool  found = false;
	int   dbnum;
	int   i_viewname;

	prep_status("Checking for views with removed functions");

	snprintf(output_path, sizeof(output_path), "views_with_removed_functions.txt");

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

		/*
		 * Disabling track_counts results in a large performance improvement of
		 * several orders of magnitude when walking the views. This is because
		 * calling try_relation_open to get a handle of the view calls
		 * pgstat_initstats which has been profiled to be very expensive. For
		 * our purposes, this is not needed and disabled for performance.
		 */
		PQclear(executeQueryOrDie(conn, "SET track_counts TO off;"));

		/* Install check support function */
		PQclear(executeQueryOrDie(conn,
								  "CREATE OR REPLACE FUNCTION "
								  "view_has_removed_functions(OID) "
								  "RETURNS BOOL "
								  "AS '$libdir/pg_upgrade_support' "
								  "LANGUAGE C STRICT;"));
		res = executeQueryOrDie(conn,
								"SELECT quote_ident(n.nspname) || '.' || quote_ident(c.relname) AS badviewname "
								"FROM pg_class c JOIN pg_namespace n on c.relnamespace=n.oid "
								"WHERE c.relkind = 'v' "
								"AND c.oid >= 16384 "
								"AND view_has_removed_functions(c.oid) = TRUE;");

		PQclear(executeQueryOrDie(conn, "DROP FUNCTION view_has_removed_functions(OID);"));
		PQclear(executeQueryOrDie(conn, "SET search_path to 'pg_catalog';"));
		PQclear(executeQueryOrDie(conn, "RESET track_counts;"));

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
			fprintf(script, "  %s\n", PQgetvalue(res, rowno, i_viewname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation contains views using removed functions.\n"
			   "| These functions are no longer present on the target version.\n"
			   "| These views must be updated to use functions supported in the\n"
			   "| target version or removed before upgrade can continue. A list\n"
			   "| of the problem views is in the file:\n\t%s\n\n", output_path);
	}
	else
		check_ok();
}

static void
check_views_with_removed_types()
{
	if (GET_MAJOR_VERSION(old_cluster.major_version) > 904)
		return;

	char  output_path[MAXPGPATH];
	FILE *script = NULL;
	bool  found = false;
	int   dbnum;
	int   i_viewname;

	prep_status("Checking for views with removed types");

	snprintf(output_path, sizeof(output_path), "views_with_removed_types.txt");

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

		/*
		 * Disabling track_counts results in a large performance improvement of
		 * several orders of magnitude when walking the views. This is because
		 * calling try_relation_open to get a handle of the view calls
		 * pgstat_initstats which has been profiled to be very expensive. For
		 * our purposes, this is not needed and disabled for performance.
		 */
		PQclear(executeQueryOrDie(conn, "SET track_counts TO off;"));

		/* Install check support function */
		PQclear(executeQueryOrDie(conn,
								  "CREATE OR REPLACE FUNCTION "
								  "view_has_removed_types(OID) "
								  "RETURNS BOOL "
								  "AS '$libdir/pg_upgrade_support' "
								  "LANGUAGE C STRICT;"));
		res = executeQueryOrDie(conn,
								"SELECT quote_ident(n.nspname) || '.' || quote_ident(c.relname) AS badviewname "
								"FROM pg_class c JOIN pg_namespace n on c.relnamespace=n.oid "
								"WHERE c.relkind = 'v' "
								"AND c.oid >= 16384 "
								"AND view_has_removed_types(c.oid) = TRUE;");

		PQclear(executeQueryOrDie(conn, "DROP FUNCTION view_has_removed_types(OID);"));
		PQclear(executeQueryOrDie(conn, "SET search_path to 'pg_catalog';"));
		PQclear(executeQueryOrDie(conn, "RESET track_counts;"));

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
			fprintf(script, "  %s\n", PQgetvalue(res, rowno, i_viewname));
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (found)
	{
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
			   "| Your installation contains views using removed types.\n"
			   "| These types are no longer present on the target version.\n"
			   "| These views must be updated to use types supported in the\n"
			   "| target version or removed before upgrade can continue. A list\n"
			   "| of the problem views is in the file:\n\t%s\n\n", output_path);
	}
	else
		check_ok();
}

/*
 * check_for_disallowed_pg_operator(void)
 *
 * Versions greater than 9.4 disallows `CREATE OPERATOR =>`
 */
static void
check_for_disallowed_pg_operator(void)
{
	int			dbnum;
	char		output_path[MAXPGPATH];
	bool		found = false;
	FILE		*script = NULL;

	if (GET_MAJOR_VERSION(old_cluster.major_version) > 904)
		return;

	prep_status("Checking for disallowed OPERATOR =>");

	snprintf(output_path, sizeof(output_path), "%s/%s",
			 log_opts.basedir,
			 "databases_with_disallowed_pg_operator.txt");

	for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		DbInfo	   *active_db = &old_cluster.dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(&old_cluster, active_db->db_name);

		/* Find any disallowed operator '=>' in all the databases */
		res = executeQueryOrDie(conn,
								"SELECT * "
								"FROM pg_operator "
								"WHERE oprname = '=>' AND "
								"	   oid >= 16384");

		if (PQntuples(res) != 0)
		{
			found = true;
			if (script == NULL && (script = fopen(output_path, "w")) == NULL)
				pg_fatal("Could not create necessary file:  %s\n", output_path);

			fprintf(script, "%s\n", active_db->db_name);
		}

		PQclear(res);
		PQfinish(conn);
	}

	if (script)
		fclose(script);

	if (found)
	{
		pg_log(PG_REPORT, "fatal\n");
		gp_fatal_log(
					 "| Your installation contains disallowed OPERATOR =>.\n"
					 "| You will need to remove the disallowed OPERATOR =>\n"
					 "| from the list of databases in the file:\n"
					 "|    %s\n\n", output_path);
	}
	else
		check_ok();
}
