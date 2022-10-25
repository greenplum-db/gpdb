/*-------------------------------------------------------------------------
 *
 * foreign.c
 *		  support for foreign-data wrappers, servers and user mappings.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  src/backend/foreign/foreign.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/reloptions.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "cdb/cdbutil.h"
#include "commands/defrem.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"


extern Datum pg_options_to_table(PG_FUNCTION_ARGS);
extern Datum postgresql_fdw_validator(PG_FUNCTION_ARGS);

/* Get and separate out the mpp_execute option. */
char
SeparateOutMppExecute(List **options)
{
	ListCell *lc = NULL;
	ListCell *prev = NULL;
	char *mpp_execute = NULL;
	char exec_location = FTEXECLOCATION_NOT_DEFINED;

	foreach(lc, *options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "mpp_execute") == 0)
		{
			mpp_execute = defGetString(def);

			if (pg_strcasecmp(mpp_execute, "any") == 0)
				exec_location = FTEXECLOCATION_ANY;
			else if (pg_strcasecmp(mpp_execute, "master") == 0)
				exec_location = FTEXECLOCATION_COORDINATOR;
			else if (pg_strcasecmp(mpp_execute, "coordinator") == 0)
				exec_location = FTEXECLOCATION_COORDINATOR;
			else if (pg_strcasecmp(mpp_execute, "all segments") == 0)
				exec_location = FTEXECLOCATION_ALL_SEGMENTS;
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("\"%s\" is not a valid mpp_execute value",
								mpp_execute)));
			}

			*options = list_delete_cell(*options, lc, prev);
			break;
		}
		prev = lc;
	}

	return exec_location;
}

/* Get and separate out the num_segments option */
int32
SeparateOutNumSegments(List **options)
{
	ListCell *lc = NULL;
	ListCell *prev = NULL;
	char *num_segments_str = NULL;
	int32 num_segments = 0;

	foreach(lc, *options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "num_segments") == 0)
		{
			num_segments_str = defGetString(def);
			num_segments = pg_atoi(num_segments_str, sizeof(int32), 0);

			if (num_segments <= 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("\"%d\" is not a valid num_segments value",
								num_segments)));
			}

			*options = list_delete_cell(*options, lc, prev);
			break;
		}

		prev = lc;
	}
	return num_segments;
}

/*
 * GetForeignDataWrapper -	look up the foreign-data wrapper by OID.
 */
ForeignDataWrapper *
GetForeignDataWrapper(Oid fdwid)
{
	return GetForeignDataWrapperExtended(fdwid, 0);
}


/*
 * GetForeignDataWrapperExtended -	look up the foreign-data wrapper
 * by OID. If flags uses FDW_MISSING_OK, return NULL if the object cannot
 * be found instead of raising an error.
 */
ForeignDataWrapper *
GetForeignDataWrapperExtended(Oid fdwid, bits16 flags)
{
	Form_pg_foreign_data_wrapper fdwform;
	ForeignDataWrapper *fdw;
	Datum		datum;
	HeapTuple	tp;
	bool		isnull;

	tp = SearchSysCache1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdwid));

	if (!HeapTupleIsValid(tp))
	{
		if ((flags & FDW_MISSING_OK) == 0)
			elog(ERROR, "cache lookup failed for foreign-data wrapper %u", fdwid);
		return NULL;
	}

	fdwform = (Form_pg_foreign_data_wrapper) GETSTRUCT(tp);

	fdw = (ForeignDataWrapper *) palloc(sizeof(ForeignDataWrapper));
	fdw->fdwid = fdwid;
	fdw->owner = fdwform->fdwowner;
	fdw->fdwname = pstrdup(NameStr(fdwform->fdwname));
	fdw->fdwhandler = fdwform->fdwhandler;
	fdw->fdwvalidator = fdwform->fdwvalidator;

	/* Extract the fdwoptions */
	datum = SysCacheGetAttr(FOREIGNDATAWRAPPEROID,
							tp,
							Anum_pg_foreign_data_wrapper_fdwoptions,
							&isnull);
	if (isnull)
		fdw->options = NIL;
	else
		fdw->options = untransformRelOptions(datum);

	fdw->exec_location = SeparateOutMppExecute(&fdw->options);
	if (fdw->exec_location == FTEXECLOCATION_NOT_DEFINED)
		fdw->exec_location = FTEXECLOCATION_COORDINATOR;

	ReleaseSysCache(tp);

	return fdw;
}


/*
 * GetForeignDataWrapperByName - look up the foreign-data wrapper
 * definition by name.
 */
ForeignDataWrapper *
GetForeignDataWrapperByName(const char *fdwname, bool missing_ok)
{
	Oid			fdwId = get_foreign_data_wrapper_oid(fdwname, missing_ok);

	if (!OidIsValid(fdwId))
		return NULL;

	return GetForeignDataWrapper(fdwId);
}


/*
 * GetForeignServer - look up the foreign server definition.
 */
ForeignServer *
GetForeignServer(Oid serverid)
{
	return GetForeignServerExtended(serverid, 0);
}


/*
 * GetForeignServerExtended - look up the foreign server definition. If
 * flags uses FSV_MISSING_OK, return NULL if the object cannot be found
 * instead of raising an error.
 */
ForeignServer *
GetForeignServerExtended(Oid serverid, bits16 flags)
{
	Form_pg_foreign_server serverform;
	ForeignServer *server;
	HeapTuple	tp;
	Datum		datum;
	bool		isnull;

	tp = SearchSysCache1(FOREIGNSERVEROID, ObjectIdGetDatum(serverid));

	if (!HeapTupleIsValid(tp))
	{
		if ((flags & FSV_MISSING_OK) == 0)
			elog(ERROR, "cache lookup failed for foreign server %u", serverid);
		return NULL;
	}

	serverform = (Form_pg_foreign_server) GETSTRUCT(tp);

	server = (ForeignServer *) palloc(sizeof(ForeignServer));
	server->serverid = serverid;
	server->servername = pstrdup(NameStr(serverform->srvname));
	server->owner = serverform->srvowner;
	server->fdwid = serverform->srvfdw;

	/* Extract server type */
	datum = SysCacheGetAttr(FOREIGNSERVEROID,
							tp,
							Anum_pg_foreign_server_srvtype,
							&isnull);
	server->servertype = isnull ? NULL : TextDatumGetCString(datum);

	/* Extract server version */
	datum = SysCacheGetAttr(FOREIGNSERVEROID,
							tp,
							Anum_pg_foreign_server_srvversion,
							&isnull);
	server->serverversion = isnull ? NULL : TextDatumGetCString(datum);

	/* Extract the srvoptions */
	datum = SysCacheGetAttr(FOREIGNSERVEROID,
							tp,
							Anum_pg_foreign_server_srvoptions,
							&isnull);
	if (isnull)
		server->options = NIL;
	else
		server->options = untransformRelOptions(datum);

	server->exec_location = SeparateOutMppExecute(&server->options);
	if (server->exec_location == FTEXECLOCATION_NOT_DEFINED)
	{
		ForeignDataWrapper *fdw = GetForeignDataWrapper(server->fdwid);
		server->exec_location = fdw->exec_location;
	}

	server->num_segments = SeparateOutNumSegments(&server->options);
	if (server->num_segments <= 0)
	{
		server->num_segments = getgpsegmentCount();
	}

	ReleaseSysCache(tp);

	return server;
}


/*
 * GetForeignServerByName - look up the foreign server definition by name.
 */
ForeignServer *
GetForeignServerByName(const char *srvname, bool missing_ok)
{
	Oid			serverid = get_foreign_server_oid(srvname, missing_ok);

	if (!OidIsValid(serverid))
		return NULL;

	return GetForeignServer(serverid);
}


/*
 * GetUserMapping - look up the user mapping.
 *
 * If no mapping is found for the supplied user, we also look for
 * PUBLIC mappings (userid == InvalidOid).
 */
UserMapping *
GetUserMapping(Oid userid, Oid serverid)
{
	Datum		datum;
	HeapTuple	tp;
	bool		isnull;
	UserMapping *um;

	tp = SearchSysCache2(USERMAPPINGUSERSERVER,
						 ObjectIdGetDatum(userid),
						 ObjectIdGetDatum(serverid));

	if (!HeapTupleIsValid(tp))
	{
		/* Not found for the specific user -- try PUBLIC */
		tp = SearchSysCache2(USERMAPPINGUSERSERVER,
							 ObjectIdGetDatum(InvalidOid),
							 ObjectIdGetDatum(serverid));
	}

	if (!HeapTupleIsValid(tp))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("user mapping not found for \"%s\"",
						MappingUserName(userid))));

	um = (UserMapping *) palloc(sizeof(UserMapping));
	um->umid = ((Form_pg_user_mapping) GETSTRUCT(tp))->oid;
	um->userid = userid;
	um->serverid = serverid;

	/* Extract the umoptions */
	datum = SysCacheGetAttr(USERMAPPINGUSERSERVER,
							tp,
							Anum_pg_user_mapping_umoptions,
							&isnull);
	if (isnull)
		um->options = NIL;
	else
		um->options = untransformRelOptions(datum);

	ReleaseSysCache(tp);

	return um;
}


/*
 * GetForeignTable - look up the foreign table definition by relation oid.
 */
ForeignTable *
GetForeignTable(Oid relid)
{
	Form_pg_foreign_table tableform;
	ForeignTable *ft;
	HeapTuple	tp;
	Datum		datum;
	bool		isnull;

	tp = SearchSysCache1(FOREIGNTABLEREL, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for foreign table %u", relid);
	tableform = (Form_pg_foreign_table) GETSTRUCT(tp);

	ft = (ForeignTable *) palloc(sizeof(ForeignTable));
	ft->relid = relid;
	ft->serverid = tableform->ftserver;

	/* Extract the ftoptions */
	datum = SysCacheGetAttr(FOREIGNTABLEREL,
							tp,
							Anum_pg_foreign_table_ftoptions,
							&isnull);
	if (isnull)
		ft->options = NIL;
	else
		ft->options = untransformRelOptions(datum);

	ft->exec_location = SeparateOutMppExecute(&ft->options);
	if (ft->exec_location == FTEXECLOCATION_NOT_DEFINED)
	{
		ForeignServer *server = GetForeignServer(ft->serverid);
		ft->exec_location = server->exec_location;
	}

	ReleaseSysCache(tp);

	return ft;
}

/*
 * Is the given table a GPDB external table, rather than a normal foreign
 * table?
 */
bool
rel_is_external_table(Oid relid)
{
	Form_pg_foreign_table tableform;
	HeapTuple	tp;
	bool		result;

	tp = SearchSysCache1(FOREIGNTABLEREL, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tp))
		return false;
	tableform = (Form_pg_foreign_table) GETSTRUCT(tp);

	result = (tableform->ftserver == get_foreign_server_oid(GP_EXTTABLE_SERVER_NAME, false));

	ReleaseSysCache(tp);

	return result;
}

/*
 * GetForeignColumnOptions - Get attfdwoptions of given relation/attnum
 * as list of DefElem.
 */
List *
GetForeignColumnOptions(Oid relid, AttrNumber attnum)
{
	List	   *options;
	HeapTuple	tp;
	Datum		datum;
	bool		isnull;

	tp = SearchSysCache2(ATTNUM,
						 ObjectIdGetDatum(relid),
						 Int16GetDatum(attnum));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 attnum, relid);
	datum = SysCacheGetAttr(ATTNUM,
							tp,
							Anum_pg_attribute_attfdwoptions,
							&isnull);
	if (isnull)
		options = NIL;
	else
		options = untransformRelOptions(datum);

	ReleaseSysCache(tp);

	return options;
}


/*
 * GetFdwRoutine - call the specified foreign-data wrapper handler routine
 * to get its FdwRoutine struct.
 */
FdwRoutine *
GetFdwRoutine(Oid fdwhandler)
{
	Datum		datum;
	FdwRoutine *routine;

	datum = OidFunctionCall0(fdwhandler);
	routine = (FdwRoutine *) DatumGetPointer(datum);

	if (routine == NULL || !IsA(routine, FdwRoutine))
		elog(ERROR, "foreign-data wrapper handler function %u did not return an FdwRoutine struct",
			 fdwhandler);

	return routine;
}


/*
 * GetForeignServerIdByRelId - look up the foreign server
 * for the given foreign table, and return its OID.
 */
Oid
GetForeignServerIdByRelId(Oid relid)
{
	HeapTuple	tp;
	Form_pg_foreign_table tableform;
	Oid			serverid;

	tp = SearchSysCache1(FOREIGNTABLEREL, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for foreign table %u", relid);
	tableform = (Form_pg_foreign_table) GETSTRUCT(tp);
	serverid = tableform->ftserver;
	ReleaseSysCache(tp);

	return serverid;
}


/*
 * GetFdwRoutineByServerId - look up the handler of the foreign-data wrapper
 * for the given foreign server, and retrieve its FdwRoutine struct.
 */
FdwRoutine *
GetFdwRoutineByServerId(Oid serverid)
{
	HeapTuple	tp;
	Form_pg_foreign_data_wrapper fdwform;
	Form_pg_foreign_server serverform;
	Oid			fdwid;
	Oid			fdwhandler;

	/* Get foreign-data wrapper OID for the server. */
	tp = SearchSysCache1(FOREIGNSERVEROID, ObjectIdGetDatum(serverid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for foreign server %u", serverid);
	serverform = (Form_pg_foreign_server) GETSTRUCT(tp);
	fdwid = serverform->srvfdw;
	ReleaseSysCache(tp);

	/* Get handler function OID for the FDW. */
	tp = SearchSysCache1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdwid));
	if (!HeapTupleIsValid(tp))
		elog(ERROR, "cache lookup failed for foreign-data wrapper %u", fdwid);
	fdwform = (Form_pg_foreign_data_wrapper) GETSTRUCT(tp);
	fdwhandler = fdwform->fdwhandler;

	/* Complain if FDW has been set to NO HANDLER. */
	if (!OidIsValid(fdwhandler))
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("foreign-data wrapper \"%s\" has no handler",
						NameStr(fdwform->fdwname))));

	ReleaseSysCache(tp);

	/* And finally, call the handler function. */
	return GetFdwRoutine(fdwhandler);
}


/*
 * GetFdwRoutineByRelId - look up the handler of the foreign-data wrapper
 * for the given foreign table, and retrieve its FdwRoutine struct.
 */
FdwRoutine *
GetFdwRoutineByRelId(Oid relid)
{
	Oid			serverid;

	/* Get server OID for the foreign table. */
	serverid = GetForeignServerIdByRelId(relid);

	/* Now retrieve server's FdwRoutine struct. */
	return GetFdwRoutineByServerId(serverid);
}

/*
 * GetFdwRoutineForRelation - look up the handler of the foreign-data wrapper
 * for the given foreign table, and retrieve its FdwRoutine struct.
 *
 * This function is preferred over GetFdwRoutineByRelId because it caches
 * the data in the relcache entry, saving a number of catalog lookups.
 *
 * If makecopy is true then the returned data is freshly palloc'd in the
 * caller's memory context.  Otherwise, it's a pointer to the relcache data,
 * which will be lost in any relcache reset --- so don't rely on it long.
 */
FdwRoutine *
GetFdwRoutineForRelation(Relation relation, bool makecopy)
{
	FdwRoutine *fdwroutine;
	FdwRoutine *cfdwroutine;

	if (relation->rd_fdwroutine == NULL)
	{
		/* Get the info by consulting the catalogs and the FDW code */
		fdwroutine = GetFdwRoutineByRelId(RelationGetRelid(relation));

		/* Save the data for later reuse in CacheMemoryContext */
		cfdwroutine = (FdwRoutine *) MemoryContextAlloc(CacheMemoryContext,
														sizeof(FdwRoutine));
		memcpy(cfdwroutine, fdwroutine, sizeof(FdwRoutine));
		relation->rd_fdwroutine = cfdwroutine;

		/* Give back the locally palloc'd copy regardless of makecopy */
		return fdwroutine;
	}

	/* We have valid cached data --- does the caller want a copy? */
	if (makecopy)
	{
		fdwroutine = (FdwRoutine *) palloc(sizeof(FdwRoutine));
		memcpy(fdwroutine, relation->rd_fdwroutine, sizeof(FdwRoutine));
		return fdwroutine;
	}

	/* Only a short-lived reference is needed, so just hand back cached copy */
	return relation->rd_fdwroutine;
}


/*
 * IsImportableForeignTable - filter table names for IMPORT FOREIGN SCHEMA
 *
 * Returns true if given table name should be imported according to the
 * statement's import filter options.
 */
bool
IsImportableForeignTable(const char *tablename,
						 ImportForeignSchemaStmt *stmt)
{
	ListCell   *lc;

	switch (stmt->list_type)
	{
		case FDW_IMPORT_SCHEMA_ALL:
			return true;

		case FDW_IMPORT_SCHEMA_LIMIT_TO:
			foreach(lc, stmt->table_list)
			{
				RangeVar   *rv = (RangeVar *) lfirst(lc);

				if (strcmp(tablename, rv->relname) == 0)
					return true;
			}
			return false;

		case FDW_IMPORT_SCHEMA_EXCEPT:
			foreach(lc, stmt->table_list)
			{
				RangeVar   *rv = (RangeVar *) lfirst(lc);

				if (strcmp(tablename, rv->relname) == 0)
					return false;
			}
			return true;
	}
	return false;				/* shouldn't get here */
}


/*
 * deflist_to_tuplestore - Helper function to convert DefElem list to
 * tuplestore usable in SRF.
 */
static void
deflist_to_tuplestore(ReturnSetInfo *rsinfo, List *options)
{
	ListCell   *cell;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	Datum		values[2];
	bool		nulls[2];
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize) ||
		rsinfo->expectedDesc == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not allowed in this context")));

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/*
	 * Now prepare the result set.
	 */
	tupdesc = CreateTupleDescCopy(rsinfo->expectedDesc);
	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	foreach(cell, options)
	{
		DefElem    *def = lfirst(cell);

		values[0] = CStringGetTextDatum(def->defname);
		nulls[0] = false;
		if (def->arg)
		{
			values[1] = CStringGetTextDatum(((Value *) (def->arg))->val.str);
			nulls[1] = false;
		}
		else
		{
			values[1] = (Datum) 0;
			nulls[1] = true;
		}
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	MemoryContextSwitchTo(oldcontext);
}


/*
 * Convert options array to name/value table.  Useful for information
 * schema and pg_dump.
 */
Datum
pg_options_to_table(PG_FUNCTION_ARGS)
{
	Datum		array = PG_GETARG_DATUM(0);

	deflist_to_tuplestore((ReturnSetInfo *) fcinfo->resultinfo,
						  untransformRelOptions(array));

	return (Datum) 0;
}


/*
 * Describes the valid options for postgresql FDW, server, and user mapping.
 */
struct ConnectionOption
{
	const char *optname;
	Oid			optcontext;		/* Oid of catalog in which option may appear */
};

/*
 * Copied from fe-connect.c PQconninfoOptions.
 *
 * The list is small - don't bother with bsearch if it stays so.
 */
static const struct ConnectionOption libpq_conninfo_options[] = {
	{"authtype", ForeignServerRelationId},
	{"service", ForeignServerRelationId},
	{"user", UserMappingRelationId},
	{"password", UserMappingRelationId},
	{"connect_timeout", ForeignServerRelationId},
	{"dbname", ForeignServerRelationId},
	{"host", ForeignServerRelationId},
	{"hostaddr", ForeignServerRelationId},
	{"port", ForeignServerRelationId},
	{"tty", ForeignServerRelationId},
	{"options", ForeignServerRelationId},
	{"requiressl", ForeignServerRelationId},
	{"sslmode", ForeignServerRelationId},
	{"gsslib", ForeignServerRelationId},
	{NULL, InvalidOid}
};


/*
 * Check if the provided option is one of libpq conninfo options.
 * context is the Oid of the catalog the option came from, or 0 if we
 * don't care.
 */
static bool
is_conninfo_option(const char *option, Oid context)
{
	const struct ConnectionOption *opt;

	for (opt = libpq_conninfo_options; opt->optname; opt++)
		if (context == opt->optcontext && strcmp(opt->optname, option) == 0)
			return true;
	return false;
}


/*
 * Validate the generic option given to SERVER or USER MAPPING.
 * Raise an ERROR if the option or its value is considered invalid.
 *
 * Valid server options are all libpq conninfo options except
 * user and password -- these may only appear in USER MAPPING options.
 *
 * Caution: this function is deprecated, and is now meant only for testing
 * purposes, because the list of options it knows about doesn't necessarily
 * square with those known to whichever libpq instance you might be using.
 * Inquire of libpq itself, instead.
 */
Datum
postgresql_fdw_validator(PG_FUNCTION_ARGS)
{
	List	   *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid			catalog = PG_GETARG_OID(1);

	ListCell   *cell;

	foreach(cell, options_list)
	{
		DefElem    *def = lfirst(cell);

		if (!is_conninfo_option(def->defname, catalog))
		{
			const struct ConnectionOption *opt;
			StringInfoData buf;

			/*
			 * Unknown option specified, complain about it. Provide a hint
			 * with list of valid options for the object.
			 */
			initStringInfo(&buf);
			for (opt = libpq_conninfo_options; opt->optname; opt++)
				if (catalog == opt->optcontext)
					appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "",
									 opt->optname);

			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("invalid option \"%s\"", def->defname),
					 errhint("Valid options in this context are: %s",
							 buf.data)));

			PG_RETURN_BOOL(false);
		}
	}

	PG_RETURN_BOOL(true);
}


/*
 * get_foreign_data_wrapper_oid - given a FDW name, look up the OID
 *
 * If missing_ok is false, throw an error if name not found.  If true, just
 * return InvalidOid.
 */
Oid
get_foreign_data_wrapper_oid(const char *fdwname, bool missing_ok)
{
	Oid			oid;

	oid = GetSysCacheOid1(FOREIGNDATAWRAPPERNAME,
						  Anum_pg_foreign_data_wrapper_oid,
						  CStringGetDatum(fdwname));
	if (!OidIsValid(oid) && !missing_ok)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("foreign-data wrapper \"%s\" does not exist",
						fdwname)));
	return oid;
}


/*
 * get_foreign_server_oid - given a server name, look up the OID
 *
 * If missing_ok is false, throw an error if name not found.  If true, just
 * return InvalidOid.
 */
Oid
get_foreign_server_oid(const char *servername, bool missing_ok)
{
	Oid			oid;

	oid = GetSysCacheOid1(FOREIGNSERVERNAME, Anum_pg_foreign_server_oid,
						  CStringGetDatum(servername));
	if (!OidIsValid(oid) && !missing_ok)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("server \"%s\" does not exist", servername)));
	return oid;
}

/*
 * Get a copy of an existing local path for a given join relation.
 *
 * This function is usually helpful to obtain an alternate local path for EPQ
 * checks.
 *
 * Right now, this function only supports unparameterized foreign joins, so we
 * only search for unparameterized path in the given list of paths. Since we
 * are searching for a path which can be used to construct an alternative local
 * plan for a foreign join, we look for only MergeJoin, HashJoin or NestLoop
 * paths.
 *
 * If the inner or outer subpath of the chosen path is a ForeignScan, we
 * replace it with its outer subpath.  For this reason, and also because the
 * planner might free the original path later, the path returned by this
 * function is a shallow copy of the original.  There's no need to copy
 * the substructure, so we don't.
 *
 * Since the plan created using this path will presumably only be used to
 * execute EPQ checks, efficiency of the path is not a concern. But since the
 * path list in RelOptInfo is anyway sorted by total cost we are likely to
 * choose the most efficient path, which is all for the best.
 */
Path *
GetExistingLocalJoinPath(RelOptInfo *joinrel)
{
	ListCell   *lc;

	Assert(IS_JOIN_REL(joinrel));

	foreach(lc, joinrel->pathlist)
	{
		Path	   *path = (Path *) lfirst(lc);
		JoinPath   *joinpath = NULL;

		/* Skip parameterized paths. */
		if (path->param_info != NULL)
			continue;

		switch (path->pathtype)
		{
			case T_HashJoin:
				{
					HashPath   *hash_path = makeNode(HashPath);

					memcpy(hash_path, path, sizeof(HashPath));
					joinpath = (JoinPath *) hash_path;
				}
				break;

			case T_NestLoop:
				{
					NestPath   *nest_path = makeNode(NestPath);

					memcpy(nest_path, path, sizeof(NestPath));
					joinpath = (JoinPath *) nest_path;
				}
				break;

			case T_MergeJoin:
				{
					MergePath  *merge_path = makeNode(MergePath);

					memcpy(merge_path, path, sizeof(MergePath));
					joinpath = (JoinPath *) merge_path;
				}
				break;

			default:

				/*
				 * Just skip anything else. We don't know if corresponding
				 * plan would build the output row from whole-row references
				 * of base relations and execute the EPQ checks.
				 */
				break;
		}

		/* This path isn't good for us, check next. */
		if (!joinpath)
			continue;

		/*
		 * If either inner or outer path is a ForeignPath corresponding to a
		 * pushed down join, replace it with the fdw_outerpath, so that we
		 * maintain path for EPQ checks built entirely of local join
		 * strategies.
		 */
		if (IsA(joinpath->outerjoinpath, ForeignPath))
		{
			ForeignPath *foreign_path;

			foreign_path = (ForeignPath *) joinpath->outerjoinpath;
			if (IS_JOIN_REL(foreign_path->path.parent))
				joinpath->outerjoinpath = foreign_path->fdw_outerpath;
		}

		if (IsA(joinpath->innerjoinpath, ForeignPath))
		{
			ForeignPath *foreign_path;

			foreign_path = (ForeignPath *) joinpath->innerjoinpath;
			if (IS_JOIN_REL(foreign_path->path.parent))
				joinpath->innerjoinpath = foreign_path->fdw_outerpath;
		}

		return (Path *) joinpath;
	}
	return NULL;
}

ForeignScan *
BuildForeignScan(Oid relid, Index scanrelid, List *qual, List *targetlist, Query *query, RangeTblEntry * rte)
{

	/*
	 * We need to make "dummy" or at least somewhat populated
	 * PlannerInfo and RelOptInfo structs here to ensure that fdw_private is properly
	 * populated. Although this isn't used during planning, some FDWs populate and use
	 * this info in the executor, which means Orca needs to call these functions in case.
	 *
	 * The simple_rte_array is used in build_simple_rel, and populates
	 * fields for the fdw. We only need 1 entry here, as we process only 1
	 * scan. We construt these arrays/structures to create a RelOptInfo,
	 * which is needed by the FDW API functions
	 */

	PlannerInfo		*root;
	root = makeNode(PlannerInfo);
	root->parse = query; // used to create RelOptInfo
	/* Arrays are accessed using RT indexes (1..N). We only need 1 entry
	 * here, as we're only processing 1 scan
	 */
	root->simple_rel_array_size = 2;

	/* simple_rel_array is initialized to all NULLs */
	root->simple_rel_array = (RelOptInfo **) palloc0(root->simple_rel_array_size * sizeof(RelOptInfo *));

	/* simple_rte_array is an array equivalent of the rtable list */
	root->simple_rte_array = (RangeTblEntry **) palloc0(root->simple_rel_array_size * sizeof(RangeTblEntry *));
	root->simple_rte_array[1] = rte;

	RelOptInfo *rel = build_simple_rel(root, 1 /* index 1 */, NULL);
	rel->fdwroutine->GetForeignRelSize(root, rel, relid);
	rel->fdwroutine->GetForeignPaths(root, rel, relid);

	// Use any path, we really just care about the fdw_private field here
	ForeignPath *path = (ForeignPath*) linitial(rel->pathlist);

	ForeignScan *fscan = make_foreignscan(targetlist,
					 qual,
					 relid,
					 NIL,
					 path->fdw_private,
					 NIL,
					 NIL,
					 NULL);
	fscan->fs_server = rel->serverid;

	// Set fsSystemCol if any system attributes are projected
	fscan->fsSystemCol = false;
	if (scanrelid > 0)
	{
		Bitmapset  *attrs_used = NULL;
		int			i;

		/*
		 * First, examine all the attributes needed for joins or final output.
		 * Note: we must look at rel's targetlist, not the attr_needed data,
		 * because attr_needed isn't computed for inheritance child rels.
		 */
		pull_varattnos((Node *) targetlist, scanrelid, &attrs_used);

		/* Now, are any system columns requested from rel? */
		for (i = FirstLowInvalidHeapAttributeNumber + 1; i < 0; i++)
		{
			if (bms_is_member(i - FirstLowInvalidHeapAttributeNumber, attrs_used))
			{
				fscan->fsSystemCol = true;
				break;
			}
		}

		bms_free(attrs_used);
	}
	return fscan;
}

