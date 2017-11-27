/*-------------------------------------------------------------------------
 *
 * indexcmds.c
 *	  POSTGRES define and remove index code.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/indexcmds.c,v 1.181 2009/01/01 17:23:38 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/reloptions.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_tablespace.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

#include "cdb/cdbcat.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdboidsync.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbsrlz.h"
#include "cdb/cdbvars.h"
#include "libpq-fe.h"
#include "utils/faultinjector.h"

/* non-export function prototypes */
static void CheckPredicate(Expr *predicate);
static void ComputeIndexAttrs(IndexInfo *indexInfo,
				  Oid *classOidP,
				  int16 *colOptionP,
				  List *attList,
				  Oid relId,
				  char *accessMethodName, Oid accessMethodId,
				  bool amcanorder,
				  bool isconstraint);
static Oid GetIndexOpClass(List *opclass, Oid attrType,
				char *accessMethodName, Oid accessMethodId);
static bool relationHasPrimaryKey(Relation rel);
static bool relationHasUniqueIndex(Relation rel);


/*
 * Helper function, to check indcheckxmin for an index on all segments, and
 * set it on the master if it was set on any segment.
 *
 * If CREATE INDEX creates a "broken" HOT chain, the new index must not be
 * used by new queries, with an old snapshot, that would need to see the old
 * values. See src/backend/access/heap/README.HOT. This is enforced by
 * setting indcheckxmin in the pg_index row. In GPDB, we use the pg_index
 * row in the master for planning, but all the data is stored in the
 * segments, so indcheckxmin must be set in the master, if it's set in any
 * of the segments.
 */
static void
cdb_sync_indcheckxmin_with_segments(Oid indexRelationId)
{
	CdbPgResults cdb_pgresults = {NULL, 0};
	int			i;
	char		cmd[100];
	bool		indcheckxmin_set_in_any_segment;

	Assert(Gp_role == GP_ROLE_DISPATCH && !IsBootstrapProcessingMode());

	/*
	 * Query all the segments, for their indcheckxmin value for this index.
	 */
	snprintf(cmd, sizeof(cmd),
			 "select indcheckxmin from pg_catalog.pg_index where indexrelid = '%u'",
			 indexRelationId);

	CdbDispatchCommand(cmd, DF_WITH_SNAPSHOT, &cdb_pgresults);

	indcheckxmin_set_in_any_segment = false;
	for (i = 0; i < cdb_pgresults.numResults; i++)
	{
		char	   *val;

		if (PQresultStatus(cdb_pgresults.pg_results[i]) != PGRES_TUPLES_OK)
		{
			cdbdisp_clearCdbPgResults(&cdb_pgresults);
			elog(ERROR, "could not fetch indcheckxmin from segment");
		}

		if (PQntuples(cdb_pgresults.pg_results[i]) != 1 ||
			PQnfields(cdb_pgresults.pg_results[i]) != 1 ||
			PQgetisnull(cdb_pgresults.pg_results[i], 0, 0))
			elog(ERROR, "unexpected shape of result set for indcheckxmin query");

		val = PQgetvalue(cdb_pgresults.pg_results[i], 0, 0);
		if (val[0] == 't')
		{
			indcheckxmin_set_in_any_segment = true;
			break;
		}
		else if (val[0] != 'f')
			elog(ERROR, "invalid boolean value received from segment: %s", val);
	}

	cdbdisp_clearCdbPgResults(&cdb_pgresults);

	/*
	 * If indcheckxmin was set on any segment, also set it in the master.
	 */
	if (indcheckxmin_set_in_any_segment)
	{
		Relation	pg_index;
		HeapTuple	indexTuple;
		Form_pg_index indexForm;

		pg_index = heap_open(IndexRelationId, RowExclusiveLock);

		indexTuple = SearchSysCacheCopy(INDEXRELID,
										ObjectIdGetDatum(indexRelationId),
										0, 0, 0);
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexRelationId);
		indexForm = (Form_pg_index) GETSTRUCT(indexTuple);

		if (!indexForm->indcheckxmin)
		{
			indexForm->indcheckxmin = true;
			simple_heap_update(pg_index, &indexTuple->t_self, indexTuple);
			CatalogUpdateIndexes(pg_index, indexTuple);
		}

		heap_freetuple(indexTuple);
		heap_close(pg_index, RowExclusiveLock);
	}
}


/*
 * DefineIndex
 *		Creates a new index.
 *
 * 'heapRelation': the relation the index will apply to.
 * 'indexRelationName': the name for the new index, or NULL to indicate
 *		that a nonconflicting default name should be picked.
 * 'indexRelationId': normally InvalidOid, but during bootstrap can be
 *		nonzero to specify a preselected OID for the index.
 * 'accessMethodName': name of the AM to use.
 * 'tableSpaceName': name of the tablespace to create the index in.
 *		NULL specifies using the appropriate default.
 * 'attributeList': a list of IndexElem specifying columns and expressions
 *		to index on.
 * 'predicate': the partial-index condition, or NULL if none.
 * 'options': reloptions from WITH (in list-of-DefElem form).
 * 'unique': make the index enforce uniqueness.
 * 'primary': mark the index as a primary key in the catalogs.
 * 'isconstraint': index is for a PRIMARY KEY or UNIQUE constraint,
 *		so build a pg_constraint entry for it.
 * 'is_alter_table': this is due to an ALTER rather than a CREATE operation.
 * 'check_rights': check for CREATE rights in the namespace.  (This should
 *		be true except when ALTER is deleting/recreating an index.)
 * 'skip_build': make the catalog entries but leave the index file empty;
 *		it will be filled later.
 * 'quiet': suppress the NOTICE chatter ordinarily provided for constraints.
 * 'concurrent': avoid blocking writers to the table while building.
 * 'stmt': the IndexStmt for this index.  Many other arguments are just values
 *		of fields in here.  
 *		XXX One day it might pay to eliminate the redundancy.
 */
void
DefineIndex(RangeVar *heapRelation,
			char *indexRelationName,
			Oid indexRelationId,
			char *accessMethodName,
			char *tableSpaceName,
			List *attributeList,
			Expr *predicate,
			List *options,
			bool unique,
			bool primary,
			bool isconstraint,
			bool is_alter_table,
			bool check_rights,
			bool skip_build,
			bool quiet,
			bool concurrent,
			IndexStmt *stmt)
{
	Oid		   *classObjectId;
	Oid			accessMethodId;
	Oid			relationId;
	Oid			namespaceId;
	Oid			tablespaceId;
	Relation	rel;
	Relation	indexRelation;
	HeapTuple	tuple;
	Form_pg_am	accessMethodForm;
	bool		amcanorder;
	RegProcedure amoptions;
	Datum		reloptions;
	int16	   *coloptions;
	IndexInfo  *indexInfo;
	int			numberOfAttributes;
	VirtualTransactionId *old_lockholders;
	VirtualTransactionId *old_snapshots;
	LockRelId	heaprelid;
	LOCKTAG		heaplocktag;
	Snapshot	snapshot;
	LOCKMODE	heap_lockmode;
	bool		need_longlock = true;
	bool		shouldDispatch = Gp_role == GP_ROLE_DISPATCH && !IsBootstrapProcessingMode();
	List	   *dispatch_oids;
	char	   *altconname = stmt ? stmt->altconname : NULL;

	/*
	 * count attributes in index
	 */
	numberOfAttributes = list_length(attributeList);
	if (numberOfAttributes <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("must specify at least one column")));
	if (numberOfAttributes > INDEX_MAX_KEYS)
		ereport(ERROR,
				(errcode(ERRCODE_TOO_MANY_COLUMNS),
				 errmsg("cannot use more than %d columns in an index",
						INDEX_MAX_KEYS)));

	/*
	 * Open heap relation, acquire a suitable lock on it, remember its OID
	 *
	 * Only SELECT ... FOR UPDATE/SHARE are allowed while doing a standard
	 * index build; but for concurrent builds we allow INSERT/UPDATE/DELETE
	 * (but not VACUUM).
	 */
	heap_lockmode = concurrent ? ShareUpdateExclusiveLock : ShareLock;
	rel = heap_openrv(heapRelation, heap_lockmode);

	relationId = RelationGetRelid(rel);
	namespaceId = RelationGetNamespace(rel);

	if(RelationIsExternal(rel))
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot create indexes on external tables.")));
		
	/* Note: during bootstrap may see uncataloged relation */
	if (rel->rd_rel->relkind != RELKIND_RELATION &&
		rel->rd_rel->relkind != RELKIND_UNCATALOGED)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table",
						heapRelation->relname)));

	/*
	 * Don't try to CREATE INDEX on temp tables of other backends.
	 */
	if (isOtherTempNamespace(namespaceId))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot create indexes on temporary tables of other sessions")));

	/*
	 * Verify we (still) have CREATE rights in the rel's namespace.
	 * (Presumably we did when the rel was created, but maybe not anymore.)
	 * Skip check if caller doesn't want it.  Also skip check if
	 * bootstrapping, since permissions machinery may not be working yet.
	 */
	if (check_rights && !IsBootstrapProcessingMode())
	{
		AclResult	aclresult;

		aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(),
										  ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
						   get_namespace_name(namespaceId));
	}

	/*
	 * Select tablespace to use.  If not specified, use default tablespace
	 * (which may in turn default to database's default).
	 *
	 * Note: This code duplicates code in tablecmds.c
	 */
	if (tableSpaceName)
	{
		tablespaceId = get_tablespace_oid(tableSpaceName, false);
	}
	else
	{
		tablespaceId = GetDefaultTablespace(rel->rd_istemp);
		/* note InvalidOid is OK in this case */

		/* Need the real tablespace id for dispatch */
		if (!OidIsValid(tablespaceId)) 
			tablespaceId = MyDatabaseTableSpace;

		/* 
		 * MPP-8238 : inconsistent tablespaces between segments and master 
		 */
		if (shouldDispatch)
			stmt->tableSpace = get_tablespace_name(tablespaceId);
	}

	/* Check permissions except when using database's default */
	if (OidIsValid(tablespaceId) && tablespaceId != MyDatabaseTableSpace)
	{
		AclResult	aclresult;

		aclresult = pg_tablespace_aclcheck(tablespaceId, GetUserId(),
										   ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_TABLESPACE,
						   get_tablespace_name(tablespaceId));
	}

	/*
	 * Force shared indexes into the pg_global tablespace.	This is a bit of a
	 * hack but seems simpler than marking them in the BKI commands.
	 */
	if (rel->rd_rel->relisshared)
		tablespaceId = GLOBALTABLESPACE_OID;

	/*
	 * Select name for index if caller didn't specify
	 */
	if (indexRelationName == NULL)
	{
		if (primary)
			indexRelationName = ChooseRelationName(RelationGetRelationName(rel),
												   NULL,
												   "pkey",
												   namespaceId);
		else
		{
			IndexElem  *iparam = (IndexElem *) linitial(attributeList);

			indexRelationName = ChooseRelationName(RelationGetRelationName(rel),
												   iparam->name,
												   "key",
												   namespaceId);
		}
		stmt->idxname = indexRelationName;
	}

	/*
	 * look up the access method, verify it can handle the requested features
	 */
	tuple = SearchSysCache(AMNAME,
						   PointerGetDatum(accessMethodName),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
	{
		/*
		 * Hack to provide more-or-less-transparent updating of old RTREE
		 * indexes to GIST: if RTREE is requested and not found, use GIST.
		 */
		if (strcmp(accessMethodName, "rtree") == 0)
		{
			ereport(NOTICE,
					(errmsg("substituting access method \"gist\" for obsolete method \"rtree\"")));
			accessMethodName = "gist";
			tuple = SearchSysCache(AMNAME,
								   PointerGetDatum(accessMethodName),
								   0, 0, 0);
		}

		if (!HeapTupleIsValid(tuple))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("access method \"%s\" does not exist",
							accessMethodName)));
	}
	accessMethodId = HeapTupleGetOid(tuple);
	accessMethodForm = (Form_pg_am) GETSTRUCT(tuple);

	if (accessMethodId == HASH_AM_OID)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("hash indexes are not supported")));

	/* MPP-9329: disable creation of GIN indexes */
	if (accessMethodId == GIN_AM_OID)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("GIN indexes are not supported")));

	if (unique && !accessMethodForm->amcanunique)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			   errmsg("access method \"%s\" does not support unique indexes",
					  accessMethodName)));
	if (numberOfAttributes > 1 && !accessMethodForm->amcanmulticol)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		  errmsg("access method \"%s\" does not support multicolumn indexes",
				 accessMethodName)));

    if  (unique && (RelationIsAoRows(rel) || RelationIsAoCols(rel)))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("append-only tables do not support unique indexes")));

	amcanorder = accessMethodForm->amcanorder;
	amoptions = accessMethodForm->amoptions;

	ReleaseSysCache(tuple);

	/*
	 * Validate predicate, if given
	 */
	if (predicate)
		CheckPredicate(predicate);

	/*
	 * Extra checks when creating a PRIMARY KEY index.
	 */
	if (primary)
	{
		List	   *cmds;
		ListCell   *keys;

		/*
		 * If ALTER TABLE, check that there isn't already a PRIMARY KEY. In
		 * CREATE TABLE, we have faith that the parser rejected multiple pkey
		 * clauses; and CREATE INDEX doesn't have a way to say PRIMARY KEY, so
		 * it's no problem either.
		 */
		if (is_alter_table &&
			relationHasPrimaryKey(rel))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
			 errmsg("multiple primary keys for table \"%s\" are not allowed",
					RelationGetRelationName(rel))));
		}

		/*
		 * Check that all of the attributes in a primary key are marked as not
		 * null, otherwise attempt to ALTER TABLE .. SET NOT NULL
		 */
		cmds = NIL;
		foreach(keys, attributeList)
		{
			IndexElem  *key = (IndexElem *) lfirst(keys);
			HeapTuple	atttuple;

			if (!key->name)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("primary keys cannot be expressions")));

			/* System attributes are never null, so no problem */
			if (SystemAttributeByName(key->name, rel->rd_rel->relhasoids))
				continue;

			atttuple = SearchSysCacheAttName(relationId, key->name);
			if (HeapTupleIsValid(atttuple))
			{
				if (!((Form_pg_attribute) GETSTRUCT(atttuple))->attnotnull)
				{
					/* Add a subcommand to make this one NOT NULL */
					AlterTableCmd *cmd = makeNode(AlterTableCmd);

					cmd->subtype = AT_SetNotNull;
					cmd->name = key->name;
					cmd->part_expanded = true;

					cmds = lappend(cmds, cmd);
				}
				ReleaseSysCache(atttuple);
			}
			else
			{
				/*
				 * This shouldn't happen during CREATE TABLE, but can happen
				 * during ALTER TABLE.	Keep message in sync with
				 * transformIndexConstraints() in parser/parse_utilcmd.c.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_COLUMN),
						 errmsg("column \"%s\" named in key does not exist",
								key->name)));
			}
		}

		/*
		 * XXX: Shouldn't the ALTER TABLE .. SET NOT NULL cascade to child
		 * tables?	Currently, since the PRIMARY KEY itself doesn't cascade,
		 * we don't cascade the notnull constraint(s) either; but this is
		 * pretty debatable.
		 *
		 * XXX: possible future improvement: when being called from ALTER
		 * TABLE, it would be more efficient to merge this with the outer
		 * ALTER TABLE, so as to avoid two scans.  But that seems to
		 * complicate DefineIndex's API unduly.
		 */
		if (cmds)
			AlterTableInternal(relationId, cmds, false);
	}

	/*
	 * Parse AM-specific options, convert to text array form, validate.
	 */
	reloptions = transformRelOptions((Datum) 0, options, false, false);

	(void) index_reloptions(amoptions, reloptions, true);

	/*
	 * Prepare arguments for index_create, primarily an IndexInfo structure.
	 * Note that ii_Predicate must be in implicit-AND format.
	 */
	indexInfo = makeNode(IndexInfo);
	indexInfo->ii_NumIndexAttrs = numberOfAttributes;
	indexInfo->ii_Expressions = NIL;	/* for now */
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_Predicate = make_ands_implicit(predicate);
	indexInfo->ii_PredicateState = NIL;
	indexInfo->ii_Unique = unique;
	/* In a concurrent build, mark it not-ready-for-inserts */
	indexInfo->ii_ReadyForInserts = !concurrent;
	indexInfo->ii_Concurrent = concurrent;
	indexInfo->ii_BrokenHotChain = false;

	classObjectId = (Oid *) palloc(numberOfAttributes * sizeof(Oid));
	coloptions = (int16 *) palloc(numberOfAttributes * sizeof(int16));
	ComputeIndexAttrs(indexInfo, classObjectId, coloptions, attributeList,
					  relationId, accessMethodName, accessMethodId,
					  amcanorder, isconstraint);

	if (shouldDispatch)
	{
		if ((primary || unique) && rel->rd_cdbpolicy)
			checkPolicyForUniqueIndex(rel,
									  indexInfo->ii_KeyAttrNumbers,
									  indexInfo->ii_NumIndexAttrs,
									  primary,
									  list_length(indexInfo->ii_Expressions),
									  relationHasPrimaryKey(rel),
									  relationHasUniqueIndex(rel));
		
		/* We don't have to worry about constraints on parts.  Already checked. */
		if ( isconstraint && rel_is_partitioned(relationId) )
			checkUniqueConstraintVsPartitioning(rel,
												indexInfo->ii_KeyAttrNumbers,
												indexInfo->ii_NumIndexAttrs,
												primary);

	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		if (stmt)
			quiet = true;
	}

	/*
	 * Report index creation if appropriate (delay this till after most of the
	 * error checks)
	 */
	if (isconstraint && !quiet && Gp_role != GP_ROLE_EXECUTE)
		ereport(NOTICE,
		  (errmsg("%s %s will create implicit index \"%s\" for table \"%s\"",
				  is_alter_table ? "ALTER TABLE / ADD" : "CREATE TABLE /",
				  primary ? "PRIMARY KEY" : "UNIQUE",
				  indexRelationName, RelationGetRelationName(rel))));

	if (rel_needs_long_lock(RelationGetRelid(rel)))
		need_longlock = true;
	/* if this is a concurrent build, we must lock you long time */
	else if (concurrent)
		need_longlock = true;
	else
		need_longlock = false;

	/* save lockrelid and locktag for below, then close rel */
	heaprelid = rel->rd_lockInfo.lockRelId;
	SET_LOCKTAG_RELATION(heaplocktag, heaprelid.dbId, heaprelid.relId);
	if (need_longlock)
		heap_close(rel, NoLock);
	else
		heap_close(rel, heap_lockmode);

   	if (shouldDispatch)
	{
		cdb_sync_oid_to_segments();

		/*
		 * We defer the dispatch of the utility command until after
		 * index_create(), because that call will *wait*
		 * for any other transactions touching this new relation,
		 * which can cause a non-local deadlock if we've already
		 * dispatched
		 */
	}

	if (!concurrent)
	{
		indexRelationId =
			index_create(relationId, indexRelationName, indexRelationId,
					  indexInfo, accessMethodId, tablespaceId, classObjectId,
						 coloptions, reloptions, primary, isconstraint,
						 allowSystemTableModsDDL, skip_build, concurrent, altconname);

		/*
		 * Dispatch the command to all primary and mirror segment dbs.
		 * Start a global transaction and reconfigure cluster if needed.
		 * Wait for QEs to finish.  Exit via ereport(ERROR,...) if error.
		 * (For a concurrent build, we do this later, see below.)
		 */
		if (shouldDispatch)
		{
			CdbDispatchUtilityStatement((Node *) stmt,
										DF_CANCEL_ON_ERROR |
										DF_WITH_SNAPSHOT |
										DF_NEED_TWO_PHASE,
										GetAssignedOidsForDispatch(),
										NULL);

			/* Set indcheckxmin in the master, if it was set on any segment */
			if (!indexInfo->ii_BrokenHotChain)
				cdb_sync_indcheckxmin_with_segments(indexRelationId);
		}

		return;					/* We're done, in the standard case */
	}

	/*
	 * For a concurrent build, we next insert the catalog entry and add
	 * constraints.  We don't build the index just yet; we must first make the
	 * catalog entry so that the new index is visible to updating
	 * transactions.  That will prevent them from making incompatible HOT
	 * updates.  The new index will be marked not indisready and not
	 * indisvalid, so that no one else tries to either insert into it or use
	 * it for queries.	We pass skip_build = true to prevent the build.
	 */
	indexRelationId =
		index_create(relationId, indexRelationName, indexRelationId,
					 indexInfo, accessMethodId, tablespaceId, classObjectId,
					 coloptions, reloptions, primary, isconstraint,
					 allowSystemTableModsDDL, true, concurrent, altconname);

	/*
	 * We must commit our current transaction so that the index becomes
	 * visible; then start another.  Note that all the data structures we just
	 * built are lost in the commit.  The only data we keep past here are the
	 * relation IDs.
	 *
	 * Before committing, get a session-level lock on the table, to ensure
	 * that neither it nor the index can be dropped before we finish. This
	 * cannot block, even if someone else is waiting for access, because we
	 * already have the same lock within our transaction.
	 *
	 * Note: we don't currently bother with a session lock on the index,
	 * because there are no operations that could change its state while we
	 * hold lock on the parent table.  This might need to change later.
	 */
	LockRelationIdForSession(&heaprelid, ShareUpdateExclusiveLock);

	/*
	 * CommitTransactionCommand will throw an error, if we haven't dispatched
	 * the assigned oids to the segments, so pick them up first. We will
	 * dispatch them below, right after committing the transaction.
	 *
	 * FIXME: this has to be copied into TopMemoryContext, because the commit
	 * releases anything else. It is currently leaked!
	 */
	{
		MemoryContext old_context = MemoryContextSwitchTo(TopMemoryContext);

		dispatch_oids = copyObject(GetAssignedOidsForDispatch());

		MemoryContextSwitchTo(old_context);
	}

	PopActiveSnapshot();
	CommitTransactionCommand();

	/*
	 * We dispatch the command to QEs after we've committed the creation of
	 * the empty index in the master, but before we proceed to fill it.
	 * This ensures that if something goes wrong, we don't end up in
	 * a state where the index exists on some segments but not the master.
	 * It also ensures that the index is only marked as valid on the
	 * master, after it's been successfully built and marked as valid on
	 * all the segments.
	 */
	if (shouldDispatch)
	{
		/*
		 * Note: We don't use a snapshot. Each QE will create their own
		 * transactions and take their own snapshots. We will wait later
		 * later for all the current distributed transactions to finish, so
		 * it's not important what exact snapshot is used here.
		 */
		CdbDispatchUtilityStatement((Node *)stmt,
									DF_CANCEL_ON_ERROR,
									dispatch_oids,
									NULL);
	}

	StartTransactionCommand();

	/*
	 * Phase 2 of concurrent index build (see comments for validate_index()
	 * for an overview of how this works)
	 *
	 * Now we must wait until no running transaction could have the table open
	 * with the old list of indexes.  To do this, inquire which xacts
	 * currently would conflict with ShareLock on the table -- ie, which ones
	 * have a lock that permits writing the table.	Then wait for each of
	 * these xacts to commit or abort.	Note we do not need to worry about
	 * xacts that open the table for writing after this point; they will see
	 * the new index when they open it.
	 *
	 * Note: the reason we use actual lock acquisition here, rather than just
	 * checking the ProcArray and sleeping, is that deadlock is possible if
	 * one of the transactions in question is blocked trying to acquire an
	 * exclusive lock on our table.  The lock code will detect deadlock and
	 * error out properly.
	 *
	 * Note: GetLockConflicts() never reports our own xid, hence we need not
	 * check for that.	Also, prepared xacts are not reported, which is fine
	 * since they certainly aren't going to do anything more.
	 */
	old_lockholders = GetLockConflicts(&heaplocktag, ShareLock);

	while (VirtualTransactionIdIsValid(*old_lockholders))
	{
		VirtualXactLockTableWait(*old_lockholders);
		old_lockholders++;
	}

	/*
	 * At this moment we are sure that there are no transactions with the
	 * table open for write that don't have this new index in their list of
	 * indexes.  We have waited out all the existing transactions and any new
	 * transaction will have the new index in its list, but the index is still
	 * marked as "not-ready-for-inserts".  The index is consulted while
	 * deciding HOT-safety though.	This arrangement ensures that no new HOT
	 * chains can be created where the new tuple and the old tuple in the
	 * chain have different index keys.
	 *
	 * We now take a new snapshot, and build the index using all tuples that
	 * are visible in this snapshot.  We can be sure that any HOT updates to
	 * these tuples will be compatible with the index, since any updates made
	 * by transactions that didn't know about the index are now committed or
	 * rolled back.  Thus, each visible tuple is either the end of its
	 * HOT-chain or the extension of the chain is HOT-safe for this index.
	 */

	/* Open and lock the parent heap relation */
	rel = heap_openrv(heapRelation, ShareUpdateExclusiveLock);

	/* And the target index relation */
	indexRelation = index_open(indexRelationId, RowExclusiveLock);

	/* Set ActiveSnapshot since functions in the indexes may need it */
	PushActiveSnapshot(GetTransactionSnapshot());

	/* We have to re-build the IndexInfo struct, since it was lost in commit */
	indexInfo = BuildIndexInfo(indexRelation);
	Assert(!indexInfo->ii_ReadyForInserts);
	indexInfo->ii_Concurrent = true;
	indexInfo->ii_BrokenHotChain = false;

	/* Now build the index */
	index_build(rel, indexRelation, indexInfo, primary, false);

	/* Close both the relations, but keep the locks */
	heap_close(rel, NoLock);
	index_close(indexRelation, NoLock);

	/*
	 * Update the pg_index row to mark the index as ready for inserts. Once we
	 * commit this transaction, any new transactions that open the table must
	 * insert new entries into the index for insertions and non-HOT updates.
	 */
	index_set_state_flags(indexRelationId, INDEX_CREATE_SET_READY);

	/* we can do away with our snapshot */
	PopActiveSnapshot();

	/*
	 * Commit this transaction to make the indisready update visible.
	 */
	CommitTransactionCommand();
	StartTransactionCommand();

	/*
	 * Phase 3 of concurrent index build
	 *
	 * We once again wait until no transaction can have the table open with
	 * the index marked as read-only for updates.
	 */
	old_lockholders = GetLockConflicts(&heaplocktag, ShareLock);

	while (VirtualTransactionIdIsValid(*old_lockholders))
	{
		VirtualXactLockTableWait(*old_lockholders);
		old_lockholders++;
	}

	/*
	 * Now take the "reference snapshot" that will be used by validate_index()
	 * to filter candidate tuples.	Beware!  There might still be snapshots in
	 * use that treat some transaction as in-progress that our reference
	 * snapshot treats as committed.  If such a recently-committed transaction
	 * deleted tuples in the table, we will not include them in the index; yet
	 * those transactions which see the deleting one as still-in-progress will
	 * expect them to be there once we mark the index as valid.
	 *
	 * We solve this by waiting for all endangered transactions to exit before
	 * we mark the index as valid.
	 *
	 * We also set ActiveSnapshot to this snap, since functions in indexes may
	 * need a snapshot.
	 */
	snapshot = RegisterSnapshot(GetTransactionSnapshot());
	PushActiveSnapshot(snapshot);

	/*
	 * Scan the index and the heap, insert any missing index entries.
	 */
	validate_index(relationId, indexRelationId, snapshot);

	/*
	 * The index is now valid in the sense that it contains all currently
	 * interesting tuples.	But since it might not contain tuples deleted just
	 * before the reference snap was taken, we have to wait out any
	 * transactions that might have older snapshots.  Obtain a list of VXIDs
	 * of such transactions, and wait for them individually.
	 *
	 * We can exclude any running transactions that have xmin >= the xmax of
	 * our reference snapshot, since they are clearly not interested in any
	 * missing older tuples.  Transactions in other DBs aren't a problem
	 * either, since they'll never even be able to see this index.
	 *
	 * We can also exclude autovacuum processes and processes running manual
	 * lazy VACUUMs, because they won't be fazed by missing index entries
	 * either.  (Manual ANALYZEs, however, can't be excluded because they
	 * might be within transactions that are going to do arbitrary operations
	 * later.)
	 *
	 * Also, GetCurrentVirtualXIDs never reports our own vxid, so we need not
	 * check for that.
	 */
#if 0  /* Upstream code not applicable to GPDB */
	old_snapshots = GetCurrentVirtualXIDs(snapshot->xmax, false,
										  PROC_IS_AUTOVACUUM | PROC_IN_VACUUM);
#else
	old_snapshots = GetCurrentVirtualXIDs(snapshot->xmax, false,
										  PROC_IS_AUTOVACUUM);
#endif
	while (VirtualTransactionIdIsValid(*old_snapshots))
	{
		VirtualXactLockTableWait(*old_snapshots);
		old_snapshots++;
	}

	/*
	 * Index can now be marked valid -- update its pg_index entry
	 */
	index_set_state_flags(indexRelationId, INDEX_CREATE_SET_VALID);

	/*
	 * The pg_index update will cause backends (including this one) to update
	 * relcache entries for the index itself, but we should also send a
	 * relcache inval on the parent table to force replanning of cached plans.
	 * Otherwise existing sessions might fail to use the new index where it
	 * would be useful.  (Note that our earlier commits did not create reasons
	 * to replan; so relcache flush on the index itself was sufficient.)
	 */
	CacheInvalidateRelcacheByRelid(heaprelid.relId);

	/* we can now do away with our active snapshot */
	PopActiveSnapshot();

	/* And we can remove the validating snapshot too */
	UnregisterSnapshot(snapshot);

	/*
	 * Last thing to do is release the session-level lock on the parent table.
	 */
	UnlockRelationIdForSession(&heaprelid, ShareUpdateExclusiveLock);
}


/*
 * CheckPredicate
 *		Checks that the given partial-index predicate is valid.
 *
 * This used to also constrain the form of the predicate to forms that
 * indxpath.c could do something with.	However, that seems overly
 * restrictive.  One useful application of partial indexes is to apply
 * a UNIQUE constraint across a subset of a table, and in that scenario
 * any evaluatable predicate will work.  So accept any predicate here
 * (except ones requiring a plan), and let indxpath.c fend for itself.
 */
static void
CheckPredicate(Expr *predicate)
{
	/*
	 * transformExpr() should have already rejected subqueries, aggregates,
	 * and window functions, based on the EXPR_KIND_ for a predicate.
	 */

	/*
	 * A predicate using mutable functions is probably wrong, for the same
	 * reasons that we don't allow an index expression to use one.
	 */
	if (contain_mutable_functions((Node *) predicate))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
		   errmsg("functions in index predicate must be marked IMMUTABLE")));
}

/*
 * Compute per-index-column information, including indexed column numbers
 * or index expressions, opclasses, and indoptions.
 */
static void
ComputeIndexAttrs(IndexInfo *indexInfo,
				  Oid *classOidP,
				  int16 *colOptionP,
				  List *attList,	/* list of IndexElem's */
				  Oid relId,
				  char *accessMethodName,
				  Oid accessMethodId,
				  bool amcanorder,
				  bool isconstraint)
{
	ListCell   *rest;
	int			attn = 0;

	/*
	 * process attributeList
	 */
	foreach(rest, attList)
	{
		IndexElem  *attribute = (IndexElem *) lfirst(rest);
		Oid			atttype;

		/*
		 * Process the column-or-expression to be indexed.
		 */
		if (attribute->name != NULL)
		{
			/* Simple index attribute */
			HeapTuple	atttuple;
			Form_pg_attribute attform;

			Assert(attribute->expr == NULL);
			atttuple = SearchSysCacheAttName(relId, attribute->name);
			if (!HeapTupleIsValid(atttuple))
			{
				/* difference in error message spellings is historical */
				if (isconstraint)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
						  errmsg("column \"%s\" named in key does not exist",
								 attribute->name)));
				else
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" does not exist",
									attribute->name)));
			}
			attform = (Form_pg_attribute) GETSTRUCT(atttuple);
			indexInfo->ii_KeyAttrNumbers[attn] = attform->attnum;
			atttype = attform->atttypid;
			ReleaseSysCache(atttuple);
		}
		else if (attribute->expr && IsA(attribute->expr, Var) &&
				 ((Var *) attribute->expr)->varattno != InvalidAttrNumber)
		{
			/* Tricky tricky, he wrote (column) ... treat as simple attr */
			Var		   *var = (Var *) attribute->expr;

			indexInfo->ii_KeyAttrNumbers[attn] = var->varattno;
			atttype = get_atttype(relId, var->varattno);
		}
		else
		{
			/* Index expression */
			Assert(attribute->expr != NULL);
			indexInfo->ii_KeyAttrNumbers[attn] = 0;		/* marks expression */
			indexInfo->ii_Expressions = lappend(indexInfo->ii_Expressions,
												attribute->expr);
			atttype = exprType(attribute->expr);

			/*
			 * transformExpr() should have already rejected subqueries,
			 * aggregates, and window functions, based on the EXPR_KIND_
			 * for an index expression.
			 */

			/*
			 * A expression using mutable functions is probably wrong, since
			 * if you aren't going to get the same result for the same data
			 * every time, it's not clear what the index entries mean at all.
			 */
			if (contain_mutable_functions(attribute->expr))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("functions in index expression must be marked IMMUTABLE")));
		}

		/*
		 * Identify the opclass to use.
		 */
		classOidP[attn] = GetIndexOpClass(attribute->opclass,
										  atttype,
										  accessMethodName,
										  accessMethodId);

		/*
		 * Set up the per-column options (indoption field).  For now, this is
		 * zero for any un-ordered index, while ordered indexes have DESC and
		 * NULLS FIRST/LAST options.
		 */
		colOptionP[attn] = 0;
		if (amcanorder)
		{
			/* default ordering is ASC */
			if (attribute->ordering == SORTBY_DESC)
				colOptionP[attn] |= INDOPTION_DESC;
			/* default null ordering is LAST for ASC, FIRST for DESC */
			if (attribute->nulls_ordering == SORTBY_NULLS_DEFAULT)
			{
				if (attribute->ordering == SORTBY_DESC)
					colOptionP[attn] |= INDOPTION_NULLS_FIRST;
			}
			else if (attribute->nulls_ordering == SORTBY_NULLS_FIRST)
				colOptionP[attn] |= INDOPTION_NULLS_FIRST;
		}
		else
		{
			/* index AM does not support ordering */
			if (attribute->ordering != SORTBY_DEFAULT)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("access method \"%s\" does not support ASC/DESC options",
								accessMethodName)));
			if (attribute->nulls_ordering != SORTBY_NULLS_DEFAULT)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("access method \"%s\" does not support NULLS FIRST/LAST options",
								accessMethodName)));
		}

		attn++;
	}
}

/*
 * Resolve possibly-defaulted operator class specification
 */
static Oid
GetIndexOpClass(List *opclass, Oid attrType,
				char *accessMethodName, Oid accessMethodId)
{
	char	   *schemaname;
	char	   *opcname;
	HeapTuple	tuple;
	Oid			opClassId,
				opInputType;

	/*
	 * Release 7.0 removed network_ops, timespan_ops, and datetime_ops, so we
	 * ignore those opclass names so the default *_ops is used.  This can be
	 * removed in some later release.  bjm 2000/02/07
	 *
	 * Release 7.1 removes lztext_ops, so suppress that too for a while.  tgl
	 * 2000/07/30
	 *
	 * Release 7.2 renames timestamp_ops to timestamptz_ops, so suppress that
	 * too for awhile.	I'm starting to think we need a better approach. tgl
	 * 2000/10/01
	 *
	 * Release 8.0 removes bigbox_ops (which was dead code for a long while
	 * anyway).  tgl 2003/11/11
	 */
	if (list_length(opclass) == 1)
	{
		char	   *claname = strVal(linitial(opclass));

		if (strcmp(claname, "network_ops") == 0 ||
			strcmp(claname, "timespan_ops") == 0 ||
			strcmp(claname, "datetime_ops") == 0 ||
			strcmp(claname, "lztext_ops") == 0 ||
			strcmp(claname, "timestamp_ops") == 0 ||
			strcmp(claname, "bigbox_ops") == 0)
			opclass = NIL;
	}

	if (opclass == NIL)
	{
		/* no operator class specified, so find the default */
		opClassId = GetDefaultOpClass(attrType, accessMethodId);
		if (!OidIsValid(opClassId))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("data type %s has no default operator class for access method \"%s\"",
							format_type_be(attrType), accessMethodName),
					 errhint("You must specify an operator class for the index or define a default operator class for the data type.")));
		return opClassId;
	}

	/*
	 * Specific opclass name given, so look up the opclass.
	 */

	/* deconstruct the name list */
	DeconstructQualifiedName(opclass, &schemaname, &opcname);

	if (schemaname)
	{
		/* Look in specific schema only */
		Oid			namespaceId;

		namespaceId = LookupExplicitNamespace(schemaname);
		tuple = SearchSysCache(CLAAMNAMENSP,
							   ObjectIdGetDatum(accessMethodId),
							   PointerGetDatum(opcname),
							   ObjectIdGetDatum(namespaceId),
							   0);
	}
	else
	{
		/* Unqualified opclass name, so search the search path */
		opClassId = OpclassnameGetOpcid(accessMethodId, opcname);
		if (!OidIsValid(opClassId))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("operator class \"%s\" does not exist for access method \"%s\"",
							opcname, accessMethodName)));
		tuple = SearchSysCache(CLAOID,
							   ObjectIdGetDatum(opClassId),
							   0, 0, 0);
	}

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("operator class \"%s\" does not exist for access method \"%s\"",
						NameListToString(opclass), accessMethodName)));

	/*
	 * Verify that the index operator class accepts this datatype.	Note we
	 * will accept binary compatibility.
	 */
	opClassId = HeapTupleGetOid(tuple);
	opInputType = ((Form_pg_opclass) GETSTRUCT(tuple))->opcintype;

	if (!IsBinaryCoercible(attrType, opInputType))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("operator class \"%s\" does not accept data type %s",
					  NameListToString(opclass), format_type_be(attrType))));

	ReleaseSysCache(tuple);

	return opClassId;
}

/*
 * GetDefaultOpClass
 *
 * Given the OIDs of a datatype and an access method, find the default
 * operator class, if any.	Returns InvalidOid if there is none.
 */
Oid
GetDefaultOpClass(Oid type_id, Oid am_id)
{
	Oid			result = InvalidOid;
	int			nexact = 0;
	int			ncompatible = 0;
	int			ncompatiblepreferred = 0;
	Relation	rel;
	ScanKeyData skey[1];
	SysScanDesc scan;
	HeapTuple	tup;
	TYPCATEGORY	tcategory;

	/* If it's a domain, look at the base type instead */
	type_id = getBaseType(type_id);

	tcategory = TypeCategory(type_id);

	/*
	 * We scan through all the opclasses available for the access method,
	 * looking for one that is marked default and matches the target type
	 * (either exactly or binary-compatibly, but prefer an exact match).
	 *
	 * We could find more than one binary-compatible match.  If just one is
	 * for a preferred type, use that one; otherwise we fail, forcing the user
	 * to specify which one he wants.  (The preferred-type special case is a
	 * kluge for varchar: it's binary-compatible to both text and bpchar, so
	 * we need a tiebreaker.)  If we find more than one exact match, then
	 * someone put bogus entries in pg_opclass.
	 */
	rel = heap_open(OperatorClassRelationId, AccessShareLock);

	ScanKeyInit(&skey[0],
				Anum_pg_opclass_opcmethod,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(am_id));

	scan = systable_beginscan(rel, OpclassAmNameNspIndexId, true,
							  SnapshotNow, 1, skey);

	while (HeapTupleIsValid(tup = systable_getnext(scan)))
	{
		Form_pg_opclass opclass = (Form_pg_opclass) GETSTRUCT(tup);

		/* ignore altogether if not a default opclass */
		if (!opclass->opcdefault)
			continue;
		if (opclass->opcintype == type_id)
		{
			nexact++;
			result = HeapTupleGetOid(tup);
		}
		else if (nexact == 0 &&
				 IsBinaryCoercible(type_id, opclass->opcintype))
		{
			if (IsPreferredType(tcategory, opclass->opcintype))
			{
				ncompatiblepreferred++;
				result = HeapTupleGetOid(tup);
			}
			else if (ncompatiblepreferred == 0)
			{
				ncompatible++;
				result = HeapTupleGetOid(tup);
			}
		}
	}

	systable_endscan(scan);

	heap_close(rel, AccessShareLock);

	/* raise error if pg_opclass contains inconsistent data */
	if (nexact > 1)
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
		errmsg("there are multiple default operator classes for data type %s",
			   format_type_be(type_id))));

	if (nexact == 1 ||
		ncompatiblepreferred == 1 ||
		(ncompatiblepreferred == 0 && ncompatible == 1))
		return result;

	return InvalidOid;
}

/*
 *	makeObjectName()
 *
 *	Create a name for an implicitly created index, sequence, constraint, etc.
 *
 *	The parameters are typically: the original table name, the original field
 *	name, and a "type" string (such as "seq" or "pkey").	The field name
 *	and/or type can be NULL if not relevant.
 *
 *	The result is a palloc'd string.
 *
 *	The basic result we want is "name1_name2_label", omitting "_name2" or
 *	"_label" when those parameters are NULL.  However, we must generate
 *	a name with less than NAMEDATALEN characters!  So, we truncate one or
 *	both names if necessary to make a short-enough string.	The label part
 *	is never truncated (so it had better be reasonably short).
 *
 *	The caller is responsible for checking uniqueness of the generated
 *	name and retrying as needed; retrying will be done by altering the
 *	"label" string (which is why we never truncate that part).
 */
char *
makeObjectName(const char *name1, const char *name2, const char *label)
{
	char	   *name;
	int			overhead = 0;	/* chars needed for label and underscores */
	int			availchars;		/* chars available for name(s) */
	int			name1chars;		/* chars allocated to name1 */
	int			name2chars;		/* chars allocated to name2 */
	int			ndx;

	name1chars = strlen(name1);
	if (name2)
	{
		name2chars = strlen(name2);
		overhead++;				/* allow for separating underscore */
	}
	else
		name2chars = 0;
	if (label)
		overhead += strlen(label) + 1;

	availchars = NAMEDATALEN - 1 - overhead;
	Assert(availchars > 0);		/* else caller chose a bad label */

	/*
	 * If we must truncate,  preferentially truncate the longer name. This
	 * logic could be expressed without a loop, but it's simple and obvious as
	 * a loop.
	 */
	while (name1chars + name2chars > availchars)
	{
		if (name1chars > name2chars)
			name1chars--;
		else
			name2chars--;
	}

	name1chars = pg_mbcliplen(name1, name1chars, name1chars);
	if (name2)
		name2chars = pg_mbcliplen(name2, name2chars, name2chars);

	/* Now construct the string using the chosen lengths */
	name = palloc(name1chars + name2chars + overhead + 1);
	memcpy(name, name1, name1chars);
	ndx = name1chars;
	if (name2)
	{
		name[ndx++] = '_';
		memcpy(name + ndx, name2, name2chars);
		ndx += name2chars;
	}
	if (label)
	{
		name[ndx++] = '_';
		strcpy(name + ndx, label);
	}
	else
		name[ndx] = '\0';

	return name;
}

/*
 * Select a nonconflicting name for a new relation.  This is ordinarily
 * used to choose index names (which is why it's here) but it can also
 * be used for sequences, or any autogenerated relation kind.
 *
 * name1, name2, and label are used the same way as for makeObjectName(),
 * except that the label can't be NULL; digits will be appended to the label
 * if needed to create a name that is unique within the specified namespace.
 *
 * Note: it is theoretically possible to get a collision anyway, if someone
 * else chooses the same name concurrently.  This is fairly unlikely to be
 * a problem in practice, especially if one is holding an exclusive lock on
 * the relation identified by name1.  
 *
 * If choosing multiple names within a single command, there are two options:
 *   1) Create the new object and do CommandCounterIncrement
 *   2) Pass a hash-table to this function to use as a cache of objects 
 *      created in this statement.
 *
 * Returns a palloc'd string.
 */
char *
ChooseRelationName(const char *name1, const char *name2,
				   const char *label, Oid namespace)
{
	return ChooseRelationNameWithCache(name1, name2, label, namespace, NULL);
}

char *
ChooseRelationNameWithCache(const char *name1, const char *name2,
				   const char *label, Oid namespace,
				   HTAB *cache)
{
	int			pass = 0;
	char	   *relname = NULL;
	char		modlabel[NAMEDATALEN];
	bool		found = false;

	/* try the unmodified label first */
	StrNCpy(modlabel, label, sizeof(modlabel));

	for (;;)
	{
		relname = makeObjectName(name1, name2, modlabel);

		if (cache)
			hash_search(cache, (void *) relname, HASH_FIND, &found);

		if (!found && !OidIsValid(get_relname_relid(relname, namespace)))
			break;

		/* found a conflict, so try a new name component */
		pfree(relname);
		snprintf(modlabel, sizeof(modlabel), "%s%d", label, ++pass);
	}

	/* If we are caching found values add the value to our hash */
	if (cache)
	{
		hash_search(cache, (void *) relname, HASH_ENTER, &found);
		Assert(!found);
	}

	return relname;
}

/*
 * relationHasPrimaryKey -
 *
 *	See whether an existing relation has a primary key.
 */
static bool
relationHasPrimaryKey(Relation rel)
{
	bool		result = false;
	List	   *indexoidlist;
	ListCell   *indexoidscan;

	/*
	 * Get the list of index OIDs for the table from the relcache, and look up
	 * each one in the pg_index syscache until we find one marked primary key
	 * (hopefully there isn't more than one such).
	 */
	indexoidlist = RelationGetIndexList(rel);

	foreach(indexoidscan, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(indexoidscan);
		HeapTuple	indexTuple;

		indexTuple = SearchSysCache(INDEXRELID,
									ObjectIdGetDatum(indexoid),
									0, 0, 0);
		if (!HeapTupleIsValid(indexTuple))		/* should not happen */
			elog(ERROR, "cache lookup failed for index %u", indexoid);
		result = ((Form_pg_index) GETSTRUCT(indexTuple))->indisprimary;
		ReleaseSysCache(indexTuple);
		if (result)
			break;
	}

	list_free(indexoidlist);

	return result;
}

/*
 * relationHasUniqueIndex -
 *
 *	See whether an existing relation has a unique index.
 */
static bool
relationHasUniqueIndex(Relation rel)
{
	bool		result = false;
	List	   *indexoidlist;
	ListCell   *indexoidscan;

	/*
	 * Get the list of index OIDs for the table from the relcache, and look up
	 * each one in the pg_index syscache until we find one marked unique
	 */
	indexoidlist = RelationGetIndexList(rel);

	foreach(indexoidscan, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(indexoidscan);
		HeapTuple	indexTuple;

		indexTuple = SearchSysCache(INDEXRELID,
									ObjectIdGetDatum(indexoid),
									0, 0, 0);
		if (!HeapTupleIsValid(indexTuple))		/* should not happen */
			elog(ERROR, "cache lookup failed for index %u", indexoid);
		result = ((Form_pg_index) GETSTRUCT(indexTuple))->indisunique;
		ReleaseSysCache(indexTuple);
		if (result)
			break;
	}

	list_free(indexoidlist);

	return result;
}

/*
 * ReindexIndex
 *		Recreate a specific index.
 */
void
ReindexIndex(ReindexStmt *stmt)
{
	Oid			indOid;
	HeapTuple	tuple;

	indOid = RangeVarGetRelid(stmt->relation, false);
	tuple = SearchSysCache(RELOID,
						   ObjectIdGetDatum(indOid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))		/* shouldn't happen */
		elog(ERROR, "cache lookup failed for relation %u", indOid);

	if (((Form_pg_class) GETSTRUCT(tuple))->relkind != RELKIND_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not an index",
						stmt->relation->relname)));

	/* Check permissions */
	if (!pg_class_ownercheck(indOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   stmt->relation->relname);

	ReleaseSysCache(tuple);

	reindex_index(indOid);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR |
									DF_WITH_SNAPSHOT |
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
	}
}

/*
 * Perform REINDEX on each relation of the relids list.  The function
 * opens and closes a transaction per relation.  This is designed for
 * QD/utility, and is not useful for QE.
 */
static void
ReindexRelationList(List *relids)
{
	ListCell   *lc;

	Assert(Gp_role != GP_ROLE_EXECUTE);

	/*
	 * Commit ongoing transaction so that we can start a new
	 * transaction per relation.
	 */
	PopActiveSnapshot();
	CommitTransactionCommand();

	SIMPLE_FAULT_INJECTOR(ReindexDB);

	foreach (lc, relids)
	{
		Oid			relid = lfirst_oid(lc);
		Relation	rel = NULL;

		StartTransactionCommand();
		/* functions in indexes may want a snapshot set */
		PushActiveSnapshot(GetTransactionSnapshot());

		/*
		 * Try to open the relation. If the try fails it may mean that
		 * someone dropped the relation before we started processing
		 * (reindexing) it. This should be tolerable. Move on to the next
		 * one.
		 */
		rel = try_heap_open(relid, ShareLock, false);

		if (rel != NULL)
		{
			ReindexStmt	   *stmt;

			stmt = makeNode(ReindexStmt);

			stmt->relid = relid;
			stmt->kind = OBJECT_TABLE;

			/* perform reindex locally */
			if (!reindex_relation(relid, true))
				ereport(NOTICE,
					(errmsg("table \"%s\" has no indexes",
							RelationGetRelationName(rel))));
			/* no need to dispatch if the relation has no indexes. */
			else if (Gp_role == GP_ROLE_DISPATCH)
				CdbDispatchUtilityStatement((Node *) stmt,
											DF_CANCEL_ON_ERROR |
											DF_WITH_SNAPSHOT |
											DF_NEED_TWO_PHASE,
											GetAssignedOidsForDispatch(), /* FIXME */
											NULL);

			/* keep lock until end of transaction (which comes soon) */
			heap_close(rel, NoLock);
		}

		PopActiveSnapshot();
		CommitTransactionCommand();
	}

	/*
	 * We committed the transaction above, so start a new one before
	 * returning.
	 */
	StartTransactionCommand();
}

/*
 * ReindexTable
 *		Recreate all indexes of a table (and of its toast table, if any)
 */
void
ReindexTable(ReindexStmt *stmt)
{
	Oid			relid;
	MemoryContext	private_context, oldcontext;
	List	   *prels = NIL, *relids = NIL;
	ListCell   *lc;

	/*
	 * QE simply performs reindex work.  All other work has been done in QD.
	 */
	if (Gp_role == GP_ROLE_EXECUTE)
	{
		reindex_relation(stmt->relid, true);
		return;
	}

	relid = RangeVarGetRelid(stmt->relation, false);

	/*
	 * Gather child partition relations.
	 */
	if (rel_is_partitioned(relid))
	{
		PartitionNode *pn;

		pn = get_parts(relid, 0 /* level */, 0 /* parent */, false /* inctemplate */,
					   true /* includesubparts */);
		prels = all_partition_relids(pn);
	}
	else if (rel_is_child_partition(relid))
		prels = find_all_inheritors(relid);

	/*
	 * Create a memory context that will survive forced transaction commits we
	 * do below.  Since it is a child of PortalContext, it will go away
	 * eventually even if we suffer an error; there's no need for special
	 * abort cleanup logic.
	 */
	private_context = AllocSetContextCreate(PortalContext,
											"ReindexTable",
											ALLOCSET_DEFAULT_MINSIZE,
											ALLOCSET_DEFAULT_INITSIZE,
											ALLOCSET_DEFAULT_MAXSIZE);
	oldcontext = MemoryContextSwitchTo(private_context);
	relids = lappend_oid(relids, relid);
	relids = list_concat_unique_oid(relids, prels);
	MemoryContextSwitchTo(oldcontext);

	/* various checks on each partition */
	foreach (lc, relids)
	{
		Oid			heapOid = lfirst_oid(lc);
		HeapTuple	tuple;
		Form_pg_class pg_class_tuple;

		tuple = SearchSysCache1(RELOID,
								ObjectIdGetDatum(heapOid));
		if (!HeapTupleIsValid(tuple))		/* shouldn't happen */
			elog(ERROR, "cache lookup failed for relation %u", heapOid);

		pg_class_tuple = (Form_pg_class) GETSTRUCT(tuple);
		if (pg_class_tuple->relkind != RELKIND_RELATION &&
			pg_class_tuple->relkind != RELKIND_TOASTVALUE)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a table",
							NameStr(pg_class_tuple->relname))));

		/*
		 * Check appropriate permissions
		 */
		if (!pg_class_ownercheck(heapOid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
						   NameStr(pg_class_tuple->relname));

		/* Can't reindex shared tables except in standalone mode */
		if (((Form_pg_class) GETSTRUCT(tuple))->relisshared && IsUnderPostmaster)
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("shared table \"%s\" can only be reindexed in stand-alone mode",
							NameStr(pg_class_tuple->relname))));

		ReleaseSysCache(tuple);
	}

	ReindexRelationList(relids);

	MemoryContextDelete(private_context);
}

/*
 * ReindexDatabase
 *		Recreate indexes of a database.
 *
 * To reduce the probability of deadlocks, each table is reindexed in a
 * separate transaction, so we can release the lock on it right away.
 * That means this must not be called within a user transaction block!
 */
void
ReindexDatabase(ReindexStmt *stmt)
{
	Relation	relationRelation;
	HeapScanDesc scan;
	HeapTuple	tuple;
	MemoryContext private_context;
	MemoryContext old;
	List	   *relids = NIL;
	bool do_system = stmt->do_system;
	bool do_user = stmt->do_user;
	const char *databaseName = stmt->name;

	AssertArg(databaseName);
	Assert(Gp_role != GP_ROLE_EXECUTE);

	if (strcmp(databaseName, get_database_name(MyDatabaseId)) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("can only reindex the currently open database")));

	if (!pg_database_ownercheck(MyDatabaseId, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_DATABASE,
					   databaseName);

	/*
	 * Create a memory context that will survive forced transaction commits we
	 * do below.  Since it is a child of PortalContext, it will go away
	 * eventually even if we suffer an error; there's no need for special
	 * abort cleanup logic.
	 */
	private_context = AllocSetContextCreate(PortalContext,
											"ReindexDatabase",
											ALLOCSET_DEFAULT_MINSIZE,
											ALLOCSET_DEFAULT_INITSIZE,
											ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * We always want to reindex pg_class first.  This ensures that if there
	 * is any corruption in pg_class' indexes, they will be fixed before we
	 * process any other tables.  This is critical because reindexing itself
	 * will try to update pg_class.
	 */
	if (do_system)
	{
		old = MemoryContextSwitchTo(private_context);
		relids = lappend_oid(relids, RelationRelationId);
		MemoryContextSwitchTo(old);
	}

	/*
	 * Scan pg_class to build a list of the relations we need to reindex.
	 *
	 * We only consider plain relations here (toast rels will be processed
	 * indirectly by reindex_relation).
	 */
	relationRelation = heap_open(RelationRelationId, AccessShareLock);
	scan = heap_beginscan(relationRelation, SnapshotNow, 0, NULL);
	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		Form_pg_class classtuple = (Form_pg_class) GETSTRUCT(tuple);

		if (classtuple->relkind != RELKIND_RELATION)
			continue;

		/* Skip temp tables of other backends; we can't reindex them at all */
		if (isOtherTempNamespace(classtuple->relnamespace))
			continue;

		/* Check user/system classification, and optionally skip */
		if (IsSystemClass(classtuple))
		{
			if (!do_system)
				continue;
		}
		else
		{
			if (!do_user)
				continue;
		}

		if (IsUnderPostmaster)	/* silently ignore shared tables */
		{
			if (classtuple->relisshared)
				continue;
		}

		if (HeapTupleGetOid(tuple) == RelationRelationId)
			continue;			/* got it already */

		old = MemoryContextSwitchTo(private_context);
		relids = lappend_oid(relids, HeapTupleGetOid(tuple));
		MemoryContextSwitchTo(old);
	}
	heap_endscan(scan);
	heap_close(relationRelation, AccessShareLock);

	ReindexRelationList(relids);

	MemoryContextDelete(private_context);
}
