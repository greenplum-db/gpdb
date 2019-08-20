/*
 * contrib/pg_upgrade/gp/checks.h
 *
 * Definitions of Greenplum-specific check functions
 */

#include "pg_upgrade.h"

#include "checks.h"

/*
 *	check_online_expansion
 *
 *	Check for online expansion status and refuse the upgrade if online
 *	expansion is in progress.
 */
bool
check_online_expansion(ClusterInfo *cluster)
{
	bool		expansion = false;
	int			dbnum;

	/*
	 * Only need to check cluster expansion status in gpdb6 or later.
	 */
	if (GET_MAJOR_VERSION(cluster->major_version) < 804)
		return true;

	/*
	 * We only need to check the cluster expansion status on master. On the
	 * other hand the status can not be detected correctly on segments.
	 */
	if (user_opts.segment_mode == SEGMENT)
		return true;

	prep_status("Checking for online expansion status");

	/* Check if the cluster is in expansion status */
	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn	   *conn;

		conn = connectToServer(cluster, active_db->db_name);
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
		report_failure(
						 "| Your installation is in progress of online expansion,\n"
						 "| must complete that job before the upgrade.\n\n");
		return false;
	}

	return true;
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
bool
check_external_partition(ClusterInfo *cluster)
{
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	prep_status("Checking for external tables used in partitioning");

	snprintf(output_path, sizeof(output_path), "external_partitions.txt");

	/*
	 * We need to query the inheritance catalog rather than the partitioning
	 * catalogs since they are not available on the segments.
	 */

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn	   *conn;

		conn = connectToServer(cluster, active_db->db_name);
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
		report_failure("| Your installation contains partitioned tables with external\n"
						 "| tables as partitions.  These partitions need to be removed\n"
						 "| from the partition hierarchy before the upgrade.  A list of\n"
						 "| external partitions to remove is in the file:\n"
						 "| \t%s\n\n", output_path);
		return false;
	}

	return true;
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
bool
check_covering_aoindex(ClusterInfo *cluster)
{
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	prep_status("Checking for non-covering indexes on partitioned AO tables");

	snprintf(output_path, sizeof(output_path), "mismatched_aopartition_indexes.txt");

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		PGconn	   *conn;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];

		conn = connectToServer(cluster, active_db->db_name);
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
		report_failure(
			"| Your installation contains partitioned append-only tables\n"
			"| with an index defined on the partition parent which isn't\n"
			"| present on all partition members.  These indexes must be\n"
			"| dropped before the upgrade.  A list of relations, and the\n"
			"| partitions in question is in the file:\n"
			"| \t%s\n\n", output_path);
		return false;

	}

	return true;
}

bool
check_orphaned_toastrels(ClusterInfo *cluster)
{
	bool		found = false;
	int			dbnum;
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;

	prep_status("Checking for orphaned TOAST relations");

	snprintf(output_path, sizeof(output_path), "partitioned_tables.txt");

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		PGconn	   *conn;
		int			ntups;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];

		conn = connectToServer(cluster, active_db->db_name);
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
		report_failure(
						 "| Your installation contains orphaned toast tables which\n"
						 "| must be dropped before upgrade.\n"
						 "| A list of the problem databases is in the file:\n"
						 "| \t%s\n\n", output_path);
		return false;
	}

	return true;
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
bool
check_partition_indexes(ClusterInfo *cluster)
{
	int			dbnum;
	FILE	   *script = NULL;
	bool		found = false;
	char		output_path[MAXPGPATH];

	prep_status("Checking for indexes on partitioned tables");

	snprintf(output_path, sizeof(output_path), "partitioned_tables_indexes.txt");

	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		bool		db_used = false;
		int			ntups;
		int			rowno;
		int			i_nspname;
		int			i_relname;
		int			i_indexes;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn	   *conn = connectToServer(cluster, active_db->db_name);

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
		report_failure(
						 "| Your installation contains partitioned tables with\n"
						 "| indexes defined on them.  Indexes on partition parents,\n"
						 "| as well as children, must be dropped before upgrade.\n"
						 "| A list of the problem tables is in the file:\n"
						 "| \t%s\n\n", output_path);
		return false;
	}

	return true;
}

/*
 * check_gphdfs_external_tables
 *
 * Check if there are any remaining gphdfs external tables in the database.
 * We error if any gphdfs external tables remain and let the users know that,
 * any remaining gphdfs external tables have to be removed.
 */
bool
check_gphdfs_external_tables(ClusterInfo *cluster)
{
	char		output_path[MAXPGPATH];
	FILE	   *script = NULL;
	bool		found = false;
	int			dbnum;

	/* GPDB only supported gphdfs in this version range */
	if (!(cluster->major_version >= 80215 && cluster->major_version < 80400))
		return true;

	prep_status("Checking for gphdfs external tables");

	snprintf(output_path, sizeof(output_path), "gphdfs_external_tables.txt");


	for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
	{
		PGresult   *res;
		int			ntups;
		int			rowno;
		DbInfo	   *active_db = &cluster->dbarr.dbs[dbnum];
		PGconn	   *conn;

		conn = connectToServer(cluster, active_db->db_name);
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
		report_failure(
						 "| Your installation contains gphdfs external tables.  These \n"
						 "| tables need to be dropped before upgrade.  A list of\n"
						 "| external gphdfs tables to remove is provided in the file:\n"
						 "| \t%s\n\n", output_path);
		return false;
	}

	return true;
}

/*
 * check_gphdfs_user_roles
 *
 * Check if there are any remaining users with gphdfs roles.
 * We error if this is the case and let the users know how to proceed.
 */
bool
check_gphdfs_user_roles(ClusterInfo *cluster)
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
	if (!(cluster->major_version >= 80215 && cluster->major_version < 80400))
		return true;

	prep_status("Checking for users assigned the gphdfs role");

	snprintf(output_path, sizeof(output_path), "gphdfs_user_roles.txt");

	conn = connectToServer(cluster, "template1");
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
			bool		hasReadRole = (PQgetvalue(res, rowno, i_hdfs_read)[0] == 't');
			bool		hasWriteRole = (PQgetvalue(res, rowno, i_hdfs_write)[0] == 't');

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
		report_failure("| Your installation contains roles that have gphdfs privileges.\n"
						 "| These privileges need to be revoked before upgrade.  A list\n"
						 "| of roles and their corresponding gphdfs privileges that\n"
						 "| must be revoked is provided in the file:\n"
						 "| \t%s\n\n", output_path);
		return false;
	}

	return true;
}
