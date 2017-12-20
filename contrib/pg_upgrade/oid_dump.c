/*
 *	oid_dump.c
 *
 *	code for dumping object OIDs from system catalogs.
 *
 *
 * The OIDs of types and relations need to be preserved from the old cluster,
 * even in the upstream, because TOAST pointers contain the TOAST table's OID
 * and arrays contain the array type's OID. In the upstream, there's a
 * mechanism built into pg_dump --binary-upgrade mode, which includes calls
 * like "SELECT binary_upgrade.set_next_pg_type_oid(<oid>)" for  each type
 * and relation, to dictate which OID they will get. However,  things are
 * more complicated in GPDB, because of partitions, auxiliary AO segment
 * tables, and bitmap index LOV tables. Furthermore, we need to use the same
 * OIDs for every object, not just types and relations, between the QD node
 * and the segments. The QD node and the segments are pg_upgraded separately,
 * however. So we use a different strategy in GPDB.
 *
 * In GPDB, instead of setting the "next OID" just before creating a relation
 * or type, we issue calls like:
 *
 * SELECT binary_upgrade.preassign_type_oid(<oid>, <type name>, <namespace>);
 *
 * for every type and relation in the old cluster. The backend keeps a mapping
 * from the type name and namespace to OID in memory, and uses the preassigned
 * OIDs when the types/relations are created later on. These "preassign" calls
 * are constructed in get_old_oids() function, by reading the pg_type and
 * pg_class catalog tables from the old cluster, right after dumping the
 * schema with pg_dump.
 *
 * When restoring the schema, we may create more types or relations than what
 * existed in the old cluster. In particular, when upgrading from GPDB4 to
 * GPDB5, we may need to create array types for user-defined types that didn't
 * exist before. To ensure that the OIDs chosen for the new objects don't
 * conflict with OIDs of any objects from the old cluster that will be
 * restored later, the backend checks the list of pre-assigned OIDs when creating
 * an OID for a new object, and refrains from using an OID that's been pre-assigned
 * for another object. Note that this is different from the upstream. In the
 * upstream, pg_dump dictates the OIDs of *all* types and relations, even OIDs of
 * e.g. indexes that cannot appear in the upgraded tuple data, to avoid conflicts
 * between those OIDs that must be preserved and newly assigned ones.
 *
 * To ensure that the OIDs between QD and QE nodes match, even for objects that
 * didn't exist in the old cluster (like the array types), we use a similar
 * mechanism with binary_upgrade.preassign_*_oid() support functions. After
 * the QD node has been upgraded, pg_upgrade dumps the OIDs of *all* objects,
 * not only types and relations, into files named as "pg_upgrade_dump_*_oids.sql".
 * Note that these dump files are created from the new QD cluster, after
 * upgrading, so the SQL to create those files only need to work with the latest
 * server version. Those files should be transferred from the QD node to the
 * QE nodes, after the QD upgrade has finished. pg_upgrade will load the file
 * into memory, and inject it to the pg_dump output, just after the \connect
 * line.
 *
 *	Copyright (c) 2017, Pivotal Software Inc
 */

#include "pg_upgrade.h"
#include "pqexpbuffer.h"

#include "catalog/pg_namespace.h"
#include "catalog/pg_magic_oid.h"

static char *simple_escape_literal(migratorContext *ctx, PGconn *conn, const char *s);

static void dump_rows(migratorContext *ctx, PQExpBuffer buf, FILE *file, PGconn *conn, const char *sql, const char *funcname);

/*
 * Read all pg_type and pg_class OIDs from old cluster, into the DbInfo structs.
 *
 * The OIDs of types and relations need to be preserved from the old cluster.
 * In the upstream, we use a different mechanism, integrated into pg_dump, for
 * this, but things are a bit more complicated in GPDB, because of partitions,
 * auxiliary AO segment tables, and bitmap index LOV tables. So we use a
 * different strategy.
 */
void
get_old_oids(migratorContext *ctx)
{
	int			dbnum;

	prep_status(ctx, "Exporting object OIDs from the old cluster");

	for (dbnum = 0; dbnum < ctx->old.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &ctx->old.dbarr.dbs[dbnum];
		PGconn	   *conn;
		PQExpBuffer	buf = createPQExpBuffer();

		conn = connectToServer(ctx, olddb->db_name, CLUSTER_OLD);
		PQclear(executeQueryOrDie(ctx, conn, "set search_path='pg_catalog';"));

		dump_rows(ctx, buf, NULL, conn,
				  "SELECT oid, nspname FROM pg_namespace",
				  "preassign_namespace_oid");
		dump_rows(ctx, buf, NULL, conn,
				  "SELECT oid, typname, typnamespace FROM pg_type",
				  "preassign_type_oid");

		/*
		 * A table's TOAST table might not be named the usual way in the old
		 * cluster, so dump it according to the way it will be named when created
		 * in the new cluster, instead of the current name.
		 *
		 * For example, "alter_distpol_g_char_11_false_false", in the regression
		 * database. FIXME: I don't quite understand how that happens.
		 *
		 * We don't preserve the OIDs of AO segment tables.
		 */
		dump_rows(ctx, buf, NULL, conn,
				  "SELECT oid, relname, relnamespace FROM pg_class "
				  " WHERE relnamespace NOT IN "
				  "   ( " CppAsString2(PG_TOAST_NAMESPACE) ", "
				  "" CppAsString2(PG_AOSEGMENT_NAMESPACE) ") "
				  " AND oid >= " CppAsString2(FirstNormalObjectId) " "
				  "UNION ALL "
				  "SELECT reltoastrelid, 'pg_toast_' || oid, " CppAsString2(PG_TOAST_NAMESPACE) " FROM pg_class WHERE reltoastrelid <> 0 AND oid >= " CppAsString2(FirstNormalObjectId) " ",
				  "preassign_relation_oid");

		PQfinish(conn);

		olddb->reserved_oids = buf->data;
	}

	check_ok(ctx);
}

/*
 * Dump OIDs of all objects, after upgrading the QD cluster.
 */
void
dump_new_oids(migratorContext *ctx)
{
	PGconn	   *conn;
	char		filename[MAXPGPATH];
	FILE	   *oid_dump;
	int			dbnum;

	prep_status(ctx, "Exporting object OIDs from the new cluster");

	/* Dump OIDs of global objects */
	snprintf(filename, sizeof(filename), "%s/%s", ctx->cwd, GLOBAL_OIDS_DUMP_FILE);
	oid_dump = fopen(filename, "w");
	if (!oid_dump)
		pg_log(ctx, PG_FATAL, "Could not create necessary file:  %s\n", filename);

	conn = connectToServer(ctx, "template1", CLUSTER_NEW);
	PQclear(executeQueryOrDie(ctx, conn, "set search_path='pg_catalog';"));

	dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, spcname FROM pg_tablespace", "preassign_tablespace_oid");
	dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, rsqname FROM pg_resqueue", "preassign_resqueue_oid");
	dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, resqueueid, restypid FROM pg_resqueuecapability", "preassign_resqueuecb_oid");
	dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, rolname FROM pg_authid", "preassign_authid_oid");
	dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, datname FROM pg_database", "preassign_database_oid");

	PQfinish(conn);
	fclose(oid_dump);

	/*
	 * We use the old database OIDs, because we won't have convenient access
	 * to the new OIDs when we read these back in. It doesn't really matter
	 * which ones we use, as long as we're consistent.
	 */
	for (dbnum = 0; dbnum < ctx->old.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &ctx->old.dbarr.dbs[dbnum];

		snprintf(filename, sizeof(filename), "%s/" DB_OIDS_DUMP_FILE_MASK, ctx->cwd, olddb->db_oid);
		oid_dump = fopen(filename, "w");
		if (!oid_dump)
			pg_log(ctx, PG_FATAL, "Could not create necessary file:  %s\n", filename);

		conn = connectToServer(ctx, olddb->db_name, CLUSTER_NEW);

		PQclear(executeQueryOrDie(ctx, conn, "set search_path='pg_catalog';"));

		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, typname, typnamespace FROM pg_type", "preassign_type_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, oprnamespace, oprname FROM pg_operator", "preassign_operator_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, nspname FROM pg_namespace", "preassign_namespace_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, lanname FROM pg_language", "preassign_language_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, castsource, casttarget FROM pg_cast", "preassign_cast_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, connamespace, conname FROM pg_conversion", "preassign_conversion_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, ev_class, rulename FROM pg_rewrite", "preassign_rule_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, opfname, opfnamespace FROM pg_opfamily", "preassign_opfam_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, opcname, opcnamespace FROM pg_opclass", "preassign_opclass_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, prsnamespace, prsname FROM pg_ts_parser", "preassign_tsparser_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, dictnamespace, dictname FROM pg_ts_dict", "preassign_tsdict_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, tmplnamespace, tmplname FROM pg_ts_template", "preassign_tstemplate_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, cfgnamespace, cfgname FROM pg_ts_config", "preassign_tsconfig_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, extname FROM pg_extension", "preassign_extension_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, enumtypid, enumlabel FROM pg_enum", "preassign_enum_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, connamespace, conname, conrelid, contypid FROM pg_constraint", "preassign_constraint_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, ptcname FROM pg_extprotocol", "preassign_extprotocol_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, adrelid, adnum FROM pg_attrdef", "preassign_attrdef_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, relname, relnamespace FROM pg_class", "preassign_relation_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, proname, pronamespace FROM pg_proc", "preassign_procedure_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, amopmethod FROM pg_amop", "preassign_amop_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, fdwname, fdwowner FROM pg_foreign_data_wrapper", "preassign_fdw_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, srvname, srvowner, srvfdw FROM pg_foreign_server", "preassign_fdw_server_oid");
		dump_rows(ctx, NULL, oid_dump, conn, "SELECT oid, umuser, umserver FROM pg_user_mapping", "preassign_user_mapping_oid");

		PQfinish(conn);

		fclose(oid_dump);
	}

	check_ok(ctx);
}

/*
 * Read all OID dump files into memory.
 */
void
slurp_oid_files(migratorContext *ctx)
{
	int			dbnum;
	char		filename[MAXPGPATH];
	FILE	   *oid_dump;
	struct stat st;
	char	   *reserved_oids;

	/* Read the Oids of global objects */
	oid_dump = fopen(GLOBAL_OIDS_DUMP_FILE, "r");
	if (!oid_dump)
		pg_log(ctx, PG_FATAL, "Could not open necessary file:  %s\n", GLOBAL_OIDS_DUMP_FILE);

	if (fstat(fileno(oid_dump), &st) != 0)
		pg_log(ctx, PG_FATAL, "Could not read file \"%s\": %s\n",
			   GLOBAL_OIDS_DUMP_FILE, strerror(errno));

	reserved_oids = pg_malloc(ctx, st.st_size + 1);
	fread(reserved_oids, st.st_size, 1, oid_dump);
	fclose(oid_dump);
	reserved_oids[st.st_size] = '\0';

	ctx->old.global_reserved_oids = reserved_oids;

	/* Read per-DB Oids */
	for (dbnum = 0; dbnum < ctx->old.dbarr.ndbs; dbnum++)
	{
		DbInfo	   *olddb = &ctx->old.dbarr.dbs[dbnum];

		snprintf(filename, sizeof(filename), "%s/" DB_OIDS_DUMP_FILE_MASK, ctx->cwd, olddb->db_oid);
		oid_dump = fopen(filename, "r");
		if (!oid_dump)
			pg_log(ctx, PG_FATAL, "Could not open necessary file:  %s\n", filename);

		if (fstat(fileno(oid_dump), &st) != 0)
			pg_log(ctx, PG_FATAL, "Could not read file \"%s\": %s\n",
				   filename, strerror(errno));

		reserved_oids = pg_malloc(ctx, st.st_size + 1);
		fread(reserved_oids, st.st_size, 1, oid_dump);
		fclose(oid_dump);
		reserved_oids[st.st_size] = '\0';

		olddb->reserved_oids = reserved_oids;
	}
}


/*
 * A helper function around PQescapeStringConn. Returns a statically
 * allocated buffer, so the returned value is only valid until the
 * next call.
 */
static char *
simple_escape_literal(migratorContext *ctx, PGconn *conn, const char *s)
{
	static char *buf = NULL;
	static int	buf_size = 0;
	int			input_len;
	int			req_size;

	input_len = strlen(s);

	req_size = input_len * 2 + 1;
	if (req_size > buf_size)
	{
		if (buf)
			pg_free(buf);

		if (req_size < NAMEDATALEN * 2 + 1)
			req_size = NAMEDATALEN * 2 + 1;

		buf = pg_malloc(ctx, req_size);
		buf_size = req_size;
	}

	PQescapeStringConn(conn, buf, s, input_len, NULL);

	return buf;
}

/*
 * Issue a query on a catalog table, and produce calls to a preassign support
 * function from the result set.
 *
 * The output is a string, containing SQL calls like:
 *
 * SELECT binary_upgrade.preassign_*_oid(<oid>, <other args);
 *
 * 'funcname' is the "preassign_*_oid" function to use.
 * 'sql' is the query to issue. The columns of the result set are passed as
 * arguments to the preassign-support function.
 *
 */
static void
dump_rows(migratorContext *ctx, PQExpBuffer buf, FILE *file, PGconn *conn,
		  const char *sql, const char *funcname)
{
	int			ntups;
	int			ncols;
	int			row;
	int			col;
	PGresult   *res;

	if (file != NULL)
		buf = createPQExpBuffer();

	/*
	 * Add a WHERE or AND clause to filter out built-in objects.
	 *
	 * If the query contains "UNION ALL", then it's the caller's
	 * responsibility to do the filtering. This special case is for the
	 * one more complicated query in get_old_oids() function; all the
	 * other queries are very simple ones.
	 */
	if (strstr(sql, "WHERE ") == NULL)
		res = executeQueryOrDie(ctx, conn, "%s WHERE oid >= %u", sql, FirstNormalObjectId);
	else if (strstr(sql, "UNION ALL") == NULL)
		res = executeQueryOrDie(ctx, conn, "%s AND oid >= %u", sql, FirstNormalObjectId);
	else
		res = executeQueryOrDie(ctx, conn, "%s", sql);

	ntups = PQntuples(res);
	ncols = PQnfields(res);

	for (row = 0; row < ntups; row++)
	{
		appendPQExpBuffer(buf, "SELECT binary_upgrade.%s('%s'",
						  funcname,
						  simple_escape_literal(ctx, conn, PQgetvalue(res, row, 0)));

		for (col = 1; col < ncols; col++)
			appendPQExpBuffer(buf, ", '%s'",
							  simple_escape_literal(ctx, conn, PQgetvalue(res, row, col)));
		appendPQExpBuffer(buf, ");\n");

		if (file)
		{
			fwrite(buf->data, buf->len, 1, file);
			resetPQExpBuffer(buf);
		}
	}
	PQclear(res);

	if (file != NULL)
		destroyPQExpBuffer(buf);
}

