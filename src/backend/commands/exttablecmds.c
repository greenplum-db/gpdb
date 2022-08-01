/*-------------------------------------------------------------------------
 *
 * exttablecmds.c
 *	  Commands for creating and altering external tables
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/exttablecmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/extprotocol.h"
#include "access/reloptions.h"
#include "catalog/namespace.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_authid.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/syscache.h"
#include "utils/uri.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbsreh.h"

static Datum transformLocationUris(List *locs, bool isweb, bool iswritable);
static Datum transformExecOnClause(List *on_clause);
static char transformFormatType(char *formatname);
static Datum transformFormatOpts(char formattype, List *formatOpts, int numcols, bool iswritable);
static void InvokeProtocolValidation(Oid procOid, char *procName, bool iswritable, List *locs);
static Datum optionsListToArray(List *options);

/* ----------------------------------------------------------------
*		DefineExternalRelation
*				Creates a new external relation.
*
* In here we first dispatch a normal DefineRelation() (with relstorage
* external) in order to create the external relation entries in pg_class
* pg_type etc. Then once this is done we dispatch ourselves (DefineExternalRelation)
* in order to create the pg_exttable entry across the gp array.
*
* Why don't we just do all of this in one dispatch run? Because that
* involves duplicating the DefineRelation() code or severely modifying it
* to have special cases for external tables. IMHO it's better and cleaner
* to leave it intact and do another dispatch.
* ----------------------------------------------------------------
*/
extern void
DefineExternalRelation(CreateExternalStmt *createExtStmt)
{
	CreateStmt *createStmt = makeNode(CreateStmt);
	ExtTableTypeDesc *exttypeDesc = (ExtTableTypeDesc *) createExtStmt->exttypedesc;
	SingleRowErrorDesc *singlerowerrorDesc = NULL;
	DefElem    *dencoding = NULL;
	ListCell   *option;
	Oid			reloid = 0;
	Datum		formatOptStr;
	Datum		optionsStr;
	Datum		locationUris = 0;
	Datum		locationExec = 0;
	char	   *commandString = NULL;
	char		rejectlimittype = '\0';
	char		formattype;
	int			rejectlimit = -1;
	int			encoding = -1;
	bool		issreh = false; /* is single row error handling requested? */
	bool 		logerrors = false;
	bool		iswritable = createExtStmt->iswritable;
	bool		isweb = createExtStmt->isweb;
	bool		shouldDispatch = (Gp_role == GP_ROLE_DISPATCH &&
								  IsNormalProcessingMode());

	/*
	 * now set the parameters for keys/inheritance etc. Most of these are
	 * uninteresting for external relations...
	 */
	createStmt->relation = createExtStmt->relation;
	createStmt->tableElts = createExtStmt->tableElts;
	createStmt->inhRelations = NIL;
	createStmt->constraints = NIL;
	createStmt->options = NIL;
	createStmt->oncommit = ONCOMMIT_NOOP;
	createStmt->tablespacename = NULL;
	createStmt->distributedBy = createExtStmt->distributedBy; /* policy was set in transform */

	switch (exttypeDesc->exttabletype)
	{
		case EXTTBL_TYPE_LOCATION:

			/* Parse and validate URI strings (LOCATION clause) */
			locationExec = transformExecOnClause(exttypeDesc->on_clause);
			locationUris = transformLocationUris(exttypeDesc->location_list,
												 isweb, iswritable);
			break;

		case EXTTBL_TYPE_EXECUTE:
			locationExec = transformExecOnClause(exttypeDesc->on_clause);
			commandString = exttypeDesc->command_string;

			if (strlen(commandString) == 0)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("invalid EXECUTE clause, command string is empty")));
			break;

		default:
			elog(ERROR, "internal error: unknown external table type: %i",
				 exttypeDesc->exttabletype);
	}

	/*----------
	 * check permissions to create this external table.
	 *
	 * - Always allow if superuser.
	 * - Never allow EXECUTE or 'file' exttables if not superuser.
	 * - Allow http, gpfdist or gpfdists tables if pg_auth has the right
	 *	 permissions for this role and for this type of table
	 *----------
	 */
	if (!superuser() && Gp_role == GP_ROLE_DISPATCH)
	{
		if (exttypeDesc->exttabletype == EXTTBL_TYPE_EXECUTE)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to create an EXECUTE external web table")));
		}
		else
		{
			ListCell   *first_uri = list_head(exttypeDesc->location_list);
			Value	   *v = lfirst(first_uri);
			char	   *uri_str = pstrdup(v->val.str);
			Uri		   *uri = ParseExternalTableUri(uri_str);

			Assert(exttypeDesc->exttabletype == EXTTBL_TYPE_LOCATION);

			if (uri->protocol == URI_FILE)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("must be superuser to create an external table with a file protocol")));
			}
			else
			{
				/*
				 * Check if this role has the proper 'gpfdist', 'gpfdists' or
				 * 'http' permissions in pg_auth for creating this table.
				 */

				bool		isnull;
				Oid			userid = GetUserId();
				HeapTuple	tuple;

				tuple = SearchSysCache1(AUTHOID, ObjectIdGetDatum(userid));
				if (!HeapTupleIsValid(tuple))
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_OBJECT),
							 errmsg("role \"%s\" does not exist (in DefineExternalRelation)",
									GetUserNameFromId(userid))));

				if ((uri->protocol == URI_GPFDIST || uri->protocol == URI_GPFDISTS) && iswritable)
				{
					Datum	 	d_wextgpfd;
					bool		createwextgpfd;

					d_wextgpfd = SysCacheGetAttr(AUTHOID, tuple,
												 Anum_pg_authid_rolcreatewextgpfd,
												 &isnull);
					createwextgpfd = (isnull ? false : DatumGetBool(d_wextgpfd));

					if (!createwextgpfd)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("permission denied: no privilege to create a writable gpfdist(s) external table")));
				}
				else if ((uri->protocol == URI_GPFDIST || uri->protocol == URI_GPFDISTS) && !iswritable)
				{
					Datum		d_rextgpfd;
					bool		createrextgpfd;

					d_rextgpfd = SysCacheGetAttr(AUTHOID, tuple,
												 Anum_pg_authid_rolcreaterextgpfd,
												 &isnull);
					createrextgpfd = (isnull ? false : DatumGetBool(d_rextgpfd));

					if (!createrextgpfd)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("permission denied: no privilege to create a readable gpfdist(s) external table")));
				}
				else if (uri->protocol == URI_HTTP && !iswritable)
				{
					Datum		d_exthttp;
					bool		createrexthttp;

					d_exthttp = SysCacheGetAttr(AUTHOID, tuple,
												Anum_pg_authid_rolcreaterexthttp,
												&isnull);
					createrexthttp = (isnull ? false : DatumGetBool(d_exthttp));

					if (!createrexthttp)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("permission denied: no privilege to create an http external table")));
				}
				else if (uri->protocol == URI_CUSTOM)
				{
					Oid			ownerId = GetUserId();
					char	   *protname = uri->customprotocol;
					Oid			ptcId = get_extprotocol_oid(protname, false);
					AclResult	aclresult;

					/* Check we have the right permissions on this protocol */
					if (!pg_extprotocol_ownercheck(ptcId, ownerId))
					{
						AclMode		mode = (iswritable ? ACL_INSERT : ACL_SELECT);

						aclresult = pg_extprotocol_aclcheck(ptcId, ownerId, mode);

						if (aclresult != ACLCHECK_OK)
							aclcheck_error(aclresult, ACL_KIND_EXTPROTOCOL, protname);
					}
				}
				else
					ereport(ERROR,
							(errcode(ERRCODE_INTERNAL_ERROR),
							 errmsg("internal error in DefineExternalRelation"),
							 errdetail("Protocol is %d, writable is %d.",
									   uri->protocol, iswritable)));

				ReleaseSysCache(tuple);
			}
			FreeExternalTableUri(uri);
			pfree(uri_str);
		}
	}

	/*
	 * Parse and validate FORMAT clause.
	 */
	formattype = transformFormatType(createExtStmt->format);

	formatOptStr = transformFormatOpts(formattype,
									   createExtStmt->formatOpts,
									   list_length(createExtStmt->tableElts),
									   iswritable);

	/*
	 * Parse and validate OPTION clause.
	 */
	optionsStr = optionsListToArray(createExtStmt->extOptions);
	ValidateExtTableOptions(createExtStmt->extOptions);
	if (DatumGetPointer(optionsStr) == NULL)
	{
		optionsStr = PointerGetDatum(construct_empty_array(TEXTOID));
	}
	/*
	 * Parse single row error handling info if available
	 */
	singlerowerrorDesc = (SingleRowErrorDesc *) createExtStmt->sreh;

	if (singlerowerrorDesc)
	{
		Assert(!iswritable);

		issreh = true;

		logerrors = singlerowerrorDesc->into_file;

		/* get reject limit, and reject limit type */
		rejectlimit = singlerowerrorDesc->rejectlimit;
		rejectlimittype = (singlerowerrorDesc->is_limit_in_rows ? 'r' : 'p');
		VerifyRejectLimit(rejectlimittype, rejectlimit);
	}

	/*
	 * Parse external table data encoding
	 */
	foreach(option, createExtStmt->encoding)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		Assert(strcmp(defel->defname, "encoding") == 0);

		if (dencoding)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("conflicting or redundant ENCODING specification")));
		dencoding = defel;
	}

	if (dencoding && dencoding->arg)
	{
		const char *encoding_name;

		if (IsA(dencoding->arg, Integer))
		{
			encoding = intVal(dencoding->arg);
			encoding_name = pg_encoding_to_char(encoding);
			if (strcmp(encoding_name, "") == 0 ||
				pg_valid_client_encoding(encoding_name) < 0)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("%d is not a valid encoding code", encoding)));
		}
		else if (IsA(dencoding->arg, String))
		{
			encoding_name = strVal(dencoding->arg);
			if (pg_valid_client_encoding(encoding_name) < 0)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("%s is not a valid encoding name",
								encoding_name)));
			encoding = pg_char_to_encoding(encoding_name);
		}
		else
			elog(ERROR, "unrecognized node type: %d",
				 nodeTag(dencoding->arg));
	}

	/* If encoding is defaulted, use database server encoding */
	if (encoding < 0)
		encoding = GetDatabaseEncoding();

	/*
	 * If the number of locations (file or http URIs) exceed the number of
	 * segments in the cluster, then all queries against the table will fail
	 * since locations must be mapped at most one per segment. Allow the
	 * creation since this is old pre-existing behavior but throw a WARNING
	 * that the user must expand the cluster in order to use it (or alter
	 * the table).
	 */
	if (exttypeDesc->exttabletype == EXTTBL_TYPE_LOCATION)
	{
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			Value	*loc = lfirst(list_head(exttypeDesc->location_list));
			Uri 	*uri = ParseExternalTableUri(loc->val.str);

			if (uri->protocol == URI_FILE || uri->protocol == URI_HTTP)
			{
				if (getgpsegmentCount() < list_length(exttypeDesc->location_list))
					ereport(WARNING,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("number of locations (%d) exceeds the number of segments (%d)",
									list_length(exttypeDesc->location_list),
									getgpsegmentCount()),
							 errhint("The table cannot be queried until cluster is expanded so that there are at least as many segments as locations.")));
			}
		}
	}

	/*
	 * First, create the pg_class and other regular relation catalog entries.
	 * Under the covers this will dispatch a CREATE TABLE statement to all the
	 * QEs.
	 */
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
		reloid = DefineRelation(createStmt, RELKIND_RELATION, InvalidOid,
								RELSTORAGE_EXTERNAL, true, true, NULL);

	/*
	 * Now we take care of pg_exttable.
	 *
	 * get our pg_class external rel OID. If we're the QD we just created it
	 * above. If we're a QE DefineRelation() was already dispatched to us and
	 * therefore we have a local entry in pg_class. get the OID from cache.
	 */
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
		Assert(reloid != InvalidOid);
	else
		reloid = RangeVarGetRelid(createExtStmt->relation, NoLock, true);

	/*
	 * create a pg_exttable entry for this external table.
	 */
	InsertExtTableEntry(reloid,
						iswritable,
						issreh,
						formattype,
						rejectlimittype,
						commandString,
						rejectlimit,
						logerrors,
						encoding,
						formatOptStr,
						optionsStr,
						locationExec,
						locationUris);

	/*
	 * DefineRelation loaded the new relation into relcache, but the relcache
	 * contains the distribution policy, which in turn depends on the contents
	 * of pg_exttable, for EXECUTE-type external tables (see GpPolicyFetch()).
	 * Now that we have created the pg_exttable entry, invalidate the
	 * relcache, so that it gets loaded with the correct information.
	 */
	CacheInvalidateRelcacheByRelid(reloid);

	if (shouldDispatch)
	{
		/*
		 * Dispatch the statement tree to all primary segdbs. Doesn't wait for
		 * the QEs to finish execution.
		 */
		CdbDispatchUtilityStatement((Node *) createExtStmt,
									DF_CANCEL_ON_ERROR |
									DF_WITH_SNAPSHOT |
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
	}
}


/*
 * Transform the URI string list into a text array (the form that is
 * used in the catalog table pg_exttable). While at it we validate
 * the URI strings.
 *
 * The result is a text array but we declare it as Datum to avoid
 * including array.h in analyze.h.
 */
static Datum
transformLocationUris(List *locs, bool isweb, bool iswritable)
{
	ListCell   *cell;
	ArrayBuildState *astate;
	Datum		result;
	UriProtocol first_protocol = URI_FILE;		/* initialize to keep gcc
												 * quiet */
	bool		first_uri = true;

#define FDIST_DEF_PORT 8080

	/* Parser should not let this happen */
	Assert(locs != NIL);

	/* We build new array using accumArrayResult */
	astate = NULL;

	/*
	 * iterate through the user supplied URI list from LOCATION clause.
	 */
	foreach(cell, locs)
	{
		Uri		   *uri;
		char	   *uri_str_orig;
		char	   *uri_str_final;
		Value	   *v = lfirst(cell);

		/* get the current URI string from the command */
		uri_str_orig = v->val.str;

		/* parse it to its components */
		uri = ParseExternalTableUri(uri_str_orig);

		/*
		 * in here edit the uri string if needed
		 */

		/*
		 * no port was specified for gpfdist, gpfdists or hdfs. add the
		 * default
		 */
		if ((uri->protocol == URI_GPFDIST || uri->protocol == URI_GPFDISTS) && uri->port == -1)
		{
			char	   *at_hostname = (char *) uri_str_orig
			+ strlen(uri->protocol == URI_GPFDIST ? "gpfdist://" : "gpfdists://");
			char	   *after_hostname = strchr(at_hostname, '/');
			int			len = after_hostname - at_hostname;
			char	   *hostname = pstrdup(at_hostname);

			hostname[len] = '\0';

			/* add the default port number to the uri string */
			uri_str_final = psprintf("%s%s:%d%s",
					(uri->protocol == URI_GPFDIST ? PROTOCOL_GPFDIST : PROTOCOL_GPFDISTS),
					hostname,
					FDIST_DEF_PORT, after_hostname);

			pfree(hostname);
		}
		else
		{
			/* no changes to original uri string */
			uri_str_final = pstrdup(uri_str_orig);
		}

		/*
		 * If a custom protocol is used, validate its existence. If it exists,
		 * and a custom protocol url validator exists as well, invoke it now.
		 */
		if (first_uri && uri->protocol == URI_CUSTOM)
		{
			Oid			procOid = InvalidOid;

			procOid = LookupExtProtocolFunction(uri->customprotocol,
												EXTPTC_FUNC_VALIDATOR,
												false);

			if (OidIsValid(procOid) && Gp_role == GP_ROLE_DISPATCH)
				InvokeProtocolValidation(procOid,
										 uri->customprotocol,
										 iswritable,
										 locs);
		}

		if (first_uri)
		{
			first_protocol = uri->protocol;
			first_uri = false;
		}

		if (uri->protocol != first_protocol)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("URI protocols must be the same for all data sources"),
					 errhint("Available protocols are 'http', 'file', 'gpfdist' and 'gpfdists'.")));

		}

		if (uri->protocol != URI_HTTP && isweb)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("an EXTERNAL WEB TABLE may only use http URI\'s, problem in: \'%s\'", uri_str_final),
					 errhint("Use CREATE EXTERNAL TABLE instead.")));

		if (uri->protocol == URI_HTTP && !isweb)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("http URI\'s can only be used in an external web table"),
					 errhint("Use CREATE EXTERNAL WEB TABLE instead.")));

		if (iswritable && (uri->protocol == URI_HTTP || uri->protocol == URI_FILE))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("unsupported URI protocol \'%s\' for writable external table",
							(uri->protocol == URI_HTTP ? "http" : "file")),
					 errhint("Writable external tables may use \'gpfdist\' or \'gpfdists\' URIs only.")));

		if (uri->protocol != URI_CUSTOM && iswritable && strchr(uri->path, '*'))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("unsupported use of wildcard in a writable external web table definition: \'%s\'",
							uri_str_final),
					 errhint("Specify the explicit path and file name to write into.")));

		if ((uri->protocol == URI_GPFDIST || uri->protocol == URI_GPFDISTS) && iswritable && uri->path[strlen(uri->path) - 1] == '/')
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("unsupported use of a directory name in a writable gpfdist(s) external table : \'%s\'",
							uri_str_final),
					 errhint("Specify the explicit path and file name to write into.")));

		astate = accumArrayResult(astate,
								  CStringGetTextDatum(uri_str_final), false,
								  TEXTOID,
								  CurrentMemoryContext);

		FreeExternalTableUri(uri);
		pfree(uri_str_final);
	}

	if (astate)
		result = makeArrayResult(astate, CurrentMemoryContext);
	else
		result = (Datum) 0;

	return result;

}

static Datum
transformExecOnClause(List *on_clause)
{
	ArrayBuildState *astate;
	Datum		result;

	ListCell   *exec_location_opt;
	char	   *exec_location_str = NULL;

	if (on_clause == NIL)
		exec_location_str = "ALL_SEGMENTS";
	else
	{
		/*
		 * Extract options from the statement node tree NOTE: as of now we only
		 * support one option in the ON clause and therefore more than one is an
		 * error (check here in case the sql parser isn't strict enough).
		 */
		foreach(exec_location_opt, on_clause)
		{
			DefElem    *defel = (DefElem *) lfirst(exec_location_opt);

			/* only one element is allowed! */
			if (exec_location_str)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("ON clause must not have more than one element")));

			if (strcmp(defel->defname, "all") == 0)
			{
				/* result: "ALL_SEGMENTS" */
				exec_location_str = "ALL_SEGMENTS";
			}
			else if (strcmp(defel->defname, "hostname") == 0)
			{
				/* result: "HOST:<hostname>" */
				exec_location_str = psprintf("HOST:%s", strVal(defel->arg));
			}
			else if (strcmp(defel->defname, "eachhost") == 0)
			{
				/* result: "PER_HOST" */
				exec_location_str = "PER_HOST";
			}
			else if (strcmp(defel->defname, "master") == 0)
			{
				/* result: "MASTER_ONLY" */
				exec_location_str = "MASTER_ONLY";
			}
			else if (strcmp(defel->defname, "segment") == 0)
			{
				/* result: "SEGMENT_ID:<segid>" */
				exec_location_str = psprintf("SEGMENT_ID:%d", (int) intVal(defel->arg));
			}
			else if (strcmp(defel->defname, "random") == 0)
			{
				/* result: "TOTAL_SEGS:<number>" */
				exec_location_str = psprintf("TOTAL_SEGS:%d", (int) intVal(defel->arg));
			}
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("unknown location code for EXECUTE in tablecmds")));
			}
		}
	}

	/* convert to text[] */
	astate = accumArrayResult(NULL,
							  CStringGetTextDatum(exec_location_str), false,
							  TEXTOID,
							  CurrentMemoryContext);
	result = makeArrayResult(astate, CurrentMemoryContext);

	return result;
}

/*
 * Transform format name for external table FORMAT option to format code and
 * validate that the requested format is supported.
 */
static char
transformFormatType(char *formatname)
{
	char		result = '\0';

	if (pg_strcasecmp(formatname, "text") == 0)
		result = 't';
	else if (pg_strcasecmp(formatname, "csv") == 0)
		result = 'c';
	else if (pg_strcasecmp(formatname, "custom") == 0)
		result = 'b';
	else
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("unsupported format '%s'", formatname),
				 errhint("Available formats for external tables are \"text\", \"csv\" and \"custom\".")));

	return result;
}

/*
 * Transform the external table options into a text array format.
 *
 * The result is an array that includes the format string.
 *
 * This method is a backported FDW's function from upper stream .
 */
static Datum
optionsListToArray(List *options)
{
	ArrayBuildState *astate = NULL;
	ListCell   *option;

	foreach(option, options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);
		char       *key = defel->defname;
		char       *val = defGetString(defel);
		char	   *s;

		s = psprintf("%s=%s", key, val);
		astate = accumArrayResult(astate,
								  CStringGetTextDatum(s), false,
				                  TEXTOID, CurrentMemoryContext);
	}

	if (astate)
		return makeArrayResult(astate, CurrentMemoryContext);

	return PointerGetDatum(NULL);
}

/*
 * Transform the FORMAT options into a text field. Parse the
 * options and validate them for their respective format type.
 *
 * The result is a text field that includes the format string.
 */
static Datum
transformFormatOpts(char formattype, List *formatOpts, int numcols, bool iswritable)
{
	ListCell   *option;
	Datum		result;
	char	   *format_str;
	char	   *formatter = NULL;
	StringInfoData cfbuf;

	CopyState cstate = palloc0(sizeof(CopyStateData));

	Assert(fmttype_is_custom(formattype) ||
		   fmttype_is_text(formattype) ||
		   fmttype_is_csv(formattype));

	/* Extract options from the statement node tree */
	if (fmttype_is_text(formattype) || fmttype_is_csv(formattype))
	{
		foreach(option, formatOpts)
		{
			DefElem    *defel = (DefElem *) lfirst(option);

			if (strcmp(defel->defname, "delimiter") == 0 ||
				strcmp(defel->defname, "null") == 0 ||
				strcmp(defel->defname, "header") == 0 ||
				strcmp(defel->defname, "quote") == 0 ||
				strcmp(defel->defname, "escape") == 0 ||
				strcmp(defel->defname, "force_not_null") == 0 ||
				strcmp(defel->defname, "force_quote") == 0 ||
				strcmp(defel->defname, "fill_missing_fields") == 0 ||
				strcmp(defel->defname, "newline") == 0)
			{
				/* ok */
			}
			else if (strcmp(defel->defname, "formatter") == 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("formatter option only valid for custom formatters")));
			}
			else
				elog(ERROR, "option \"%s\" not recognized",
					 defel->defname);
		}

		/* If CSV format was chosen, make it visible to ProcessCopyOptions. */
		if (fmttype_is_csv(formattype))
		{
			formatOpts = list_copy(formatOpts);
			formatOpts = lappend(formatOpts, makeDefElem("format", (Node *)makeString("csv")));
		}

		/* verify all user supplied control char combinations are legal */
		ProcessCopyOptions(cstate,
						   !iswritable, /* is_from */
						   formatOpts,
						   numcols,
						   false /* is_copy */);

		/*
		 * Build the format option string that will get stored in the catalog.
		 *
		 * NOTE: These are intentionally not escaped "correctly"! For
		 * historical reasons, these options are stored in a weird format
		 * that looks like they're SQL literals, but the escaping is
		 * different. See comments in escape_fmtopts_string(), in
		 * src/bin/pg_dump/dumputils.c.
		 */
		initStringInfo(&cfbuf);
		appendStringInfo(&cfbuf, "delimiter '%s'", cstate->delim);
		appendStringInfo(&cfbuf, " null '%s'", cstate->null_print);
		appendStringInfo(&cfbuf, " escape '%s'", cstate->escape);
		if (fmttype_is_csv(formattype))
			appendStringInfo(&cfbuf, " quote '%s'", cstate->quote);
		if (cstate->header_line)
			appendStringInfo(&cfbuf, " header");
		if (cstate->fill_missing)
			appendStringInfo(&cfbuf, " fill missing fields");

		/*
		 * re-construct the FORCE NOT NULL list string. TODO: is there no
		 * existing util function that does this? can't find.
		 */
		if (cstate->force_notnull)
		{
			ListCell   *l;
			bool		is_first_col = true;

			appendStringInfo(&cfbuf, " force not null");

			foreach(l, cstate->force_notnull)
			{
				const char *col_name = strVal(lfirst(l));

				appendStringInfo(&cfbuf, (is_first_col ? " %s" : ",%s"),
								 quote_identifier(col_name));
				is_first_col = false;
			}
		}

		/*
		 * re-construct the FORCE QUOTE list string.
		 */
		if (cstate->force_quote)
		{
			ListCell   *l;
			bool		is_first_col = true;

			appendStringInfo(&cfbuf, " force quote");

			foreach(l, cstate->force_quote)
			{
				const char *col_name = strVal(lfirst(l));

				appendStringInfo(&cfbuf, (is_first_col ? " %s" : ",%s"),
								 quote_identifier(col_name));
				is_first_col = false;
			}
		}

		if (cstate->force_quote_all)
			appendStringInfo(&cfbuf, " force quote *");

		if (cstate->eol_str)
			appendStringInfo(&cfbuf, " newline '%s'", cstate->eol_str);

		format_str = cfbuf.data;
	}
	else
	{
		/* custom format */
		StringInfoData cfbuf;

		initStringInfo(&cfbuf);

		foreach(option, formatOpts)
		{
			DefElem    *defel = (DefElem *) lfirst(option);
			char	   *key = defel->defname;
			char	   *val = defGetString(defel);

			if (strcmp(key, "formatter") == 0)
			{
				if (formatter)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("conflicting or redundant options")));

				formatter = strVal(defel->arg);
			}

			/*
			 * Output "<key> '<val>' ", but replace any space chars in the key
			 * with meta char (MPP-14467)
			 */
			while (*key)
			{
				if (*key == ' ')
					appendStringInfoString(&cfbuf, "<gpx20>");
				else
					appendStringInfoChar(&cfbuf, *key);
				key++;
			}
			appendStringInfo(&cfbuf, " '%s' ", val);
		}

		if (!formatter)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("no formatter function specified")));

		format_str = cfbuf.data;
	}

	/* convert c string to text datum */
	result = CStringGetTextDatum(format_str);

	/* clean up */
	pfree(format_str);

	return result;

}

static void
InvokeProtocolValidation(Oid procOid, char *procName, bool iswritable, List *locs)
{
	ExtProtocolValidatorData *validator_data;
	FmgrInfo   *validator_udf;
	FunctionCallInfoData fcinfo;

	validator_data = (ExtProtocolValidatorData *) palloc0(sizeof(ExtProtocolValidatorData));
	validator_udf = palloc(sizeof(FmgrInfo));
	fmgr_info(procOid, validator_udf);

	validator_data->type = T_ExtProtocolValidatorData;
	validator_data->url_list = locs;
	validator_data->errmsg = NULL;
	validator_data->direction = (iswritable ? EXT_VALIDATE_WRITE :
								 EXT_VALIDATE_READ);

	InitFunctionCallInfoData( /* FunctionCallInfoData */ fcinfo,
							  /* FmgrInfo */ validator_udf,
							  /* nArgs */ 0,
							  /* Collation */ InvalidOid, 
							  /* Call Context */ (Node *) validator_data,
							  /* ResultSetInfo */ NULL);

	/* invoke validator. if this function returns - validation passed */
	FunctionCallInvoke(&fcinfo);

	/* We do not expect a null result */
	if (fcinfo.isnull)
		elog(ERROR, "validator function %u returned NULL",
			 fcinfo.flinfo->fn_oid);

	pfree(validator_data);
	pfree(validator_udf);
}
