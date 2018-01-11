/*-------------------------------------------------------------------------
 *
 * cdb_dump_include.h
 *	  cdb_dump_include is collecion of functions used to determine the
 *	  set of tables to be included in a dump.  These functions are
 *	  largely extracted from pg_dump.c.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/bin/pg_dump/cdb/cdb_dump_include.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDB_DUMP_INCLUDE_H
#define CDB_DUMP_INCLUDE_H

#include "pg_dump.h"


/*
 * Must be set to the program identifier to be used in status and diagnostic
 * messages.
 */
extern const char *progname;	/* Copied from pg_backup_archiver.h */

/*
 * Must be set to an open/active connection.
 */
extern PGconn *g_conn;			/* the database connection */

/*
 * String designating the mode used to LOCK the tables to be dumped.
 */
extern const char *table_lock_mode;

/*
 * Subquery used to convert user ID (eg, datdba) to user name. This
 * defaults to "SELECT rolname FROM pg_catalog.pg_roles WHERE oid ="
 * but may be overridden with a value of similar form.
 */
extern const char *username_subquery;


/*
 * Indicates whether or not the GPDB cluster supports partitioning.
 * This boolean is used in cdb_dump_include.c to remove partitioned
 * table children from dump consideration.
 */
extern bool g_gp_supportsPartitioning;

/*
 * MPP-6095: This boolean is used in cdb_dump_include.c to remove
 * partition templates from dump consideration.
 */
extern bool g_gp_supportsPartitionTemplates;

/*
 * Indicates whether or not the GPDB cluster supports opfamilies.
 */
extern bool g_gp_supportsOpfamilies;

/*
 * Indicates whether or not the GPDB cluster supports column attributes.
 */
extern bool g_gp_supportsAttributeEncoding;

/*
 * Indicates whether or not the GPDB cluster supports full text search
 */
extern bool g_gp_supportsFullText;

/*
 * Indicates whether or not the GPDB cluster supports languages owned by
 * other than OID 10.
 */
extern bool g_gp_supportsLanOwner;

/*
 * Indicate whether or not the GPDB cluster supports Foreign Data Wrappers
 */
extern bool	g_gp_fdw;

/*
 * Indicates whether or not only catalogs are dumped.  True
 * when the agent job is dumping the MPP schema only.
 */
extern bool g_gp_catalog_only;


extern const CatalogId nilCatalogId;

extern SimpleStringList schema_include_patterns;
extern SimpleOidList schema_include_oids;
extern SimpleStringList schema_exclude_patterns;
extern SimpleOidList schema_exclude_oids;

extern SimpleStringList table_include_patterns;
extern SimpleOidList table_include_oids;
extern SimpleStringList table_exclude_patterns;
extern SimpleOidList table_exclude_oids;

const char *fmtQualifiedId(const char *schema, const char *id);

void		do_sql_command(PGconn *conn, const char *query);
void check_sql_result(PGresult *res, PGconn *conn, const char *query,
				 ExecStatusType expected);

void		selectSourceSchema(const char *schemaName);

void		convert_patterns_to_oids(void);

#endif
