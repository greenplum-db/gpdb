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
static void preassign_pg_class_oids(PGconn *conn, Archive *AH, Oid pg_class_oid);
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
			 "'%u'::pg_catalog.oid, '%s'::text);\n",
			 info->dobj.catId.oid, info->dobj.name);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, NULL, NULL,
				 NULL, 0,
				 NULL, NULL);
}

void
dumpProcedureOid(Archive *AH, FuncInfo *info)
{
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_procedure_oid("
			 "'%u'::pg_catalog.oid, '%s'::text, '%u'::pg_catalog.oid);\n",
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
					  "WHERE  tmplname = '%s'::text;",
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
					  "'%u'::pg_catalog.oid, '%s'::text, '%u'::pg_catalog.oid);\n",
					  procoid, proname, nsoid);

	/* Validator function */
	procoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "validatoroid")));
	if (OidIsValid(procoid))
	{
		nsoid = atooid(PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "validatorns")));
		proname = PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "validator"));
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.preassign_procedure_oid("
						  "'%u'::pg_catalog.oid, '%s'::text, '%u'::pg_catalog.oid);\n",
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
							  "'%u'::pg_catalog.oid, '%s'::text, '%u'::pg_catalog.oid);\n",
							  procoid, proname, nsoid);
		}
	}

	appendPQExpBuffer(upgrade_buffer,
					  "SELECT binary_upgrade.preassign_language_oid("
					  "'%u'::pg_catalog.oid, '%s'::text);\n",
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
			 "'%s'::text, '%u'::pg_catalog.oid);\n",
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
			 "'%u'::pg_catalog.oid, '%u'::pg_catalog.oid, '%s'::text);\n",
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
			 "'%s'::text, '%u'::pg_catalog.oid);",
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
dumpOpClassOid(PGconn *conn, Archive *AH, OpclassInfo *info)
{
	PQExpBuffer	upgrade_query;
	int			ntups;
	PGresult   *upgrade_res;
	Oid			pg_opclass_oid;
	Oid			opcnamespace;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	upgrade_query = createPQExpBuffer();

	appendPQExpBuffer(upgrade_query,
					  "SELECT oid, opcnamespace "
					  "FROM pg_catalog.pg_opclass "
					  "WHERE opcname = '%s'::text",
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

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_opclass_oid('%u'::pg_opclass.oid, "
			 "'%s'::text, '%u'::pg_opclass.opcnamespace);",
			 info->dobj.catId.oid, info->dobj.name, opcnamespace);

	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 info->dobj.name,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
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
			 "'%u'::pg_catalog.oid, '%s'::text);\n",
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
			 "'%u'::pg_catalog.oid, '%s'::text);\n",
			 info->dobj.catId.oid, info->dobj.namespace->dobj.catId.oid,
			 info->dobj.name);

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
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	preassign_type_oid(conn, fout, AH, info->dobj.catId.oid, info->dobj.name);
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
		preassign_pg_class_oids(conn, AH, info->typrelid);
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
						  "SELECT binary_upgrade.preassign_enum_oid('%u'::pg_catalog.oid, "
																   "'%u'::pg_catalog.oid, "
																   "'%s'::text);\n",
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
					char array_name[NAMEDATALEN];

					typecache[i].arraytypoid = InvalidOid,
					typecache[i].arraytypnsp = atooid(PQgetvalue(upgrade_res, i, i_nsp));

					snprintf(array_name, NAMEDATALEN, "_%s", PQgetvalue(upgrade_res, i, i_name));
					typecache[i].arraytypname = strdup(array_name);
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
		write_msg(NULL, "ERROR: didn't find type information in cache\n");
		exit_nicely();
	}

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT binary_upgrade.preassign_type_oid('%u'::pg_catalog.oid, "
			 "'%s'::text, '%u'::pg_catalog.oid);\n"
			 "SELECT binary_upgrade.preassign_arraytype_oid('%u'::pg_catalog.oid, "
			 "'%s'::text, '%u'::pg_catalog.oid);\n",
			 pg_type_oid, type->dobj.name, type->typnsp,
			 type->arraytypoid, type->arraytypname, type->arraytypnsp);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 objname,
				 NULL, NULL, "",
				 false, "BINARY UPGRADE", query_buffer, "", NULL,
				 NULL, 0,
				 NULL, NULL);
}

/*
 * dumpConstraintOid
 *
 * Writes out preassigned Oids for user defined constraints. Error handling
 * for non-existing Indexes is performed in dumpConstraint() so defer there
 * for consistency with upstream even though we are being executed first.
 */
void
dumpConstraintOid(PGconn *conn, Archive *AH, ConstraintInfo *info)
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

		preassign_pg_class_oids(conn, AH, indxinfo->dobj.catId.oid);
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
			preassign_pg_class_oids(conn, AH, tbinfo->dobj.catId.oid);

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
			 "'%u'::pg_catalog.oid, '%s'::text, '%u'::pg_catalog.oid, '%u'::pg_catalog.oid);\n",
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
			 "'%u'::pg_catalog.oid, '%s'::text);\n",
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

void
dumpIndexOid(PGconn *conn, Archive *AH, IndxInfo *info)
{
	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	preassign_pg_class_oids(conn, AH, info->dobj.catId.oid);
}

void
dumpTableOid(PGconn *conn, Archive *fout, Archive *AH, TableInfo *info)
{
	int		j;

	/* Skip if not to be dumped */
	if (!info->dobj.dump)
		return;

	preassign_pg_class_oids(conn, AH, info->dobj.catId.oid);
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
			 "'%u'::pg_catalog.oid, '%u'::pg_catalog.oid, '%s'::text);\n",
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
preassign_pg_class_oids(PGconn *conn, Archive *AH, Oid pg_class_oid)
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

	appendPQExpBuffer(upgrade_query,
					  "SELECT c.reltoastrelid, t.reltoastidxid, "
					  "       t.relnamespace as toastnamespace, "
					  "       ao.segrelid, c.relnamespace, "
					  "       ao.blkdirrelid, ao.blkdiridxid, "
					  "       ao.visimaprelid, ao.visimapidxid, "
					  "       c.relname, ao.columnstore "
					  "FROM pg_catalog.pg_class c LEFT JOIN "
					  "pg_catalog.pg_class t ON (c.reltoastrelid = t.oid) "
					  "LEFT JOIN pg_catalog.pg_appendonly ao ON (ao.relid = c.oid) "
					  "WHERE c.oid = '%u'::pg_catalog.oid;",
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

	appendPQExpBuffer(upgrade_buffer,
					  "SELECT binary_upgrade.preassign_relation_oid('%u'::pg_catalog.oid, "
																   "'%s'::text, "
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

	/*
	 * If the table is partitioned and is the parent, we need to dump the Oids
	 * of the child tables as well
	 */
	if (!PQgetisnull(upgrade_res, 0, PQfnumber(upgrade_res, "par_parent")))
	{
		PQExpBuffer parquery = createPQExpBuffer();
		PGresult   *par_res;
		int			i;
		char		name[NAMEDATALEN];
		Oid			part_oid;
		Oid			conns_oid;
		Oid			contyp_oid;
		Oid			con_oid;
		Oid			prev_oid = InvalidOid;

		appendPQExpBuffer(parquery,
						  "SELECT cc.oid, "
						  "       p.partitiontablename AS name, "
						  "       co.oid AS conoid, "
						  "       co.conname, "
						  "       co.connamespace, "
						  "       co.contypid "
						  "FROM pg_partitions p "
						  "JOIN pg_catalog.pg_class c ON "
						  "  (p.tablename = c.relname AND c.oid = '%u'::pg_catalog.oid) "
						  "JOIN pg_catalog.pg_class cc ON "
						  "  (p.partitiontablename = cc.relname) "
						  "LEFT JOIN pg_catalog.pg_constraint co ON "
						  "  (cc.oid = co.conrelid);",
						  pg_rel_oid);

		par_res = PQexec(conn, parquery->data);
		check_sql_result(par_res, conn, parquery->data, PGRES_TUPLES_OK);

		if (PQntuples(par_res) > 0)
		{
			for (i = 0; i < PQntuples(par_res); i++)
			{
				part_oid = atooid(PQgetvalue(par_res, i, PQfnumber(par_res, "oid")));

				/*
				 * Partitions with multiple constraint will be on multiple
				 * rows so ensure to only save their Oids once.
				 */
				if (part_oid != prev_oid)
				{
					strlcpy(name, PQgetvalue(par_res, i, PQfnumber(par_res, "name")), sizeof(name));
					preassign_type_oids_by_rel_oid(conn, fout, AH, part_oid, name);
					preassign_pg_class_oids(conn, AH, part_oid);
				}

				if (!PQgetisnull(par_res, i, PQfnumber(par_res, "conname")))
				{
					strlcpy(name, PQgetvalue(par_res, i, PQfnumber(par_res, "conname")), sizeof(name));
					con_oid = atooid(PQgetvalue(par_res, i, PQfnumber(par_res, "conoid")));
					conns_oid = atooid(PQgetvalue(par_res, i, PQfnumber(par_res, "connamespace")));
					contyp_oid = atooid(PQgetvalue(par_res, i, PQfnumber(par_res, "contypid")));

					preassign_constraint_oid(AH, con_oid, conns_oid, name, part_oid, contyp_oid);
				}

				prev_oid = part_oid;
			}
		}

		PQclear(par_res);
		destroyPQExpBuffer(parquery);
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
