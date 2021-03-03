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

#define RELSTORAGE_EXTERNAL	'x'

static void check_external_partition(void);
static void check_covering_aoindex(void);
static void check_partition_indexes(void);
static void check_orphaned_toastrels(void);
static void check_online_expansion(void);
static void check_gphdfs_external_tables(void);
static void check_gphdfs_user_roles(void);
static void check_for_array_of_partition_table_types(ClusterInfo *cluster);


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
	check_online_expansion();
	check_external_partition();
	check_covering_aoindex();
	check_partition_indexes();
	check_orphaned_toastrels();
	check_gphdfs_external_tables();
	check_gphdfs_user_roles();
	check_for_array_of_partition_table_types(&old_cluster);
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
		pg_log(PG_FATAL,
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
	 * GPDB_12_MERGE_FIXME: Could we support upgrading these to GPDB 7,
	 * even though it wasn't possible before? The upstream syntax used in
	 * GPDB 7 to recreate the partition hierarchy is more flexible, and
	 * could possibly handle this. If so, we could remove this check
	 * entirely.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 1000)
		return;

	prep_status("Checking for external tables used in partitioning");

	snprintf(output_path, sizeof(output_path), "external_partitions.txt");
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
		pg_log(PG_FATAL,
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
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
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
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
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
	 * GPDB_12_MERGE_FIXME: Could we support upgrading these to GPDB 7,
	 * even though it wasn't possible before? The upstream syntax used in
	 * GPDB 7 to recreate the partition hierarchy is more flexible, and
	 * could possibly handle this. If so, we could remove this check
	 * entirely.
	 */
	if (GET_MAJOR_VERSION(old_cluster.major_version) >= 1000)
		return;

	prep_status("Checking for indexes on partitioned tables");

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
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains partitioned tables with\n"
			   "| indexes defined on them.  Indexes on partition parents,\n"
			   "| as well as children, must be dropped before upgrade.\n"
			   "| A list of the problem tables is in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
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
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	/* GPDB only supported gphdfs in this version range */
	if (!(old_cluster.major_version >= 80215 && old_cluster.major_version < 80400))
		return;

	prep_status("Checking for gphdfs external tables");

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
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains gphdfs external tables.  These \n"
			   "| tables need to be dropped before upgrade.  A list of\n"
			   "| external gphdfs tables to remove is provided in the file:\n"
			   "| \t%s\n\n", output_path);
	}
	else
		check_ok();
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

	prep_status("Checking for users assigned the gphdfs role");

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
		pg_log(PG_REPORT, "fatal\n");
		pg_log(PG_FATAL,
			   "| Your installation contains roles that have gphdfs privileges.\n"
			   "| These privileges need to be revoked before upgrade.  A list\n"
			   "| of roles and their corresponding gphdfs privileges that\n"
			   "| must be revoked is provided in the file:\n"
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
	 * GPDB_12_MERGE_FIXME: Could we support upgrading these to GPDB 7,
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
		pg_fatal(
			"Array types derived from partitions of a partitioned table must not have dependants.\n"
			"OIDs of such types found and their original partitions:\n%s",
			dependee_partition_report
		);
	}
	pfree(dependee_partition_report);

	check_ok();
}
