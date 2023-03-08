/*-------------------------------------------------------------------------
 *
 * external.c
 *	  routines for getting external info from external table fdw.
 *
 * Portions Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *	    src/backend/access/external/external.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <fstream/gfile.h>

#include "access/external.h"
#include "access/reloptions.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"
#include "commands/defrem.h"
#include "foreign/foreign.h"
#include "mb/pg_wchar.h"
#include "nodes/makefuncs.h"
#include "optimizer/optimizer.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/uri.h"

static List *create_external_scan_uri_list(ExtTableEntry *ext, bool *iscoordinatoronly);

void
gfile_printf_then_putc_newline(const char *format,...)
{
	char	   *a;
	va_list		va;
	int			i;

	va_start(va, format);
	i = vsnprintf(0, 0, format, va);
	va_end(va);

	if (i < 0)
		elog(NOTICE, "gfile_printf_then_putc_newline vsnprintf failed.");
	else if (!(a = palloc(i + 1)))
		elog(NOTICE, "gfile_printf_then_putc_newline palloc failed.");
	else
	{
		va_start(va, format);
		vsnprintf(a, i + 1, format, va);
		va_end(va);
		elog(NOTICE, "%s", a);
		pfree(a);
	}
}

void *
gfile_malloc(size_t size)
{
	return palloc(size);
}

void
gfile_free(void *a)
{
	pfree(a);
}

/* transform the locations string to a list */
List*
TokenizeLocationUris(char *uris)
{
	char *uri = NULL;
	List *result = NIL;

	Assert(uris != NULL);

	while ((uri = strsep(&uris, "|")) != NULL)
	{
		result = lappend(result, makeString(uri));
	}

	return result;
}

/*
 * Get the entry for an exttable relation (from pg_foreign_table)
 */
ExtTableEntry*
GetExtTableEntry(Oid relid)
{
	ExtTableEntry *extentry;

	extentry = GetExtTableEntryIfExists(relid);
	if (!extentry)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("missing pg_foreign_table entry for relation \"%s\"",
						get_rel_name(relid))));
	return extentry;
}

/*
 * Like GetExtTableEntry(Oid), but returns NULL instead of throwing
 * an error if no pg_foreign_table entry is found.
 */
ExtTableEntry*
GetExtTableEntryIfExists(Oid relid)
{
	Relation	pg_foreign_table_rel;
	ScanKeyData ftkey;
	SysScanDesc ftscan;
	HeapTuple	fttuple;
	ExtTableEntry *extentry;
	bool		isNull;
	List		*ftoptions_list = NIL;;

	pg_foreign_table_rel = table_open(ForeignTableRelationId, RowExclusiveLock);

	ScanKeyInit(&ftkey,
				Anum_pg_foreign_table_ftrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	ftscan = systable_beginscan(pg_foreign_table_rel, ForeignTableRelidIndexId,
								true, NULL, 1, &ftkey);
	fttuple = systable_getnext(ftscan);

	if (!HeapTupleIsValid(fttuple))
	{
		systable_endscan(ftscan);
		heap_close(pg_foreign_table_rel, RowExclusiveLock);

		return NULL;
	}

	/* get the foreign table options */
	Datum ftoptions = heap_getattr(fttuple,
						   Anum_pg_foreign_table_ftoptions,
						   RelationGetDescr(pg_foreign_table_rel),
						   &isNull);

	if (isNull)
	{
		/* options array is always populated, {} if no options set */
		elog(ERROR, "could not find options for external protocol");
	}
	else
	{
		ftoptions_list = untransformRelOptions(ftoptions);
	}

	extentry = GetExtFromForeignTableOptions(ftoptions_list, relid);

	/* Finish up scan and close catalogs */
	systable_endscan(ftscan);
	table_close(pg_foreign_table_rel, RowExclusiveLock);

	return extentry;
}

ExtTableEntry *
GetExtFromForeignTableOptions(List *ftoptons, Oid relid)
{
	ExtTableEntry	   *extentry;
	ListCell		   *lc;
	List			   *entryOptions = NIL;
	char			   *arg;
	bool				fmtcode_found = false;
	bool				rejectlimit_found = false;
	bool				rejectlimittype_found = false;
	bool				logerrors_found = false;
	bool				encoding_found = false;
	bool				iswritable_found = false;
	bool				locationuris_found = false;
	bool				command_found = false;
	bool				executeon_found = false;

	extentry = (ExtTableEntry *) palloc0(sizeof(ExtTableEntry));

	foreach(lc, ftoptons)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (pg_strcasecmp(def->defname, "location_uris") == 0)
		{
			extentry->urilocations = TokenizeLocationUris(defGetString(def));
			locationuris_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "execute_on") == 0)
		{
			extentry->execlocations = list_make1(makeString(defGetString(def)));
			executeon_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "command") == 0)
		{
			extentry->command = defGetString(def);
			command_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "format_type") == 0)
		{
			arg = defGetString(def);
			extentry->fmtcode = arg[0];
			fmtcode_found = true;
			continue;
		}

		/* only CSV format needs this for ProcessCopyOptions(), will do it later */
		if (pg_strcasecmp(def->defname, "format") == 0)
		{
			continue;
		}

		if (pg_strcasecmp(def->defname, "reject_limit") == 0)
		{
			extentry->rejectlimit = atoi(defGetString(def));
			rejectlimit_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "reject_limit_type") == 0)
		{
			arg = defGetString(def);
			extentry->rejectlimittype = arg[0];
			rejectlimittype_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "log_errors") == 0)
		{
			arg = defGetString(def);
			extentry->logerrors = arg[0];
			logerrors_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "encoding") == 0)
		{
			extentry->encoding = atoi(defGetString(def));
			encoding_found = true;
			continue;
		}

		if (pg_strcasecmp(def->defname, "is_writable") == 0)
		{
			extentry->iswritable = defGetBoolean(def);
			iswritable_found = true;
			continue;
		}

		entryOptions = lappend(entryOptions, makeDefElem(def->defname, (Node *) makeString(pstrdup(defGetString(def))), -1));
	}

	/* If CSV format was chosen, make it visible to ProcessCopyOptions. */
	if (fmttype_is_csv(extentry->fmtcode))
		entryOptions = lappend(entryOptions, makeDefElem("format", (Node *) makeString("csv"), -1));

	/*
	 * external table syntax does have these for sure, but errors could happen
	 * if using foreign table syntax
	 */
	if (!fmtcode_found || !logerrors_found || !encoding_found || !iswritable_found)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("missing format, logerrors, encoding or iswritable options for relation \"%s\"",
						get_rel_name(relid))));

	if (locationuris_found && command_found)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("locationuris and command options conflict with each other")));

	if (!fmttype_is_custom(extentry->fmtcode) &&
		!fmttype_is_csv(extentry->fmtcode) &&
		!fmttype_is_text(extentry->fmtcode))
		elog(ERROR, "unsupported format type %d for external table", extentry->fmtcode);

	if (!rejectlimit_found) {
		/* mark that no SREH requested */
		extentry->rejectlimit = -1;
	}

	if (rejectlimittype_found)
	{
		if (extentry->rejectlimittype != 'r' && extentry->rejectlimittype != 'p')
			elog(ERROR, "unsupported reject limit type %c for external table",
				 extentry->rejectlimittype);
	}
	else
		extentry->rejectlimittype = -1;

	if (!executeon_found)
	{
		extentry->execlocations = list_make1(makeString("ALL_SEGMENTS"));
	}

	if (!PG_VALID_ENCODING(extentry->encoding))
		elog(ERROR, "invalid encoding found for external table");

	extentry->options = entryOptions;

	return extentry;
}

ExternalScanInfo *
MakeExternalScanInfo(ExtTableEntry *extEntry)
{
	ExternalScanInfo *node = makeNode(ExternalScanInfo);
	List	   *urilist;
	bool		iscoordinatoronly = false;
	bool		islimitinrows = false;
	int			rejectlimit = -1;
	char		logerrors = LOG_ERRORS_DISABLE;
	static uint32 scancounter = 0;

	if (extEntry->rejectlimit != -1)
	{
		/*
		 * single row error handling is requested, make sure reject limit and
		 * reject type are valid.
		 *
		 * NOTE: this should never happen unless somebody modified the catalog
		 * manually. We are just being pedantic here.
		 */
		VerifyRejectLimit(extEntry->rejectlimittype, extEntry->rejectlimit);
	}

	/* assign Uris to segments. */
	urilist = create_external_scan_uri_list(extEntry, &iscoordinatoronly);

	/* single row error handling */
	if (extEntry->rejectlimit != -1)
	{
		islimitinrows = (extEntry->rejectlimittype == 'r' ? true : false);
		rejectlimit = extEntry->rejectlimit;
		logerrors = extEntry->logerrors;
	}

	node->uriList = urilist;
	node->fmtType = extEntry->fmtcode;
	node->isCoordinatorOnly = iscoordinatoronly;
	node->rejLimit = rejectlimit;
	node->rejLimitInRows = islimitinrows;
	node->logErrors = logerrors;
	node->encoding = extEntry->encoding;
	node->scancounter = scancounter++;
	node->extOptions = extEntry->options;

	return node;
}

static List *
create_external_scan_uri_list(ExtTableEntry *ext, bool *iscoordinatoronly)
{
	ListCell   *c;
	List	   *modifiedloclist = NIL;
	int			i;
	CdbComponentDatabases *db_info;
	int			total_primaries;
	char	  **segdb_file_map;

	/* various processing flags */
	bool		using_execute = false;	/* true if EXECUTE is used */
	bool		using_location; /* true if LOCATION is used */
	bool		found_candidate = false;
	bool		found_match = false;
	bool		done = false;
	List	   *filenames;

	/* gpfdist(s) or EXECUTE specific variables */
	int			total_to_skip = 0;
	int			max_participants_allowed = 0;
	int			num_segs_participating = 0;
	bool	   *skip_map = NULL;
	bool		should_skip_randomly = false;

	Uri		   *uri;
	char	   *on_clause;

	*iscoordinatoronly = false;

	/* is this an EXECUTE table or a LOCATION (URI) table */
	if (ext->command)
	{
		using_execute = true;
		using_location = false;
	}
	else
	{
		using_execute = false;
		using_location = true;
	}

	/* is this an EXECUTE table or a LOCATION (URI) table */
	if (ext->command && !gp_external_enable_exec)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_CONFIGURED),	/* any better errcode? */
				 errmsg("using external tables with OS level commands (EXECUTE clause) is disabled"),
				 errhint("To enable set gp_external_enable_exec=on.")));
	}

	/* various validations */
	if (ext->iswritable)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot read from a WRITABLE external table"),
				 errhint("Create the table as READABLE instead.")));

	/*
	 * take a peek at the first URI so we know which protocol we'll deal with
	 */
	if (!using_execute)
	{
		char	   *first_uri_str;

		first_uri_str = strVal(linitial(ext->urilocations));
		uri = ParseExternalTableUri(first_uri_str);
	}
	else
		uri = NULL;

	/* get the ON clause information, and restrict 'ON COORDINATOR' to custom
	 * protocols only */
	on_clause = (char *) strVal(linitial(ext->execlocations));
	if ((strcmp(on_clause, "COORDINATOR_ONLY") == 0)
		&& using_location && (uri->protocol != URI_CUSTOM)) {
		ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				errmsg("\'ON COORDINATOR\' is not supported by this protocol yet")));
	}

	/* get the total valid primary segdb count */
	db_info = cdbcomponent_getCdbComponents();
	total_primaries = 0;
	for (i = 0; i < db_info->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];

		if (SEGMENT_IS_ACTIVE_PRIMARY(p))
			total_primaries++;
	}

	/*
	 * initialize a file-to-segdb mapping. segdb_file_map string array indexes
	 * segindex and the entries are the external file path is assigned to this
	 * segment database. For example if segdb_file_map[2] has "/tmp/emp.1" then
	 * this file is assigned to primary segdb 2. if an entry has NULL then
	 * that segdb isn't assigned any file.
	 */
	segdb_file_map = (char **) palloc0(total_primaries * sizeof(char *));

	/*
	 * Now we do the actual assignment of work to the segment databases (where
	 * work is either a URI to open or a command to execute). Due to the big
	 * differences between the different protocols we handle each one
	 * separately. Unfortunately this means some code duplication, but keeping
	 * this separation makes the code much more understandable and (even) more
	 * maintainable.
	 *
	 * Outline of the following code blocks (from simplest to most complex):
	 * (only one of these will get executed for a statement)
	 *
	 * 1) segment mapping for tables with LOCATION http:// or file:// .
	 *
	 * These two protocols are very similar in that they enforce a
	 * 1-URI:1-segdb relationship. The only difference between them is that
	 * file:// URI must be assigned to a segdb on a host that is local to that
	 * URI.
	 *
	 * 2) segment mapping for tables with LOCATION gpfdist(s):// or custom
	 * protocol
	 *
	 * This protocol is more complicated - in here we usually duplicate the
	 * user supplied gpfdist(s):// URIs until there is one available to every
	 * segdb. However, in some cases (as determined by gp_external_max_segs
	 * GUC) we don't want to use *all* segdbs but instead figure out how many
	 * and pick them randomly (this is mainly for better performance and
	 * resource mgmt).
	 *
	 * 3) segment mapping for tables with EXECUTE 'cmd' ON.
	 *
	 * In here we don't have URI's. We have a single command string and a
	 * specification of the segdb granularity it should get executed on (the
	 * ON clause). Depending on the ON clause specification we could go many
	 * different ways, for example: assign the command to all segdb, or one
	 * command per host, or assign to 5 random segments, etc...
	 */

	/* (1) */
	if (using_location && (uri->protocol == URI_FILE || uri->protocol == URI_HTTP))
	{
		/*
		 * extract file path and name from URI strings and assign them a
		 * primary segdb
		 */
		foreach(c, ext->urilocations)
		{
			const char *uri_str = (char *) strVal(lfirst(c));

			uri = ParseExternalTableUri(uri_str);

			found_candidate = false;
			found_match = false;

			/*
			 * look through our segment database list and try to find a
			 * database that can handle this uri.
			 */
			for (i = 0; i < db_info->total_segment_dbs && !found_match; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int segind = p->config->segindex;

				/*
				 * Assign mapping of external file to this segdb only if:
				 * 1) This segdb is a valid primary.
				 * 2) An external file wasn't already assigned to it.
				 * 3) If 'file' protocol, host of segdb and file must be
				 *    the same.
				 *
				 * This logic also guarantees that file that appears first in
				 * the external location list for the same host gets assigned
				 * the segdb with the lowest index for this host.
				 */
				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					if (uri->protocol == URI_FILE)
					{
						if (pg_strcasecmp(uri->hostname, p->config->hostname) != 0 && pg_strcasecmp(uri->hostname, p->config->address) != 0)
							continue;
					}

					/* a valid primary segdb exist on this host */
					found_candidate = true;

					if (segdb_file_map[segind] == NULL)
					{
						/* segdb not taken yet. assign this URI to this segdb */
						segdb_file_map[segind] = pstrdup(uri_str);
						found_match = true;
					}

					/*
					 * too bad. this segdb already has an external source
					 * assigned
					 */
				}
			}

			/*
			 * We failed to find a segdb for this URI.
			 */
			if (!found_match)
			{
				if (uri->protocol == URI_FILE)
				{
					if (found_candidate)
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("could not assign a segment database for \"%s\"",
										uri_str),
								 errdetail("There are more external files than primary segment databases on host \"%s\"",
										   uri->hostname)));
					}
					else
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("could not assign a segment database for \"%s\"",
										uri_str),
								 errdetail("There isn't a valid primary segment database on host \"%s\"",
										   uri->hostname)));
					}
				}
				else	/* HTTP */
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("could not assign a segment database for \"%s\"",
									uri_str),
							 errdetail("There are more URIs than total primary segment databases")));
				}
			}
		}


	}
	/* (2) */
	else if (using_location && (uri->protocol == URI_GPFDIST ||
							   uri->protocol == URI_GPFDISTS ||
							   uri->protocol == URI_CUSTOM))
	{
		if ((strcmp(on_clause, "COORDINATOR_ONLY") == 0) && (uri->protocol == URI_CUSTOM))
		{
			const char *uri_str = strVal(linitial(ext->urilocations));
			segdb_file_map[0] = pstrdup(uri_str);
			*iscoordinatoronly = true;
		}
		else
		{
			/*
			 * Re-write the location list for GPFDIST or GPFDISTS before mapping to segments.
			 *
			 * If we happen to be dealing with URI's with the 'gpfdist' (or 'gpfdists') protocol
			 * we do an extra step here.
			 *
			 * (*) We modify the urilocationlist so that every
			 * primary segdb will get a URI (therefore we duplicate the existing
			 * URI's until the list is of size = total_primaries).
			 * Example: 2 URIs, 7 total segdbs.
			 * Original LocationList: URI1->URI2
			 * Modified LocationList: URI1->URI2->URI1->URI2->URI1->URI2->URI1
			 *
			 * (**) We also make sure that we don't allocate more segdbs than
			 * (# of URIs x gp_external_max_segs).
			 * Example: 2 URIs, 7 total segdbs, gp_external_max_segs = 3
			 * Original LocationList: URI1->URI2
			 * Modified LocationList: URI1->URI2->URI1->URI2->URI1->URI2 (6 total).
			 *
			 * (***) In that case that we need to allocate only a subset of primary
			 * segdbs and not all we then also create a random map of segments to skip.
			 * Using the previous example a we create a map of 7 entries and need to
			 * randomly select 1 segdb to skip (7 - 6 = 1). so it may look like this:
			 * [F F T F F F F] - in which case we know to skip the 3rd segment only.
			 */

			/* total num of segs that will participate in the external operation */
			num_segs_participating = total_primaries;

			/* max num of segs that are allowed to participate in the operation */
			if ((uri->protocol == URI_GPFDIST) || (uri->protocol == URI_GPFDISTS))
			{
				max_participants_allowed = list_length(ext->urilocations) *
					gp_external_max_segs;
			}
			else
			{
				/*
				 * for custom protocol, set max_participants_allowed to
				 * num_segs_participating so that assignment to segments will use
				 * all available segments
				 */
				max_participants_allowed = num_segs_participating;
			}

			elog(DEBUG5,
				 "num_segs_participating = %d. max_participants_allowed = %d. number of URIs = %d",
				 num_segs_participating, max_participants_allowed, list_length(ext->urilocations));

			/* see (**) above */
			if (num_segs_participating > max_participants_allowed)
			{
				total_to_skip = num_segs_participating - max_participants_allowed;
				num_segs_participating = max_participants_allowed;
				should_skip_randomly = true;

				elog(NOTICE, "External scan %s will utilize %d out "
					 "of %d segment databases",
					 (uri->protocol == URI_GPFDIST ? "from gpfdist(s) server" : "using custom protocol"),
					 num_segs_participating,
					 total_primaries);
			}

			if (list_length(ext->urilocations) > num_segs_participating)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("there are more external files (URLs) than primary segments that can read them"),
						 errdetail("Found %d URLs and %d primary segments.",
								   list_length(ext->urilocations),
								   num_segs_participating)));

			/*
			 * restart location list and fill in new list until number of
			 * locations equals the number of segments participating in this
			 * action (see (*) above for more details).
			 */
			while (!done)
			{
				foreach(c, ext->urilocations)
				{
					char	   *uri_str = (char *) strVal(lfirst(c));

					/* append to a list of Value nodes, size nelems */
					modifiedloclist = lappend(modifiedloclist, makeString(pstrdup(uri_str)));

					if (list_length(modifiedloclist) == num_segs_participating)
					{
						done = true;
						break;
					}

					if (list_length(modifiedloclist) > num_segs_participating)
					{
						elog(ERROR, "External scan location list failed building distribution.");
					}
				}
			}

			/* See (***) above for details */
			if (should_skip_randomly)
				skip_map = makeRandomSegMap(total_primaries, total_to_skip);

			/*
			 * assign each URI from the new location list a primary segdb
			 */
			foreach(c, modifiedloclist)
			{
				const char *uri_str = strVal(lfirst(c));

				uri = ParseExternalTableUri(uri_str);

				found_match = false;

				/*
				 * look through our segment database list and try to find a
				 * database that can handle this uri.
				 */
				for (i = 0; i < db_info->total_segment_dbs && !found_match; i++)
				{
					CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
					int			segind = p->config->segindex;

					/*
					 * Assign mapping of external file to this segdb only if:
					 * 1) This segdb is a valid primary.
					 * 2) An external file wasn't already assigned to it.
					 */
					if (SEGMENT_IS_ACTIVE_PRIMARY(p))
					{
						/*
						 * skip this segdb if skip_map for this seg index tells us
						 * to skip it (set to 'true').
						 */
						if (should_skip_randomly)
						{
							Assert(segind < total_primaries);

							if (skip_map[segind])
								continue;	/* skip it */
						}

						if (segdb_file_map[segind] == NULL)
						{
							/* segdb not taken yet. assign this URI to this segdb */
							segdb_file_map[segind] = pstrdup(uri_str);
							found_match = true;
						}

						/*
						 * too bad. this segdb already has an external source
						 * assigned
						 */
					}
				}

				/* We failed to find a segdb for this gpfdist(s) URI */
				if (!found_match)
				{
					/* should never happen */
					elog(LOG,
						 "external tables gpfdist(s) allocation error. "
						 "total_primaries: %d, num_segs_participating %d "
						 "max_participants_allowed %d, total_to_skip %d",
						 total_primaries, num_segs_participating,
						 max_participants_allowed, total_to_skip);

					elog(ERROR,
						 "internal error in createplan for external tables when trying to assign segments for gpfdist(s)");
				}
			}
		}
	}
	/* (3) */
	else if (using_execute)
	{
		const char *command = ext->command;
		const char *prefix = "execute:";
		char	   *prefixed_command;

		/* build the command string for the executor - 'execute:command' */
		StringInfo	buf = makeStringInfo();

		appendStringInfo(buf, "%s%s", prefix, command);
		prefixed_command = pstrdup(buf->data);

		pfree(buf->data);
		pfree(buf);
		buf = NULL;

		/*
		 * Now we handle each one of the ON locations separately:
		 *
		 * 1) all segs
		 * 2) one per host
		 * 3) all segs on host <foo>
		 * 4) seg <n> only
		 * 5) <n> random segs
		 * 6) coordinator only
		 */
		if (strcmp(on_clause, "ALL_SEGMENTS") == 0)
		{
			/* all segments get a copy of the command to execute */

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
					segdb_file_map[segind] = pstrdup(prefixed_command);
			}

		}
		else if (strcmp(on_clause, "PER_HOST") == 0)
		{
			/* 1 seg per host */

			List	   *visited_hosts = NIL;
			ListCell   *lc;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					bool		host_taken = false;

					foreach(lc, visited_hosts)
					{
						const char *hostname = strVal(lfirst(lc));

						if (pg_strcasecmp(hostname, p->config->hostname) == 0)
						{
							host_taken = true;
							break;
						}
					}

					/*
					 * if not assigned to a seg on this host before - do it
					 * now and add this hostname to the list so that we don't
					 * use segs on this host again.
					 */
					if (!host_taken)
					{
						segdb_file_map[segind] = pstrdup(prefixed_command);
						visited_hosts = lappend(visited_hosts,
										   makeString(pstrdup(p->config->hostname)));
					}
				}
			}
		}
		else if (strncmp(on_clause, "HOST:", strlen("HOST:")) == 0)
		{
			/* all segs on the specified host get copy of the command */
			char	   *hostname = on_clause + strlen("HOST:");
			bool		match_found = false;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p) &&
					pg_strcasecmp(hostname, p->config->hostname) == 0)
				{
					segdb_file_map[segind] = pstrdup(prefixed_command);
					match_found = true;
				}
			}

			if (!match_found)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("could not assign a segment database for command \"%s\")",
								command),
						 errdetail("No valid primary segment was found in the requested host name \"%s\".",
								hostname)));
		}
		else if (strncmp(on_clause, "SEGMENT_ID:", strlen("SEGMENT_ID:")) == 0)
		{
			/* 1 seg with specified id gets a copy of the command */
			int			target_segid = atoi(on_clause + strlen("SEGMENT_ID:"));
			bool		match_found = false;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p) && segind == target_segid)
				{
					segdb_file_map[segind] = pstrdup(prefixed_command);
					match_found = true;
				}
			}

			if (!match_found)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("could not assign a segment database for command \"%s\"",
								command),
						 errdetail("The requested segment id %d is not a valid primary segment or doesn't exist in the database",
								   target_segid)));
		}
		else if (strncmp(on_clause, "TOTAL_SEGS:", strlen("TOTAL_SEGS:")) == 0)
		{
			/* total n segments selected randomly */

			int			num_segs_to_use = atoi(on_clause + strlen("TOTAL_SEGS:"));

			if (num_segs_to_use > total_primaries)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("table defined with EXECUTE ON %d but there are only %d valid primary segments in the database",
								num_segs_to_use, total_primaries)));

			total_to_skip = total_primaries - num_segs_to_use;
			skip_map = makeRandomSegMap(total_primaries, total_to_skip);

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					Assert(segind < total_primaries);
					if (skip_map[segind])
						continue;		/* skip it */

					segdb_file_map[segind] = pstrdup(prefixed_command);
				}
			}
		}
		else if (strcmp(on_clause, "COORDINATOR_ONLY") == 0)
		{
			/*
			 * store the command in first array entry and indicate that it is
			 * meant for the coordinator segment (not seg o).
			 */
			segdb_file_map[0] = pstrdup(prefixed_command);
			*iscoordinatoronly = true;
		}
		else
		{
			elog(ERROR, "Internal error in createplan for external tables: got invalid ON clause code %s",
				 on_clause);
		}
	}
	else
	{
		/* should never get here */
		elog(ERROR, "Internal error in createplan for external tables");
	}

	/*
	 * convert array map to a list so it can be serialized as part of the plan
	 */
	filenames = NIL;
	for (i = 0; i < total_primaries; i++)
	{
		if (segdb_file_map[i] != NULL)
			filenames = lappend(filenames, makeString(segdb_file_map[i]));
		else
		{
			/* no file for this segdb. add a null entry */
			Value	   *n = makeNode(Value);

			n->type = T_Null;
			filenames = lappend(filenames, n);
		}
	}

	return filenames;
}
