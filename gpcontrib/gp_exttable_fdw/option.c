/*-------------------------------------------------------------------------
 *
 * option.c
 *		  FDW option handling for gp_exttable_fdw
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *		  gpcontrib/gp_exttable_fdw/option.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbvars.h"
#include "utils/uri.h"
#include "utils/syscache.h"
#include "access/reloptions.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "commands/defrem.h"
#include "access/external.h"
#include "catalog/pg_extprotocol.h"
#include "mb/pg_wchar.h"
/*
 * Describes the valid options for objects that this wrapper uses.
 */
typedef struct GpExttableFdwMustOption
{
	const char	*keyword;
	Oid		optcontext;		/* OID of catalog in which option may appear */
} GpExttableFdwMustOption;

/*
 * Valid options for gp_exttable_fdw.
 */
static const GpExttableFdwMustOption gp_exttable_fdw_must_options[] = {
	{"location_uris", ForeignTableRelationId},
	{"command", ForeignTableRelationId},
	{"format", ForeignTableRelationId},
	{NULL, InvalidOid}
};

/*
 * Helper functions
 */
static bool is_must_option(const char *keyword, Oid context);
static void is_valid_locationuris(List *location_list, bool is_writable);
static void is_valid_rejectlimit(const char *reject_limit_type, const int32 reject_limit);

/*
 * Validate the generic options given to a FOREIGN DATA WRAPPER, SERVER,
 * USER MAPPING or FOREIGN TABLE that uses gp_exttable_fdw.
 *
 * Raise an ERROR if the option or its value is considered invalid.
 */
PG_FUNCTION_INFO_V1(gp_exttable_permission_check);

/* FDW validator for external tables */
Datum
gp_exttable_permission_check(PG_FUNCTION_ARGS)
{
	List		*options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid			catalog = PG_GETARG_OID(1);
	ListCell	*cell;
	bool		is_writable = false;
	bool		is_superuser = superuser();
	List		*location_list = NIL;
	/* default reject_limit_type is rows */
	char		*reject_limit_type = "rows";
	int			reject_limit = -1;
	bool		format_found = false;
	bool		locationuris_found = false;
	bool		command_found = false;
	bool		rejectlimit_found = false;

	/*
	 * Check that only options supported by gp_exttable_fdw, and allowed for the
	 * current object type, are given.
	 */
	foreach(cell, options_list)
	{
		DefElem *def = (DefElem *)lfirst(cell);

		/*
		 * Validate option value, when we can do so without any context.
		 */
		if (pg_strcasecmp(def->defname, "is_writable") == 0)
		{
			/* these accept only boolean values */
			is_writable = defGetBoolean(def);
			continue;
		}

		if (pg_strcasecmp(def->defname, "command") == 0)
		{
			/* Never allow EXECUTE if not superuser. */
			if (!is_superuser)
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("must be superuser to create an EXECUTE external web table")));
			command_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "location_uris") == 0)
		{
			location_list = TokenizeLocationUris(defGetString(def));
			locationuris_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "format") == 0)
		{
			char *format = (char *)defGetString(def);
			if (pg_strcasecmp(format, "text") != 0 && pg_strcasecmp(format, "csv") != 0 && pg_strcasecmp(format, "custom") != 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
						 errmsg("format must be [text | csv | custom]")));
			}

			format_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "reject_limit_type") == 0)
		{
			reject_limit_type = (char *)defGetString(def);
			/*
			 * rows and percentage are more precise, but external table
			 * syntax uses row and percent, be tolerant of them.
			 */
			if (pg_strcasecmp(reject_limit_type, "rows") != 0 &&
				pg_strcasecmp(reject_limit_type, "percentage") != 0 &&
				pg_strcasecmp(reject_limit_type, "row") != 0 &&
				pg_strcasecmp(reject_limit_type, "percent") != 0)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
						 errmsg("reject_limit_type must be \'rows\' or \'percentage\'")));
			continue;
		}

		if (pg_strcasecmp(def->defname, "reject_limit") == 0)
		{
			reject_limit = atoi((char *)defGetString(def));
			rejectlimit_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "encoding") == 0)
		{
			char *encoding = (char *)defGetString(def);
			if (pg_valid_client_encoding(encoding) < 0)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("%s is not a valid encoding name", encoding)));
			continue;
		}
	}

	if (!format_found && is_must_option("format", catalog))
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("must specify format option([text | csv | custom])")));
	}

	if (locationuris_found && command_found)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("location_uris and command options conflict with each other")));

	if (is_must_option("location_uris", catalog) && !locationuris_found && !command_found)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("must specify one of location_uris and command option")));

	if (!is_superuser && Gp_role == GP_ROLE_DISPATCH)
	{
		/*----------
		 * check permissions to create this external table.
		 *
		 * - Never allow 'file' exttables if not superuser.
		 * - Allow http, gpfdist or gpfdists tables if pg_auth has the right
		 *	 permissions for this role and for this type of table
		 *----------
		 */
		is_valid_locationuris(location_list, is_writable);
	}

	if (rejectlimit_found)
	{
		if (is_writable)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("single row error handling may not be used with a writable external table")));
		is_valid_rejectlimit(reject_limit_type, reject_limit);
	}

	PG_RETURN_VOID();
}

/*
 * Check whether the given option is a must for gp_exttable_fdw.
 * context is the Oid of the catalog holding the object the option is for.
 */
static bool
is_must_option(const char *keyword, Oid context)
{
	const GpExttableFdwMustOption *opt;

	for (opt = gp_exttable_fdw_must_options; opt->keyword; opt++)
	{
		if (context == opt->optcontext && strcmp(opt->keyword, keyword) == 0)
			return true;
	}
	return false;
}

static void
is_valid_locationuris(List *location_list, bool is_writable)
{
	ListCell	*first_uri = list_head(location_list);
	Value		*v = lfirst(first_uri);
	char		*uri_str = pstrdup(v->val.str);
	Uri		*uri = ParseExternalTableUri(uri_str);

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
		HeapTuple	tuple;

		tuple = SearchSysCache1(AUTHOID, ObjectIdGetDatum(GetUserId()));
		if (!HeapTupleIsValid(tuple))
			ereport(ERROR,
			        (errcode(ERRCODE_UNDEFINED_OBJECT),
			         errmsg("role \"%s\" does not exist (in DefineExternalRelation)",
			                GetUserNameFromId(GetUserId(), false))));

		if ((uri->protocol == URI_GPFDIST || uri->protocol == URI_GPFDISTS) && is_writable)
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
		else if ((uri->protocol == URI_GPFDIST || uri->protocol == URI_GPFDISTS) && !is_writable)
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
		else if (uri->protocol == URI_HTTP && !is_writable)
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
			Oid		ownerId = GetUserId();
			char		*protname = uri->customprotocol;
			Oid		ptcId = get_extprotocol_oid(protname, false);
			AclResult	aclresult;

			/* Check we have the right permissions on this protocol */
			if (!pg_extprotocol_ownercheck(ptcId, ownerId))
			{
				AclMode		mode = (is_writable ? ACL_INSERT : ACL_SELECT);

				aclresult = pg_extprotocol_aclcheck(ptcId, ownerId, mode);

				if (aclresult != ACLCHECK_OK)
					aclcheck_error(aclresult, OBJECT_EXTPROTOCOL, protname);
			}
		}
		else
			ereport(ERROR,
			        (errcode(ERRCODE_INTERNAL_ERROR),
			         errmsg("internal error in DefineExternalRelation"),
			         errdetail("Protocol is %d, writable is %d.",
			                   uri->protocol, is_writable)));

	 	ReleaseSysCache(tuple);
	}
				
	FreeExternalTableUri(uri);
	pfree(uri_str);
}

/* Don't reuse VerifyRejectLimit() because here it can't Assert() */
static void is_valid_rejectlimit(const char *reject_limit_type, const int32 reject_limit)
{
	/*
	 * rows and percentage are more precise, but external table syntax uses
	 * row and percent, be tolerant of them.
	 */
	if (pg_strcasecmp(reject_limit_type, "rows") == 0 || pg_strcasecmp(reject_limit_type, "row") == 0)
	{
		if (reject_limit < 2)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("segment reject limit in ROWS must be 2 or larger (got %d)",
							reject_limit)));
	}
	else if (pg_strcasecmp(reject_limit_type, "percentage") == 0 || pg_strcasecmp(reject_limit_type, "percent") == 0)
	{
		if (reject_limit < 1 || reject_limit > 100)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("segment reject limit in PERCENTAGE must be between 1 and 100 (got %d)",
							reject_limit)));
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
				 errmsg("reject_limit_type must be \'rows\' or \'percentage\'")));
	}
}
