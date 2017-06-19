/*-------------------------------------------------------------------------
 *
 * binary_upgrade.c
 *		Functions to create ArchiveEntries for Oid preassignment in dumps
 *
 * Portions Copyright 2017 Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/bin/pg_dump/binary_upgrade.c
 *
 *-------------------------------------------------------------------------
 */

#include "binary_upgrade.h"
#include "pg_dump.h"
#include "pqexpbuffer.h"
#include "catalog/pg_class.h"
#include "catalog/pg_type.h"

static char query_buffer[QUERY_ALLOC];

static const CatalogId nilCatalogId = {0, 0};

/* Cache for array types during binary_upgrade dumping */
static TypeCache  	   *typecache;
static DumpableObject **typecacheindex;
static int				numtypecache;

/* Internal helper methods for preassigning the various object types */
static void preassign_type_oid(PGconn *conn, Archive *fout, Archive *AH, Oid pg_type_oid, char *objname);
static void preassign_constraint_oid(Archive *AH, Oid constroid, Oid nsoid, char *objname, Oid contable, Oid condomain);
static void preassign_attrdefs_oid(Archive *AH, Oid attrdefoid, Oid attreloid, int adnum);
static void preassign_pg_class_oids(PGconn *conn, Archive *fout, Archive *AH, Oid pg_class_oid);
static void preassign_type_oids_by_rel_oid(PGconn *conn, Archive *fout, Archive *AH, Oid pg_rel_oid, char *objname);
static void preassign_enum_oid(PGconn *conn, Archive *AH, Oid enum_oid, char *objname);
static void preassign_view_rule_oids(PGconn *conn, Archive *AH, Oid view_oid);

void
dumpOperatorOid(Archive *AH, OprInfo *info)
{
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_operator_oid("
			 "'%u'::pg_catalog.oid, '%u'::pg_catalog.oid, '%s'::text);\n",
			 info->dobj.catId.oid, info->dobj.namespace->dobj.catId.oid,
			 info->dobj.name);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, NULL, NULL,
				 NULL, 0,
				 NULL, NULL);
}

void
dumpNamespaceOid(Archive *AH, NamespaceInfo *info)
{
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_namespace_oid("
			 "'%u'::pg_catalog.oid, $$%s$$::text);\n",
			 info->dobj.catId.oid, info->dobj.name);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, NULL, NULL,
				 NULL, 0,
				 NULL, NULL);
}

void
dumpAggProcedureOid(PGconn *conn, Archive *fout, Archive *AH, AggInfo *info)
{
	char		upgrade_query[QUERY_ALLOC];
	PQExpBuffer	upgrade_buffer;
	int 		i;
	int			ntups;
	PGresult   *upgrade_res;

	if (!info->aggfn.dobj.dump)
		return;

	upgrade_buffer = createPQExpBuffer();

	for (i = 0; i < AGG_FUNCS; i++)
	{
		Oid			procoid;
		Oid			nsoid;
		char	   *proname;

		if (agg_fns[i].version > fout->remoteVersion)
			continue;

		snprintf(upgrade_query, sizeof(upgrade_query),
				 "SELECT p.oid, p.proname, p.pronamespace "
				 "FROM   pg_catalog.pg_proc p JOIN pg_catalog.pg_aggregate a "
				 "       ON (p.oid = a.%s) where a.aggfnoid = '%u'::pg_catalog.oid;",
				 agg_fns[i].name, info->aggfn.dobj.catId.oid);

		upgrade_res = PQexec(conn, upgrade_query);
		check_sql_result(upgrade_res, conn, upgrade_query, PGRES_TUPLES_OK);
		ntups = PQntuples(upgrade_res);

		if (ntups <= 0)
			continue;

		procoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "oid")));
		nsoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "pronamespace")));
		proname = PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "proname"));

		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_procedure_oid("
						  "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid);\n",
						  procoid, proname, nsoid);

		PQclear(upgrade_res);
	}

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->aggfn.dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE AGGREGATE", upgrade_buffer->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(upgrade_buffer);
}

void
dumpProcedureOid(PGconn *conn, Archive *fout, Archive *AH, FuncInfo *info)
{
	char		upgrade_query[QUERY_ALLOC];
	PGresult   *upgrade_res;
	Oid			typoid;
	char	   *typname;
	int			ntups;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	snprintf(upgrade_query, sizeof(upgrade_query),
			 "SELECT t.oid, t.typname "
			 "FROM   pg_catalog.pg_proc p "
			 "       JOIN pg_catalog.pg_type t ON (p.prorettype = t.oid) "
			 "WHERE  p.oid = '%u'::pg_catalog.oid AND p.proretset = True;",
			 info->dobj.catId.oid);

	upgrade_res = PQexec(conn, upgrade_query);
	check_sql_result(upgrade_res, conn, upgrade_query, PGRES_TUPLES_OK);
	ntups = PQntuples(upgrade_res);

	if (ntups > 0)
	{
		typoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "oid")));
		typname = PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "typname"));

		preassign_type_oid(conn, fout, AH, typoid, typname);
	}

	PQclear(upgrade_res);

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_procedure_oid("
			 "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid);\n",
			 info->dobj.catId.oid, info->dobj.name, info->dobj.namespace->dobj.catId.oid);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

/*
 *	dumpProcLangOid
 *
 * Writes out the Oid for custom procedural languages
 */
void
dumpProcLangOid(PGconn *conn, Archive *fout, Archive *AH, ProcLangInfo *info)
{
	PQExpBuffer upgrade_query;
	PQExpBuffer upgrade_buffer;
	Oid			procoid;
	char	   *proname;
	Oid			nsoid;
	int			ntups;
	PGresult   *upgrade_res;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	upgrade_query = createPQExpBuffer();
	upgrade_buffer = createPQExpBuffer();

	/*
	 * The inline function first appeared in upstream PostgreSQL 9.0, but was
	 * backported into GPDB which is based on PostgreSQL 8.3.
	 */
	appendPQExpBuffer(upgrade_query,
					  "SELECT h.oid AS handleroid, "
					  "       h.pronamespace AS handlerns, "
					  "       h.proname AS handler, "
					  "       v.oid AS validatoroid, "
					  "       v.pronamespace AS validatorns, "
					  "       v.proname AS validator "
					  "       %s "
					  "FROM   pg_catalog.pg_pltemplate "
					  "       JOIN pg_catalog.pg_proc h "
					  "            ON (h.proname = tmplhandler) "
					  "       LEFT OUTER JOIN pg_catalog.pg_proc v "
					  "            ON (v.proname = tmplvalidator) "
					  "       %s "
					  "WHERE  tmplname = $$%s$$::text;",
					  (fout->remoteVersion >= 80300)
					  ? ",i.oid AS inlineoid, i.pronamespace AS inlinens, i.proname AS inline"
					  : "",
					  (fout->remoteVersion >= 80300)
					  ? "LEFT OUTER JOIN pg_catalog.pg_proc i ON (i.proname = tmplinline)"
					  : "",
					  info->dobj.name);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(upgrade_res);

	if (ntups != 1)
	{
		write_msg(NULL, "ERROR: language functions for %s not found in catalog", info->dobj.name);
		exit_nicely();
	}

	/* Handler function */
	procoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "handleroid")));
	nsoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "handlerns")));
	proname = PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "handler"));
	appendPQExpBuffer(upgrade_buffer,
					  "SELECT binary_upgrade.preassign_procedure_oid("
					  "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid);\n",
					  procoid, proname, nsoid);

	/* Validator function */
	procoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "validatoroid")));
	if (OidIsValid(procoid))
	{
		nsoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "validatorns")));
		proname = PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "validator"));
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_procedure_oid("
						  "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid);\n",
						  procoid, proname, nsoid);
	}

	/* Inline function */
	if (fout->remoteVersion >= 80300)
	{
		procoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "inlineoid")));
		if (OidIsValid(procoid))
		{
			nsoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "inlinens")));
			proname = PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "inline"));
			appendPQExpBuffer(upgrade_buffer,
							  "SELECT binary_upgrade.preassign_procedure_oid("
							  "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid);\n",
							  procoid, proname, nsoid);
		}
	}

	appendPQExpBuffer(upgrade_buffer,
					  "SELECT binary_upgrade.preassign_language_oid("
					  "'%u'::pg_catalog.oid, $$%s$$::text);\n",
					  info->dobj.catId.oid, info->dobj.name);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", upgrade_buffer->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(upgrade_buffer);
	destroyPQExpBuffer(upgrade_query);
}

/*
 *	dumpCastOid()
 *
 * Write out preassigned Oid for user defined casts
 */
void
dumpCastOid(Archive *AH, CastInfo *info)
{
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_cast_oid("
			 "'%u'::pg_catalog.oid, '%u'::pg_catalog.oid, '%u'::pg_catalog.oid);\n",
			 info->dobj.catId.oid, info->castsource, info->casttarget);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "preassign_cast",
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

/*
 * CREATE CONVERSION ..
 */
void
dumpConversionOid(PGconn *conn, Archive *AH, ConvInfo *info)
{
	PQExpBuffer	upgrade_query;
	int			ntups;
	PGresult   *upgrade_res;
	Oid			connamespace;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	upgrade_query = createPQExpBuffer();

	appendPQExpBuffer(upgrade_query,
					  "SELECT connamespace "
					  "FROM pg_catalog.pg_conversion "
					  "WHERE oid = '%u'::pg_catalog.oid",
					  info->dobj.catId.oid);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, "ERROR: conversion %s not found in catalog", info->dobj.name);
		exit_nicely();
	}

	connamespace = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "connamespace")));

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_conversion_oid('%u'::pg_catalog.oid, "
			 "$$%s$$::text, '%u'::pg_catalog.oid);\n",
			 info->dobj.catId.oid, info->dobj.name, connamespace);

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

/*
 *	dumpRuleOid
 *
 * Writes out preassignment operations for user defined rules.
 */
void
dumpRuleOid(Archive *AH, RuleInfo *info)
{
	TableInfo *tbinfo = (TableInfo *) info->ruletable;

	if (!info->dobj.dump || !info->separate)
		return;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_rule_oid("
			 "'%u'::pg_catalog.oid, '%u'::pg_catalog.oid, $$%s$$::text);\n",
			 info->dobj.catId.oid, tbinfo->dobj.catId.oid, info->dobj.name);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

/*
 * dumpOpFamilyOid
 *
 * Preassign Oid for CREATE OPERATOR FAMILY .. operations
 */
void
dumpOpFamilyOid(PGconn *conn, Archive *AH, OpfamilyInfo *info)
{
	PQExpBuffer	upgrade_query;
	int			ntups;
	PGresult   *upgrade_res;
	Oid			opfnamespace;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	upgrade_query = createPQExpBuffer();

	appendPQExpBuffer(upgrade_query,
					  "SELECT opfnamespace "
					  "FROM pg_catalog.pg_opfamily "
					  "WHERE oid = '%u'::pg_catalog.oid",
					  info->dobj.catId.oid);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, "ERROR: opfam %s not found in catalog", info->dobj.name);
		exit_nicely();
	}

	opfnamespace = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "opfnamespace")));

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_opfam_oid('%u'::pg_catalog.oid, "
			 "$$%s$$::text, '%u'::pg_catalog.oid);",
			 info->dobj.catId.oid, info->dobj.name, opfnamespace);

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

/*
 *	dumpOpClassOid
 *
 * Preassign Oid for CREATE OPERATOR CLASS .. operations
 */
void
dumpOpClassOid(PGconn *conn, Archive *fout, Archive *AH, OpclassInfo *info)
{
	PQExpBuffer	upgrade_query;
	PQExpBuffer upgrade_buffer;
	int			ntups;
	PGresult   *upgrade_res;
	Oid			pg_opclass_oid;
	Oid			opcnamespace;
	Oid			amopoid;
	Oid			amopmethod;
	int			i;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	upgrade_query = createPQExpBuffer();
	upgrade_buffer = createPQExpBuffer();

	/* Start by dumping the amop entries */
	if (fout->remoteVersion >= 80300)
	{
		/*
		 * Print only those opfamily members that are tied to the opclass by
		 * pg_depend entries.
		 */
		appendPQExpBuffer(upgrade_query,
						  "SELECT ao.oid, ao.amopmethod "
						  "FROM   pg_catalog.pg_amop ao, pg_catalog.pg_depend "
						  "WHERE  refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "       AND refobjid = '%u'::pg_catalog.oid "
						  "       AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "       AND objid = ao.oid",
						  info->dobj.catId.oid);
	}
	else
	{
		/* XXX: no amopmethod in 4.3 */
	}
	
	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(upgrade_res);
	if (ntups > 0)
	{
		for (i = 0; i < ntups; i++)
		{
			amopoid = atooid(PQgetvalue(upgrade_res, i, PQfnumber(upgrade_res, "oid")));
			amopmethod = atooid(PQgetvalue(upgrade_res, i, PQfnumber(upgrade_res, "amopmethod")));

			appendPQExpBuffer(upgrade_buffer,
							  "SELECT binary_upgrade.preassign_amop_oid("
							  "'%u'::pg_catalog.oid, '%u'::pg_catalog.oid);",
							  amopoid, amopmethod);
		}
	}

	PQclear(upgrade_res);
	resetPQExpBuffer(upgrade_query);

	/* Now dump the opclass */
	appendPQExpBuffer(upgrade_query,
					  "SELECT oid, opcnamespace "
					  "FROM   pg_catalog.pg_opclass "
					  "WHERE  opcname = $$%s$$::text",
					  info->dobj.name);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, "ERROR: opclass %s not found in catalog", info->dobj.name);
		exit_nicely();
	}
	pg_opclass_oid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "oid")));
	opcnamespace = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "opcnamespace")));

	appendPQExpBuffer(upgrade_buffer,
					  "SELECT binary_upgrade.preassign_opclass_oid("
					  "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid);",
					  info->dobj.catId.oid, info->dobj.name, opcnamespace);

	PQclear(upgrade_res);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", upgrade_buffer->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(upgrade_buffer);
	destroyPQExpBuffer(upgrade_query);
}

void
dumpTSObjectOid(Archive *AH, DumpableObject *info)
{
	char		   *funcname;
	DumpableObject *d;

	switch(info->objType)
	{
		case DO_TSPARSER:
			d = &((TSParserInfo *) info)->dobj;
			funcname = "preassign_tsparser_oid";
			break;
		case DO_TSDICT:
			d = &((TSDictInfo *) info)->dobj;
			funcname = "preassign_tsdict_oid";
			break;
		case DO_TSTEMPLATE:
			d = &((TSTemplateInfo *) info)->dobj;
			funcname = "preassign_tstemplate_oid";
			break;
		case DO_TSCONFIG:
			d = &((TSConfigInfo *) info)->dobj;
			funcname = "preassign_tsconfig_oid";
			break;
		default:
			write_msg(NULL, "ERROR: incorrect object type passed to TS Oid dumping");
			exit_nicely();
			return; /* not reached */
	}

	/* Skip if not to be dumped */
	if (!d->dump)
		return;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.%s('%u'::pg_catalog.oid, "
			 "'%u'::pg_catalog.oid, $$%s$$::text);\n",
			 funcname, d->catId.oid, d->namespace->dobj.catId.oid, d->name);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 funcname,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

void
dumpExtensionOid(Archive *AH, ExtensionInfo *info)
{
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_extension_oid('%u'::pg_catalog.oid, "
			 "$$%s$$::text);\n",
			 info->dobj.catId.oid, info->dobj.name);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "preassign_extension",
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

void
dumpShellTypeOid(PGconn *conn, Archive *fout, Archive *AH, ShellTypeInfo *info)
{
	PQExpBuffer		upgrade_query;
	int				ntups;
	Oid				pg_type_oid;
	PGresult	   *upgrade_res;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	upgrade_query = createPQExpBuffer();

	appendPQExpBuffer(upgrade_query,
					  "SELECT oid "
					  "FROM   pg_catalog.pg_type "
					  "WHERE  typname = $$%s$$::text;",
					  info->dobj.name);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, "ERROR: type \"%s\" not found in catalog", info->dobj.name);
		exit_nicely();
	}

	pg_type_oid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "oid")));
	preassign_type_oid(conn, fout, AH, pg_type_oid, info->dobj.name);

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);
}

void
dumpTypeOid(PGconn *conn, Archive *fout, Archive *AH, TypeInfo *info)
{
	int		i;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	if (info->typtype == TYPTYPE_ENUM)
	{
		preassign_type_oid(conn, fout, AH, info->dobj.catId.oid, info->dobj.name);
		preassign_enum_oid(conn, AH, info->dobj.catId.oid, info->dobj.name);
	}
	else if (info->typtype == TYPTYPE_BASE)
	{
		/* We might already have a shell type, but setting pg_type_oid is harmless */
		preassign_type_oid(conn, fout, AH, info->dobj.catId.oid, info->dobj.name);
	}
	else if (info->typtype == TYPTYPE_DOMAIN)
	{
		preassign_type_oid(conn, fout, AH, info->dobj.catId.oid, info->dobj.name);
		for (i = 0; i < info->nDomChecks; i++)
		{
			ConstraintInfo *c = &(info->domChecks[i]);

			preassign_constraint_oid(fout, c->dobj.catId.oid,
									 c->dobj.namespace->dobj.catId.oid,
									 c->dobj.name,
									 c->contable ? c->contable->dobj.catId.oid : InvalidOid,
									 c->condomain ? c->condomain->dobj.catId.oid : InvalidOid);
		}
	}
	else if (info->typtype == TYPTYPE_COMPOSITE)
	{
		preassign_type_oid(conn, fout, AH, info->dobj.catId.oid, info->dobj.name);
		preassign_pg_class_oids(conn, fout, AH, info->typrelid);
	}
}

static void
preassign_enum_oid(PGconn *conn, Archive *AH, Oid enum_oid, char *objname)
{
	PQExpBuffer	upgrade_query = createPQExpBuffer();
	PQExpBuffer upgrade_buffer = createPQExpBuffer();
	int			ntups;
	int			i;
	PGresult   *upgrade_res;
	const char *label;
	Oid			oid;

	appendPQExpBuffer(upgrade_query,
					  "SELECT oid, enumlabel "
					  "FROM pg_catalog.pg_enum "
					  "WHERE enumtypid = '%u'::pg_catalog.oid",
					  enum_oid);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(upgrade_res);
	if (ntups < 1)
	{
		write_msg(NULL, "ERROR: enum \"%s\" (typid %u) not found in catalog",
				  objname, enum_oid);
		exit_nicely();
	}

	for (i = 0; i < ntups; i++)
	{
		oid = atooid(PQgetvalue(upgrade_res, i, PQfnumber(upgrade_res, "oid")));
		label = PQgetvalue(upgrade_res, i, PQfnumber(upgrade_res, "enumlabel"));

		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_enum_oid("
								"'%u'::pg_catalog.oid, "
								"'%u'::pg_catalog.oid, "
								"$$%s$$::text);\n",
						  oid, enum_oid, label);
	}

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "preassign_enum",
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", upgrade_buffer->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(upgrade_buffer);
}

/*
 * preassign_type_oid
 *
 * Preassign Oids for all pg_type operations, such as CREATE TYPE .. as well
 * as creating base/array types via CREATE TABLE .. etc. On the first call a
 * cache will be populated by interrogating pg_type, this cache will then be
 * used for all lookups to reduce the amount of required SQL calls.
 */
static void
preassign_type_oid(PGconn *conn, Archive *fout, Archive *AH, Oid pg_type_oid, char *objname)
{
	PQExpBuffer upgrade_query;
	PQExpBuffer	upgrade_buffer;
	int			ntups;
	PGresult   *upgrade_res;
	int			i;
	int			i_arr_oid;
	int			i_arr_name;
	int			i_arr_nsp;
	int			i_oid;
	int			i_name;
	int			i_nsp;
	TypeCache  *type;

	if (typecache == NULL)
	{
		upgrade_query = createPQExpBuffer();

		if (fout->remoteVersion >= 80300)
		{
			appendPQExpBuffer(upgrade_query,
							  "SELECT typ.oid as typoid, typ.typname, typ.typnamespace, "
							  "       typ.typarray as arr_typoid, arr.typname as arr_typname, arr.typnamespace as arr_typnamespace "
							  "FROM pg_catalog.pg_type typ "
							  "  LEFT OUTER JOIN pg_catalog.pg_type arr ON typ.typarray = arr.oid "
							  "WHERE typ.oid NOT IN (SELECT typarray FROM pg_type)");
		}
		else
		{
			/*
			 * Query to get the array type of a base type in GPDB 4.3, should we
			 * need to support older versions then this would have to be extended.
			 */
			appendPQExpBuffer(upgrade_query,
							  "SELECT typ.oid as typoid, typ.typname, typ.typnamespace, "
							  "       arr.oid as arr_typoid, arr.typname as arr_typname, arr.typnamespace as arr_typnamespace "
							  "FROM pg_catalog.pg_type typ "
							  "  LEFT OUTER JOIN pg_catalog.pg_type arr ON arr.typname = '_' || typ.typname "
							  "WHERE typ.oid NOT IN (SELECT oid FROM pg_type WHERE substring(typname, 1, 1) = '_')");
		}

		upgrade_res = PQexec(conn, upgrade_query->data);
		check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);

		ntups = PQntuples(upgrade_res);

		if (ntups > 0)
		{
			i_oid = PQfnumber(upgrade_res, "typoid");
			i_name = PQfnumber(upgrade_res, "typname");
			i_nsp = PQfnumber(upgrade_res, "typnamespace");
			i_arr_oid = PQfnumber(upgrade_res, "arr_typoid");
			i_arr_name = PQfnumber(upgrade_res, "arr_typname");
			i_arr_nsp = PQfnumber(upgrade_res, "arr_typnamespace");

			typecache = calloc(ntups, sizeof(TypeCache));
			numtypecache = ntups;

			for (i = 0; i < ntups; i++)
			{
				typecache[i].dobj.objType = DO_TYPE_CACHE;
				typecache[i].dobj.catId.oid = atooid(PQgetvalue(upgrade_res, i, i_oid));
				typecache[i].dobj.name = strdup(PQgetvalue(upgrade_res, i, i_name));

				typecache[i].typnsp = atooid(PQgetvalue(upgrade_res, i, i_nsp));

				/*
				 * Before PostgreSQL 8.3 arrays for composite types weren't supported
				 * and base relation types didn't automatically have an array type
				 * counterpart. If an array type isn't found we need to force a new
				 * Oid to be allocated even in binary_upgrade mode which otherwise
				 * work by preassigning Oids. Inject InvalidOid in the preassign call
				 * to ensure we get a new Oid.
				 */
				if (PQgetisnull(upgrade_res, i, i_arr_name))
				{
					if (fout->remoteVersion <= 80200)
					{
						char array_name[NAMEDATALEN];

						typecache[i].arraytypoid = InvalidOid,
						typecache[i].arraytypnsp = atooid(PQgetvalue(upgrade_res, i, i_nsp));

						snprintf(array_name, NAMEDATALEN, "_%s", PQgetvalue(upgrade_res, i, i_name));
						typecache[i].arraytypname = strdup(array_name);
					}
				}
				else
				{
					typecache[i].arraytypoid = atooid(PQgetvalue(upgrade_res, i, i_arr_oid));
					typecache[i].arraytypname = strdup(PQgetvalue(upgrade_res, i, i_arr_name));
					typecache[i].arraytypnsp = atooid(PQgetvalue(upgrade_res, i, i_arr_nsp));
				}
			}

			typecacheindex = buildIndexArray(typecache, ntups, sizeof(TypeCache));
		}

		PQclear(upgrade_res);
		destroyPQExpBuffer(upgrade_query);
	}

	/* Query the cached type information */
	type = (TypeCache *) findObjectByOid(pg_type_oid, typecacheindex, numtypecache);

	/* This shouldn't happen.. */
	if (!type)
	{
		write_msg(NULL, "ERROR: didn't find type information in cache for %u (%s)\n", pg_type_oid, objname);
		exit_nicely();
	}

	upgrade_buffer = createPQExpBuffer();

	appendPQExpBuffer(upgrade_buffer,
			 		  "SELECT binary_upgrade.preassign_type_oid("
					  "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid);\n",
					  pg_type_oid, type->dobj.name, type->typnsp);

	/*
	 * All types doesn't automatically have an arraytype, ensure we have found
	 * (or added) one above before preassigning.
	 */
	if (OidIsValid(type->arraytypoid))
	{
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_arraytype_oid("
						  "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid);\n",
						  type->arraytypoid, type->arraytypname, type->arraytypnsp);
	}

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 objname,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", upgrade_buffer->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(upgrade_buffer);
}

/*
 * dumpConstraintOid
 *
 * Writes out preassigned Oids for user defined constraints. Error handling
 * for non-existing Indexes is performed in dumpConstraint() so defer there
 * for consistency with upstream even though we are being executed first.
 */
void
dumpConstraintOid(PGconn *conn, Archive *fout, Archive *AH, ConstraintInfo *info)
{
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	/* Index related constraint */
	if (info->contype == 'p' || info->contype == 'u')
	{
		IndxInfo *indxinfo = (IndxInfo *) findObjectByDumpId(info->conindex);
		/* Error handling for this is performed in dumpConstraints */
		if (indxinfo == NULL)
			return;

		preassign_pg_class_oids(conn, fout, AH, indxinfo->dobj.catId.oid);
		preassign_constraint_oid(AH, info->dobj.catId.oid,
								 info->dobj.namespace->dobj.catId.oid,
								 info->dobj.name,
								 info->contable ? info->contable->dobj.catId.oid : InvalidOid,
								 info->condomain ? info->condomain->dobj.catId.oid : InvalidOid);
	}
	/* CHECK constraint on a table or domain */
	else if (info->contype == 'c')
	{
		TableInfo *tbinfo = (TableInfo *) info->contable;

		if (!info->separate)
			return;

		if (tbinfo)
			preassign_pg_class_oids(conn, fout, AH, tbinfo->dobj.catId.oid);

		preassign_constraint_oid(AH, info->dobj.catId.oid,
								 info->dobj.namespace->dobj.catId.oid,
								 info->dobj.name,
								 info->contable ? info->contable->dobj.catId.oid : InvalidOid,
								 info->condomain ? info->condomain->dobj.catId.oid : InvalidOid);
	}
	/*
	 * FOREIGN KEY constraint. While FK constraints aren't enforced in
	 * GPDB they are still created so preserve any Oids.
	 */
	else if (info->contype == 'f')
	{
		preassign_constraint_oid(AH, info->dobj.catId.oid,
								 info->dobj.namespace->dobj.catId.oid,
								 info->dobj.name,
								 info->contable ? info->contable->dobj.catId.oid : InvalidOid,
								 info->condomain ? info->condomain->dobj.catId.oid : InvalidOid);
	}
}

/*
 * preassign_constraint_oid
 *
 * Preassign Oids for CREATE CONSTRAINT .. calls.
 */
static void
preassign_constraint_oid(Archive *AH, Oid constroid, Oid nsoid, char *objname, Oid contable, Oid condomain)
{
	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_constraint_oid('%u'::pg_catalog.oid, "
			 "'%u'::pg_catalog.oid, $$%s$$::text, '%u'::pg_catalog.oid, '%u'::pg_catalog.oid);\n",
			 constroid, nsoid, objname, contable, condomain);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 objname,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

/*
 *	dumpExternalProtocolOid
 *
 * Preassign Oids for CREATE EXTERNAL PROTOCOL ..
 */
void
dumpExternalProtocolOid(Archive *AH, ExtProtInfo *info)
{
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_extprotocol_oid("
			 "'%u'::pg_catalog.oid, $$%s$$::text);\n",
			 info->dobj.catId.oid, info->dobj.name);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

void
dumpAttrDefsOid(Archive *AH, AttrDefInfo *info)
{
	TableInfo   *tbinfo = (TableInfo *) info->adtable;

	if (!info->dobj.dump || !info->separate)
		return;

	preassign_attrdefs_oid(AH, info->dobj.catId.oid, tbinfo->dobj.catId.oid, info->adnum);
}

/*
 * preassign_attrdefs_oid
 *
 * Preassign Oids for default attribute values
 */
static void
preassign_attrdefs_oid(Archive *AH, Oid attrdefoid, Oid attreloid, int adnum)
{
	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_attrdef_oid('%u'::pg_catalog.oid, "
			 "'%u'::pg_catalog.oid, '%u'::pg_catalog.oid);\n",
			 attrdefoid, attreloid, adnum);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "preassign_attrdef",
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

/*
 * dumpIndexOid
 *
 * Preassign the Oid of an index, and potentially all related indexes in case
 * the index is a on a partitioned table.
 */
void
dumpIndexOid(PGconn *conn, Archive *fout, Archive *AH, IndxInfo *info)
{
	PQExpBuffer		index_query;
	int				i;
	int				i_oid;
	Oid				idx_oid;
	PGresult	   *index_res;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	preassign_pg_class_oids(conn, fout, AH, info->dobj.catId.oid);

	/*
	 * If the index is created on a partition hierarchy parent, it will be
	 * created on the partitions and subpartitions as well. These indexes will
	 * however not be dumped separately so we need to figure it out manually
	 * and ensure to preassign the Oids for the partition indexes. The indexes
	 * which are created on the partitions have a strict naming scheme, but
	 * since a partition which is exchanged into the hierarchy may have a
	 * pre-existing index not adhering to the naming convention we must grab
	 * all the indexes we can find and preassign them.
	 *
	 * Since the pg_partition catalogs aren't replicated to segments, we avoid
	 * them here and instead use the inheritance catalogs.
	 */
	index_query = createPQExpBuffer();
	appendPQExpBuffer(index_query,
					  "WITH partitions AS ( "
					  "     ( "
					  "      SELECT p.inhrelid "
					  "      FROM   pg_catalog.pg_index i "
					  "             join pg_catalog.pg_inherits p "
					  "               ON (i.indrelid = p.inhparent) "
					  "      WHERE  i.indexrelid = '%u'::pg_catalog.oid "
					  "     ) "
					  "     UNION "
					  "     ( "
					  "      SELECT sp.inhrelid "
					  "      FROM   pg_catalog.pg_index i "
					  "             JOIN pg_catalog.pg_inherits p "
					  "               ON (i.indrelid = p.inhparent) "
					  "             JOIN pg_catalog.pg_inherits sp "
					  "               ON (sp.inhparent = p.inhrelid) "
					  "      WHERE  i.indexrelid = '%u'::pg_catalog.oid "
					  "     ) "
					  ") "
					  "SELECT ir.indexrelid "
					  "FROM   pg_catalog.pg_index ir "
					  "       JOIN partitions p "
					  "         ON (p.inhrelid = ir.indrelid);",
					  info->dobj.catId.oid, info->dobj.catId.oid);

	index_res = PQexec(conn, index_query->data);

	/*
	 * If there were indexes, preassign the index Oid
	 */
	if (PQntuples(index_res) > 0)
	{
		i_oid = PQfnumber(index_res, "indexrelid");

		for (i = 0; i < PQntuples(index_res); i++)
		{
			idx_oid = atooid(PQgetvalue(index_res, i, i_oid));
			preassign_pg_class_oids(conn, fout, AH, idx_oid);
		}
	}

	PQclear(index_res);
	destroyPQExpBuffer(index_query);
}

void
dumpTableOid(PGconn *conn, Archive *fout, Archive *AH, TableInfo *info)
{
	int		j;

	/* Skip if not to be dumped */
	if (!info->dobj.dump && info->parrelid == 0)
		return;

	preassign_pg_class_oids(conn, fout, AH, info->dobj.catId.oid);
	preassign_type_oids_by_rel_oid(conn, fout, AH, info->dobj.catId.oid,
								   info->dobj.name);

	if (info->relkind == RELKIND_RELATION && info->relstorage != RELSTORAGE_EXTERNAL)
	{
		/* Dump Oids for attribute defaults */
		for (j = 0; j < info->numatts; j++)
		{
			if (info->attrdefs[j] != NULL)
				preassign_attrdefs_oid(AH, info->attrdefs[j]->dobj.catId.oid,
									   info->dobj.catId.oid,
									   info->attrdefs[j]->adnum);
		}

		/* Dump Oids for constraints */
		for (j = 0; j < info->ncheck; j++)
		{
			ConstraintInfo *c = &(info->checkexprs[j]);
			preassign_constraint_oid(AH, c->dobj.catId.oid,
									 c->dobj.namespace->dobj.catId.oid,
									 c->dobj.name,
									 c->contable ? c->contable->dobj.catId.oid : InvalidOid,
									 c->condomain ? c->condomain->dobj.catId.oid : InvalidOid);
		}
	}
	else if (info->relkind == RELKIND_VIEW)
	{
		preassign_view_rule_oids(conn, AH, info->dobj.catId.oid);
	}
}

static void
preassign_view_rule_oids(PGconn *conn, Archive *AH, Oid view_oid)
{
	PQExpBuffer	upgrade_query = createPQExpBuffer();
	PQExpBuffer upgrade_buffer = createPQExpBuffer();
	int			ntups;
	int			i;
	PGresult   *upgrade_res;
	Oid			rule_oid;
	char	   *rule_name;

	appendPQExpBuffer(upgrade_query,
					  "SELECT oid, rulename "
					  "FROM   pg_catalog.pg_rewrite "
					  "WHERE  ev_class='%u'::pg_catalog.oid;",
					  view_oid);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(upgrade_res);

	if (ntups < 1)
	{
		write_msg(NULL, "query returned no rows: %s\n", upgrade_query->data);
		exit_nicely();
	}

	for (i = 0; i < ntups; i++)
	{
		rule_oid = atooid(PQgetvalue(upgrade_res, i, PQfnumber(upgrade_res, "oid")));
		rule_name = PQgetvalue(upgrade_res, i, PQfnumber(upgrade_res, "rulename"));

		appendPQExpBuffer(upgrade_buffer,
			 "SELECT binary_upgrade.preassign_rule_oid("
			 "'%u'::pg_catalog.oid, '%u'::pg_catalog.oid, $$%s$$::text);\n",
			 rule_oid, view_oid, rule_name);
	}

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "pg_rewrite",
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", upgrade_buffer->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);
	destroyPQExpBuffer(upgrade_buffer);
}

static void
preassign_pg_class_oids(PGconn *conn, Archive *fout, Archive *AH, Oid pg_class_oid)
{
	PQExpBuffer upgrade_query = createPQExpBuffer();
	PQExpBuffer upgrade_buffer = createPQExpBuffer();
	int			ntups;
	PGresult   *upgrade_res;
	Oid			pg_class_reltoastnamespace;
	Oid			pg_class_reltoastrelid;
	Oid			pg_class_reltoastidxid;
	Oid			pg_class_relnamespace;
	char	   *pg_class_relname;
	Oid			pg_appendonly_segrelid;
	Oid			pg_appendonly_blkdirrelid;
	Oid			pg_appendonly_blkdiridxid;
	Oid			pg_appendonly_visimaprelid;
	Oid			pg_appendonly_visimapidxid;
	PQExpBuffer aoseg_query;
	PGresult   *aoseg_res;
	Oid			aoseg_namespace;
	bool		columnstore;
	bool		bitmapindex;
	PQExpBuffer bm_query;
	PGresult   *bm_res;
	Oid			bm_oid;
	Oid			bm_ns;
	char	   *bm_name;

	appendPQExpBuffer(upgrade_query,
					  "SELECT c.reltoastrelid, t.reltoastidxid, "
					  "       t.relnamespace as toastnamespace, "
					  "       ao.segrelid, c.relnamespace, "
					  "       ao.blkdirrelid, ao.blkdiridxid, "
					  "       ao.visimaprelid, ao.visimapidxid, "
					  "       c.relname, ao.columnstore, "
					  "       a.amname "
					  "FROM   pg_catalog.pg_class c "
					  "       LEFT JOIN pg_catalog.pg_class t ON (c.reltoastrelid = t.oid) "
					  "       LEFT JOIN pg_catalog.pg_appendonly ao ON (ao.relid = c.oid) "
					  "       LEFT JOIN pg_catalog.pg_am a ON (a.oid = c.relam AND c.relam <> 0) "
					  "WHERE  c.oid = '%u'::pg_catalog.oid;",
					  pg_class_oid);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
								 "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, upgrade_query->data);
		exit_nicely();
	}

	pg_class_reltoastnamespace = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "toastnamespace")));
	pg_class_reltoastrelid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "reltoastrelid")));
	pg_class_reltoastidxid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "reltoastidxid")));
	pg_class_relnamespace = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "relnamespace")));
	pg_class_relname = PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "relname"));
	pg_appendonly_segrelid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "segrelid")));
	pg_appendonly_blkdirrelid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "blkdirrelid")));
	pg_appendonly_blkdiridxid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "blkdiridxid")));
	pg_appendonly_visimaprelid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "visimaprelid")));
	pg_appendonly_visimapidxid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "visimapidxid")));
	columnstore = (strcmp(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "columnstore")), "t") == 0) ? true : false;
	bitmapindex = (strcmp(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "amname")), "bitmap") == 0) ? true : false;

	appendPQExpBuffer(upgrade_buffer,
					  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																   "$$%s$$::text, "
																   "'%u'::pg_catalog.oid);\n",
					  pg_class_oid, pg_class_relname, pg_class_relnamespace);

	/*
	 * If we have an AO relation we will need the aoseg namespace so
	 * extract and save
	 */
	if (OidIsValid(pg_appendonly_segrelid))
	{
		aoseg_query = createPQExpBuffer();

		appendPQExpBuffer(aoseg_query, "SELECT oid from pg_namespace WHERE nspname = 'pg_aoseg';");
		aoseg_res = PQexec(conn, aoseg_query->data);
		aoseg_namespace = atooid(PQgetvalue(aoseg_res, 0, PQfnumber(aoseg_res, "oid")));

		PQclear(aoseg_res);
		destroyPQExpBuffer(aoseg_query);
	}

	/* only tables have toast tables, not indexes */
	if (OidIsValid(pg_class_reltoastrelid))
	{
		/*
		 * One complexity is that the table definition might not require
		 * the creation of a TOAST table, and the TOAST table might have
		 * been created long after table creation, when the table was
		 * loaded with wide data.  By setting the TOAST oid we force
		 * creation of the TOAST heap and TOAST index by the backend so we
		 * can cleanly copy the files during binary upgrade.
		 */
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "'pg_toast_%u'::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  pg_class_reltoastrelid, pg_class_oid, pg_class_reltoastnamespace);


		/* every toast table has an index */
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "'pg_toast_%u_index'::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  pg_class_reltoastidxid, pg_class_oid, pg_class_reltoastnamespace);
	}
	if (OidIsValid(pg_appendonly_segrelid))
	{
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "'pg_ao%sseg_%u'::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  pg_appendonly_segrelid, (columnstore ? "cs" : ""), pg_class_oid, aoseg_namespace);
	}
	if (OidIsValid(pg_appendonly_blkdirrelid))
	{
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "'pg_aoblkdir_%u'::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  pg_appendonly_blkdirrelid, pg_class_oid, aoseg_namespace);

		/* every aoblkdir table has an index */
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "'pg_aoblkdir_%u_index'::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  pg_appendonly_blkdiridxid, pg_class_oid, aoseg_namespace);
	}
	if (OidIsValid(pg_appendonly_visimaprelid))
	{
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "'pg_aovisimap_%u'::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  pg_appendonly_visimaprelid, pg_class_oid, aoseg_namespace);

		/* every aovisimap table has an index */
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "'pg_aovisimap_%u_index'::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  pg_appendonly_visimapidxid, pg_class_oid, aoseg_namespace);
	}

	/*
	 * Bitmap indexes have an auxiliary heap table called pg_bm_<oid> which we
	 * need to find the Oid for as well. We could LEFT JOIN this information in
	 * the above query but rather than paying for that extra join in every rel
	 * lookup we'll perform a simpler query only for when we know we need it.
	 */
	if (bitmapindex)
	{
		bm_query = createPQExpBuffer();

		appendPQExpBuffer(bm_query,
						  "SELECT c.oid AS bm_oid, c.relnamespace AS bm_ns, c.relname AS bm_name, "
						  "       i.oid AS bmi_oid, i.relnamespace AS bmi_ns, i.relname AS bmi_name "
						  "FROM   pg_catalog.pg_class c "
						  "       LEFT JOIN pg_catalog.pg_index ii ON (ii.indrelid = c.oid) "
						  "       LEFT JOIN pg_catalog.pg_class i ON (ii.indexrelid = i.oid) "
						  "WHERE  c.relname = 'pg_bm_%u'::text;",
						  pg_class_oid);

		bm_res = PQexec(conn, bm_query->data);

		/* Extract the auxiliary bitmap index heap table */
		bm_oid = atooid(PQgetvalue(bm_res, 0, PQfnumber(bm_res, "bm_oid")));
		bm_ns = atooid(PQgetvalue(bm_res, 0, PQfnumber(bm_res, "bm_ns")));
		bm_name = PQgetvalue(bm_res, 0, PQfnumber(bm_res, "bm_name"));
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "$$%s$$::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  bm_oid, bm_name, bm_ns);

		preassign_type_oids_by_rel_oid(conn, fout, AH, bm_oid, bm_name);

		/* Extract the auxiliary bitmap index heap table btree index.. */
		bm_oid = atooid(PQgetvalue(bm_res, 0, PQfnumber(bm_res, "bmi_oid")));
		bm_ns = atooid(PQgetvalue(bm_res, 0, PQfnumber(bm_res, "bmi_ns")));
		bm_name = PQgetvalue(bm_res, 0, PQfnumber(bm_res, "bmi_name"));
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																	   "$$%s$$::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  bm_oid, bm_name, bm_ns);

		PQclear(bm_res);
		destroyPQExpBuffer(bm_query);
	}

	appendPQExpBuffer(upgrade_buffer, "\n");

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "preassign_pg_class",
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", upgrade_buffer->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(upgrade_buffer);
}

static void
preassign_type_oids_by_rel_oid(PGconn *conn, Archive *fout, Archive *AH, Oid pg_rel_oid, char *objname)
{
	PQExpBuffer upgrade_query;
	PQExpBuffer upgrade_buffer;
	int			ntups;
	PGresult   *upgrade_res;
	Oid			pg_type_oid;
	bool		columnstore;

	upgrade_query = createPQExpBuffer();
	upgrade_buffer = createPQExpBuffer();

	appendPQExpBuffer(upgrade_query,
					  "SELECT c.reltype AS crel, t.reltype AS trel, "
					  "       t.relnamespace AS trelnamespace, "
					  "       aoseg.reltype AS aosegrel, "
					  "       aoseg.relnamespace AS aonamespace, "
					  "       aoblkdir.reltype AS aoblkdirrel, "
					  "       aoblkdir.relnamespace AS aoblkdirnamespace, "
					  "       aovisimap.reltype AS aovisimaprel, "
					  "       aovisimap.relnamespace AS aovisimapnamespace, "
					  "       ao.columnstore, "
					  "       CASE WHEN c.relhassubclass THEN True "
					  "       ELSE NULL END AS par_parent "
					  "FROM pg_catalog.pg_class c "
					  "LEFT JOIN pg_catalog.pg_class t ON "
					  "  (c.reltoastrelid = t.oid) "
					  "LEFT JOIN pg_catalog.pg_appendonly ao ON "
					  "  (c.oid = ao.relid) "
					  "LEFT JOIN pg_catalog.pg_class aoseg ON "
					  "  (ao.segrelid = aoseg.oid) "
					  "LEFT JOIN pg_catalog.pg_class aoblkdir ON "
					  "  (ao.blkdirrelid = aoblkdir.oid) "
					  "LEFT JOIN pg_catalog.pg_class aovisimap ON "
					  "  (ao.visimaprelid = aovisimap.oid) "
					  "WHERE c.oid = '%u'::pg_catalog.oid;",
					  pg_rel_oid);

	upgrade_res = PQexec(conn, upgrade_query->data);
	check_sql_result(upgrade_res, conn, upgrade_query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(upgrade_res);
	if (ntups != 1)
	{
		write_msg(NULL, ngettext("query returned %d row instead of one: %s\n",
							   "query returned %d rows instead of one: %s\n",
								 ntups),
				  ntups, upgrade_query->data);
		exit_nicely();
	}

	pg_type_oid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "crel")));
	columnstore = (strcmp(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "columnstore")), "t") == 0) ? true : false;

	preassign_type_oid(conn, fout, AH, pg_type_oid, objname);

	if (!PQgetisnull(upgrade_res, 0, PQfnumber(upgrade_res, "trel")))
	{
		/* Toast tables do not have pg_type array rows */
		Oid			pg_type_toast_oid = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "trel")));
		Oid			pg_type_toast_namespace_oid = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "trelnamespace")));

		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_type_oid('%u'::pg_catalog.oid, "
																	   "'pg_toast_%u'::text, "
																	   "'%u'::pg_catalog.oid);\n",
						  pg_type_toast_oid, pg_rel_oid, pg_type_toast_namespace_oid);
	}

	if (!PQgetisnull(upgrade_res, 0, PQfnumber(upgrade_res, "aosegrel")))
	{
		/* AO segment tables do not have pg_type array rows */
		Oid			pg_type_aosegments_oid = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "aosegrel")));
		Oid			pg_type_aonamespace_oid = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "aonamespace")));

		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_type_oid('%u'::pg_catalog.oid, "
																   "'pg_ao%sseg_%u'::text, "
																   "'%u'::pg_catalog.oid);\n",
						  pg_type_aosegments_oid, (columnstore ? "cs" : ""), pg_rel_oid, pg_type_aonamespace_oid);
	}

	if (!PQgetisnull(upgrade_res, 0, PQfnumber(upgrade_res, "aoblkdirrel")))
	{
		/* AO blockdir tables do not have pg_type array rows */
		Oid			pg_type_aoblockdir_oid = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "aoblkdirrel")));
		Oid			pg_type_aoblockdir_namespace = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "aoblkdirnamespace")));

		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_type_oid('%u'::pg_catalog.oid, "
																   "'pg_aoblkdir_%u'::text, "
																   "'%u'::pg_catalog.oid);\n",
						  pg_type_aoblockdir_oid, pg_rel_oid, pg_type_aoblockdir_namespace);
	}

	if (!PQgetisnull(upgrade_res, 0, PQfnumber(upgrade_res, "aovisimaprel")))
	{
		/* AO visimap tables do not have pg_type array rows */
		Oid			pg_type_aovisimap_oid = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "aovisimaprel")));
		Oid			pg_type_aovisimap_namespace = atooid(PQgetvalue(upgrade_res, 0,
											PQfnumber(upgrade_res, "aovisimapnamespace")));

		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_type_oid('%u'::pg_catalog.oid, "
																   "'pg_aovisimap_%u'::text, "
																   "'%u'::pg_catalog.oid);\n",
						  pg_type_aovisimap_oid, pg_rel_oid, pg_type_aovisimap_namespace);
	}

	PQclear(upgrade_res);

	/*
	 * It's not unlikely that neither check resulted in more Oids preassigned
	 * here since the type Oid is handled in the call to preassign_type_oid
	 * leaving this function to deal with toast and AO auxiliary tables etc.
	 * Skip adding an archive entry if nothing was added.
	 */
	if (upgrade_buffer->len > 0)
	{
		ArchiveEntry(AH, nilCatalogId, createDumpId(),
					 objname,
					 NULL, NULL, "",
					 false, "BINARY UPGRADE", upgrade_buffer->data, "", NULL,
					 NULL, 0,
					 NULL, NULL);
	}

	destroyPQExpBuffer(upgrade_query);
	destroyPQExpBuffer(upgrade_buffer);
}
