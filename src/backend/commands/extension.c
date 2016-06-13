/*-------------------------------------------------------------------------
 *
 * extension.c
 *	  Commands to manipulate extensions
 *
 * Extensions in PostgreSQL allow management of collections of SQL objects.
 *
 * All we need internally to manage an extension is an OID so that the
 * dependent objects can be associated with it.  An extension is created by
 * populating the pg_extension catalog from a "control" file.
 * The extension control file is parsed with the same parser we use for
 * postgresql.conf and recovery.conf.  An extension also has an installation
 * script file, containing SQL commands to create the extension's objects.
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/extension.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <dirent.h>
#include <limits.h>
#include <unistd.h>

#include "access/sysattr.h"
#include "access/genam.h"
#include "access/xact.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/comment.h"
#include "commands/extension.h"
#include "commands/schemacmds.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/acl.h"
#include "utils/guc.h"
#include "utils/tqual.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbdisp.h"

/* Globally visible state variables */
bool		creating_extension = false;

/* Knob to control if we treat the invoking user of CREATE/ALTER EXTENSION as super user */
bool		rds_enable_promoting_privilege = true;
Oid		CurrentExtensionObject = InvalidOid;

/*
 * Internal data structure to hold the results of parsing a control file
 */
typedef struct ExtensionControlFile
{
	char	   *name;			/* name of the extension */
	char	   *directory;		/* directory for script files */
	char	   *default_version;	/* default install target version, if any */
	char	   *module_pathname;	/* string to substitute for MODULE_PATHNAME */
	char	   *comment;		/* comment, if any */
	char	   *schema;			/* target schema (allowed if !relocatable) */
	bool		relocatable;	/* is ALTER EXTENSION SET SCHEMA supported? */
	bool		superuser;		/* must be superuser to install? */
	int			encoding;		/* encoding of the script file, or -1 */
	List	   *requires;		/* names of prerequisite extensions */
} ExtensionControlFile;

/*
 * Internal data structure for update path information
 */
typedef struct ExtensionVersionInfo
{
	char	   *name;			/* name of the starting version */
	List	   *reachable;		/* List of ExtensionVersionInfo's */
	bool		installable;	/* does this version have an install script? */
	/* working state for Dijkstra's algorithm: */
	bool		distance_known; /* is distance from start known yet? */
	int			distance;		/* current worst-case distance estimate */
	struct ExtensionVersionInfo *previous;		/* current best predecessor */
} ExtensionVersionInfo;

/* Local functions */
static List *find_update_path(List *evi_list,
				 ExtensionVersionInfo *evi_start,
				 ExtensionVersionInfo *evi_target,
				 bool reinitialize);
static void ApplyExtensionUpdates(Oid extensionOid,
					  ExtensionControlFile *pcontrol,
					  const char *initialVersion,
					  List *updateVersions);


/*
 * get_extension_oid - given an extension name, look up the OID
 *
 * If missing_ok is false, throw an error if extension name not found.  If
 * true, just return InvalidOid.
 */
Oid
get_extension_oid(const char *extname, bool missing_ok)
{
	Oid			oid;
	int			fetchCount;

	/*
	 * There's no syscache for pg_extension indexed by name, so we must look
	 * the hard way.
	 */
	oid = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oid FROM pg_extension" 
				" WHERE extname = :1 ", 
				CStringGetDatum((char *) extname)));

	/* We assume that there can be at most one matching tuple */
	if (!fetchCount)
		oid = InvalidOid;

    if (!OidIsValid(oid) && !missing_ok)
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_OBJECT),
                 errmsg("extension \"%s\" does not exist", extname)));

	return oid;
}

/*
 * get_extension_name - given an extension OID, look up the name
 *
 * Returns a palloc'd string, or NULL if no such extension.
 */
char *
get_extension_name(Oid ext_oid)
{
	char	   *result = NULL;
	int			fetchCount;

	result = caql_getcstring_plus(
					NULL,
					&fetchCount,
					NULL,
					cql("SELECT extname FROM pg_extension "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(ext_oid)));

	if (!fetchCount)
		return NULL;

	return result;
}

/*
 * get_extension_schema - given an extension OID, fetch its extnamespace
 *
 * Returns InvalidOid if no such extension.
 */
static Oid
get_extension_schema(Oid ext_oid)
{
	Oid			oid;
	int			fetchCount;

	/*
	 * There's no syscache for pg_database indexed by name, so we must look
	 * the hard way.
	 */
	oid = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT extnamespace FROM pg_extension" 
				" WHERE oid = :1 ", 
				ObjectIdGetDatum(ext_oid)));

	/* We assume that there can be at most one matching tuple */
	if (!fetchCount)
		oid = InvalidOid;

	return oid;
}

/*
 * Check if an item exists in a list
 *
 * 'itemname': name of item to find
 * 'stringlist': a comma-separated list of items
 *
 * Note that, if either of itemname or stringlist is NULL or empty, false will
 * be returned.
 */
bool
find_in_string_list(const char *itemname, const char *stringlist)
{
	bool		ret = false;
	char	   *rawstring;
	List	   *elemlist;
	ListCell   *l;

	if (stringlist == NULL || stringlist[0] == '\0' ||
		itemname == NULL || itemname[0] == '\0')
		/* no cover line */
		return false;			/* nothing to do */

	/* Need a modifiable copy of string */
	rawstring = pstrdup(stringlist);

	/* Parse string into list of identifiers */
	if (!SplitIdentifierString(rawstring, ',', &elemlist))
	{
		/* no cover begin */
		/* syntax error in list */
		list_free(elemlist);
		pfree(rawstring);
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid list syntax in parsing allowed extensions string \"%s\"",
						stringlist)));
		return false;
		/* no cover end */
	}

	foreach(l, elemlist)
	{
		if (strcmp((char *) lfirst(l), itemname) == 0)
		{
			ret = true;
			break;
		}
	}

	pfree(rawstring);
	list_free(elemlist);

	return ret;
}

/*
 * Utility functions to check validity of extension and version names
 */
static void
check_valid_extension_name(const char *extensionname)
{
	int			namelen = strlen(extensionname);

	/*
	 * Disallow empty names (the parser rejects empty identifiers anyway, but
	 * let's check).
	 */
	if (namelen == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid extension name: \"%s\"", extensionname),
				 errdetail("Extension names must not be empty.")));

	/*
	 * No double dashes, since that would make script filenames ambiguous.
	 */
	if (strstr(extensionname, "--"))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid extension name: \"%s\"", extensionname),
				 errdetail("Extension names must not contain \"--\".")));

	/*
	 * No leading or trailing dash either.  (We could probably allow this, but
	 * it would require much care in filename parsing and would make filenames
	 * visually if not formally ambiguous.  Since there's no real-world use
	 * case, let's just forbid it.)
	 */
	if (extensionname[0] == '-' || extensionname[namelen - 1] == '-')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid extension name: \"%s\"", extensionname),
			errdetail("Extension names must not begin or end with \"-\".")));

	/*
	 * No directory separators either (this is sufficient to prevent ".."
	 * style attacks).
	 */
	if (first_dir_separator(extensionname) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid extension name: \"%s\"", extensionname),
				 errdetail("Extension names must not contain directory separator characters.")));
}

static void
check_valid_version_name(const char *versionname)
{
	int			namelen = strlen(versionname);

	/*
	 * Disallow empty names (we could possibly allow this, but there seems
	 * little point).
	 */
	if (namelen == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			   errmsg("invalid extension version name: \"%s\"", versionname),
				 errdetail("Version names must not be empty.")));

	/*
	 * No double dashes, since that would make script filenames ambiguous.
	 */
	if (strstr(versionname, "--"))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			   errmsg("invalid extension version name: \"%s\"", versionname),
				 errdetail("Version names must not contain \"--\".")));

	/*
	 * No leading or trailing dash either.
	 */
	if (versionname[0] == '-' || versionname[namelen - 1] == '-')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			   errmsg("invalid extension version name: \"%s\"", versionname),
			  errdetail("Version names must not begin or end with \"-\".")));

	/*
	 * No directory separators either (this is sufficient to prevent ".."
	 * style attacks).
	 */
	if (first_dir_separator(versionname) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			   errmsg("invalid extension version name: \"%s\"", versionname),
				 errdetail("Version names must not contain directory separator characters.")));
}

/*
 * Utility functions to handle extension-related path names
 */
static bool
is_extension_control_filename(const char *filename)
{
	const char *extension = strrchr(filename, '.');

	return (extension != NULL) && (strcmp(extension, ".control") == 0);
}

static bool
is_extension_script_filename(const char *filename)
{
	const char *extension = strrchr(filename, '.');

	return (extension != NULL) && (strcmp(extension, ".sql") == 0);
}

static char *
get_extension_control_directory(void)
{
	char		sharepath[MAXPGPATH];
	char	   *result;

	get_share_path(my_exec_path, sharepath);
	result = (char *) palloc(MAXPGPATH);
	snprintf(result, MAXPGPATH, "%s/extension", sharepath);

	return result;
}

static char *
get_extension_control_filename(const char *extname)
{
	char		sharepath[MAXPGPATH];
	char	   *result;

	get_share_path(my_exec_path, sharepath);
	result = (char *) palloc(MAXPGPATH);
	snprintf(result, MAXPGPATH, "%s/extension/%s.control",
			 sharepath, extname);

	return result;
}

static char *
get_extension_script_directory(ExtensionControlFile *control)
{
	char		sharepath[MAXPGPATH];
	char	   *result;

	/*
	 * The directory parameter can be omitted, absolute, or relative to the
	 * installation's share directory.
	 */
	if (!control->directory)
		return get_extension_control_directory();

	if (is_absolute_path(control->directory))
		return pstrdup(control->directory);

	get_share_path(my_exec_path, sharepath);
	result = (char *) palloc(MAXPGPATH);
	snprintf(result, MAXPGPATH, "%s/%s", sharepath, control->directory);

	return result;
}

static char *
get_extension_aux_control_filename(ExtensionControlFile *control,
								   const char *version)
{
	char	   *result;
	char	   *scriptdir;

	scriptdir = get_extension_script_directory(control);

	result = (char *) palloc(MAXPGPATH);
	snprintf(result, MAXPGPATH, "%s/%s--%s.control",
			 scriptdir, control->name, version);

	pfree(scriptdir);

	return result;
}

static char *
get_extension_script_filename(ExtensionControlFile *control,
							  const char *from_version, const char *version)
{
	char	   *result;
	char	   *scriptdir;

	scriptdir = get_extension_script_directory(control);

	result = (char *) palloc(MAXPGPATH);
	if (from_version)
		snprintf(result, MAXPGPATH, "%s/%s--%s--%s.sql",
				 scriptdir, control->name, from_version, version);
	else
		snprintf(result, MAXPGPATH, "%s/%s--%s.sql",
				 scriptdir, control->name, version);

	pfree(scriptdir);

	return result;
}


/*
 * Parse contents of primary or auxiliary control file, and fill in
 * fields of *control.  We parse primary file if version == NULL,
 * else the optional auxiliary file for that version.
 *
 * Control files are supposed to be very short, half a dozen lines,
 * so we don't worry about memory allocation risks here.  Also we don't
 * worry about what encoding it's in; all values are expected to be ASCII.
 */
static void
parse_extension_control_file(ExtensionControlFile *control,
							 const char *version)
{
	char	   *filename;
	FILE	   *file;
	struct name_value_pair *item,
			   *head = NULL,
			   *tail = NULL;

	/*
	 * Locate the file to read.  Auxiliary files are optional.
	 */
	if (version)
		filename = get_extension_aux_control_filename(control, version);
	else
		filename = get_extension_control_filename(control->name);

	if ((file = AllocateFile(filename, "r")) == NULL)
	{
		if (version && errno == ENOENT)
		{
			/* no auxiliary file for this version */
			pfree(filename);
			return;
		}
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open extension control file \"%s\": %m",
						filename)));
	}

	FreeFile(file);

	/*
	 * Parse the file content, using GUC's file parsing code.  We need not
	 * check the return value since any errors will be thrown at ERROR level.
	 */
	(void) ParseConfigFile(filename, NULL, 0, PGC_SIGHUP, ERROR, &head, &tail);


	/*
	 * Convert the ConfigVariable list into ExtensionControlFile entries.
	 */
	for (item = head; item != NULL; item = item->next)
	{
		if (strcmp(item->name, "directory") == 0)
		{
			if (version)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("parameter \"%s\" cannot be set in a secondary extension control file",
								item->name)));

			control->directory = pstrdup(item->value);
		}
		else if (strcmp(item->name, "default_version") == 0)
		{
			if (version)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("parameter \"%s\" cannot be set in a secondary extension control file",
								item->name)));

			control->default_version = pstrdup(item->value);
		}
		else if (strcmp(item->name, "module_pathname") == 0)
		{
			control->module_pathname = pstrdup(item->value);
		}
		else if (strcmp(item->name, "comment") == 0)
		{
			control->comment = pstrdup(item->value);
		}
		else if (strcmp(item->name, "schema") == 0)
		{
			control->schema = pstrdup(item->value);
		}
		else if (strcmp(item->name, "relocatable") == 0)
		{
			if (!parse_bool(item->value, &control->relocatable))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("parameter \"%s\" requires a Boolean value",
								item->name)));
		}
		else if (strcmp(item->name, "superuser") == 0)
		{
			if (!parse_bool(item->value, &control->superuser))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("parameter \"%s\" requires a Boolean value",
								item->name)));
		}
		else if (strcmp(item->name, "encoding") == 0)
		{
			control->encoding = pg_valid_server_encoding(item->value);
			if (control->encoding < 0)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("\"%s\" is not a valid encoding name",
								item->value)));
		}
		else if (strcmp(item->name, "requires") == 0)
		{
			/* Need a modifiable copy of string */
			char	   *rawnames = pstrdup(item->value);

			/* Parse string into list of identifiers */
			if (!SplitIdentifierString(rawnames, ',', &control->requires))
			{
				/* syntax error in name list */
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("parameter \"%s\" must be a list of extension names",
						item->name)));
			}
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("unrecognized parameter \"%s\" in file \"%s\"",
							item->name, filename)));
	}

	free_name_value_list(head);

	if (control->relocatable && control->schema != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("parameter \"schema\" cannot be specified when \"relocatable\" is true")));

	pfree(filename);
}

/*
 * Read the primary control file for the specified extension.
 */
static ExtensionControlFile *
read_extension_control_file(const char *extname)
{
	ExtensionControlFile *control;

	/*
	 * Set up default values.  Pointer fields are initially null.
	 */
	control = (ExtensionControlFile *) palloc0(sizeof(ExtensionControlFile));
	control->name = pstrdup(extname);
	control->relocatable = false;
	control->superuser = true;
	control->encoding = -1;

	/*
	 * Parse the primary control file.
	 */
	parse_extension_control_file(control, NULL);

	return control;
}

/*
 * Read the auxiliary control file for the specified extension and version.
 *
 * Returns a new modified ExtensionControlFile struct; the original struct
 * (reflecting just the primary control file) is not modified.
 */
static ExtensionControlFile *
read_extension_aux_control_file(const ExtensionControlFile *pcontrol,
								const char *version)
{
	ExtensionControlFile *acontrol;

	/*
	 * Flat-copy the struct.  Pointer fields share values with original.
	 */
	acontrol = (ExtensionControlFile *) palloc(sizeof(ExtensionControlFile));
	memcpy(acontrol, pcontrol, sizeof(ExtensionControlFile));

	/*
	 * Parse the auxiliary control file, overwriting struct fields
	 */
	parse_extension_control_file(acontrol, version);

	return acontrol;
}

/*
 * Read an SQL script file into a string, and convert to database encoding
 */
static char *
read_extension_script_file(const ExtensionControlFile *control,
						   const char *filename)
{
	int			src_encoding;
	bytea	   *content;
	char	   *src_str;
	char	   *dest_str;
	int			len;

	content = read_binary_file(filename, 0, -1);

	/* use database encoding if not given */
	if (control->encoding < 0)
		src_encoding = GetDatabaseEncoding();
	else
		src_encoding = control->encoding;

	/* make sure that source string is valid in the expected encoding */
	len = VARSIZE_ANY_EXHDR(content);
	src_str = VARDATA_ANY(content);
	pg_verify_mbstr_len(src_encoding, src_str, len, false);

	/* convert the encoding to the database encoding */
	dest_str = pg_custom_to_server(src_str, len, src_encoding, NULL);

	/* if no conversion happened, we have to arrange for null termination */
	if (dest_str == src_str)
	{
		dest_str = (char *) palloc(len + 1);
		memcpy(dest_str, src_str, len);
		dest_str[len] = '\0';
	}

	return dest_str;
}

/*
 * Execute given SQL string.
 *
 * filename is used only to report errors.
 *
 * Note: it's tempting to just use SPI to execute the string, but that does
 * not work very well.  The really serious problem is that SPI will parse,
 * analyze, and plan the whole string before executing any of it; of course
 * this fails if there are any plannable statements referring to objects
 * created earlier in the script.  A lesser annoyance is that SPI insists
 * on printing the whole string as errcontext in case of any error, and that
 * could be very long.
 */
static void
execute_sql_string(const char *sql, const char *filename)
{
	List	   *raw_parsetree_list;
	DestReceiver *dest;
	ListCell   *lc1;
	Snapshot	saveActiveSnapshot;


	/*
	 * Parse the SQL string into a list of raw parse trees.
	 */
	raw_parsetree_list = pg_parse_query(sql);

	/* All output from SELECTs goes to the bit bucket */
	dest = CreateDestReceiver(DestNone, NULL);

	/* Be sure to restore ActiveSnapshot on error exit */
	saveActiveSnapshot = ActiveSnapshot;

	/*
	 * Do parse analysis, rule rewrite, planning, and execution for each raw
	 * parsetree.  We must fully execute each query before beginning parse
	 * analysis on the next one, since there may be interdependencies.
	 */
	foreach(lc1, raw_parsetree_list)
	{
		Node	   *parsetree = (Node *) lfirst(lc1);
		List	   *stmt_list;
		ListCell   *lc2;

		stmt_list = pg_analyze_and_rewrite(parsetree,
										   sql,
										   NULL,
										   0);
		stmt_list = pg_plan_queries(stmt_list, NULL, false);

		foreach(lc2, stmt_list)
		{
			Node	   *stmt = (Node *) lfirst(lc2);

			if (IsA(stmt, TransactionStmt))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("transaction control statements are not allowed within an extension script")));

			CommandCounterIncrement();

			ActiveSnapshot = CopySnapshot(GetTransactionSnapshot());

			if (IsA(stmt, PlannedStmt) &&
				((PlannedStmt *) stmt)->utilityStmt == NULL)
			{
				QueryDesc  *qdesc;

				qdesc = CreateQueryDesc((PlannedStmt *) stmt,
										sql,
										ActiveSnapshot, NULL,
										dest, NULL, false);

				ExecutorStart(qdesc, 0);
				ExecutorRun(qdesc, ForwardScanDirection, 0);
				ExecutorEnd(qdesc);

				FreeQueryDesc(qdesc);
			}
			else
			{
				ProcessUtility(stmt,
							   sql,
							   NULL,
							   false,
							   dest,
							   NULL);
			}

			FreeSnapshot(ActiveSnapshot);
			ActiveSnapshot = saveActiveSnapshot;
		}
	}

	/* Be sure to advance the command counter after the last script command */
	CommandCounterIncrement();
}

/*
 * Execute the appropriate script file for installing or updating the extension
 *
 * If from_version isn't NULL, it's an update
 */
static void
execute_extension_script(CreateExtensionStmt *stmt,
						 Oid extensionOid, ExtensionControlFile *control,
						 const char *from_version,
						 const char *version,
						 List *requiredSchemas,
						 const char *schemaName, Oid schemaOid)
{
	char	   *filename;
	int			save_nestlevel;
	StringInfoData pathbuf;
	ListCell   *lc;

	filename = get_extension_script_filename(control, from_version, version);

	/*
	 * Force client_min_messages and log_min_messages to be at least WARNING,
	 * so that we won't spam the user with useless NOTICE messages from common
	 * script actions like creating shell types.
	 *
	 * We use the equivalent of a function SET option to allow the setting to
	 * persist for exactly the duration of the script execution.  guc.c also
	 * takes care of undoing the setting on error.
	 */
	save_nestlevel = NewGUCNestLevel();

	if (client_min_messages < WARNING)
		(void) set_config_option("client_min_messages", "warning",
								 PGC_USERSET, PGC_S_SESSION,
								 true, true);
	if (log_min_messages < WARNING)
		(void) set_config_option("log_min_messages", "warning",
								 PGC_SUSET, PGC_S_SESSION,
								 true, true);

	/*
	 * Set up the search path to contain the target schema, then the schemas
	 * of any prerequisite extensions, and nothing else.  In particular this
	 * makes the target schema be the default creation target namespace.
	 *
	 * Note: it might look tempting to use PushOverrideSearchPath for this,
	 * but we cannot do that.  We have to actually set the search_path GUC in
	 * case the extension script examines or changes it.  In any case, the
	 * GUC_ACTION_SAVE method is just as convenient.
	 */
	initStringInfo(&pathbuf);
	appendStringInfoString(&pathbuf, quote_identifier(schemaName));
	foreach(lc, requiredSchemas)
	{
		Oid			reqschema = lfirst_oid(lc);
		char	   *reqname = get_namespace_name(reqschema);

		if (reqname)
			appendStringInfo(&pathbuf, ", %s", quote_identifier(reqname));
	}

	(void) set_config_option("search_path", pathbuf.data,
							 PGC_USERSET, PGC_S_SESSION,
							 true, true);

	/*
	 * Set creating_extension and related variables so that
	 * recordDependencyOnCurrentExtension and other functions do the right
	 * things.  On failure, ensure we reset these variables.
	 */
	creating_extension = true;
	CurrentExtensionObject = extensionOid;
	PG_TRY();
	{
		char	   *c_sql = read_extension_script_file(control, filename);
		Datum		t_sql;

		/* We use various functions that want to operate on text datums */
		t_sql = CStringGetTextDatum(c_sql);

		/*
		 * Reduce any lines beginning with "\echo" to empty.  This allows
		 * scripts to contain messages telling people not to run them via
		 * psql, which has been found to be necessary due to old habits.
		 */
		t_sql = DirectFunctionCall4(textregexreplace,
										t_sql,
										CStringGetTextDatum("^\\\\echo.*$"),
										CStringGetTextDatum(""),
										CStringGetTextDatum("ng"));

		/*
		 * If it's not relocatable, substitute the target schema name for
		 * occurrences of @extschema@.
		 *
		 * For a relocatable extension, we needn't do this.  There cannot be
		 * any need for @extschema@, else it wouldn't be relocatable.
		 */
		if (!control->relocatable)
		{
			const char *qSchemaName = quote_identifier(schemaName);

			t_sql = DirectFunctionCall3(replace_text,
										t_sql,
										CStringGetTextDatum("@extschema@"),
										CStringGetTextDatum(qSchemaName));
		}

		/*
		 * If module_pathname was set in the control file, substitute its
		 * value for occurrences of MODULE_PATHNAME.
		 */
		if (control->module_pathname)
		{
			t_sql = DirectFunctionCall3(replace_text,
										t_sql,
									  CStringGetTextDatum("MODULE_PATHNAME"),
							  CStringGetTextDatum(control->module_pathname));
		}

		/* And now back to C string */
		c_sql = text_to_cstring(DatumGetTextPP(t_sql));

		execute_sql_string(c_sql, filename);
	}
	PG_CATCH();
	{
		creating_extension = false;
		CurrentExtensionObject = InvalidOid;

		/*
		 * Restore the GUC variables we set above.
	 	*/
		AtEOXact_GUC(true, save_nestlevel);
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			/* We must reset QE CurrentExtensionObject to InvalidOid */
			stmt->create_ext_state = CREATE_EXTENSION_END;
			CdbDispatchUtilityStatement((Node *) stmt, "CreateExtension");
		}
		PG_RE_THROW();
	}
	PG_END_TRY();

	creating_extension = false;
	CurrentExtensionObject = InvalidOid;

	/*
	 * Restore the GUC variables we set above.
	 */
	AtEOXact_GUC(true, save_nestlevel);
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* We must reset QE CurrentExtensionObject to InvalidOid */
		stmt->create_ext_state = CREATE_EXTENSION_END;
		CdbDispatchUtilityStatement((Node *) stmt, "CreateExtension");
	}
}

/*
 * Find or create an ExtensionVersionInfo for the specified version name
 *
 * Currently, we just use a List of the ExtensionVersionInfo's.  Searching
 * for them therefore uses about O(N^2) time when there are N versions of
 * the extension.  We could change the data structure to a hash table if
 * this ever becomes a bottleneck.
 */
static ExtensionVersionInfo *
get_ext_ver_info(const char *versionname, List **evi_list)
{
	ExtensionVersionInfo *evi;
	ListCell   *lc;

	foreach(lc, *evi_list)
	{
		evi = (ExtensionVersionInfo *) lfirst(lc);
		if (strcmp(evi->name, versionname) == 0)
			return evi;
	}

	evi = (ExtensionVersionInfo *) palloc(sizeof(ExtensionVersionInfo));
	evi->name = pstrdup(versionname);
	evi->reachable = NIL;
	evi->installable = false;
	/* initialize for later application of Dijkstra's algorithm */
	evi->distance_known = false;
	evi->distance = INT_MAX;
	evi->previous = NULL;

	*evi_list = lappend(*evi_list, evi);

	return evi;
}

/*
 * Locate the nearest unprocessed ExtensionVersionInfo
 *
 * This part of the algorithm is also about O(N^2).  A priority queue would
 * make it much faster, but for now there's no need.
 */
static ExtensionVersionInfo *
get_nearest_unprocessed_vertex(List *evi_list)
{
	ExtensionVersionInfo *evi = NULL;
	ListCell   *lc;

	foreach(lc, evi_list)
	{
		ExtensionVersionInfo *evi2 = (ExtensionVersionInfo *) lfirst(lc);

		/* only vertices whose distance is still uncertain are candidates */
		if (evi2->distance_known)
			continue;
		/* remember the closest such vertex */
		if (evi == NULL ||
			evi->distance > evi2->distance)
			evi = evi2;
	}

	return evi;
}

/*
 * Obtain information about the set of update scripts available for the
 * specified extension.  The result is a List of ExtensionVersionInfo
 * structs, each with a subsidiary list of the ExtensionVersionInfos for
 * the versions that can be reached in one step from that version.
 */
static List *
get_ext_ver_list(ExtensionControlFile *control)
{
	List	   *evi_list = NIL;
	int			extnamelen = strlen(control->name);
	char	   *location;
	DIR		   *dir;
	struct dirent *de;

	location = get_extension_script_directory(control);
	dir = AllocateDir(location);
	while ((de = ReadDir(dir, location)) != NULL)
	{
		char	   *vername;
		char	   *vername2;
		ExtensionVersionInfo *evi;
		ExtensionVersionInfo *evi2;

		/* must be a .sql file ... */
		if (!is_extension_script_filename(de->d_name))
			continue;

		/* ... matching extension name followed by separator */
		if (strncmp(de->d_name, control->name, extnamelen) != 0 ||
			de->d_name[extnamelen] != '-' ||
			de->d_name[extnamelen + 1] != '-')
			continue;

		/* extract version name(s) from 'extname--something.sql' filename */
		vername = pstrdup(de->d_name + extnamelen + 2);
		*strrchr(vername, '.') = '\0';
		vername2 = strstr(vername, "--");
		if (!vername2)
		{
			/* It's an install, not update, script; record its version name */
			evi = get_ext_ver_info(vername, &evi_list);
			evi->installable = true;
			continue;
		}
		*vername2 = '\0';		/* terminate first version */
		vername2 += 2;			/* and point to second */

		/* if there's a third --, it's bogus, ignore it */
		if (strstr(vername2, "--"))
			continue;

		/* Create ExtensionVersionInfos and link them together */
		evi = get_ext_ver_info(vername, &evi_list);
		evi2 = get_ext_ver_info(vername2, &evi_list);
		evi->reachable = lappend(evi->reachable, evi2);
	}
	FreeDir(dir);

	return evi_list;
}

/*
 * Given an initial and final version name, identify the sequence of update
 * scripts that have to be applied to perform that update.
 *
 * Result is a List of names of versions to transition through (the initial
 * version is *not* included).
 */
static List *
identify_update_path(ExtensionControlFile *control,
					 const char *oldVersion, const char *newVersion)
{
	List	   *result;
	List	   *evi_list;
	ExtensionVersionInfo *evi_start;
	ExtensionVersionInfo *evi_target;

	/* Extract the version update graph from the script directory */
	evi_list = get_ext_ver_list(control);

	/* Initialize start and end vertices */
	evi_start = get_ext_ver_info(oldVersion, &evi_list);
	evi_target = get_ext_ver_info(newVersion, &evi_list);

	/* Find shortest path */
	result = find_update_path(evi_list, evi_start, evi_target, false);

	if (result == NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("extension \"%s\" has no update path from version \"%s\" to version \"%s\"",
						control->name, oldVersion, newVersion)));

	return result;
}

/*
 * Apply Dijkstra's algorithm to find the shortest path from evi_start to
 * evi_target.
 *
 * If reinitialize is false, assume the ExtensionVersionInfo list has not
 * been used for this before, and the initialization done by get_ext_ver_info
 * is still good.
 *
 * Result is a List of names of versions to transition through (the initial
 * version is *not* included).  Returns NIL if no such path.
 */
static List *
find_update_path(List *evi_list,
				 ExtensionVersionInfo *evi_start,
				 ExtensionVersionInfo *evi_target,
				 bool reinitialize)
{
	List	   *result;
	ExtensionVersionInfo *evi;
	ListCell   *lc;

	/* Caller error if start == target */
	Assert(evi_start != evi_target);

	if (reinitialize)
	{
		foreach(lc, evi_list)
		{
			evi = (ExtensionVersionInfo *) lfirst(lc);
			evi->distance_known = false;
			evi->distance = INT_MAX;
			evi->previous = NULL;
		}
	}

	evi_start->distance = 0;

	while ((evi = get_nearest_unprocessed_vertex(evi_list)) != NULL)
	{
		if (evi->distance == INT_MAX)
			break;				/* all remaining vertices are unreachable */
		evi->distance_known = true;
		if (evi == evi_target)
			break;				/* found shortest path to target */
		foreach(lc, evi->reachable)
		{
			ExtensionVersionInfo *evi2 = (ExtensionVersionInfo *) lfirst(lc);
			int			newdist;

			newdist = evi->distance + 1;
			if (newdist < evi2->distance)
			{
				evi2->distance = newdist;
				evi2->previous = evi;
			}
			else if (newdist == evi2->distance &&
					 evi2->previous != NULL &&
					 strcmp(evi->name, evi2->previous->name) < 0)
			{
				/*
				 * Break ties in favor of the version name that comes first
				 * according to strcmp().  This behavior is undocumented and
				 * users shouldn't rely on it.  We do it just to ensure that
				 * if there is a tie, the update path that is chosen does not
				 * depend on random factors like the order in which directory
				 * entries get visited.
				 */
				evi2->previous = evi;
			}
		}
	}

	/* Return NIL if target is not reachable from start */
	if (!evi_target->distance_known)
		return NIL;

	/* Build and return list of version names representing the update path */
	result = NIL;
	for (evi = evi_target; evi != evi_start; evi = evi->previous)
		result = lcons(evi->name, result);

	return result;
}

/*
 * CREATE EXTENSION
 */
Oid
CreateExtension(CreateExtensionStmt *stmt)
{
	DefElem    *d_schema = NULL;
	DefElem    *d_new_version = NULL;
	DefElem    *d_old_version = NULL;
	char	   *schemaName;
	Oid			schemaOid;
	char	   *versionName;
	char	   *oldVersionName;
	Oid			extowner = GetUserId();
	ExtensionControlFile *pcontrol;
	ExtensionControlFile *control;
	List	   *updateVersions;
	List	   *requiredExtensions;
	List	   *requiredSchemas;
	Oid			extensionOid;
	ListCell   *lc;

	/* Check extension name validity before any filesystem access */
	check_valid_extension_name(stmt->extname);

	/*
	 * Check for duplicate extension name.  The unique index on
	 * pg_extension.extname would catch this anyway, and serves as a backstop
	 * in case of race conditions; but this is a friendlier error message, and
	 * besides we need a check to support IF NOT EXISTS.
	 */
	if (stmt->create_ext_state != CREATE_EXTENSION_END &&
			get_extension_oid(stmt->extname, true) != InvalidOid)
	{
		if (stmt->if_not_exists)
		{
			ereport(NOTICE,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("extension \"%s\" already exists, skipping",
							stmt->extname)));
			return InvalidOid;
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("extension \"%s\" already exists",
							stmt->extname)));
	}

	/* We want to promote user to superuser in QE when creating extension */
	if (Gp_role == GP_ROLE_EXECUTE)
	{
		switch (stmt->create_ext_state)
		{
			case CREATE_EXTENSION_INIT:
				elog(WARNING, "Can not be here when QE create extension");
				Insist(0);
				creating_extension = false;
				CurrentExtensionObject = InvalidOid;
				return InvalidOid;
			case CREATE_EXTENSION_BEGIN:	/* Mark creating_extensin flag and add pg_extension catalog tuple */
				creating_extension = true;
				break;
			case CREATE_EXTENSION_END:		/* Mark creating_extensin flag = false */
				creating_extension = false;
				CurrentExtensionObject = InvalidOid;
				return InvalidOid;
			default:
				elog(ERROR, "unrecognized create_ext_state: %d",
						stmt->create_ext_state);
		}
	}

	/*
	 * We use global variables to track the extension being created, so we can
	 * create only one extension at the same time except QE do CREATE_EXTENSION_BEGIN.
	 */
	if (creating_extension && !(stmt->create_ext_state == CREATE_EXTENSION_BEGIN &&
				Gp_role == GP_ROLE_EXECUTE))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("nested CREATE EXTENSION is not supported")));

	/*
	 * Read the primary control file.  Note we assume that it does not contain
	 * any non-ASCII data, so there is no need to worry about encoding at this
	 * point.
	 */
	pcontrol = read_extension_control_file(stmt->extname);

	/*
	 * Read the statement option list
	 */
	foreach(lc, stmt->options)
	{
		DefElem    *defel = (DefElem *) lfirst(lc);

		if (strcmp(defel->defname, "schema") == 0)
		{
			if (d_schema)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			d_schema = defel;
		}
		else if (strcmp(defel->defname, "new_version") == 0)
		{
			if (d_new_version)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			d_new_version = defel;
		}
		else if (strcmp(defel->defname, "old_version") == 0)
		{
			if (d_old_version)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			d_old_version = defel;
		}
		else
			elog(ERROR, "unrecognized option: %s", defel->defname);
	}

	/*
	 * Determine the version to install
	 */
	if (d_new_version && d_new_version->arg)
		versionName = strVal(d_new_version->arg);
	else if (pcontrol->default_version)
		versionName = pcontrol->default_version;
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("version to install must be specified")));
		versionName = NULL;		/* keep compiler quiet */
	}
	check_valid_version_name(versionName);

	/*
	 * Determine the (unpackaged) version to update from, if any, and then
	 * figure out what sequence of update scripts we need to apply.
	 */
	if (d_old_version && d_old_version->arg)
	{
		oldVersionName = strVal(d_old_version->arg);
		check_valid_version_name(oldVersionName);

		if (strcmp(oldVersionName, versionName) == 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("FROM version must be different from installation target version \"%s\"",
							versionName)));

		updateVersions = identify_update_path(pcontrol,
											  oldVersionName,
											  versionName);

		if (list_length(updateVersions) == 1)
		{
			/*
			 * Simple case where there's just one update script to run. We
			 * will not need any follow-on update steps.
			 */
			Assert(strcmp((char *) linitial(updateVersions), versionName) == 0);
			updateVersions = NIL;
		}
		else
		{
			/*
			 * Multi-step sequence.  We treat this as installing the version
			 * that is the target of the first script, followed by successive
			 * updates to the later versions.
			 */
			versionName = (char *) linitial(updateVersions);
			updateVersions = list_delete_first(updateVersions);
		}
	}
	else
	{
		oldVersionName = NULL;
		updateVersions = NIL;
	}

	/*
	 * Fetch control parameters for installation target version
	 */
	control = read_extension_aux_control_file(pcontrol, versionName);

	/*
	 * Determine the target schema to install the extension into
	 */
	if (d_schema && d_schema->arg)
	{
		/*
		 * User given schema, CREATE EXTENSION ... WITH SCHEMA ...
		 *
		 * It's an error to give a schema different from control->schema if
		 * control->schema is specified.
		 */
		schemaName = strVal(d_schema->arg);

		if (control->schema != NULL &&
			strcmp(control->schema, schemaName) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("extension \"%s\" must be installed in schema \"%s\"",
					   control->name,
					   control->schema)));

		/* If the user is giving us the schema name, it must exist already */
		schemaOid = get_namespace_oid(schemaName, false);
	}
	else if (control->schema != NULL)
	{
		/*
		 * The extension is not relocatable and the author gave us a schema
		 * for it.  We create the schema here if it does not already exist.
		 */
		schemaName = control->schema;
		schemaOid = get_namespace_oid(schemaName, true);

		if (schemaOid == InvalidOid)
		{
			CreateSchemaStmt *csstmt = makeNode(CreateSchemaStmt);

			csstmt->schemaname = schemaName;
			csstmt->authid = NULL;		/* will be created by current user */
			csstmt->schemaElts = NIL;
			csstmt->istemp = false;
			CreateSchemaCommand(csstmt, NULL);

			/*
			 * CreateSchemaCommand includes CommandCounterIncrement, so new
			 * schema is now visible
			 */
			schemaOid = get_namespace_oid(schemaName, false);
		}
	}
	else
	{
		/*
		 * Else, use the current default creation namespace, which is the
		 * first explicit entry in the search_path.
		 */
		List	   *search_path = fetch_search_path(false);

		if (search_path == NIL) /* nothing valid in search_path? */
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_SCHEMA),
					 errmsg("no schema has been selected to create in")));
		schemaOid = linitial_oid(search_path);
		schemaName = get_namespace_name(schemaOid);
		if (schemaName == NULL) /* recently-deleted namespace? */
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_SCHEMA),
					 errmsg("no schema has been selected to create in")));

		list_free(search_path);
	}

	/*
	 * We don't check creation rights on the target namespace here.  If the
	 * extension script actually creates any objects there, it will fail if
	 * the user doesn't have such permissions.  But there are cases such as
	 * procedural languages where it's convenient to set schema = pg_catalog
	 * yet we don't want to restrict the command to users with ACL_CREATE for
	 * pg_catalog.
	 */

	/*
	 * Look up the prerequisite extensions, and build lists of their OIDs and
	 * the OIDs of their target schemas.
	 */
	requiredExtensions = NIL;
	requiredSchemas = NIL;
	foreach(lc, control->requires)
	{
		char	   *curreq = (char *) lfirst(lc);
		Oid			reqext;
		Oid			reqschema;

		/*
		 * We intentionally don't use get_extension_oid's default error
		 * message here, because it would be confusing in this context.
		 */
		reqext = get_extension_oid(curreq, true);
		if (!OidIsValid(reqext))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("required extension \"%s\" is not installed",
							curreq)));
		reqschema = get_extension_schema(reqext);
		requiredExtensions = lappend_oid(requiredExtensions, reqext);
		requiredSchemas = lappend_oid(requiredSchemas, reqschema);
	}

	/*
	 * Enforce superuser-ness if appropriate.  We postpone this check until
	 * here so that the flag is correctly associated with the right script(s)
	 * if it's set in secondary control files.
	 *
	 * Loose the privilege check, and allow any user that has createdb,
	 * createrole and login privilege to create extensions.
	 */
	if (control->superuser &&
		!superuser())
	{
		if (oldVersionName == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied to create extension \"%s\"",
							control->name),
					 errhint("Must be superuser or user with all of login/createdb/createrole privileges to create this extension.")));
		else
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied to update extension \"%s\"",
							control->name),
					 errhint("Must be superuser or user with all of login/createdb/createrole privileges to update this extension.")));
	}

	/*
	 * Insert new tuple into pg_extension, and create dependency entries.
	 */
	extensionOid = InsertExtensionTuple(control->name, extowner,
										schemaOid, control->relocatable,
										versionName,
										PointerGetDatum(NULL),
										PointerGetDatum(NULL),
										requiredExtensions);

	/*
	 * Apply any control-file comment on extension
	 */
	if (control->comment != NULL)
		CreateComments(extensionOid, ExtensionRelationId, 0, control->comment);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* We must tell QE to create extension */
		stmt->create_ext_state = CREATE_EXTENSION_BEGIN;
		CdbDispatchUtilityStatement((Node *) stmt, "CreateExtension");
	}
	else
	{
		CurrentExtensionObject = extensionOid;
	}

	/*
	 * Execute the installation script file only happen in QD
	 */
	if (Gp_role != GP_ROLE_EXECUTE)
		execute_extension_script(stmt, extensionOid, control,
								 oldVersionName, versionName,
								 requiredSchemas,
								 schemaName, schemaOid);

	return extensionOid;
}

/*
 * InsertExtensionTuple
 *
 * Insert the new pg_extension row, and create extension's dependency entries.
 * Return the OID assigned to the new row.
 *
 * This is exported for the benefit of pg_upgrade, which has to create a
 * pg_extension entry (and the extension-level dependencies) without
 * actually running the extension's script.
 *
 * extConfig and extCondition should be arrays or PointerGetDatum(NULL).
 * We declare them as plain Datum to avoid needing array.h in extension.h.
 */
Oid
InsertExtensionTuple(const char *extName, Oid extOwner,
					 Oid schemaOid, bool relocatable, const char *extVersion,
					 Datum extConfig, Datum extCondition,
					 List *requiredExtensions)
{
	Oid			extensionOid;
	Relation	rel;
	Datum		values[Natts_pg_extension];
	bool		nulls[Natts_pg_extension];
	HeapTuple	tuple;
	ObjectAddress myself;
	ObjectAddress nsp;
	ListCell   *lc;

	/*
	 * Build and insert the pg_extension tuple
	 */
	rel = heap_open(ExtensionRelationId, RowExclusiveLock);

	memset(values, 0, sizeof(values));
	memset(nulls, 0, sizeof(nulls));

	values[Anum_pg_extension_extname - 1] =
		DirectFunctionCall1(namein, CStringGetDatum(extName));
	values[Anum_pg_extension_extowner - 1] = ObjectIdGetDatum(extOwner);
	values[Anum_pg_extension_extnamespace - 1] = ObjectIdGetDatum(schemaOid);
	values[Anum_pg_extension_extrelocatable - 1] = BoolGetDatum(relocatable);
	values[Anum_pg_extension_extversion - 1] = CStringGetTextDatum(extVersion);

	if (extConfig == PointerGetDatum(NULL))
		nulls[Anum_pg_extension_extconfig - 1] = true;
	else
		values[Anum_pg_extension_extconfig - 1] = extConfig;

	if (extCondition == PointerGetDatum(NULL))
		nulls[Anum_pg_extension_extcondition - 1] = true;
	else
		values[Anum_pg_extension_extcondition - 1] = extCondition;

	tuple = heap_form_tuple(rel->rd_att, values, nulls);

	extensionOid = simple_heap_insert(rel, tuple);
	CatalogUpdateIndexes(rel, tuple);

	heap_freetuple(tuple);
	heap_close(rel, RowExclusiveLock);

	/*
	 * Record dependencies on owner, schema, and prerequisite extensions
	 */
	recordDependencyOnOwner(ExtensionRelationId, extensionOid, extOwner);

	myself.classId = ExtensionRelationId;
	myself.objectId = extensionOid;
	myself.objectSubId = 0;

	nsp.classId = NamespaceRelationId;
	nsp.objectId = schemaOid;
	nsp.objectSubId = 0;

	recordDependencyOn(&myself, &nsp, DEPENDENCY_NORMAL);

	foreach(lc, requiredExtensions)
	{
		Oid			reqext = lfirst_oid(lc);
		ObjectAddress otherext;

		otherext.classId = ExtensionRelationId;
		otherext.objectId = reqext;
		otherext.objectSubId = 0;

		recordDependencyOn(&myself, &otherext, DEPENDENCY_NORMAL);
	}

	return extensionOid;
}

/*
 * RemoveExtProtocol
 *		Deletes an external protocol.
 */
void
RemoveExtension(List *names, DropBehavior behavior, bool missing_ok)
{
	char	   *extName;
	Oid			extOid = InvalidOid;
	ObjectAddress object;

	/* 
	 * General DROP (object) syntax allows fully qualified names, but
	 * protocols are global objects that do not live in schemas, so
	 * it is a syntax error if a fully qualified name was given.
	 */
	if (list_length(names) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("protocol name may not be qualified")));
	extName = strVal(linitial(names));

	/* find extensin Oid. error inline if doesn't exist */
	extOid = get_extension_oid(extName, missing_ok);

	if (!OidIsValid(extOid))
	{
		if (!missing_ok)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("extension \"%s\" does not exist",
							extName)));
		}
		else
		{
			if (Gp_role != GP_ROLE_EXECUTE)
				ereport(NOTICE,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("extension \"%s\" does not exist, skipping",
								extName)));
		}

		return;
	}

	/* Permission check: must own protocol */
	if (!pg_extension_ownercheck(extOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_EXTENSION, extName);

	/*
	 * Do the deletion
	 */
	object.classId = ExtensionRelationId;
	object.objectId = extOid;
	object.objectSubId = 0;

	performDeletion(&object, behavior);
	
}

/*
 * Guts of extension deletion.
 *
 * All we need do here is remove the pg_extension tuple itself.  Everything
 * else is taken care of by the dependency infrastructure.
 */
void
RemoveExtensionById(Oid extId)
{
	Relation	rel;
	cqContext	cqc;

	/*
	 * Disallow deletion of any extension that's currently open for insertion;
	 * else subsequent executions of recordDependencyOnCurrentExtension()
	 * could create dangling pg_depend records that refer to a no-longer-valid
	 * pg_extension OID.  This is needed not so much because we think people
	 * might write "DROP EXTENSION foo" in foo's own script files, as because
	 * errors in dependency management in extension script files could give
	 * rise to cases where an extension is dropped as a result of recursing
	 * from some contained object.  Because of that, we must test for the case
	 * here, not at some higher level of the DROP EXTENSION command.
	 */
	if (extId == CurrentExtensionObject)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
		  errmsg("cannot drop extension \"%s\" because it is being modified",
				 get_extension_name(extId))));

	rel = heap_open(ExtensionRelationId, RowExclusiveLock);

	caql_getcount(
				caql_addrel(cqclr(&cqc), rel),
				cql("DELETE FROM pg_extension "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(extId)));
	heap_close(rel, RowExclusiveLock);
}
