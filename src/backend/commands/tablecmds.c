/*-------------------------------------------------------------------------
 *
 * tablecmds.c
 *	  Commands for creating and altering table structures and settings
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/tablecmds.c,v 1.263 2008/08/25 22:42:32 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/aocs_compaction.h"
#include "access/appendonlywriter.h"
#include "access/bitmap.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/nbtree.h"
#include "access/reloptions.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_attribute_encoding.h"
#include "catalog/pg_compression.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "catalog/pg_type_fn.h"
#include "catalog/pg_partition.h"
#include "catalog/pg_partition_rule.h"
#include "catalog/toasting.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbpartition.h"
#include "commands/cluster.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "commands/trigger.h"
#include "commands/typecmds.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/parsenodes.h"
#include "optimizer/clauses.h"
#include "optimizer/plancat.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "parser/gramparse.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_partition.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "parser/parser.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "storage/sinval.h"
#include "storage/smgr.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbrelsize.h"
#include "cdb/cdboidsync.h"

#include "cdb/cdbmirroredfilesysobj.h"
#include "cdb/cdbmirroredbufferpool.h"
#include "cdb/cdbmirroredappendonly.h"
#include "cdb/cdbpersistentfilesysobj.h"


/*
 * ON COMMIT action list
 */
typedef struct OnCommitItem
{
	Oid			relid;			/* relid of relation */
	OnCommitAction oncommit;	/* what to do at end of xact */

	/*
	 * If this entry was created during the current transaction,
	 * creating_subid is the ID of the creating subxact; if created in a prior
	 * transaction, creating_subid is zero.  If deleted during the current
	 * transaction, deleting_subid is the ID of the deleting subxact; if no
	 * deletion request is pending, deleting_subid is zero.
	 */
	SubTransactionId creating_subid;
	SubTransactionId deleting_subid;
} OnCommitItem;

static List *on_commits = NIL;


/*
 * State information for ALTER TABLE
 *
 * The pending-work queue for an ALTER TABLE is a List of AlteredTableInfo
 * structs, one for each table modified by the operation (the named table
 * plus any child tables that are affected).  We save lists of subcommands
 * to apply to this table (possibly modified by parse transformation steps);
 * these lists will be executed in Phase 2.  If a Phase 3 step is needed,
 * necessary information is stored in the constraints and newvals lists.
 *
 * Phase 2 is divided into multiple passes; subcommands are executed in
 * a pass determined by subcommand type.
 */

#define AT_PASS_DROP			0		/* DROP (all flavors) */
#define AT_PASS_ALTER_TYPE		1		/* ALTER COLUMN TYPE */
#define AT_PASS_OLD_INDEX		2		/* re-add existing indexes */
#define AT_PASS_OLD_CONSTR		3		/* re-add existing constraints */
#define AT_PASS_COL_ATTRS		4		/* set other column attributes */
/* We could support a RENAME COLUMN pass here, but not currently used */
#define AT_PASS_ADD_COL			5		/* ADD COLUMN */
#define AT_PASS_ADD_INDEX		6		/* ADD indexes */
#define AT_PASS_ADD_CONSTR		7		/* ADD constraints, defaults */
#define AT_PASS_MISC			8		/* other stuff */
#define AT_NUM_PASSES			9

typedef struct AlteredTableInfo
{
	/* Information saved before any work commences: */
	Oid			relid;			/* Relation to work on */
	char		relkind;		/* Its relkind */
	TupleDesc	oldDesc;		/* Pre-modification tuple descriptor */
	/* Information saved by Phase 1 for Phase 2: */
	List	   *subcmds[AT_NUM_PASSES]; /* Lists of AlterTableCmd */
	/* Information saved by Phases 1/2 for Phase 3: */
	List	   *constraints;	/* List of NewConstraint */
	List	   *newvals;		/* List of NewColumnValue */
	bool		new_notnull;	/* T if we added new NOT NULL constraints */
	bool		new_dropoids;	/* T if we dropped the OID column */
	Oid			newTableSpace;	/* new tablespace; 0 means no change */
	Oid			exchange_relid;	/* for EXCHANGE, the exchanged in rel */
	/* Objects to rebuild after completing ALTER TYPE operations */
	List	   *changedConstraintOids;	/* OIDs of constraints to rebuild */
	List	   *changedConstraintDefs;	/* string definitions of same */
	List	   *changedIndexOids;		/* OIDs of indexes to rebuild */
	List	   *changedIndexDefs;		/* string definitions of same */
} AlteredTableInfo;

/*
 * Struct describing one new column value that needs to be computed during
 * Phase 3 copy (this could be either a new column with a non-null default, or
 * a column that we're changing the type of).  Columns without such an entry
 * are just copied from the old table during ATRewriteTable.  Note that the
 * expr is an expression over *old* table values.
 */
typedef struct NewColumnValue
{
	AttrNumber	attnum;			/* which column */
	Expr	   *expr;			/* expression to compute */
	ExprState  *exprstate;		/* execution state */
} NewColumnValue;


static void truncate_check_rel(Relation rel);
static void MergeConstraintsIntoExisting(Relation child_rel, Relation parent_rel);
static void MergeAttributesIntoExisting(Relation child_rel, Relation parent_rel, List *inhAttrNameList,
										bool is_partition);
static bool add_nonduplicate_cooked_constraint(Constraint *cdef, List *stmtConstraints);

static void StoreCatalogInheritance(Oid relationId, List *supers);
static void StoreCatalogInheritance1(Oid relationId, Oid parentOid,
						 int16 seqNumber, Relation inhRelation,
						 bool is_partition);
static int	findAttrByName(const char *attributeName, List *schema);
static void setRelhassubclassInRelation(Oid relationId, bool relhassubclass);
static void AlterIndexNamespaces(Relation classRel, Relation rel,
					 Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved);
static void AlterSeqNamespaces(Relation classRel, Relation rel,
				   Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved,
				   LOCKMODE lockmode);
static int transformColumnNameList(Oid relId, List *colList,
						int16 *attnums, Oid *atttypids);
static int transformFkeyGetPrimaryKey(Relation pkrel, Oid *indexOid,
						   List **attnamelist,
						   int16 *attnums, Oid *atttypids,
						   Oid *opclasses);
static void validateForeignKeyConstraint(FkConstraint *fkconstraint,
							 Relation rel, Relation pkrel, Oid constraintOid);
static void createForeignKeyTriggers(Relation rel, FkConstraint *fkconstraint,
						 Oid constraintOid);
static void ATController(Relation rel, List *cmds, bool recurse);
static void ATPrepCmd(List **wqueue, Relation rel, AlterTableCmd *cmd,
		  bool recurse, bool recursing);
static void ATRewriteCatalogs(List **wqueue);
static void ATAddToastIfNeeded(List **wqueue);
static void ATExecCmd(List **wqueue, AlteredTableInfo *tab, Relation *rel,
		  AlterTableCmd *cmd);
static void ATRewriteTables(List **wqueue);
static void ATRewriteTable(AlteredTableInfo *tab, Oid OIDNewHeap);
static void ATAocsWriteNewColumns(
		AOCSAddColumnDesc idesc, AOCSHeaderScanDesc sdesc,
		AlteredTableInfo *tab, ExprContext *econtext, TupleTableSlot *slot);
static bool ATAocsNoRewrite(AlteredTableInfo *tab);
static AlteredTableInfo *ATGetQueueEntry(List **wqueue, Relation rel);
static void ATSimplePermissions(Relation rel, bool allowView);
static void ATSimplePermissionsRelationOrIndex(Relation rel);
static void ATSimpleRecursion(List **wqueue, Relation rel,
				  AlterTableCmd *cmd, bool recurse);
#if 0
static void ATOneLevelRecursion(List **wqueue, Relation rel,
					AlterTableCmd *cmd);
#endif
static void ATPrepAddColumn(List **wqueue, Relation rel, bool recurse,
				AlterTableCmd *cmd);
static void ATExecAddColumn(AlteredTableInfo *tab, Relation rel,
				ColumnDef *colDef);
static void add_column_datatype_dependency(Oid relid, int32 attnum, Oid typid);
static void ATExecDropNotNull(Relation rel, const char *colName);
static void ATExecSetNotNull(AlteredTableInfo *tab, Relation rel,
				 const char *colName);
static void ATPrepColumnDefault(Relation rel, bool recurse, AlterTableCmd *cmd);
static void ATExecColumnDefault(Relation rel, const char *colName,
					ColumnDef *newDefault);
static void ATPrepSetStatistics(Relation rel, const char *colName,
					Node *flagValue);
static void ATExecSetStatistics(Relation rel, const char *colName,
					Node *newValue);
static void ATExecSetStorage(Relation rel, const char *colName,
				 Node *newValue);
static void ATExecDropColumn(List **wqueue, Relation rel, const char *colName,
				 DropBehavior behavior,
				 bool recurse, bool recursing);
static void ATExecAddIndex(AlteredTableInfo *tab, Relation rel,
			   IndexStmt *stmt, bool is_rebuild);
static void ATExecAddConstraint(AlteredTableInfo *tab, Relation rel,
					Node *newConstraint, bool recurse);
static void ATAddCheckConstraint(AlteredTableInfo *tab, Relation rel,
					 Constraint *constr, bool recurse);

static void ATAddForeignKeyConstraint(AlteredTableInfo *tab, Relation rel,
						  FkConstraint *fkconstraint);
static void ATPrepDropConstraint(List **wqueue, Relation rel,
					 bool recurse, AlterTableCmd *cmd);
static void ATExecDropConstraint(Relation rel, const char *constrName,
					 DropBehavior behavior, bool quiet);
static void ATPrepAlterColumnType(List **wqueue,
					  AlteredTableInfo *tab, Relation rel,
					  bool recurse, bool recursing,
					  AlterTableCmd *cmd);
static void ATExecAlterColumnType(AlteredTableInfo *tab, Relation rel,
					  const char *colName, TypeName *typename);
static void ATPostAlterTypeCleanup(List **wqueue, AlteredTableInfo *tab);
static void ATPostAlterTypeParse(char *cmd, List **wqueue);
static void change_owner_recurse_to_sequences(Oid relationOid,
								  Oid newOwnerId);
static void ATExecClusterOn(Relation rel, const char *indexName);
static void ATExecDropCluster(Relation rel);
static void ATPrepSetTableSpace(AlteredTableInfo *tab, Relation rel,
					char *tablespacename);
static void ATPartsPrepSetTableSpace(List **wqueue, Relation rel, AlterTableCmd *cmd, 
									 List *oids);
static void ATExecSetTableSpace(Oid tableOid, Oid newTableSpace);
static void ATExecSetRelOptions(Relation rel, List *defList, bool isReset);
static void ATExecEnableDisableTrigger(Relation rel, char *trigname,
						   char fires_when, bool skip_system);
static void ATExecEnableDisableRule(Relation rel, char *rulename,
						char fires_when);
static void ATExecAddInherit(Relation rel, Node *node);
static void ATExecDropInherit(Relation rel, RangeVar *parent, bool is_partition);
static void ATExecSetDistributedBy(Relation rel, Node *node,
								   AlterTableCmd *cmd);
static void ATPrepExchange(Relation rel, AlterPartitionCmd *pc);

const char* synthetic_sql = "(internally generated SQL command)";

/* ALTER TABLE ... PARTITION */


static void ATPExecPartAdd(AlteredTableInfo *tab,
						   Relation rel,
                           AlterPartitionCmd *pc,
						   AlterTableType att);				/* Add */
static void ATPExecPartAlter(List **wqueue, AlteredTableInfo *tab, 
							 Relation rel,
                             AlterPartitionCmd *pc);		/* Alter */
static void ATPExecPartCoalesce(Relation rel,
                                AlterPartitionCmd *pc);		/* Coalesce */
static void ATPExecPartDrop(Relation rel,
                            AlterPartitionCmd *pc);			/* Drop */
static void ATPExecPartExchange(AlteredTableInfo *tab,
								Relation rel,
                                AlterPartitionCmd *pc);		/* Exchange */
static void ATPExecPartMerge(Relation rel,
                             AlterPartitionCmd *pc);		/* Merge */
static void ATPExecPartModify(Relation rel,
                              AlterPartitionCmd *pc);		/* Modify */
static void ATPExecPartRename(Relation rel,
                              AlterPartitionCmd *pc);		/* Rename */

static void ATPExecPartSetTemplate(AlteredTableInfo *tab,   /* Set */
								   Relation rel,            /* Subpartition */
                                   AlterPartitionCmd *pc);	/* Template */
static void ATPExecPartSplit(Relation *relp,
                             AlterPartitionCmd *pc);		/* Split */
static List *
atpxTruncateList(Relation rel, PartitionNode *pNode);
static void ATPExecPartTruncate(Relation rel,
                                AlterPartitionCmd *pc);		/* Truncate */
static void
copy_buffer_pool_data(
	Relation 	rel,

	SMgrRelation dst,

	ItemPointer persistentTid,

	int64 		persistentSerialNum,

	bool		useWal);

static bool TypeTupleExists(Oid typeId);

static void ATExecPartAddInternal(Relation rel, Node *def);

static RangeVar *make_temp_table_name(Relation rel, BackendId id);
static bool prebuild_temp_table(Relation rel, RangeVar *tmpname, List *distro,
								List *opts, bool isTmpTableAo,
								bool useExistingColumnAttributes);
static void ATPartitionCheck(AlterTableType subtype, Relation rel, bool rejectroot, bool recursing);
static void ATExternalPartitionCheck(AlterTableType subtype, Relation rel, bool recursing);

static char *alterTableCmdString(AlterTableType subtype);

static void change_dropped_col_datatypes(Relation rel);

/* ----------------------------------------------------------------
 *		DefineRelation
 *				Creates a new relation.
 *
 * If successful, returns the OID of the new relation.
 *
 * If 'dispatch' is true (and we are running in QD), the statement is
 * also dispatched to the QE nodes. Otherwise it is the caller's
 * responsibility to dispatch.
 * ----------------------------------------------------------------
 */
Oid
DefineRelation(CreateStmt *stmt, char relkind, char relstorage, bool dispatch)
{
	char		relname[NAMEDATALEN];
	Oid			namespaceId;
	Oid			relationId = InvalidOid;
	Oid			tablespaceId;
	Relation	rel;
	TupleDesc	descriptor;
	List	   *old_constraints;
	bool		localHasOids;
	List	   *colDefaults;
	Datum		reloptions;
	ListCell   *listptr;
	AttrNumber	attnum;
	bool		isPartitioned;

	ItemPointerData	persistentTid;
	int64			persistentSerialNum;

	bool		shouldDispatch = dispatch &&
								 Gp_role == GP_ROLE_DISPATCH &&
                                 IsNormalProcessingMode();

	/*
	 * Truncate relname to appropriate length (probably a waste of time, as
	 * parser should have done this already).
	 */
	StrNCpy(relname, stmt->relation->relname, NAMEDATALEN);

	/*
	 * Check consistency of arguments
	 */
	if (stmt->oncommit != ONCOMMIT_NOOP && !stmt->relation->istemp)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("ON COMMIT can only be used on temporary tables")));

	/*
	 * Security check: disallow creating temp tables from security-restricted
	 * code.  This is needed because calling code might not expect untrusted
	 * tables to appear in pg_temp at the front of its search path.
	 */
	if (stmt->relation->istemp && InSecurityRestrictedOperation())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("cannot create temporary table within security-restricted operation")));

	/*
	 * Look up the namespace in which we are supposed to create the relation.
	 * Check we have permission to create there. Skip check if bootstrapping,
	 * since permissions machinery may not be working yet.
	 */
	namespaceId = RangeVarGetCreationNamespace(stmt->relation);

	if (!IsBootstrapProcessingMode())
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
	 * Note: This code duplicates code in indexcmds.c
	 */
	if (relkind == RELKIND_SEQUENCE || 
		relkind == RELKIND_VIEW ||
		relkind == RELKIND_COMPOSITE_TYPE ||
		(relkind == RELKIND_RELATION && (
			relstorage == RELSTORAGE_EXTERNAL ||
			relstorage == RELSTORAGE_FOREIGN  ||
			relstorage == RELSTORAGE_VIRTUAL)))
	{
		/* 
		 * MPP-8262: unable to create sequence with default_tablespace
		 *
		 * These relkinds have no storage, and thus do not support tablespaces.
		 * We shouldn't go through the regular default case for these because we
		 * don't want to pick up the value from the default_tablespace guc.
		 */
		Assert(!stmt->tablespacename);
		tablespaceId = InvalidOid;
	}
	else if (stmt->tablespacename)
	{
		/*
		 * Tablespace specified on the command line, or was passed down by
		 * dispatch.
		 */
		tablespaceId = get_tablespace_oid(stmt->tablespacename, false);
	}
	else
	{
		/*
		 * Get the default tablespace specified via default_tablespace, or fall
		 * back on the database tablespace.
		 */
		tablespaceId = GetDefaultTablespace(stmt->relation->istemp);

		/* Need the real tablespace id for dispatch */
		if (!OidIsValid(tablespaceId)) 
			tablespaceId = MyDatabaseTableSpace;

		/* 
		 * MPP-8238 : inconsistent tablespaces between segments and master 
		 */
		if (shouldDispatch)
			stmt->tablespacename = get_tablespace_name(tablespaceId);
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
	 * Parse and validate reloptions, if any.
	 */
	reloptions = transformRelOptions((Datum) 0, stmt->options, true, false);

	/*
	 * Look up inheritance ancestors and generate relation schema, including
	 * inherited attributes. Update the offsets of the distribution attributes
	 * in GpPolicy if necessary. Save result to stmt->tableElts and dispatch to
	 * QEs.
	 */
	isPartitioned = stmt->partitionBy ? true : false;
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
		stmt->tableElts = MergeAttributes(stmt->tableElts, stmt->inhRelations,
							 	 	 	  stmt->relation->istemp, isPartitioned,
							 	 	 	  &stmt->inhOids, &old_constraints,
							 	 	 	  &stmt->parentOidCount, stmt->policy);

	/*
	 * Create a relation descriptor from the relation schema and create the
	 * relation.
	 *
	 * Inherited (pre-cooked) defaults and constraints will not be
	 * included into the new relation in heap_create_with_catalog. Instead, we
	 * add them along with raw defaults and constraints.
	 */
	descriptor = BuildDescForRelation(stmt->tableElts);

	localHasOids = interpretOidsOption(stmt->options);
	descriptor->tdhasoid = (localHasOids || stmt->parentOidCount > 0);

	/*
	 * old_constraints: pre-cooked constraints from CREATE TABLE ... INHERIT ...
	 * stmt->constraints: might have some pre-cooked constraints passed by parse_utilcmd.c,
	 * 		due to LIKE tab INCLUDING CONSTRAINTS
	 */
	if ((Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
			&& old_constraints)
	{
		/* deal with constraints from MergeAttributes */
		foreach(listptr, old_constraints)
		{
			Constraint *cdef = (Constraint *) lfirst(listptr);
			if (cdef->contype == CONSTR_CHECK &&
					add_nonduplicate_cooked_constraint(cdef, stmt->constraints))
				stmt->constraints = lappend(stmt->constraints, cdef);
		}
	}


	if (shouldDispatch)
	{
		Relation	pg_class_desc;
		Relation	pg_type_desc;
		MemoryContext oldContext;

		/*
		 * We use a RowExclusiveLock but hold it till end of transaction so that two
		 * DDL operations will not deadlock between QEs
		 */
		pg_class_desc = heap_open(RelationRelationId, RowExclusiveLock);
		pg_type_desc = heap_open(TypeRelationId, RowExclusiveLock);

		LockRelationOid(DependRelationId, RowExclusiveLock);

		cdb_sync_oid_to_segments();

		heap_close(pg_class_desc, NoLock);  /* gonna update, so don't unlock */

		stmt->relKind = relkind;
		stmt->relStorage = relstorage;

		if (!OidIsValid(stmt->ownerid))
			stmt->ownerid = GetUserId();

		oldContext = MemoryContextSwitchTo(CacheMemoryContext);
		stmt->relation->schemaname = get_namespace_name(namespaceId);
		MemoryContextSwitchTo(oldContext);

		heap_close(pg_type_desc, NoLock);
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		Assert(stmt->ownerid != InvalidOid);
	}
	else
	{
		if (!OidIsValid(stmt->ownerid))
			stmt->ownerid = GetUserId();
	}

	/* MPP-8405: disallow OIDS on partitioned tables */
	if ((stmt->partitionBy ||
		 stmt->is_part_child) &&
		descriptor->tdhasoid && 
		IsNormalProcessingMode() &&
        (Gp_role == GP_ROLE_DISPATCH))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg(
							 "OIDS=TRUE is not allowed on partitioned tables. "
							 "Use OIDS=FALSE"
							 )));

	bool valid_opts = (relstorage == RELSTORAGE_EXTERNAL);

	relationId = heap_create_with_catalog(relname,
										  namespaceId,
										  tablespaceId,
										  InvalidOid,
										  stmt->ownerid,
										  descriptor,
										  /* relam */ InvalidOid,
										  relkind,
										  relstorage,
										  tablespaceId==GLOBALTABLESPACE_OID,
										  localHasOids,
										  /* bufferPoolBulkLoad */ false,
										  stmt->parentOidCount,
										  stmt->oncommit,
                                          stmt->policy,  /*CDB*/
                                          reloptions,
										  allowSystemTableModsDDL,
										  valid_opts,
										  &persistentTid,
										  &persistentSerialNum);

	StoreCatalogInheritance(relationId, stmt->inhOids);

	/*
	 * We must bump the command counter to make the newly-created relation
	 * tuple visible for opening.
	 */
	CommandCounterIncrement();

	/*
	 * Open the new relation and acquire exclusive lock on it.  This isn't
	 * really necessary for locking out other backends (since they can't see
	 * the new rel anyway until we commit), but it keeps the lock manager from
	 * complaining about deadlock risks.
	 *
	 * GPDB: Don't lock it if we're creating a partition, however. Creating a
	 * heavily-partitioned table would otherwise acquire a lot of locks.
	 */
	if (stmt->is_part_child)
		rel = relation_open(relationId, NoLock);
	else
		rel = relation_open(relationId, AccessExclusiveLock);

	/*
	 * For some reason, even though we have bumped the command counter above, 
	 * occasionally we are not able to see the persistent info just stored in gp_relation_node
	 * at the XLOG level.  So, we save the values here for debugging purposes.
	 */
	rel->rd_haveCreateDebugInfo = true;
	rel->rd_createDebugIsZeroTid = PersistentStore_IsZeroTid(&persistentTid);
	rel->rd_createDebugPersistentTid = persistentTid;
	rel->rd_createDebugPersistentSerialNum = persistentSerialNum;

	/*
	 * Now add any newly specified column default values and CHECK constraints
	 * to the new relation.  These are passed to us in the form of raw or cooked
	 * parsetrees; we need to transform them if they are raw to executable expression
	 * trees before they can be added. The most convenient way to do that is to
	 * apply the parser's transformExpr routine, but transformExpr doesn't
	 * work unless we have a pre-existing relation. So, the transformation has
	 * to be postponed to this final step of CREATE TABLE.
	 *
	 * First, scan schema to find new column defaults.
	 */
	colDefaults = NIL;
	attnum = 0;

	foreach(listptr, stmt->tableElts)
	{
		ColumnDef  *colDef = lfirst(listptr);

		colDef->attnum = ++attnum;
		if (colDef->raw_default != NULL || colDef->cooked_default != NULL)
			colDefaults = lappend(colDefaults, colDef);
	}

	/*
	 * Parse and add the defaults/constraints, if any.
	 */
	if (colDefaults || stmt->constraints)
		AddRelationConstraints(rel, colDefaults, stmt->constraints);

	if (stmt->attr_encodings)
		AddRelationAttributeEncodings(rel, stmt->attr_encodings);

	/* It is now safe to dispatch */
	if (shouldDispatch)
	{
		/*
		 * Dispatch the statement tree to all primary and mirror segdbs.
		 * Doesn't wait for the QEs to finish execution.
		 *
		 * The OIDs are carried out-of-band.
		 */
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR |
									DF_NEED_TWO_PHASE |
									DF_WITH_SNAPSHOT,
									GetAssignedOidsForDispatch(),
									NULL);
	}

	/*
	 * Clean up.  We keep lock on new relation (although it shouldn't be
	 * visible to anyone else anyway, until commit).
	 */
	relation_close(rel, NoLock);

	return relationId;
}

/* ----------------------------------------------------------------
*		DefinePartitionedRelation
*				Create the rewrite rule for a partitioned table
*
* parse_partition.c/transformPartitionBy does the bulk of the work for
* partitioned tables, converting a single CREATE TABLE into a series
* of statements to create the child tables for each partition.  Each
* child table has a check constraint and a rewrite rule to ensure that
* INSERTs to the parent end up in the correct child table (partition).
* However, we cannot add a RuleStmt for a non-existent table to the
* a(fter)list for the Create statement (believe me, I tried, really
* hard).  Thus, we create the "parsed" RuleStmt in analyze, and
* finally parse_analyze it here *after* the relation is created, the
* use process_utility to dispatch.
* ----------------------------------------------------------------
*/
void
DefinePartitionedRelation(CreateStmt *stmt, Oid relOid)
{

	if (stmt->postCreate)
	{
		Query	   *pUtl;
		DestReceiver *dest = None_Receiver;
		List *pL1 = (List *)stmt->postCreate;

		pUtl = parse_analyze(linitial(pL1), NULL, NULL, 0);

		Assert(((Query *)pUtl)->commandType == CMD_UTILITY);

		ProcessUtility((Node *)(((Query *)pUtl)->utilityStmt),
					   synthetic_sql,
					   NULL,
					   false, /* not top level */
					   dest,
					   NULL);
	}
}

/*
 * Run deferred statements generated by internal operations around
 * partition addition/split.
 */
void
EvaluateDeferredStatements(List *deferredStmts)
{
	ListCell	   *lc;

	/***
	 *** XXX: Fix MPP-13750, however this fails to address the similar bug
	 ***      with ordinary inheritance and partial indexes.  When that bug,
	 ***      is fixed, this section should become unnecessary!
	 ***/
	
	foreach( lc, deferredStmts )
	{
		Query	   *uquery;
		DestReceiver *dest = None_Receiver;

		Node *dstmt = lfirst(lc);

		uquery = parse_analyze(dstmt, NULL, NULL, 0);
		Insist(uquery->commandType == CMD_UTILITY);

		ereport(DEBUG1,
				(errmsg("processing deferred utility statement")));

		ProcessUtility((Node*)uquery->utilityStmt,
					   synthetic_sql,
					   NULL,
					   false,
					   dest,
					   NULL);
	}
}

/* Don't track internal namespaces for toast, bitmap, aoseg */
#define METATRACK_VALIDNAMESPACE(namespaceId) \
	(namespaceId != PG_TOAST_NAMESPACE &&	\
	 namespaceId != PG_BITMAPINDEX_NAMESPACE && \
	 namespaceId != PG_AOSEGMENT_NAMESPACE )

/* check for valid namespace and valid relkind */
static 
bool
MetaTrackValidKindNsp(Form_pg_class rd_rel)
{
	Oid nsp = rd_rel->relnamespace;

	if (PG_CATALOG_NAMESPACE == nsp)
	{
		/* MPP-7773: don't track objects in system namespace
		 * if modifying system tables (eg during upgrade)  
		 */
		if (allowSystemTableModsDDL)
			return (false);
	}

	/* MPP-7599: watch out for toast indexes */
	return (METATRACK_VALIDNAMESPACE(nsp)
			&& MetaTrackValidRelkind(rd_rel->relkind)
			/* MPP-7572: not valid if in any temporary namespace */
			&& (!(isAnyTempNamespace(nsp))));
}

/*
 * RemoveRelation
 *		Deletes a relation.
 */
bool
RemoveRelation(const RangeVar *relation, DropBehavior behavior,
			   DropStmt *stmt, char relkind)
{
	Oid			relOid;
	ObjectAddress object;

	AcceptInvalidationMessages();

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		LockRelationOid(RelationRelationId, RowExclusiveLock);
		LockRelationOid(TypeRelationId, RowExclusiveLock);
		LockRelationOid(DependRelationId, RowExclusiveLock);
	}

	/*
	 * Perform name lookup again if we had to wait to acquire lock on
	 * OID of the relation.  The relation's name could have been
	 * altered while we were waiting.
	 */
	relOid = RangeVarGetRelidExtended(
			relation, AccessExclusiveLock, stmt?stmt->missing_ok:false,
			false /* nowait */, NULL /* callback */,
			NULL /* callback_arg */);

	if (!OidIsValid(relOid))
	{
		/*
		 * Missed to find the object to be dropped.
		 * Drop with "if exists" just notify the same, unlock as not performing
		 * any operation and return back to convey didn't drop the relation.
		 * Drop without "if exists" won't even come here, as would error
		 * inside RangeVarGetRelidExtended.
		 */
		DropErrorMsgNonExistent(relation, relkind, stmt?stmt->missing_ok:false);
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			UnlockRelationOid(DependRelationId, RowExclusiveLock);
			UnlockRelationOid(TypeRelationId, RowExclusiveLock);
			UnlockRelationOid(RelationRelationId, RowExclusiveLock);
		}
		return false;
	}

	/* MPP-3260: disallow direct DROP TABLE of a partition */
	if (stmt && rel_is_child_partition(relOid) && !stmt->bAllowPartn)
	{
		Oid		 master = rel_partition_get_master(relOid);
		char	*pretty	= rel_get_part_path_pretty(relOid,
												   " ALTER PARTITION ",
												   " DROP PARTITION ");

		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				errmsg("cannot drop partition \"%s\" directly",
					   get_rel_name(relOid)),
				errhint("Table \"%s\" is a child partition of table "
						"\"%s\".  To drop it, use ALTER TABLE \"%s\"%s...",
						get_rel_name(relOid), get_rel_name(master),
						get_rel_name(master), pretty ? pretty : "" )));

	}

	object.classId = RelationRelationId;
	object.objectId = relOid;
	object.objectSubId = 0;

	/* if we got here then we should proceed. */
	performDeletion(&object, behavior);
	return true;
}

/*
 * RelationToRemoveIsTemp
 *		Checks if an object being targeted for drop is a temporary table.
 */
bool
RelationToRemoveIsTemp(const RangeVar *relation, DropBehavior behavior)
{
	Oid			relOid;
	Oid			recheckoid;
	ObjectAddress object;
	HeapTuple	relTup;
	Form_pg_class relForm;
	char	   *nspname;
	char	   *relname;
	bool		isTemp;

	elog(DEBUG5, "Relation to remove catalogname %s, schemaname %s, relname %s",
		 (relation->catalogname == NULL ? "<empty>" : relation->catalogname),
		 (relation->schemaname == NULL ? "<empty>" : relation->schemaname),
		 (relation->relname == NULL ? "<empty>" : relation->relname));

	// UNDONE: Not sure how to interpret 'behavior'...

	relOid = RangeVarGetRelid(relation, false);

	object.classId = RelationRelationId;
	object.objectId = relOid;
	object.objectSubId = 0;

	/*
	 * Lock down the object to stablize it before we examine its
	 * charactistics.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		LockRelationOid(RelationRelationId, RowExclusiveLock);
		LockRelationOid(TypeRelationId, RowExclusiveLock);
		LockRelationOid(DependRelationId, RowExclusiveLock);
	}

	/* Lock the relation to be dropped */
	LockRelationOid(relOid, AccessExclusiveLock);

	/*
	 * When we got the relOid lock, it is possible that the relation has gone away.
	 * this will throw Error if the relation is already deleted.
	 */
	recheckoid = RangeVarGetRelid(relation, false);

	/* if we got here then we should proceed. */

	relTup = SearchSysCache1(RELOID,
							 ObjectIdGetDatum(relOid));
	if (!HeapTupleIsValid(relTup))
		elog(ERROR, "cache lookup failed for relation %u", relOid);
	relForm = (Form_pg_class) GETSTRUCT(relTup);

	/* Qualify the name if not visible in search path */
	if (RelationIsVisible(relOid))
		nspname = NULL;
	else
		nspname = get_namespace_name(relForm->relnamespace);

	/* XXX XXX: is this all just for debugging?  could just be simplified to:
	   SELECT relnamespace from pg_class 
	*/

	relname = quote_qualified_identifier(nspname, NameStr(relForm->relname));

	isTemp = isTempNamespace(relForm->relnamespace);

	elog(DEBUG5, "Relation name is %s, namespace %s, isTemp = %s",
	     relname,
	     (nspname == NULL ? "<null>" : nspname),
	     (isTemp ? "true" : "false"));

	ReleaseSysCache(relTup);

	return isTemp;
}

#ifdef USE_ASSERT_CHECKING
static bool
CheckExclusiveAccess(Relation rel)
{
	if (LockRelationNoWait(rel, AccessExclusiveLock) !=
		LOCKACQUIRE_ALREADY_HELD)
	{
		UnlockRelation(rel, AccessExclusiveLock);
		return false;
	}
	return true;
}
#endif

/*
 * Allocate new relfiles for the specified relation and schedule old
 * relfile for deletion.  Caller must hold AccessExclusiveLock on the
 * relation.
 */
void
TruncateRelfiles(Relation rel)
{
	Oid			heap_relid;
	Oid			toast_relid;
	Oid			aoseg_relid = InvalidOid;
	Oid			aoblkdir_relid = InvalidOid;
	Oid			aovisimap_relid = InvalidOid;

	Assert(CheckExclusiveAccess(rel));

	/*
	 * Create a new empty storage file for the relation, and assign it as
	 * the relfilenode value.	The old storage file is scheduled for
	 * deletion at commit.
	 */
	setNewRelfilenode(rel, RecentXmin);

	heap_relid = RelationGetRelid(rel);
	toast_relid = rel->rd_rel->reltoastrelid;

	if (RelationIsAoRows(rel) ||
		RelationIsAoCols(rel))
		GetAppendOnlyEntryAuxOids(heap_relid, SnapshotNow,
								  &aoseg_relid,
								  &aoblkdir_relid, NULL,
								  &aovisimap_relid, NULL);

	/*
	 * The same for the toast table, if any.
	 */
	if (OidIsValid(toast_relid))
	{
		rel = relation_open(toast_relid, AccessExclusiveLock);
		setNewRelfilenode(rel, RecentXmin);
		heap_close(rel, NoLock);
	}

	/*
	 * The same for the aoseg table, if any.
	 */
	if (OidIsValid(aoseg_relid))
	{
		rel = relation_open(aoseg_relid, AccessExclusiveLock);
		setNewRelfilenode(rel, RecentXmin);
		heap_close(rel, NoLock);
	}

	if (OidIsValid(aoblkdir_relid))
	{
		rel = relation_open(aoblkdir_relid, AccessExclusiveLock);
		setNewRelfilenode(rel, RecentXmin);
		heap_close(rel, NoLock);
	}

	if (OidIsValid(aovisimap_relid))
	{
		rel = relation_open(aovisimap_relid, AccessExclusiveLock);
		setNewRelfilenode(rel, RecentXmin);
		heap_close(rel, NoLock);
	}
}

/*
 * ExecuteTruncate
 *		Executes a TRUNCATE command.
 *
 * This is a multi-relation truncate.  We first open and grab exclusive
 * lock on all relations involved, checking permissions and otherwise
 * verifying that the relation is OK for truncation.  In CASCADE mode,
 * relations having FK references to the targeted relations are automatically
 * added to the group; in RESTRICT mode, we check that all FK references are
 * internal to the group that's being truncated.  Finally all the relations
 * are truncated and reindexed.
 */
void
ExecuteTruncate(TruncateStmt *stmt)
{
	List	   *rels = NIL;
	List	   *relids = NIL;
	List	   *meta_relids = NIL;
	EState	   *estate;
	ResultRelInfo *resultRelInfos;
	ResultRelInfo *resultRelInfo;
	ListCell   *cell;
    int partcheck = 2;
	List *partList = NIL;

	/*
	 * Open, exclusive-lock, and check all the explicitly-specified relations
	 *
	 * Check if table has partitions and add them too
	 */
	while (partcheck)
	{
		foreach(cell, stmt->relations)
		{
			RangeVar   *rv = lfirst(cell);
			Relation	rel;
			PartitionNode *pNode;

			rel = heap_openrv(rv, AccessExclusiveLock);
			
			truncate_check_rel(rel);

			if (partcheck == 2)
			{
				pNode = RelationBuildPartitionDesc(rel, false);

				if (pNode)
				{
					List *plist = atpxTruncateList(rel, pNode);

					if (plist)
					{
						if (partList)
							partList = list_concat(partList, plist);
						else
							partList = plist;
					}
				}
			}
			heap_close(rel, NoLock);
		}

		partcheck--;

		if (partList)
		{
			/* add the partitions to the relation list and try again */
			if (partcheck == 1)
			{
				stmt->relations = list_concat(partList, stmt->relations);

				cell = list_head(stmt->relations);
				while (cell != NULL)
				{
					RangeVar   *rv = lfirst(cell);
					Relation	rel;

					cell = lnext(cell);
					rel = heap_openrv(rv, AccessExclusiveLock);
					if (RelationIsExternal(rel))
						ereport(ERROR,
								(errcode(ERRCODE_WRONG_OBJECT_TYPE),
								 errmsg("cannot truncate table having external partition: \"%s\"",
									    RelationGetRelationName(rel))));

					heap_close(rel, NoLock);
				}
			}
		}
		else
			/* no partitions - no need to try again */
			partcheck = 0;
	} /* end while partcheck */

	/*
	 * Open, exclusive-lock, and check all the explicitly-specified relations
	 */
	foreach(cell, stmt->relations)
	{
		RangeVar   *rv = lfirst(cell);
		Relation	rel;

		rel = heap_openrv(rv, AccessExclusiveLock);
		truncate_check_rel(rel);
		rels = lappend(rels, rel);
		relids = lappend_oid(relids, RelationGetRelid(rel));

		if (MetaTrackValidKindNsp(rel->rd_rel))
			meta_relids = lappend_oid(meta_relids, RelationGetRelid(rel));
	}

	/*
	 * In CASCADE mode, suck in all referencing relations as well.	This
	 * requires multiple iterations to find indirectly-dependent relations. At
	 * each phase, we need to exclusive-lock new rels before looking for their
	 * dependencies, else we might miss something.	Also, we check each rel as
	 * soon as we open it, to avoid a faux pas such as holding lock for a long
	 * time on a rel we have no permissions for.
	 */
	if (stmt->behavior == DROP_CASCADE)
	{
		for (;;)
		{
			List	   *newrelids;

			newrelids = heap_truncate_find_FKs(relids);
			if (newrelids == NIL)
				break;			/* nothing else to add */

			foreach(cell, newrelids)
			{
				Oid			relid = lfirst_oid(cell);
				Relation	rel;

				rel = heap_open(relid, AccessExclusiveLock);
				ereport(NOTICE,
						(errmsg("truncate cascades to table \"%s\"",
								RelationGetRelationName(rel))));
				truncate_check_rel(rel);
				rels = lappend(rels, rel);
				relids = lappend_oid(relids, relid);

				if (MetaTrackValidKindNsp(rel->rd_rel))
					meta_relids = lappend_oid(meta_relids, 
											  RelationGetRelid(rel));
			}
		}
	}

	/*
	 * Check foreign key references.  In CASCADE mode, this should be
	 * unnecessary since we just pulled in all the references; but as a
	 * cross-check, do it anyway if in an Assert-enabled build.
	 */
#ifdef USE_ASSERT_CHECKING
	heap_truncate_check_FKs(rels, false);
#else
	if (stmt->behavior == DROP_RESTRICT)
		heap_truncate_check_FKs(rels, false);
#endif

	/* Prepare to catch AFTER triggers. */
	AfterTriggerBeginQuery();

	/*
	 * To fire triggers, we'll need an EState as well as a ResultRelInfo
	 * for each relation.
	 */
	estate = CreateExecutorState();
	resultRelInfos = (ResultRelInfo *)
		palloc(list_length(rels) * sizeof(ResultRelInfo));
	resultRelInfo = resultRelInfos;
	foreach(cell, rels)
	{
		Relation	rel = (Relation) lfirst(cell);

		InitResultRelInfo(resultRelInfo,
						  rel,
						  0,			/* dummy rangetable index */
						  CMD_DELETE,	/* don't need any index info */
						  false);
		resultRelInfo++;
	}
	estate->es_result_relations = resultRelInfos;
	estate->es_num_result_relations = list_length(rels);

	/*
	 * Process all BEFORE STATEMENT TRUNCATE triggers before we begin
	 * truncating (this is because one of them might throw an error).
	 * Also, if we were to allow them to prevent statement execution,
	 * that would need to be handled here.
	 */
	resultRelInfo = resultRelInfos;
	foreach(cell, rels)
	{
		estate->es_result_relation_info = resultRelInfo;
		ExecBSTruncateTriggers(estate, resultRelInfo);
		resultRelInfo++;
	}

	/*
	 * OK, truncate each table.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
		cdb_sync_oid_to_segments();

	foreach(cell, rels)
	{
		Relation	rel = (Relation) lfirst(cell);

		TruncateRelfiles(rel);

		/*
		 * Reconstruct the indexes to match, and we're done.
		 */
		reindex_relation(RelationGetRelid(rel), true);
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		ListCell	*lc;

		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR |
									DF_WITH_SNAPSHOT |
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);

		/* MPP-6929: metadata tracking */
		foreach(lc, meta_relids)
		{
			Oid			a_relid = lfirst_oid(lc);

			MetaTrackUpdObject(RelationRelationId,
							   a_relid,
							   GetUserId(),
							   "VACUUM", "TRUNCATE");

			MetaTrackUpdObject(RelationRelationId,
							   a_relid,
							   GetUserId(),
							   "TRUNCATE", "");
		}

	}

	/*
	 * Process all AFTER STATEMENT TRUNCATE triggers.
	 */
	resultRelInfo = resultRelInfos;
	foreach(cell, rels)
	{
		estate->es_result_relation_info = resultRelInfo;
		ExecASTruncateTriggers(estate, resultRelInfo);
		resultRelInfo++;
	}

	/* Handle queued AFTER triggers */
	AfterTriggerEndQuery(estate);

	/* We can clean up the EState now */
	FreeExecutorState(estate);

	/* And close the rels (can't do this while EState still holds refs) */
	foreach(cell, rels)
	{
		Relation	rel = (Relation) lfirst(cell);

		heap_close(rel, NoLock);
	}
}

/*
 * Check that a given rel is safe to truncate.	Subroutine for ExecuteTruncate
 */
static void
truncate_check_rel(Relation rel)
{
	AclResult	aclresult;

	/* Only allow truncate on regular or append-only tables */
	if (rel->rd_rel->relkind != RELKIND_RELATION)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table",
						RelationGetRelationName(rel))));

	if (RelationIsExternal(rel))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is an external relation and can't be truncated",
						RelationGetRelationName(rel))));


	/* Permissions checks */
	aclresult = pg_class_aclcheck(RelationGetRelid(rel), GetUserId(),
								  ACL_TRUNCATE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));

	if (!allowSystemTableModsDDL && IsSystemRelation(rel))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						RelationGetRelationName(rel))));

	/*
	 * We can never allow truncation of shared or nailed-in-cache relations,
	 * because we can't support changing their relfilenode values.
	 */
	if (rel->rd_rel->relisshared || rel->rd_isnailed)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot truncate system relation \"%s\"",
						RelationGetRelationName(rel))));

	/*
	 * Don't allow truncate on temp tables of other backends ... their local
	 * buffer manager is not going to cope.
	 */
	if (isOtherTempNamespace(RelationGetNamespace(rel)))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			  errmsg("cannot truncate temporary tables of other sessions")));

	/*
	 * Also check for active uses of the relation in the current transaction,
	 * including open scans and pending AFTER trigger events.
	 */
	CheckTableNotInUse(rel, "TRUNCATE");
}

/*----------
 * MergeAttributes
 *		Returns new schema given initial schema and superclasses.
 *
 * Input arguments:
 * 'schema' is the column/attribute definition for the table. (It's a list
 *		of ColumnDef's.) It is destructively changed.
 * 'supers' is a list of names (as RangeVar nodes) of parent relations.
 * 'istemp' is TRUE if we are creating a temp relation.
 * 'GpPolicy *' is NULL if the distribution policy is not to be updated
 *
 * Output arguments:
 * 'supOids' receives a list of the OIDs of the parent relations.
 * 'supconstr' receives a list of constraints belonging to the parents,
 *		updated as necessary to be valid for the child.
 * 'supOidCount' is set to the number of parents that have OID columns.
 * 'GpPolicy' is updated with the offsets of the distribution
 *      attributes in the new schema
 *
 * Return value:
 * Completed schema list.
 *
 * Notes:
 *	  The order in which the attributes are inherited is very important.
 *	  Intuitively, the inherited attributes should come first. If a table
 *	  inherits from multiple parents, the order of those attributes are
 *	  according to the order of the parents specified in CREATE TABLE.
 *
 *	  Here's an example:
 *
 *		create table person (name text, age int4, location point);
 *		create table emp (salary int4, manager text) inherits(person);
 *		create table student (gpa float8) inherits (person);
 *		create table stud_emp (percent int4) inherits (emp, student);
 *
 *	  The order of the attributes of stud_emp is:
 *
 *							person {1:name, 2:age, 3:location}
 *							/	 \
 *			   {6:gpa}	student   emp {4:salary, 5:manager}
 *							\	 /
 *						   stud_emp {7:percent}
 *
 *	   If the same attribute name appears multiple times, then it appears
 *	   in the result table in the proper location for its first appearance.
 *
 *	   Constraints (including NOT NULL constraints) for the child table
 *	   are the union of all relevant constraints, from both the child schema
 *	   and parent tables.
 *
 *	   The default value for a child column is defined as:
 *		(1) If the child schema specifies a default, that value is used.
 *		(2) If neither the child nor any parent specifies a default, then
 *			the column will not have a default.
 *		(3) If conflicting defaults are inherited from different parents
 *			(and not overridden by the child), an error is raised.
 *		(4) Otherwise the inherited default is used.
 *		Rule (3) is new in Postgres 7.1; in earlier releases you got a
 *		rather arbitrary choice of which parent default to use.
 *----------
 */
List *
MergeAttributes(List *schema, List *supers, bool istemp, bool isPartitioned,
				List **supOids, List **supconstr, int *supOidCount, GpPolicy *policy)
{
	ListCell   *entry;
	List	   *inhSchema = NIL;
	List	   *parentOids = NIL;
	List	   *constraints = NIL;
	int			parentsWithOids = 0;
	bool		have_bogus_defaults = false;
	char	   *bogus_marker = "Bogus!";		/* marks conflicting defaults */
	int			child_attno;

	/*
	 * Check for and reject tables with too many columns. We perform this
	 * check relatively early for two reasons: (a) we don't run the risk of
	 * overflowing an AttrNumber in subsequent code (b) an O(n^2) algorithm is
	 * okay if we're processing <= 1600 columns, but could take minutes to
	 * execute if the user attempts to create a table with hundreds of
	 * thousands of columns.
	 *
	 * Note that we also need to check that any we do not exceed this figure
	 * after including columns from inherited relations.
	 */
	if (list_length(schema) > MaxHeapAttributeNumber)
		ereport(ERROR,
				(errcode(ERRCODE_TOO_MANY_COLUMNS),
				 errmsg("tables can have at most %d columns",
						MaxHeapAttributeNumber)));

	/*
	 * Check for duplicate names in the explicit list of attributes.
	 *
	 * Although we might consider merging such entries in the same way that we
	 * handle name conflicts for inherited attributes, it seems to make more
	 * sense to assume such conflicts are errors.
	 */
	foreach(entry, schema)
	{
		ColumnDef  *coldef = lfirst(entry);
		ListCell   *rest;

		for_each_cell(rest, lnext(entry))
		{
			ColumnDef  *restdef = lfirst(rest);

			if (strcmp(coldef->colname, restdef->colname) == 0)
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column \"%s\" specified more than once",
								coldef->colname)));
		}
	}

	/*
	 * Scan the parents left-to-right, and merge their attributes to form a
	 * list of inherited attributes (inhSchema).  Also check to see if we need
	 * to inherit an OID column.
	 */
	child_attno = 0;
	foreach(entry, supers)
	{
		RangeVar   *parent = (RangeVar *) lfirst(entry);
		Relation	relation;
		TupleDesc	tupleDesc;
		TupleConstr *constr;
		AttrNumber *newattno;
		AttrNumber	parent_attno;

		relation = heap_openrv(parent, AccessShareLock);

		if (relation->rd_rel->relkind != RELKIND_RELATION)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("inherited relation \"%s\" is not a table",
							parent->relname)));
		/* Permanent rels cannot inherit from temporary ones */
		if (!istemp && isTempNamespace(RelationGetNamespace(relation)))
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot inherit from temporary relation \"%s\"",
							parent->relname)));

		/* Reject if parent is CO for non-partitioned table */
		if (RelationIsAoCols(relation) && !isPartitioned)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot inherit relation \"%s\" as it is column oriented",
							parent->relname)));
		/*
		 * We should have an UNDER permission flag for this, but for now,
		 * demand that creator of a child table own the parent.
		 */
		if (!pg_class_ownercheck(RelationGetRelid(relation), GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
						   RelationGetRelationName(relation));

		/*
		 * Reject duplications in the list of parents.
		 */
		if (list_member_oid(parentOids, RelationGetRelid(relation)))
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_TABLE),
			 errmsg("relation \"%s\" would be inherited from more than once",
					parent->relname)));

		parentOids = lappend_oid(parentOids, RelationGetRelid(relation));

		if (relation->rd_rel->relhasoids)
			parentsWithOids++;

		tupleDesc = RelationGetDescr(relation);
		constr = tupleDesc->constr;

		/*
		 * newattno[] will contain the child-table attribute numbers for the
		 * attributes of this parent table.  (They are not the same for
		 * parents after the first one, nor if we have dropped columns.)
		 */
		newattno = (AttrNumber *)
			palloc0(tupleDesc->natts * sizeof(AttrNumber));

		for (parent_attno = 1; parent_attno <= tupleDesc->natts;
			 parent_attno++)
		{
			Form_pg_attribute attribute = tupleDesc->attrs[parent_attno - 1];
			char	   *attributeName = NameStr(attribute->attname);
			int			exist_attno;
			ColumnDef  *def;

			/*
			 * Ignore dropped columns in the parent.
			 */
			if (attribute->attisdropped)
				continue;		/* leave newattno entry as zero */

			/*
			 * Does it conflict with some previously inherited column?
			 */
			exist_attno = findAttrByName(attributeName, inhSchema);
			if (exist_attno > 0)
			{
				Oid			defTypeId;
				int32		deftypmod;

				/*
				 * Yes, try to merge the two column definitions. They must
				 * have the same type and typmod.
				 */
				if (Gp_role == GP_ROLE_EXECUTE)
				{
					ereport(DEBUG1,
						(errmsg("merging multiple inherited definitions of column \"%s\"",
								attributeName)));
				}
				else
				ereport(NOTICE,
						(errmsg("merging multiple inherited definitions of column \"%s\"",
								attributeName)));
				def = (ColumnDef *) list_nth(inhSchema, exist_attno - 1);
				defTypeId = typenameTypeId(NULL, def->typeName, &deftypmod);
				if (defTypeId != attribute->atttypid ||
					deftypmod != attribute->atttypmod)
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
						errmsg("inherited column \"%s\" has a type conflict",
							   attributeName),
							 errdetail("%s versus %s",
									   TypeNameToString(def->typeName),
									   format_type_be(attribute->atttypid))));
				def->inhcount++;
				/* Merge of NOT NULL constraints = OR 'em together */
				def->is_not_null |= attribute->attnotnull;
				/* Default and other constraints are handled below */
				newattno[parent_attno - 1] = exist_attno;

				/*
				 * Update GpPolicy
				 */
				if (policy != NULL)
				{
					int attr_ofst = 0;

					Assert(policy->nattrs >= 0 && "the number of distribution attributes is not negative");

					/* Iterate over all distribution attribute offsets */
					for (attr_ofst = 0; attr_ofst < policy->nattrs; attr_ofst++)
					{
						/* Check if any distribution attribute has higher offset than the current */
						if (policy->attrs[attr_ofst] > child_attno)
						{
							Assert(policy->attrs[attr_ofst] > 0 && "index should not become negative");
							policy->attrs[attr_ofst]--;
						}
					}
				}

			}
			else
			{
				/*
				 * No, create a new inherited column
				 */
				def = makeNode(ColumnDef);
				def->colname = pstrdup(attributeName);
				def->typeName = makeTypeNameFromOid(attribute->atttypid,
													attribute->atttypmod);
				def->inhcount = 1;
				def->is_local = false;
				def->is_not_null = attribute->attnotnull;
				def->raw_default = NULL;
				def->cooked_default = NULL;
				def->constraints = NIL;
				inhSchema = lappend(inhSchema, def);
				newattno[parent_attno - 1] = ++child_attno;
			}

			/*
			 * Copy default if any
			 */
			if (attribute->atthasdef)
			{
				char	   *this_default = NULL;
				AttrDefault *attrdef;
				int			i;

				/* Find default in constraint structure */
				Assert(constr != NULL);
				attrdef = constr->defval;
				for (i = 0; i < constr->num_defval; i++)
				{
					if (attrdef[i].adnum == parent_attno)
					{
						this_default = attrdef[i].adbin;
						break;
					}
				}
				Assert(this_default != NULL);

				/*
				 * If default expr could contain any vars, we'd need to fix
				 * 'em, but it can't; so default is ready to apply to child.
				 *
				 * If we already had a default from some prior parent, check
				 * to see if they are the same.  If so, no problem; if not,
				 * mark the column as having a bogus default. Below, we will
				 * complain if the bogus default isn't overridden by the child
				 * schema.
				 */
				Assert(def->raw_default == NULL);
				if (def->cooked_default == NULL)
					def->cooked_default = pstrdup(this_default);
				else if (strcmp(def->cooked_default, this_default) != 0)
				{
					def->cooked_default = bogus_marker;
					have_bogus_defaults = true;
				}
			}
		}

		/*
		 * Now copy the constraints of this parent, adjusting attnos using the
		 * completed newattno[] map
		 */
		if (constr && constr->num_check > 0)
		{
			ConstrCheck *check = constr->check;
			int			i;

			for (i = 0; i < constr->num_check; i++)
			{
				Constraint *cdef = makeNode(Constraint);
				Node	   *expr;
				bool		found_whole_row;

				/* Adjust Vars to match new table's column numbering */
				expr = map_variable_attnos(stringToNode(check[i].ccbin),
										   1, 0,
										   newattno, tupleDesc->natts,
										   &found_whole_row);

				/*
				 * For the moment we have to reject whole-row variables.
				 * We could convert them, if we knew the new table's rowtype
				 * OID, but that hasn't been assigned yet.
				 */
				if (found_whole_row)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot convert whole-row table reference"),
							 errdetail("Constraint \"%s\" contains a whole-row reference to table \"%s\".",
									   check[i].ccname,
									   RelationGetRelationName(relation))));

				cdef->contype = CONSTR_CHECK;
				cdef->name = pstrdup(check[i].ccname);
				cdef->raw_expr = NULL;

				cdef->cooked_expr = nodeToString(expr);
				constraints = lappend(constraints, cdef);
			}
		}

		pfree(newattno);

		/*
		 * Close the parent rel, but keep our AccessShareLock on it until xact
		 * commit.	That will prevent someone else from deleting or ALTERing
		 * the parent before the child is committed.
		 */
		heap_close(relation, NoLock);
	}

	/*
	 * If we had no inherited attributes, the result schema is just the
	 * explicitly declared columns.  Otherwise, we need to merge the declared
	 * columns into the inherited schema list.
	 */
	if (inhSchema != NIL)
	{
		foreach(entry, schema)
		{
			ColumnDef  *newdef = lfirst(entry);
			char	   *attributeName = newdef->colname;
			int			exist_attno;

			/*
			 * Does it conflict with some previously inherited column?
			 */
			exist_attno = findAttrByName(attributeName, inhSchema);
			if (exist_attno > 0)
			{
				ColumnDef  *def;
				Oid			defTypeId,
							newTypeId;
				int32		deftypmod,
							newtypmod;

				/*
				 * Yes, try to merge the two column definitions. They must
				 * have the same type and typmod.
				 */
				ereport((Gp_role == GP_ROLE_EXECUTE) ? DEBUG1 : NOTICE,
				   (errmsg("merging column \"%s\" with inherited definition",
						   attributeName)));
				def = (ColumnDef *) list_nth(inhSchema, exist_attno - 1);
				defTypeId = typenameTypeId(NULL, def->typeName, &deftypmod);
				newTypeId = typenameTypeId(NULL, newdef->typeName, &newtypmod);
				if (defTypeId != newTypeId || deftypmod != newtypmod)
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
							 errmsg("column \"%s\" has a type conflict",
									attributeName),
							 errdetail("%s versus %s",
									   TypeNameToString(def->typeName),
									   TypeNameToString(newdef->typeName))));
				/* Mark the column as locally defined */
				def->is_local = true;
				/* Merge of NOT NULL constraints = OR 'em together */
				def->is_not_null |= newdef->is_not_null;
				/* If new def has a default, override previous default */
				if (newdef->raw_default != NULL)
				{
					def->raw_default = newdef->raw_default;
					def->cooked_default = newdef->cooked_default;
				}
			}
			else
			{
				/*
				 * No, attach new column to result schema
				 */
				inhSchema = lappend(inhSchema, newdef);
			}
		}

		schema = inhSchema;

		/*
		 * Check that we haven't exceeded the legal # of columns after merging
		 * in inherited columns.
		 */
		if (list_length(schema) > MaxHeapAttributeNumber)
			ereport(ERROR,
					(errcode(ERRCODE_TOO_MANY_COLUMNS),
					 errmsg("tables can have at most %d columns",
							MaxHeapAttributeNumber)));
	}

	/*
	 * If we found any conflicting parent default values, check to make sure
	 * they were overridden by the child.
	 */
	if (have_bogus_defaults)
	{
		foreach(entry, schema)
		{
			ColumnDef  *def = lfirst(entry);

			if (def->cooked_default == bogus_marker)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_DEFINITION),
				  errmsg("column \"%s\" inherits conflicting default values",
						 def->colname),
						 errhint("To resolve the conflict, specify a default explicitly.")));
		}
	}

	*supOids = parentOids;
	*supconstr = constraints;
	*supOidCount = parentsWithOids;
	return schema;
}


/*
 * In multiple-inheritance situations, it's possible to inherit
 * the same grandparent constraint through multiple parents.
 * Hence, we want to discard inherited constraints that match as to
 * both name and expression.  Otherwise, gripe if there are conflicting
 * names. Nonconflicting constraints are added back to the CreateStmt->constraints
 */
static bool
add_nonduplicate_cooked_constraint(Constraint *cdef, List *stmtConstraints)
{
	/* Should only see precooked constraints here */
	Assert(cdef->contype == CONSTR_CHECK);
	Assert(cdef->name != NULL);
	Assert(cdef->raw_expr == NULL && cdef->cooked_expr != NULL);

	ListCell 	*listptr;

	foreach(listptr, stmtConstraints)
	{
		Constraint *stmtCdef = (Constraint *) lfirst(listptr);

		/* Constraint in CreateStmt may be raw and maynot be a check constraint */
		if (stmtCdef->cooked_expr == NULL || stmtCdef->contype != CONSTR_CHECK)
			continue;
		if (strcmp(stmtCdef->name, cdef->name) != 0)
			continue;
		if (strcmp(stmtCdef->cooked_expr, cdef->cooked_expr) == 0)
			return false;
	}
	return true;
}


/*
 * StoreCatalogInheritance
 *		Updates the system catalogs with proper inheritance information.
 *
 * supers is a list of the OIDs of the new relation's direct ancestors.
 */
static void
StoreCatalogInheritance(Oid relationId, List *supers)
{
	Relation	relation;
	int16		seqNumber;
	ListCell   *entry;

	/*
	 * sanity checks
	 */
	AssertArg(OidIsValid(relationId));

	if (supers == NIL)
		return;

	/*
	 * Store INHERITS information in pg_inherits using direct ancestors only.
	 * Also enter dependencies on the direct ancestors, and make sure they are
	 * marked with relhassubclass = true.
	 *
	 * (Once upon a time, both direct and indirect ancestors were found here
	 * and then entered into pg_ipl.  Since that catalog doesn't exist
	 * anymore, there's no need to look for indirect ancestors.)
	 */
	relation = heap_open(InheritsRelationId, RowExclusiveLock);

	seqNumber = 1;
	foreach(entry, supers)
	{
		Oid			parentOid = lfirst_oid(entry);

		StoreCatalogInheritance1(relationId, parentOid, seqNumber, relation,
								 false);
		seqNumber++;
	}

	heap_close(relation, RowExclusiveLock);
}

/*
 * Make catalog entries showing relationId as being an inheritance child
 * of parentOid.  inhRelation is the already-opened pg_inherits catalog.
 */
static void
StoreCatalogInheritance1(Oid relationId, Oid parentOid,
						 int16 seqNumber, Relation inhRelation,
						 bool is_partition)
{
	TupleDesc	desc = RelationGetDescr(inhRelation);
	Datum		datum[Natts_pg_inherits];
	bool		nullarr[Natts_pg_inherits];
	ObjectAddress childobject,
				parentobject;
	HeapTuple	tuple;

	/*
	 * Make the pg_inherits entry
	 */
	datum[0] = ObjectIdGetDatum(relationId);	/* inhrelid */
	datum[1] = ObjectIdGetDatum(parentOid);		/* inhparent */
	datum[2] = Int16GetDatum(seqNumber);		/* inhseqno */

	nullarr[0] = false;
	nullarr[1] = false;
	nullarr[2] = false;

	tuple = heap_form_tuple(desc, datum, nullarr);

	simple_heap_insert(inhRelation, tuple);

	CatalogUpdateIndexes(inhRelation, tuple);

	heap_freetuple(tuple);

	/*
	 * Store a dependency too
	 */
	parentobject.classId = RelationRelationId;
	parentobject.objectId = parentOid;
	parentobject.objectSubId = 0;
	childobject.classId = RelationRelationId;
	childobject.objectId = relationId;
	childobject.objectSubId = 0;

	recordDependencyOn(&childobject, &parentobject,
					   is_partition ? DEPENDENCY_AUTO : DEPENDENCY_NORMAL);

	/*
	 * Mark the parent as having subclasses.
	 */
	setRelhassubclassInRelation(parentOid, true);
}

/*
 * Look for an existing schema entry with the given name.
 *
 * Returns the index (starting with 1) if attribute already exists in schema,
 * 0 if it doesn't.
 */
static int
findAttrByName(const char *attributeName, List *schema)
{
	ListCell   *s;
	int			i = 1;

	foreach(s, schema)
	{
		ColumnDef  *def = lfirst(s);

		if (strcmp(attributeName, def->colname) == 0)
			return i;

		i++;
	}
	return 0;
}

/*
 * Update a relation's pg_class.relhassubclass entry to the given value
 */
static void
setRelhassubclassInRelation(Oid relationId, bool relhassubclass)
{
	Relation	relationRelation;
	HeapTuple	tuple;
	Form_pg_class classtuple;

	/*
	 * Fetch a modifiable copy of the tuple, modify it, update pg_class.
	 *
	 * If the tuple already has the right relhassubclass setting, we don't
	 * need to update it, but we still need to issue an SI inval message.
	 */
	relationRelation = heap_open(RelationRelationId, RowExclusiveLock);
	tuple = SearchSysCacheCopy(RELOID,
							   ObjectIdGetDatum(relationId),
							   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relationId);
	classtuple = (Form_pg_class) GETSTRUCT(tuple);

	if (classtuple->relhassubclass != relhassubclass)
	{
		classtuple->relhassubclass = relhassubclass;
		simple_heap_update(relationRelation, &tuple->t_self, tuple);

		/* keep the catalog indexes up to date */
		CatalogUpdateIndexes(relationRelation, tuple);
	}
	else
	{
		/* no need to change tuple, but force relcache rebuild anyway */
		CacheInvalidateRelcacheByTuple(tuple);
	}

	heap_freetuple(tuple);
	heap_close(relationRelation, RowExclusiveLock);
}


/*
 *		renameatt		- changes the name of a attribute in a relation
 *
 *		Attname attribute is changed in attribute catalog.
 *		No record of the previous attname is kept (correct?).
 *
 *		get proper relrelation from relation catalog (if not arg)
 *		scan attribute catalog
 *				for name conflict (within rel)
 *				for original attribute (if not arg)
 *		modify attname in attribute tuple
 *		insert modified attribute in attribute catalog
 *		delete original attribute from attribute catalog
 */
void
renameatt(Oid myrelid,
		  const char *oldattname,
		  const char *newattname,
		  bool recurse,
		  bool recursing)
{
	Relation	targetrelation;
	Relation	attrelation;
	HeapTuple	atttup;
	Form_pg_attribute attform;
	int			attnum;
	List	   *indexoidlist;
	ListCell   *indexoidscan;

	/*
	 * Grab an exclusive lock on the target table, which we will NOT release
	 * until end of transaction.
	 */
	targetrelation = relation_open(myrelid, AccessExclusiveLock);

	/*
	 * permissions checking.  this would normally be done in utility.c, but
	 * this particular routine is recursive.
	 *
	 * normally, only the owner of a class can change its schema.
	 */
	if (!pg_class_ownercheck(myrelid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   RelationGetRelationName(targetrelation));
	if (!allowSystemTableModsDDL && IsSystemRelation(targetrelation))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						RelationGetRelationName(targetrelation))));

	/*
	 * if the 'recurse' flag is set then we are supposed to rename this
	 * attribute in all classes that inherit from 'relname' (as well as in
	 * 'relname').
	 *
	 * any permissions or problems with duplicate attributes will cause the
	 * whole transaction to abort, which is what we want -- all or nothing.
	 */
	if (recurse)
	{
		ListCell   *child;
		List	   *children;

		/* this routine is actually in the planner */
		children = find_all_inheritors(myrelid);

		/*
		 * find_all_inheritors does the recursive search of the inheritance
		 * hierarchy, so all we have to do is process all of the relids in the
		 * list that it returns.
		 */
		foreach(child, children)
		{
			Oid			childrelid = lfirst_oid(child);

			if (childrelid == myrelid)
				continue;
			/* note we need not recurse again */
			renameatt(childrelid, oldattname, newattname, false, true);
		}
	}
	else
	{
		/*
		 * If we are told not to recurse, there had better not be any child
		 * tables; else the rename would put them out of step.
		 */
		if (!recursing &&
			find_inheritance_children(myrelid) != NIL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("inherited column \"%s\" must be renamed in child tables too",
							oldattname)));
	}

	attrelation = heap_open(AttributeRelationId, RowExclusiveLock);

	atttup = SearchSysCacheCopyAttName(myrelid, oldattname);
	if (!HeapTupleIsValid(atttup))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" does not exist",
						oldattname)));
	attform = (Form_pg_attribute) GETSTRUCT(atttup);

	attnum = attform->attnum;
	if (attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot rename system column \"%s\"",
						oldattname)));

	/*
	 * if the attribute is inherited, forbid the renaming, unless we are
	 * already inside a recursive rename.
	 */
	if (attform->attinhcount > 0 && !recursing)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot rename inherited column \"%s\"",
						oldattname)));

	/* should not already exist */
	/* this test is deliberately not attisdropped-aware */
	if (SearchSysCacheExists(ATTNAME,
							 ObjectIdGetDatum(myrelid),
							 PointerGetDatum(newattname),
							 0, 0))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" already exists",
					  newattname, RelationGetRelationName(targetrelation))));

	namestrcpy(&(attform->attname), newattname);

	simple_heap_update(attrelation, &atttup->t_self, atttup);

	/* keep system catalog indexes current */
	CatalogUpdateIndexes(attrelation, atttup);

	heap_freetuple(atttup);

	/*
	 * Update column names of indexes that refer to the column being renamed.
	 */
	indexoidlist = RelationGetIndexList(targetrelation);

	foreach(indexoidscan, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(indexoidscan);
		HeapTuple	indextup;
		Form_pg_index indexform;
		int			i;

		/*
		 * Scan through index columns to see if there's any simple index
		 * entries for this attribute.	We ignore expressional entries.
		 */
		indextup = SearchSysCache(INDEXRELID,
								  ObjectIdGetDatum(indexoid),
								  0, 0, 0);
		if (!HeapTupleIsValid(indextup))
			elog(ERROR, "cache lookup failed for index %u", indexoid);
		indexform = (Form_pg_index) GETSTRUCT(indextup);

		for (i = 0; i < indexform->indnatts; i++)
		{
			if (attnum != indexform->indkey.values[i])
				continue;

			/*
			 * Found one, rename it.
			 */
			atttup = SearchSysCacheCopy(ATTNUM,
										ObjectIdGetDatum(indexoid),
										Int16GetDatum(i + 1),
										0, 0);
			if (!HeapTupleIsValid(atttup))
				continue;		/* should we raise an error? */

			/*
			 * Update the (copied) attribute tuple.
			 */
			namestrcpy(&(((Form_pg_attribute) GETSTRUCT(atttup))->attname),
					   newattname);

			simple_heap_update(attrelation, &atttup->t_self, atttup);

			/* keep system catalog indexes current */
			CatalogUpdateIndexes(attrelation, atttup);

			heap_freetuple(atttup);
		}

		ReleaseSysCache(indextup);
	}

	list_free(indexoidlist);

	heap_close(attrelation, RowExclusiveLock);

	/* MPP-6929, MPP-7600: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(targetrelation->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(targetrelation),
						   GetUserId(),
						   "ALTER", "RENAME COLUMN"
				);


	relation_close(targetrelation, NoLock);		/* close rel but keep lock */
}

/*
 * A helper function for RenameRelation, to createa a very minimal, fake,
 * RelationData struct for a relation. This is used in
 * gp_allow_rename_relation_without_lock mode, in place of opening the
 * relcache entry for real.
 *
 * RenameRelation only needs the rd_rel field to be filled in, so that's
 * all we fetch.
 */
static Relation
fake_relation_open(Oid myrelid)
{
	Relation	relrelation;	/* for RELATION relation */
	Relation	fakerel;
	HeapTuple	reltup;

	fakerel = palloc0(sizeof(RelationData));

	/*
	 * Find relation's pg_class tuple, and make sure newrelname isn't in use.
	 */
	relrelation = heap_open(RelationRelationId, RowExclusiveLock);

	reltup = SearchSysCacheCopy(RELOID,
								ObjectIdGetDatum(myrelid),
								0, 0, 0);
	if (!HeapTupleIsValid(reltup))		/* shouldn't happen */
		elog(ERROR, "cache lookup failed for relation %u", myrelid);
	fakerel->rd_rel = (Form_pg_class) GETSTRUCT(reltup);

	heap_close(relrelation, RowExclusiveLock);

	return fakerel;
}

/*
 * Execute ALTER TABLE/INDEX/SEQUENCE/VIEW RENAME
 *
 * Caller has already done permissions checks.
 */
void
RenameRelation(Oid myrelid, const char *newrelname, ObjectType reltype, RenameStmt *stmt)
{
	Relation	targetrelation;
	Oid			namespaceId;
	char		relkind;
	char	   *oldrelname;

	/*
	 * In Postgres, grab an exclusive lock on the target table, index, sequence
	 * or view, which we will NOT release until end of transaction.
	 *
	 * In GPDB, added supportability feature under GUC to allow rename table
	 * without AccessExclusiveLock for scenarios like directly modifying system
	 * catalogs. This will change transaction isolation behaviors, however, this
	 * won't cause any data corruption.
	 */
	if (gp_allow_rename_relation_without_lock)
		targetrelation = fake_relation_open(myrelid);
	else
		targetrelation = relation_open(myrelid, AccessExclusiveLock);

	/* if this is a child table of a partitioning configuration, complain */
	if (stmt && rel_is_child_partition(myrelid) && !stmt->bAllowPartn)
	{
		Oid		 master = rel_partition_get_master(myrelid);
		char	*pretty	= rel_get_part_path_pretty(myrelid,
												   " ALTER PARTITION ",
												   " RENAME PARTITION ");
		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				errmsg("cannot rename partition \"%s\" directly",
					   get_rel_name(myrelid)),
				errhint("Table \"%s\" is a child partition of table "
						"\"%s\".  To rename it, use ALTER TABLE \"%s\"%s...",
						get_rel_name(myrelid), get_rel_name(master),
						get_rel_name(master), pretty ? pretty : "" )));
	}

	namespaceId = RelationGetNamespace(targetrelation);
	relkind = targetrelation->rd_rel->relkind;
	oldrelname = RelationGetRelationName(targetrelation);

	/*
	 * For compatibility with prior releases, we don't complain if ALTER TABLE
	 * or ALTER INDEX is used to rename a sequence or view.
	 */
	if (reltype == OBJECT_SEQUENCE && relkind != RELKIND_SEQUENCE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a sequence",
						RelationGetRelationName(targetrelation))));

	if (reltype == OBJECT_VIEW && relkind != RELKIND_VIEW)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a view",
						RelationGetRelationName(targetrelation))));

	/*
	 * Don't allow ALTER TABLE on composite types.
	 * We want people to use ALTER TYPE for that.
	 */
	if (relkind == RELKIND_COMPOSITE_TYPE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is a composite type",
						RelationGetRelationName(targetrelation)),
				 errhint("Use ALTER TYPE instead.")));

	/* Do the work */
	RenameRelationInternal(myrelid, newrelname, namespaceId);

	/* MPP-3059: recursive rename of partitioned table */
	/* Note: the top-level RENAME has bAllowPartn=FALSE, while the
	 * generated statements from this block set it to TRUE.  That way,
	 * the rename of each partition is allowed, but this block doesn't
	 * get invoked recursively.
	 */
	if (stmt && !rel_is_child_partition(myrelid) && !stmt->bAllowPartn &&
		(Gp_role == GP_ROLE_DISPATCH))
	{
		PartitionNode *pNode;

		pNode = RelationBuildPartitionDescByOid(myrelid, false);

		if (pNode)
		{
			RenameStmt 			   	*renStmt 	 = makeNode(RenameStmt);
			DestReceiver 		   	*dest  		 = None_Receiver;
			List 					*renList 	 = NIL;
			int 					 skipped 	 = 0;
			int 					 renamed 	 = 0;

			renStmt->renameType = OBJECT_TABLE;
			renStmt->subname = NULL;
			renStmt->bAllowPartn = true; /* allow rename of partitions */

			/* rename the children as well */
			renList = atpxRenameList(pNode, oldrelname, newrelname, &skipped);

			/* process children if there are any */
			if (renList)
			{
				ListCell 		*lc;

				foreach(lc, renList)
				{
					ListCell 		*lc2;
					List  			*lpair = lfirst(lc);

					lc2 = list_head(lpair);

					renStmt->relation = (RangeVar *)lfirst(lc2);
					lc2 = lnext(lc2);
					renStmt->newname = (char *)lfirst(lc2);

					ProcessUtility((Node *) renStmt,
								   synthetic_sql,
								   NULL,
								   false, /* not top level */
								   dest,
								   NULL);
					renamed++;
				}

				/* MPP-3542: warn when skip child partitions */
				if (skipped)
				{
					ereport(WARNING,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("renamed %d partitions, "
									"skipped %d child partitions "
									"due to name truncation",
									renamed, skipped)));
				}
			}
		}
	}

	/*
	 * Close rel, but keep exclusive lock!
	 */
	if (!gp_allow_rename_relation_without_lock)
		relation_close(targetrelation, NoLock);
}

/*
 *		RenameRelationInternal - change the name of a relation
 *
 *		XXX - When renaming sequences, we don't bother to modify the
 *			  sequence name that is stored within the sequence itself
 *			  (this would cause problems with MVCC). In the future,
 *			  the sequence name should probably be removed from the
 *			  sequence, AFAIK there's no need for it to be there.
 */
void
RenameRelationInternal(Oid myrelid, const char *newrelname, Oid namespaceId)
{
	Relation	targetrelation;
	Relation	relrelation;	/* for RELATION relation */
	HeapTuple	reltup;
	Form_pg_class relform;
	bool		isSystemRelation;

	/*
	 * In Postgres:
	 * Grab an exclusive lock on the target table, index, sequence or view,
	 * which we will NOT release until end of transaction.
	 *
	 * In GPDB, added supportability feature under GUC to allow rename table
	 * without AccessExclusiveLock for scenarios like directly modifying system
	 * catalogs. This will change transaction isolation behaviors, however, this
	 * won't cause any data corruption.
	 */
	if (gp_allow_rename_relation_without_lock)
		targetrelation = fake_relation_open(myrelid);
	else
		targetrelation = relation_open(myrelid, AccessExclusiveLock);

	isSystemRelation = IsSystemNamespace(namespaceId) ||
					   IsToastNamespace(namespaceId) ||
					   IsAoSegmentNamespace(namespaceId);

	Assert (allowSystemTableModsDDL || !isSystemRelation);

	/*
	 * Find relation's pg_class tuple, and make sure newrelname isn't in use.
	 */
	relrelation = heap_open(RelationRelationId, RowExclusiveLock);

	reltup = SearchSysCacheCopy(RELOID,
								ObjectIdGetDatum(myrelid),
								0, 0, 0);
	if (!HeapTupleIsValid(reltup))		/* shouldn't happen */
		elog(ERROR, "cache lookup failed for relation %u", myrelid);
	relform = (Form_pg_class) GETSTRUCT(reltup);

	if (get_relname_relid(newrelname, namespaceId) != InvalidOid)
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_TABLE),
				 errmsg("relation \"%s\" already exists",
						newrelname)));

	/*
	 * Update pg_class tuple with new relname.	(Scribbling on reltup is OK
	 * because it's a copy...)
	 */
	namestrcpy(&(relform->relname), newrelname);

	simple_heap_update(relrelation, &reltup->t_self, reltup);

	/* keep the system catalog indexes current */
	CatalogUpdateIndexes(relrelation, reltup);

	heap_freetuple(reltup);
	heap_close(relrelation, NoLock);

	/*
	 * Also rename the associated type, if any.
	 */
	if (OidIsValid(targetrelation->rd_rel->reltype))
	{
		if (!TypeTupleExists(targetrelation->rd_rel->reltype))
			ereport(WARNING,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("type \"%s\" does not exist",
							RelationGetRelationName(targetrelation))));
		else
			RenameTypeInternal(targetrelation->rd_rel->reltype,
						   newrelname, namespaceId);
	}

	/*
	 * Also rename the associated constraint, if any.
	 */
	if (targetrelation->rd_rel->relkind == RELKIND_INDEX)
	{
		Oid			constraintId = get_index_constraint(myrelid);

		if (OidIsValid(constraintId))
			RenameConstraintById(constraintId, newrelname);
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&&
		/* MPP-7773: don't track objects in system namespace
		 * if modifying system tables (eg during upgrade)
		 */
		( ! ( (PG_CATALOG_NAMESPACE == namespaceId) && (allowSystemTableModsDDL)))
		&& (   MetaTrackValidRelkind(targetrelation->rd_rel->relkind)
			&& METATRACK_VALIDNAMESPACE(namespaceId)
			   && (!(isAnyTempNamespace(namespaceId)))
				))
		MetaTrackUpdObject(RelationRelationId,
						   myrelid,
						   GetUserId(),
						   "ALTER", "RENAME"
				);

	/*
	 * Close rel, but keep exclusive lock!
	 */
	if (!gp_allow_rename_relation_without_lock)
		relation_close(targetrelation, NoLock);
}

static bool
TypeTupleExists(Oid typeId)
{
	return SearchSysCacheExists(TYPEOID, ObjectIdGetDatum(typeId), 0, 0, 0);
}

/*
 * Disallow ALTER TABLE (and similar commands) when the current backend has
 * any open reference to the target table besides the one just acquired by
 * the calling command; this implies there's an open cursor or active plan.
 * We need this check because our AccessExclusiveLock doesn't protect us
 * against stomping on our own foot, only other people's feet!
 *
 * For ALTER TABLE, the only case known to cause serious trouble is ALTER
 * COLUMN TYPE, and some changes are obviously pretty benign, so this could
 * possibly be relaxed to only error out for certain types of alterations.
 * But the use-case for allowing any of these things is not obvious, so we
 * won't work hard at it for now.
 *
 * We also reject these commands if there are any pending AFTER trigger events
 * for the rel.  This is certainly necessary for the rewriting variants of
 * ALTER TABLE, because they don't preserve tuple TIDs and so the pending
 * events would try to fetch the wrong tuples.  It might be overly cautious
 * in other cases, but again it seems better to err on the side of paranoia.
 *
 * REINDEX calls this with "rel" referencing the index to be rebuilt; here
 * we are worried about active indexscans on the index.  The trigger-event
 * check can be skipped, since we are doing no damage to the parent table.
 *
 * The statement name (eg, "ALTER TABLE") is passed for use in error messages.
 */
void
CheckTableNotInUse(Relation rel, const char *stmt)
{
	int			expected_refcnt;

	expected_refcnt = rel->rd_isnailed ? 2 : 1;

	/*
	 * XXX For a bitmap index, since vacuum (or vacuum full) is currently done through
	 * reindex_index, the reference count could be 2 (or 3). We set it
	 * here until vacuum is done properly.
	 */
	if (expected_refcnt == 1 &&
		RelationIsBitmapIndex(rel) &&
		(rel->rd_refcnt == 2 || rel->rd_refcnt == 3))
		expected_refcnt = rel->rd_refcnt;

	if (rel->rd_refcnt != expected_refcnt)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_IN_USE),
				 /* translator: first %s is a SQL command, eg ALTER TABLE */
				 errmsg("cannot %s \"%s\" because "
						"it is being used by active queries in this session",
						stmt, RelationGetRelationName(rel))));

	if (rel->rd_rel->relkind != RELKIND_INDEX &&
		AfterTriggerPendingOnRel(RelationGetRelid(rel)))
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_IN_USE),
				 /* translator: first %s is a SQL command, eg ALTER TABLE */
				 errmsg("cannot %s \"%s\" because "
						"it has pending trigger events",
						stmt, RelationGetRelationName(rel))));
}

static void
ATVerifyObject(AlterTableStmt *stmt, Relation rel)
{
	/* Nothing to check for ALTER INDEX */
	if(stmt->relkind == OBJECT_INDEX)
		return;

	/*
	 * Verify the object specified against relstorage in the catalog.
	 * Enforce correct syntax usage. 
	 */
	if (RelationIsExternal(rel) && stmt->relkind != OBJECT_EXTTABLE)
	{
		/*
		 * special case: in order to support 3.3 dumps with ALTER TABLE OWNER of
		 * external tables, we will allow using ALTER TABLE (without EXTERNAL)
		 * temporarily, and use a deprecation warning. This should be removed
		 * in future releases.
		 */
		if(stmt->relkind == OBJECT_TABLE)
		{
			if (Gp_role == GP_ROLE_DISPATCH) /* why are WARNINGs emitted from all segments? */
				ereport(WARNING,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("\"%s\" is an external table. ALTER TABLE for external tables is deprecated.", RelationGetRelationName(rel)),
						 errhint("Use ALTER EXTERNAL TABLE instead")));
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is an external table", RelationGetRelationName(rel)),
					 errhint("Use ALTER EXTERNAL TABLE instead")));
		}
	}
	else if (!RelationIsExternal(rel) && stmt->relkind != OBJECT_TABLE)
	{
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is a base table", RelationGetRelationName(rel)),
				 errhint("Use ALTER TABLE instead")));
	}

	/*
	 * Check the ALTER command type is supported for this object
	 */
	if (RelationIsExternal(rel))
	{
		ListCell *lcmd;

		foreach(lcmd, stmt->cmds)
		{
			AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);

			switch(cmd->subtype)
			{
				/* EXTERNAL tables don't support the following AT */
				case AT_ColumnDefault:
				case AT_ColumnDefaultRecurse:
				case AT_DropNotNull:
				case AT_SetNotNull:
				case AT_SetStatistics:
				case AT_SetStorage:
				case AT_AddIndex:
				case AT_ReAddIndex:
				case AT_AddConstraint:
				case AT_AddConstraintRecurse:
				case AT_ProcessedConstraint:
				case AT_DropConstraint:
				case AT_DropConstraintQuietly:
				case AT_ClusterOn:
				case AT_DropCluster:
				case AT_DropOids:
				case AT_SetTableSpace:
				case AT_SetRelOptions:
				case AT_ResetRelOptions:
				case AT_EnableTrig:
				case AT_DisableTrig:
				case AT_EnableTrigAll:
				case AT_DisableTrigAll:
				case AT_EnableTrigUser:
				case AT_DisableTrigUser:
				case AT_AddInherit:
				case AT_DropInherit:
				case AT_SetDistributedBy:
				case AT_PartAdd:
				case AT_PartAddForSplit:
				case AT_PartAlter:
				case AT_PartCoalesce:
				case AT_PartDrop:
				case AT_PartExchange:
				case AT_PartMerge:
				case AT_PartModify:
				case AT_PartRename:
				case AT_PartSetTemplate:
				case AT_PartSplit:
				case AT_PartTruncate:
				case AT_PartAddInternal:
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_COLUMN_DEFINITION),
							 errmsg("Unsupported ALTER command for table type %s",
									"external")));
					break;

				case AT_AddColumn: /* check no constraint is added too */
					if (((ColumnDef *) cmd->def)->constraints != NIL ||
						((ColumnDef *) cmd->def)->is_not_null ||
						((ColumnDef *) cmd->def)->raw_default)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_COLUMN_DEFINITION),
								 errmsg("Unsupported ALTER command for table type %s. No constraints allowed.",
										 "external")));
					break;

				default:
					/* ALTER type supported */
					break;
			}
		}
	}
}

/*
 * AlterTable
 *		Execute ALTER TABLE, which can be a list of subcommands
 *
 * ALTER TABLE is performed in three phases:
 *		1. Examine subcommands and perform pre-transformation checking.
 *		2. Update system catalogs.
 *		3. Scan table(s) to check new constraints, and optionally recopy
 *		   the data into new table(s).
 * Phase 3 is not performed unless one or more of the subcommands requires
 * it.	The intention of this design is to allow multiple independent
 * updates of the table schema to be performed with only one pass over the
 * data.
 *
 * ATPrepCmd performs phase 1.	A "work queue" entry is created for
 * each table to be affected (there may be multiple affected tables if the
 * commands traverse a table inheritance hierarchy).  Also we do preliminary
 * validation of the subcommands, including parse transformation of those
 * expressions that need to be evaluated with respect to the old table
 * schema.
 *
 * ATRewriteCatalogs performs phase 2 for each affected table (note that
 * phases 2 and 3 do no explicit recursion, since phase 1 already did it).
 * Certain subcommands need to be performed before others to avoid
 * unnecessary conflicts; for example, DROP COLUMN should come before
 * ADD COLUMN.	Therefore phase 1 divides the subcommands into multiple
 * lists, one for each logical "pass" of phase 2.
 *
 * ATRewriteTables performs phase 3 for those tables that need it.
 *
 * Thanks to the magic of MVCC, an error anywhere along the way rolls back
 * the whole operation; we don't have to do anything special to clean up.
 */
void
AlterTable(AlterTableStmt *stmt)
{
	Relation	rel = relation_openrv(stmt->relation, AccessExclusiveLock);

	ATVerifyObject(stmt, rel);
	
	CheckTableNotInUse(rel, "ALTER TABLE");

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		LockRelationOid(RelationRelationId, RowExclusiveLock);
		LockRelationOid(TypeRelationId, RowExclusiveLock);
		LockRelationOid(DependRelationId, RowExclusiveLock);
	}

	ATController(rel, stmt->cmds, interpretInhOption(stmt->relation->inhOpt));

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/*
		 * Some ALTER TABLE commands rewrite the table, and cause new OIDs
		 * and/or relfilenodes to be assigned.
		 */
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR |
									DF_WITH_SNAPSHOT |
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
	}
}

/*
 * AlterTableInternal
 *
 * ALTER TABLE with target specified by OID
 *
 * We do not reject if the relation is already open, because it's quite
 * likely that one or more layers of caller have it open.  That means it
 * is unsafe to use this entry point for alterations that could break
 * existing query plans.  On the assumption it's not used for such, we
 * don't have to reject pending AFTER triggers, either.
 *
 * It is also unsafe to use this function for any Alter Table subcommand that
 * requires rewriting the table or creating toast tables, because that requires
 * creating relfilenodes outside of a context that understands dispatch.
 * Commands that rewrite the table include: adding or altering columns, changing
 * the tablespace, etc.
 */
void
AlterTableInternal(Oid relid, List *cmds, bool recurse)
{
	Relation	rel = relation_open(relid, AccessExclusiveLock);

	ATController(rel, cmds, recurse);
}

static void
ATController(Relation rel, List *cmds, bool recurse)
{
	List	   *wqueue = NIL;
	ListCell   *lcmd;
	bool is_partition = false;
	bool is_data_remote = RelationIsExternal(rel);

	cdb_sync_oid_to_segments();

	/* Phase 1: preliminary examination of commands, create work queue */
	foreach(lcmd, cmds)
	{
		AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);

		if (cmd->subtype == AT_PartAddInternal)
			is_partition = true;

		ATPrepCmd(&wqueue, rel, cmd, recurse, false);
	}

	if (is_partition)
		relation_close(rel, AccessExclusiveLock);
	else
		/* Close the relation, but keep lock until commit */
		relation_close(rel, NoLock);

	/* Phase 2: update system catalogs */
	ATRewriteCatalogs(&wqueue);

	/*
	 * Build list of tables OIDs that we performed SET DISTRIBUTED BY on.
	 *
	 * If we've recursed (i.e., expanded) an ALTER TABLE SET DISTRIBUTED
	 * command from a master table to its children, then we need to extract
	 * the list of tables that we created a temporary table for, from each of
	 * the sub commands, and put them in the master command. This is because
	 * we only dispatch the command on the master table to the segments, not
	 * each expanded command. We expect the segments to do the same expansion.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		AlterTableCmd *masterCmd = NULL; /* the command which was expanded */
		SetDistributionCmd *masterSetCmd = NULL;
		ListCell *lc;

		/*
		 * Iterate over the work queue looking for SET WITH DISTRIBUTED
		 * statements.
		 */
		foreach(lc, wqueue)
		{
			ListCell *lc2;
			AlteredTableInfo *tab = (AlteredTableInfo *) lfirst(lc);

			/* AT_PASS_MISC is used for SET DISTRIBUTED BY */
			List *subcmds = (List *)tab->subcmds[AT_PASS_MISC];

			foreach(lc2, subcmds)
			{
				AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lc2);

				if (cmd->subtype == AT_SetDistributedBy)
				{
					if (!masterCmd)
					{
						masterCmd = cmd;
						masterSetCmd = lsecond((List *)cmd->def);
						continue;
					}

					/*
					 * cmd->def stores the data we're sending to the QEs.
					 * The second element is where we stash QE data to drive
					 * the command.
					 */
					List *data = (List *)cmd->def;
					SetDistributionCmd *qeData = lsecond(data);

					int qeBackendId = qeData->backendId;
					if (qeBackendId != 0)
					{
						if(masterSetCmd->backendId == 0)
							masterSetCmd->backendId = qeBackendId;

						List *qeRelids = qeData->relids;
						masterSetCmd->relids = list_concat(masterSetCmd->relids, qeRelids);
					}
				}
			}
		}
	}

	/* 
	 * Phase 3: scan/rewrite tables as needed. If the data is in an external
	 * table, no need to rewrite it or to add toast.
	 */
	if (!is_data_remote)
	{
		ATRewriteTables(&wqueue);

		ATAddToastIfNeeded(&wqueue);
	}

}

/*
 * ATPrepCmd
 *
 * Traffic cop for ALTER TABLE Phase 1 operations, including simple
 * recursion and permission checks.
 *
 * Caller must have acquired AccessExclusiveLock on relation already.
 * This lock should be held until commit.
 */
static void
ATPrepCmd(List **wqueue, Relation rel, AlterTableCmd *cmd,
		  bool recurse, bool recursing)
{
	AlteredTableInfo *tab;
	int			pass;

	/* Find or create work queue entry for this table */
	tab = ATGetQueueEntry(wqueue, rel);

	/*
	 * Copy the original subcommand for each table.  This avoids conflicts
	 * when different child tables need to make different parse
	 * transformations (for example, the same column may have different column
	 * numbers in different children).
	 */
	if (recursing)
		cmd = copyObject(cmd);

	/*
	 * Do permissions checking, recursion to child tables if needed, and any
	 * additional phase-1 processing needed.
	 */
	switch (cmd->subtype)
	{
		case AT_AddColumn:		/* ADD COLUMN */
			ATSimplePermissions(rel, false);
			/*
			 * test that this is allowed for partitioning, but only if we aren't
			 * recursing.
			 */
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			/* Performs own recursion */
			ATPrepAddColumn(wqueue, rel, recurse, cmd);
			pass = AT_PASS_ADD_COL;
			break;
		case AT_AddColumnRecurse:		/* ADD COLUMN internal */
			ATSimplePermissions(rel, false);
			/* No need to do ATPartitionCheck */
			/* Performs own recursion */
			ATPrepAddColumn(wqueue, rel, recurse, cmd);
			pass = AT_PASS_ADD_COL;
			break;
		case AT_ColumnDefault:	/* ALTER COLUMN DEFAULT */

			/*
			 * We allow defaults on views so that INSERT into a view can have
			 * default-ish behavior.  This works because the rewriter
			 * substitutes default values into INSERTs before it expands
			 * rules.
			 */
			ATSimplePermissions(rel, true);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			ATPrepColumnDefault(rel, recurse, cmd);
			pass = ((ColumnDef *)(cmd->def))->raw_default ?
					AT_PASS_ADD_CONSTR : AT_PASS_DROP;
			break;
		case AT_ColumnDefaultRecurse:	/* ALTER COLUMN DEFAULT */
			ATSimplePermissions(rel, true);
			/*
			 * This is an internal Alter Table command to add default into child
			 * tables. Therefore, no need to do ATPartitionCheck.
			 */
			ATPrepColumnDefault(rel, recurse, cmd);
			pass = ((ColumnDef *)(cmd->def))->raw_default ?
					AT_PASS_ADD_CONSTR : AT_PASS_DROP;
			break;
		case AT_DropNotNull:	/* ALTER COLUMN DROP NOT NULL */
			ATSimplePermissions(rel, false);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			ATSimpleRecursion(wqueue, rel, cmd, recurse);
			/* No command-specific prep needed */
			pass = AT_PASS_DROP;
			break;
		case AT_SetNotNull:		/* ALTER COLUMN SET NOT NULL */
			ATSimplePermissions(rel, false);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATSimpleRecursion(wqueue, rel, cmd, recurse);
			if (!cmd->part_expanded)
				ATPartitionCheck(cmd->subtype, rel, false, recursing);
			/* No command-specific prep needed */
			pass = AT_PASS_ADD_CONSTR;
			break;
		case AT_SetStatistics:	/* ALTER COLUMN STATISTICS */
			ATSimpleRecursion(wqueue, rel, cmd, recurse);
			/* Performs own permission checks */
			ATPrepSetStatistics(rel, cmd->name, cmd->def);
			pass = AT_PASS_COL_ATTRS;
			break;
		case AT_SetStorage:		/* ALTER COLUMN STORAGE */
			ATSimplePermissions(rel, false);
			ATSimpleRecursion(wqueue, rel, cmd, recurse);
			/* No command-specific prep needed */
			pass = AT_PASS_COL_ATTRS;
			break;
		case AT_DropColumn:		/* DROP COLUMN */
		case AT_DropColumnRecurse:
			ATSimplePermissions(rel, false);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			/* Recursion occurs during execution phase */
			/* No command-specific prep needed except saving recurse flag */
			if (recurse)
				cmd->subtype = AT_DropColumnRecurse;
			pass = AT_PASS_DROP;
			break;
		case AT_AddIndex:		/* ADD INDEX */
			ATSimplePermissions(rel, false);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			/* Any recursion for partitioning is done in ATExecAddIndex() itself */
			/* However, if the index supports a PK or UNIQUE constraint and the
			 * relation is partitioned, we need to assure the constraint is named
			 * in a reasonable way. */
			{
				ConstrType contype = CONSTR_NULL; /* name picker will reject this */
				IndexStmt *topindexstmt = (IndexStmt*)cmd->def;
				Insist(IsA(topindexstmt, IndexStmt));
				if ( topindexstmt->isconstraint )
				{
					switch ( rel_part_status(RelationGetRelid(rel)) )
					{
						case PART_STATUS_ROOT:
 							break;
 						case PART_STATUS_INTERIOR:
 						case PART_STATUS_LEAF:
							/* Don't do this check for child parts (= internal requests). */
 							if ( ! topindexstmt->is_part_child )
 							{
 								char *what = "UNIQUE";
 								if ( contype == CONSTR_PRIMARY )
 									what = "PRIMARY KEY";
 								ereport(ERROR,
 										(errcode(ERRCODE_WRONG_OBJECT_TYPE),
 										 errmsg("can't place a %s constraint on just part of "
 												"partitioned table \"%s\"", 
 												what,RelationGetRelationName(rel)),
 										 errhint("Constrain the whole table or create a "
 												 "part-wise UNIQUE index instead.")));
 							}
 							break;
 						default:
 							break;
 					}
				}
			}
			pass = AT_PASS_ADD_INDEX;
			break;
		case AT_AddConstraint:	/* ADD CONSTRAINT */
			ATSimplePermissions(rel, false);
			/* (!recurse &&  !recursing) is supposed to detect the ONLY clause.
			 * We allow operations on the root of a partitioning hierarchy, but
			 * not ONLY the root.
			 */
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATPartitionCheck(cmd->subtype, rel, (!recurse && !recursing), recursing);
			/*
			 * Currently we recurse only for CHECK constraints, never for
			 * foreign-key constraints.  UNIQUE/PKEY constraints won't be seen
			 * here.
			 */
			/* Any recursion for partitioning/inheritance is done in ATExecAddConstraint() itself */
			if (recurse)
				cmd->subtype = AT_AddConstraintRecurse;
			pass = AT_PASS_ADD_CONSTR;
			break;
		case AT_AddConstraintRecurse: /* ADD check CONSTRAINT internal */
			/* Parent/Base CHECK constraints apply to child/part tables here.
			 * No need for ATPartitionCheck
			 */
			ATSimplePermissions(rel, false);
			pass = AT_PASS_ADD_CONSTR;
			break;
		case AT_DropConstraint:	/* DROP CONSTRAINT */
			ATSimplePermissions(rel, false);
			/* (!recurse &&  !recursing) is supposed to detect the ONLY clause.
			 * We allow operations on the root of a partitioning hierarchy, but
			 * not ONLY the root.
			 */
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATPartitionCheck(cmd->subtype, rel, (!recurse && !recursing), recursing);
			/* Performs own recursion */
			ATPrepDropConstraint(wqueue, rel, recurse, cmd);
			pass = AT_PASS_DROP;
			break;
		case AT_DropConstraintQuietly:	/* DROP CONSTRAINT for child */
			ATSimplePermissions(rel, false);
			ATSimpleRecursion(wqueue, rel, cmd, recurse);
			/* No command-specific prep needed */
			pass = AT_PASS_DROP;
			break;
		case AT_AlterColumnType:		/* ALTER COLUMN TYPE */
			ATSimplePermissions(rel, false);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			/* Performs own recursion */
			ATPrepAlterColumnType(wqueue, tab, rel, recurse, recursing, cmd);
			pass = AT_PASS_ALTER_TYPE;
			break;
		case AT_ChangeOwner:	/* ALTER OWNER */
			{
				bool do_recurse = false;

				if (Gp_role == GP_ROLE_DISPATCH)
				{
					ATPartitionCheck(cmd->subtype, rel, false, recursing);
					if (rel_is_partitioned(RelationGetRelid(rel)))
					{
						do_recurse = true;
						/* tell QE to recurse */
						cmd->def = (Node *)makeInteger(1);
					}
				}
				else if (Gp_role == GP_ROLE_EXECUTE)
				{
					if (cmd->def && intVal(cmd->def))
						do_recurse = true;
				}

				if (do_recurse)
					ATSimpleRecursion(wqueue, rel, cmd, recurse);

				pass = AT_PASS_MISC;
			}
			break;
		case AT_ClusterOn:		/* CLUSTER ON */
		case AT_DropCluster:	/* SET WITHOUT CLUSTER */
			ATSimplePermissions(rel, false);
			/* These commands never recurse */
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;
		case AT_DropOids:		/* SET WITHOUT OIDS */
			ATSimplePermissions(rel, false);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			/* Performs own recursion */
			if (rel->rd_rel->relhasoids)
			{
				AlterTableCmd *dropCmd = makeNode(AlterTableCmd);

				dropCmd->subtype = AT_DropColumn;
				dropCmd->name = pstrdup("oid");
				dropCmd->behavior = cmd->behavior;
				ATPrepCmd(wqueue, rel, dropCmd, recurse, false);
			}
			pass = AT_PASS_DROP;
			break;
		case AT_SetTableSpace:	/* SET TABLESPACE */
			ATSimplePermissionsRelationOrIndex(rel);
			/* This command never recurses, but the offered relation may be partitioned, 
			 * in which case, we need to act as if the command specified the top-level
			 * list of parts.
			 */
			if (Gp_role == GP_ROLE_DISPATCH && rel_is_partitioned(RelationGetRelid(rel)) )
			{
				PartitionNode *pnode = NULL;
				List *all_oids = NIL;
				
				pnode = get_parts(RelationGetRelid(rel), 
								  0, /**/
								  InvalidOid, /**/
								  false, /**/
								  true /*includesubparts*/);
				
				all_oids = lcons_oid(RelationGetRelid(rel), all_partition_relids(pnode));
				
				Assert( cmd->partoids == NIL );
				cmd->partoids = all_oids;
				
				ATPartsPrepSetTableSpace(wqueue, rel, cmd, all_oids);
			}
			else if (Gp_role == GP_ROLE_EXECUTE && cmd->partoids)
			{
				ATPartsPrepSetTableSpace(wqueue, rel, cmd, cmd->partoids);
			}
			else if (Gp_role == GP_ROLE_UTILITY)
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
 						 errmsg("SET TABLESPACE is not supported in utility mode")));
			}
			else
				ATPrepSetTableSpace(tab, rel, cmd->name);
			pass = AT_PASS_MISC;	/* doesn't actually matter */
			break;
		case AT_SetRelOptions:	/* SET (...) */
		case AT_ResetRelOptions:		/* RESET (...) */
			ATSimplePermissionsRelationOrIndex(rel);
			/* This command never recurses */
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;
		case AT_SetDistributedBy:	/* SET DISTRIBUTED BY */
			if ( !recursing ) /* MPP-5772, MPP-5784 */
			{
				Oid relid = RelationGetRelid(rel);
				PartStatus ps = rel_part_status(relid);

				ATExternalPartitionCheck(cmd->subtype, rel, recursing);

				if ( recurse ) /* Normal ALTER TABLE */
				{
					switch (ps)
					{
						case PART_STATUS_NONE:
						case PART_STATUS_ROOT:
						case PART_STATUS_LEAF:
							break;

						case PART_STATUS_INTERIOR:
							/*Reject interior branches of partitioned tables.*/
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
									 errmsg("can't set the distribution policy of \"%s\"",
											RelationGetRelationName(rel)),
									 errhint("Distribution policy may be set for an entire partitioned table or one of its leaf parts; not for an interior branch.")));
							break; /* tidy */
					}
				}
				else /* ALTER TABLE ONLY */
				{
					switch (ps)
					{
						case PART_STATUS_NONE:
						case PART_STATUS_LEAF:
							break;

						case PART_STATUS_ROOT:
						case PART_STATUS_INTERIOR:
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
									 errmsg("can't set the distribution policy "
											"of ONLY \"%s\"",
											RelationGetRelationName(rel)),
									 errhint("Distribution policy may be set "
											 "for an entire partitioned table"
											 " or one of its leaf parts."
											 )));
							break; /* tidy */
					}
				}

				if ( ps == PART_STATUS_LEAF )
				{
					Oid ptrelid = rel_partition_get_master(relid);
					List *dist_cnames = lsecond((List*)cmd->def);

					/*	might be null if no policy set, e.g. just
					 *  a change of storage options...
					 */
					if (dist_cnames)
					{
						Relation ptrel = heap_open(ptrelid,
												   AccessShareLock);
						Assert(IsA(dist_cnames, List));

						if (! can_implement_dist_on_part(ptrel,
														 dist_cnames) )
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("cannot SET DISTRIBUTED BY for %s",
											RelationGetRelationName(rel)),
									 errhint("Leaf distribution policy must be"
											 " random or match that of the"
											 " entire partitioned table.")
									 ));
						heap_close(ptrel, AccessShareLock);
					}
				}
			}

			ATSimplePermissions(rel, false);
			ATSimpleRecursion(wqueue, rel, cmd, recurse);
			pass = AT_PASS_MISC;
			break;
		case AT_EnableTrig:		/* ENABLE TRIGGER variants */
		case AT_EnableAlwaysTrig:
		case AT_EnableReplicaTrig:
		case AT_EnableTrigAll:
		case AT_EnableTrigUser:
		case AT_DisableTrig:	/* DISABLE TRIGGER variants */
		case AT_DisableTrigAll:
		case AT_DisableTrigUser:
			ATSimplePermissions(rel, false);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			/* These commands never recurse */
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;
		case AT_EnableRule:		/* ENABLE/DISABLE RULE variants */
		case AT_EnableAlwaysRule:
		case AT_EnableReplicaRule:
		case AT_DisableRule:
		case AT_AddInherit:		/* INHERIT / NO INHERIT */
		case AT_DropInherit:
			ATSimplePermissions(rel, false);
			ATPartitionCheck(cmd->subtype, rel, true, recursing);
			/* These commands never recurse */
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;
			/* CDB: Partitioned Table commands */
		case AT_PartExchange:			/* Exchange */
			
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			
			if (Gp_role == GP_ROLE_UTILITY)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
 						 errmsg("EXCHANGE is not supported in utility mode")));
			
			pass = AT_PASS_MISC;
			
			ATPrepExchange(rel, (AlterPartitionCmd *)cmd->def);
			
			break;
		case AT_PartMerge:
		{
			AlterPartitionCmd  *pc    	= (AlterPartitionCmd *) cmd->def;
			PgPartRule   	   *prule1	= NULL;
			PgPartRule		   *prule2	= NULL;
			PgPartRule		   *prule3	= NULL;
			ListCell		   *lc;
			List			   *source_rels = NIL;
			Relation			target = NULL;

			ATPartitionCheck(cmd->subtype, rel, false, recursing);

			if (Gp_role == GP_ROLE_UTILITY)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
 						 errmsg("MERGE is not supported in utility mode")));
			else if (Gp_role == GP_ROLE_DISPATCH)
			{
				Relation srel;
				AlterPartitionId *pid = (AlterPartitionId *) pc->partid;

				prule1 = get_part_rule(rel, pid, true, true,
									   NULL, false);

				prule2 = get_part_rule(rel, (AlterPartitionId *)pc->arg1,
									   true, true,
									   NULL, false);

				if (pc->arg2)
				{
					AlterPartitionCmd *pc2 = (AlterPartitionCmd *)pc->arg2;

					prule3 = get_part_rule(rel,
										   (AlterPartitionId *)pc2->partid,
										   true, true, NULL, false);
				}

				if (prule1)
				{
					/* cannot exchange a hash partition */
					if ('h' == prule1->pNode->part->parkind)
						ereport(ERROR,
							(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
								 errmsg("cannot merge HASH partition%s "
									    "of relation \"%s\"",
										prule1->partIdStr,
										RelationGetRelationName(rel))));
				}

				srel = heap_open(prule1->topRule->parchildrelid,
								 AccessExclusiveLock);
				source_rels = lappend(source_rels, srel);

				srel = heap_open(prule2->topRule->parchildrelid,
								 AccessExclusiveLock);
				if (prule3)
				{
					source_rels = lappend(source_rels, srel);
					target = heap_open(prule3->topRule->parchildrelid,
									   AccessExclusiveLock);
				}
				else
					target = srel;
			}
			else
			{
				Relation srel;
				srel = heap_openrv((RangeVar *)pc->partid,
								   AccessExclusiveLock);
				source_rels = lappend(source_rels, srel);

				srel = heap_openrv((RangeVar *)pc->arg1, AccessExclusiveLock);

				if (pc->arg2)
				{
					source_rels = lappend(source_rels, srel);
					target = heap_openrv((RangeVar *)pc->arg2,
										 AccessExclusiveLock);
				}
				else
					target = heap_openrv((RangeVar *)pc->arg1,
										 AccessExclusiveLock);
			}

			ATSimplePermissions(rel, false);

			foreach(lc, source_rels)
				ATSimplePermissions(lfirst(lc), false);

			ATSimplePermissions(target, false);

			foreach(lc, source_rels)
				heap_close(lfirst(lc), NoLock);

			heap_close(target, NoLock);

			pass = AT_PASS_MISC;
			break;
		}

		case AT_PartSplit:				/* Split */
		{
			AlterPartitionCmd	*pc    	= (AlterPartitionCmd *) cmd->def;

			ATPartitionCheck(cmd->subtype, rel, false, recursing);

			if (Gp_role == GP_ROLE_UTILITY)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
 						 errmsg("SPLIT is not supported in utility mode")));
			else if (Gp_role == GP_ROLE_DISPATCH)
			{
				PgPartRule			*prule1	= NULL;
				PgPartRule			*prule2	= NULL;
				PgPartRule			*prule3	= NULL;
				PgPartRule			*prule_specified	= NULL;
				Relation			 target = NULL;
				
				AlterPartitionId *pid = (AlterPartitionId *) pc->partid;
				AlterPartitionCmd *cmd2;
				Node *at;
				bool is_at = true;
				List *todo;
				ListCell *l;
				bool rollup_vals = false;

				/*
				 * Need to do a bunch of validation:
				 * 0) Target exists
				 * 0.5) Shouldn't have children
				 * 1) The usual permissions checks
				 * 2) Not called on HASH
				 * 3) AT () parameter falls into constraint specified
				 * 4) INTO partitions don't exist except for the one being split
				 */

				/* We'll error out if it doesn't exist */
				prule1 = get_part_rule(rel, pid, true, true, NULL, false);

				if (prule1->topRule->children)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot split partition with child "
									"partitions"),
							 errhint("Try splitting the child partitions.")));

				target = heap_open(prule1->topRule->parchildrelid,
								   AccessExclusiveLock);

				if (linitial((List *)pc->arg1))
					is_at = false;

				if (prule1->topRule->parisdefault &&
					prule1->pNode->part->parkind == 'r' &&
					is_at)
				{
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("AT clause cannot be used when splitting "
									"a default RANGE partition")));
				}
				else if (prule1->pNode->part->parkind == 'l' && !is_at)
				{
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("cannot SPLIT DEFAULT PARTITION "
									"with LIST"),
							errhint("Use SPLIT with the AT clause instead.")));

				}

				if (prule1->pNode->part->parkind == 'h')
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("SPLIT is not supported for "
									"HASH partitions")));

				if (linitial((List *)pc->arg1))
					is_at = false;

				todo = (List *)pc->arg1;

				pc->arg1 = (Node *)NIL;
				foreach(l, todo)
				{
					List *l1;
					ListCell *lc;
					Node *t = lfirst(l);

					if (!t)
					{
						pc->arg1 = (Node *)lappend((List *)pc->arg1, NULL);
						continue;
					}
					else
					{
						if (is_at)
							l1 = (List *)t;
						else
						{
							Node *n = t;
							PartitionRangeItem *ri = (PartitionRangeItem *)n;

							Assert(IsA(n, PartitionRangeItem));

							l1 = ri->partRangeVal;
						}
					}

					if (IsA(linitial(l1), A_Const) ||
						IsA(linitial(l1), TypeCast) ||
						IsA(linitial(l1), FuncExpr) ||
						IsA(linitial(l1), OpExpr))
					{
						List *new = NIL;
						ListCell *lc;

						foreach(lc, l1)
							new = lappend(new, list_make1(lfirst(lc)));

						l1 = new;
						rollup_vals = true;
					}
					else
						Insist(IsA(linitial(l1), List));

					foreach(lc, l1)
					{
						List *vals = lfirst(lc);
						ListCell *lc2;
						int i = 0;
						ParseState *pstate = make_parsestate(NULL);

						if (prule1->pNode->part->parnatts != list_length(vals))
						{
							AttrNumber parnatts = prule1->pNode->part->parnatts;
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("partition being split has "
											"%i column%s but parameter "
											"has %i column%s",
											parnatts,
											parnatts > 1 ? "s" : "",
											list_length(vals),
										   list_length(vals) > 1 ? "s" : "")));
						}

						vals = (List *)transformExpressionList(pstate, vals);

						free_parsestate(pstate);

						foreach(lc2, vals)
						{
							Node *n = lfirst(lc2);
							AttrNumber attnum =
									prule1->pNode->part->paratts[i];
							Oid typid =
									rel->rd_att->attrs[attnum - 1]->atttypid;
							int32 typmod =
									rel->rd_att->attrs[attnum - 1]->atttypmod;
							char parkind = prule1->pNode->part->parkind;
							PartitionByType t = (parkind == 'r') ?
												 PARTTYP_RANGE : PARTTYP_LIST;

							n = coerce_partition_value(n, typid, typmod, t);

							lfirst(lc2) = n;

							i++;
						}
						lfirst(lc) = vals;
					}

					if (rollup_vals)
					{
						/* now, unroll */
						if (prule1->pNode->part->parkind == 'r')
						{
							List *new = NIL;
							ListCell *lc;
							Node *n;

							foreach(lc, l1)
								new = lappend(new, linitial(lfirst(lc)));

							if (is_at)
								n = (Node *)new;
							else
							{
								PartitionRangeItem *ri = lfirst(l);

								ri->partRangeVal = new;
								n = (Node *)ri;
							}
							pc->arg1 = (Node *)lappend((List *)pc->arg1, n);
						}
						else
						{
							/* for LIST, leave it as is */
							pc->arg1 = (Node *)lappend((List *)pc->arg1,
														l1);
						}
					}
					else
						pc->arg1 = (Node *)lappend((List *)pc->arg1, l1);
				}

				at = lsecond((List *)pc->arg1);
				if (prule1->pNode->part->parkind == 'l')
				{

					if (prule1->topRule->parisdefault)
					{
						/*
						 * Check that AT() value doesn't exist in any existing
						 * definition.
						 */

					}
					else
					{
						/* all elements in AT() must match */
						ListCell *lcat;
						List *lv = (List *)prule1->topRule->parlistvalues;
						int16 nkeys = prule1->pNode->part->parnatts;
						List *atlist = (List *)at;
						int nmatches = 0;

						foreach(lcat, atlist)
						{
							List *cols = lfirst(lcat);
							ListCell *parvals = list_head(lv);
							bool found = false;

							Assert(list_length(cols) == nkeys);

							while (parvals)
							{
								List *vals = lfirst(parvals);
								ListCell *lcv = list_head(vals);
								ListCell *lcc = list_head(cols);
								int parcol;
								bool matched = true;

								for (parcol = 0; parcol < nkeys; parcol++)
								{
									Oid			opclass;
									Oid			opfam;
									Oid			funcid;
									Const	   *v;
									Const	   *c;
									Datum		d;
									Oid			intype;

									opclass =  prule1->pNode->part->parclass[parcol];
									intype = get_opclass_input_type(opclass);
									opfam = get_opclass_family(opclass);
									funcid = get_opfamily_proc(opfam, intype, intype,
															   BTORDER_PROC);

									v = lfirst(lcv);
									c = lfirst(lcc);
									if (v->constisnull && c->constisnull)
										continue;
									else if (v->constisnull || c->constisnull)
									{
										matched = false;
										break;
									}

									d = OidFunctionCall2(funcid, c->constvalue,
														 v->constvalue);

									if (DatumGetInt32(d) != 0)
									{
										matched = false;
										break;
									}

									lcv = lnext(lcv);
									lcc = lnext(lcc);
								}

								if (!matched)
								{
									parvals = lnext(parvals);
									continue;
								}
								else
								{
									found = true;
									nmatches++;
									break;
								}
							}
							if (!found)
								ereport(ERROR,
										(errcode(ERRCODE_WRONG_OBJECT_TYPE),
										errmsg("AT clause parameter is not "
											   "a member of the target "
											   "partition specification")));


						}

						if (nmatches >= list_length(lv))
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("AT clause cannot contain all "
											"values in partition%s",
											prule1->partIdStr)));

					}
				}
				else
				{
					/* XXX: handle split of default */
					if (prule1->topRule->parisdefault)
					{
					}
					else
					{
						/* at must be in range */
						Const	   *start	  = NULL;
						Const	   *end	  = NULL;
						Const	   *atval;
						Oid			opclass;
						Oid			opfam;
						Oid			funcid;
						Oid			intype;
						Node	   *n;
						Datum		res;
						int32		ret;
						bool		in_range = true;

						opclass =  prule1->pNode->part->parclass[0];
						intype = get_opclass_input_type(opclass);
						opfam = get_opclass_family(opclass);
						funcid = get_opfamily_proc(opfam, intype, intype,
												   BTORDER_PROC);

						n = (Node *)linitial((List *)at);

						Assert(IsA(n, Const));

						atval = (Const *)n;

						/* XXX: ignore composite key range for now */
						if (prule1->topRule->parrangestart)
							start = linitial((List *)prule1->topRule->parrangestart);

						/* MPP-6589: allow "open" start or end */
						if (start)
						{
							Insist(exprType((Node *)n) == exprType((Node *)start));
							res = OidFunctionCall2(funcid, start->constvalue,
												   atval->constvalue);

							ret = DatumGetInt32(res);
						}
						else
						{
							/* MPP-6589: no start - check end */
							Assert(prule1->topRule->parrangeend);
							ret = -1;
						}

						if (ret > 0)
							in_range = false;
						else if (ret == 0)
						{
							if (!prule1->topRule->parrangestartincl)
								in_range = false;
						}
						else
						{
							/* in the range, test end */
							if (prule1->topRule->parrangeend)
							{
								end = linitial((List *)prule1->topRule->parrangeend);
								res = OidFunctionCall2(funcid,
													   atval->constvalue,
											   		   end->constvalue);

								ret = DatumGetInt32(res);

								if (ret > 0)
									in_range = false;
								else if (ret == 0)
									if (!prule1->topRule->parrangeendincl)
										in_range = false;
							}
							/* if no end, remains "in range" */
						}

						if (!in_range)
						{
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
									errmsg("AT clause parameter is not "
										   "a member of the target "
										   "partition specification")));
						}
					}
				}

				/*
				 * pc->partid == target
				 * pc->arg1 == AT
				 * pc->arg2 == AlterPartitionCmd (partid == 1, arg1 = 2)
				 */
				if (PointerIsValid(pc->arg2))
				{
					cmd2 = (AlterPartitionCmd *)pc->arg2;

					prule2 = get_part_rule(rel,
										   (AlterPartitionId *)cmd2->partid,
										   false, false, NULL, false);

					prule3 = get_part_rule(rel, (AlterPartitionId *)cmd2->arg1,
										   false, false, NULL, false);
					/* both can't exist */
					if (prule2 && prule3)
					{
						ereport(ERROR,
								(errcode(ERRCODE_DUPLICATE_OBJECT),
								 errmsg("both INTO partitions "
										"already exist")));
					}
					else if (prule1->topRule->parisdefault &&
							 !prule2 && !prule3)
					{
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("default partition name missing "
										"from INTO clause")));

					}
				}
				
				/* MPP-18395 begs for a last minute check that we aren't about to
				 * split into an existing partition.  The case that prompts the check
				 * has to do with specifying an existing partition by name, but it
				 * may (someday) be possible to specify one by rank or constraint,
				 * so lets just refuse to use an existing partition that isn't the
				 * one we're splitting.  (We're going to drop this one, so it's okay
				 * recycle it.)
				 *
				 * Examples: 
				 *    t is target part to split, 
				 *    n, m are new parts,
				 *    x, y are existing parts != t
				 *
				 * t -> t, n -- ok
				 * t -> n, t -- ok
				 * t -> n, m -- ok
				 * t -> n, x -- no
				 * t -> x, n -- no
				 * t -> x, y -- no (unexpected)
				 *
				 * By the way, at this point
				 *   prule1 refers to the part to split
				 *   prule2 refers to the first INTO part
				 *   prule3 refers to the seconde INTO part
				 */
				Insist( prule2 == NULL || prule3 == NULL );
				
				if ( prule2 != NULL )
				    prule_specified = prule2;
				else if ( prule3 != NULL )
					prule_specified = prule3;
				else
					prule_specified = NULL;
					
				if ( prule_specified &&
					 prule_specified->topRule->parruleid != prule1->topRule->parruleid 
					 )
				{
					ereport(ERROR,
							(errcode(ERRCODE_DUPLICATE_OBJECT),
							 errmsg("cannot split into an existing partition")));
				}
				
				ATSimplePermissions(target, false);
				heap_close(target, NoLock);
			}
			else
			{
				Insist(IsA(pc->partid, Integer));
				Insist(IsA(pc->arg1, RangeVar));
				Insist(IsA(pc->arg2, RangeVar));
			}
			pass = AT_PASS_MISC;
			break;
		}

		case AT_PartAlter:				/* Alter */
			if ( Gp_role == GP_ROLE_DISPATCH && recurse && ! recursing )
			{
				AlterTableCmd *basecmd = basic_AT_cmd(cmd);
				
				switch ( basecmd->subtype )
				{
					case AT_SetTableSpace:
						cmd->partoids = basic_AT_oids(rel,cmd);
						ATPartsPrepSetTableSpace(wqueue, rel, basecmd, cmd->partoids);
						break;
					
					default:
						/* Not a for-each-subsumed-part-type command. */
						break;
				}
			}
			else if (Gp_role == GP_ROLE_EXECUTE && cmd->partoids)
			{
				AlterTableCmd *basecmd = basic_AT_cmd(cmd);

				switch ( basecmd->subtype )
				{
					case AT_SetTableSpace:
						ATPartsPrepSetTableSpace(wqueue, rel, basecmd, cmd->partoids);
						break;
						
					default:
						/* Not a for-each-subsumed-part-type command. */
						break;
				}
			}
			else if (Gp_role == GP_ROLE_UTILITY)
			{
				if (basic_AT_cmd(cmd)->subtype == AT_SetTableSpace)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("SET TABLESPACE is not supported in utility mode")));
			}
			pass = AT_PASS_MISC;
			break;				

		case AT_PartSetTemplate:		/* Set Subpartition Template */
			if (!gp_allow_non_uniform_partitioning_ddl)
			{
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Cannot modify subpartition template for partitioned table")));
			}
		case AT_PartAdd:				/* Add */
		case AT_PartAddForSplit:		/* Add, as part of a split */
		case AT_PartCoalesce:			/* Coalesce */
		case AT_PartDrop:				/* Drop */
		case AT_PartRename:				/* Rename */

		case AT_PartTruncate:			/* Truncate */
		case AT_PartAddInternal:		/* internal partition creation */
		case AT_PartModify:             /* Modify */
			ATSimplePermissions(rel, false);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
				/* XXX XXX XXX */
			/* This command never recurses */
			/* No command-specific prep needed */
			/* XXX: check permissions */
			pass = AT_PASS_MISC;
			break;

		default:				/* oops */
			elog(ERROR, "unrecognized alter table type: %d",
				 (int) cmd->subtype);
			pass = 0;			/* keep compiler quiet */
			break;
	}

	/* Add the subcommand to the appropriate list for phase 2 */
	tab->subcmds[pass] = lappend(tab->subcmds[pass], cmd);
}

/*
 * ATRewriteCatalogs
 *
 * Traffic cop for ALTER TABLE Phase 2 operations.	Subcommands are
 * dispatched in a "safe" execution order (designed to avoid unnecessary
 * conflicts).
 */
static void
ATRewriteCatalogs(List **wqueue)
{
	int			pass;
	ListCell   *ltab;

	/*
	 * We process all the tables "in parallel", one pass at a time.  This is
	 * needed because we may have to propagate work from one table to another
	 * (specifically, ALTER TYPE on a foreign key's PK has to dispatch the
	 * re-adding of the foreign key constraint to the other table).  Work can
	 * only be propagated into later passes, however.
	 */
	for (pass = 0; pass < AT_NUM_PASSES; pass++)
	{
		/* Go through each table that needs to be processed */
		foreach(ltab, *wqueue)
		{
			AlteredTableInfo *tab = (AlteredTableInfo *) lfirst(ltab);
			List	   *subcmds = tab->subcmds[pass];
			Relation	rel;
			ListCell   *lcmd;

			if (subcmds == NIL)
				continue;

			/*
			 * Exclusive lock was obtained by phase 1, needn't get it again
			 */
			rel = relation_open(tab->relid, NoLock);

			foreach(lcmd, subcmds)
			{
				AlterTableCmd *atc = lfirst(lcmd);

				ATExecCmd(wqueue, tab, &rel, atc);

				/*
				 * SET DISTRIBUTED BY() calls RelationForgetRelation(),
				 * which will scribble on rel, so we must re-open it.
				 */
				if (atc->subtype == AT_SetDistributedBy)
					rel = relation_open(tab->relid, NoLock);
			}
			/*
			 * After the ALTER TYPE pass, do cleanup work (this is not done in
			 * ATExecAlterColumnType since it should be done only once if
			 * multiple columns of a table are altered).
			 */
			if (pass == AT_PASS_ALTER_TYPE)
				ATPostAlterTypeCleanup(wqueue, tab);

			relation_close(rel, NoLock);
		}
	}

}

static void
ATAddToastIfNeeded(List **wqueue)
{
	ListCell   *ltab;

	/*
	 * Check to see if a toast table must be added, if we executed any
	 * subcommands that might have added a column or changed column storage.
	 */
	foreach(ltab, *wqueue)
	{
		AlteredTableInfo *tab = (AlteredTableInfo *) lfirst(ltab);

		if (tab->relkind == RELKIND_RELATION &&
			(tab->subcmds[AT_PASS_ADD_COL] ||
			 tab->subcmds[AT_PASS_ALTER_TYPE] ||
			 tab->subcmds[AT_PASS_COL_ATTRS]))
		{
			bool is_part = !rel_needs_long_lock(tab->relid);

			AlterTableCreateToastTable(tab->relid, is_part);
		}
	}
}

/*
 * ATExecCmd: dispatch a subcommand to appropriate execution routine
 */
static void
ATExecCmd(List **wqueue, AlteredTableInfo *tab, Relation *rel_p,
		  AlterTableCmd *cmd)
{
	Relation rel = *rel_p;

	switch (cmd->subtype)
	{
		case AT_AddColumn:		/* ADD COLUMN */
		case AT_AddColumnRecurse:
			ATExecAddColumn(tab, rel, (ColumnDef *) cmd->def);
			break;
		case AT_ColumnDefault:	/* ALTER COLUMN DEFAULT */
		case AT_ColumnDefaultRecurse:
			ATExecColumnDefault(rel, cmd->name, (ColumnDef *) cmd->def);
			break;
		case AT_DropNotNull:	/* ALTER COLUMN DROP NOT NULL */
			ATExecDropNotNull(rel, cmd->name);
			break;
		case AT_SetNotNull:		/* ALTER COLUMN SET NOT NULL */
			ATExecSetNotNull(tab, rel, cmd->name);
			break;
		case AT_SetStatistics:	/* ALTER COLUMN STATISTICS */
			ATExecSetStatistics(rel, cmd->name, cmd->def);
			break;
		case AT_SetStorage:		/* ALTER COLUMN STORAGE */
			ATExecSetStorage(rel, cmd->name, cmd->def);
			break;
		case AT_DropColumn:		/* DROP COLUMN */
			ATExecDropColumn(wqueue, rel, cmd->name,
							 cmd->behavior, false, false);
			break;
		case AT_DropColumnRecurse:		/* DROP COLUMN with recursion */
			ATExecDropColumn(wqueue, rel, cmd->name,
							 cmd->behavior, true, false);
			break;
		case AT_AddIndex:		/* ADD INDEX */
			ATExecAddIndex(tab, rel, (IndexStmt *) cmd->def, false);
			break;
		case AT_ReAddIndex:		/* ADD INDEX */
			ATExecAddIndex(tab, rel, (IndexStmt *) cmd->def, true);
			break;
		case AT_AddConstraint:	/* ADD CONSTRAINT */
			ATExecAddConstraint(tab, rel, cmd->def, false);
			break;
		case AT_AddConstraintRecurse: /* ADD CONSTRAINT with recursion: internal */
			ATExecAddConstraint(tab, rel, cmd->def, true);
			break;
		case AT_DropConstraint:	/* DROP CONSTRAINT */
			ATExecDropConstraint(rel, cmd->name, cmd->behavior, false);
			break;
		case AT_DropConstraintQuietly:	/* DROP CONSTRAINT for child */
			ATExecDropConstraint(rel, cmd->name, cmd->behavior, true);
			break;
		case AT_AlterColumnType:		/* ALTER COLUMN TYPE */
			ATExecAlterColumnType(tab, rel, cmd->name, (TypeName *) cmd->def);
			break;
		case AT_ChangeOwner:	/* ALTER OWNER */
			ATExecChangeOwner(RelationGetRelid(rel),
							  get_roleid_checked(cmd->name),
							  false);
			break;
		case AT_ClusterOn:		/* CLUSTER ON */
			ATExecClusterOn(rel, cmd->name);
			break;
		case AT_DropCluster:	/* SET WITHOUT CLUSTER */
			ATExecDropCluster(rel);
			break;
		case AT_DropOids:		/* SET WITHOUT OIDS */

			/*
			 * Nothing to do here; we'll have generated a DropColumn
			 * subcommand to do the real work
			 */
			break;
		case AT_SetTableSpace:	/* SET TABLESPACE */

			/*
			 * Nothing to do here; Phase 3 does the work
			 */
			break;
		case AT_SetRelOptions:	/* SET (...) */
			ATExecSetRelOptions(rel, (List *) cmd->def, false);
			break;
		case AT_ResetRelOptions:		/* RESET (...) */
			ATExecSetRelOptions(rel, (List *) cmd->def, true);
			break;

		case AT_EnableTrig:		/* ENABLE TRIGGER name */
			ATExecEnableDisableTrigger(rel, cmd->name,
									   TRIGGER_FIRES_ON_ORIGIN, false);
			break;
		case AT_EnableAlwaysTrig:		/* ENABLE ALWAYS TRIGGER name */
			ATExecEnableDisableTrigger(rel, cmd->name,
									   TRIGGER_FIRES_ALWAYS, false);
			break;
		case AT_EnableReplicaTrig:		/* ENABLE REPLICA TRIGGER name */
			ATExecEnableDisableTrigger(rel, cmd->name,
									   TRIGGER_FIRES_ON_REPLICA, false);
			break;
		case AT_DisableTrig:	/* DISABLE TRIGGER name */
			ATExecEnableDisableTrigger(rel, cmd->name,
									   TRIGGER_DISABLED, false);
			break;
		case AT_EnableTrigAll:	/* ENABLE TRIGGER ALL */
			ATExecEnableDisableTrigger(rel, NULL,
									   TRIGGER_FIRES_ON_ORIGIN, false);
			break;
		case AT_DisableTrigAll:	/* DISABLE TRIGGER ALL */
			ATExecEnableDisableTrigger(rel, NULL,
									   TRIGGER_DISABLED, false);
			break;
		case AT_EnableTrigUser:	/* ENABLE TRIGGER USER */
			ATExecEnableDisableTrigger(rel, NULL,
									   TRIGGER_FIRES_ON_ORIGIN, true);
			break;
		case AT_DisableTrigUser:		/* DISABLE TRIGGER USER */
			ATExecEnableDisableTrigger(rel, NULL,
									   TRIGGER_DISABLED, true);
			break;

		case AT_EnableRule:		/* ENABLE RULE name */
			ATExecEnableDisableRule(rel, cmd->name,
									RULE_FIRES_ON_ORIGIN);
			break;
		case AT_EnableAlwaysRule:		/* ENABLE ALWAYS RULE name */
			ATExecEnableDisableRule(rel, cmd->name,
									RULE_FIRES_ALWAYS);
			break;
		case AT_EnableReplicaRule:		/* ENABLE REPLICA RULE name */
			ATExecEnableDisableRule(rel, cmd->name,
									RULE_FIRES_ON_REPLICA);
			break;
		case AT_DisableRule:	/* DISABLE RULE name */
			ATExecEnableDisableRule(rel, cmd->name,
									RULE_DISABLED);
			break;

		case AT_AddInherit:
			ATExecAddInherit(rel, (Node *) cmd->def);
			break;
		case AT_DropInherit:
			ATExecDropInherit(rel, (RangeVar *) cmd->def, false);
			break;
		case AT_SetDistributedBy:	/* SET DISTRIBUTED BY */
			ATExecSetDistributedBy(rel, (Node *) cmd->def, cmd);
			break;
			/* CDB: Partitioned Table commands */
		case AT_PartAdd:				/* Add */
		case AT_PartAddForSplit:		/* Add, as part of a Split */
			ATPExecPartAdd(tab, rel, (AlterPartitionCmd *) cmd->def,
						   cmd->subtype);
            break;
		case AT_PartAlter:				/* Alter */
            ATPExecPartAlter(wqueue, tab, rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartCoalesce:			/* Coalesce */
            ATPExecPartCoalesce(rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartDrop:				/* Drop */
            ATPExecPartDrop(rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartExchange:			/* Exchange */
            ATPExecPartExchange(tab, rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartMerge:				/* Merge */
            ATPExecPartMerge(rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartModify:				/* Modify */
            ATPExecPartModify(rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartRename:				/* Rename */
            ATPExecPartRename(rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartSetTemplate:		/* Set Subpartition Template */
			ATPExecPartSetTemplate(tab, rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartSplit:				/* Split */
			/*
			 * We perform heap_open and heap_close several times on
			 * the relation object referenced by rel in this
			 * method. After the method returns, we expect to
			 * reference a valid relcache object through rel.
			 */
            ATPExecPartSplit(&rel, (AlterPartitionCmd *) cmd->def);
			*rel_p = rel;
            break;
		case AT_PartTruncate:			/* Truncate */
			ATPExecPartTruncate(rel, (AlterPartitionCmd *) cmd->def);
            break;
		case AT_PartAddInternal:
			ATExecPartAddInternal(rel, cmd->def);
			break;
		default:				/* oops */
			elog(ERROR, "unrecognized alter table type: %d",
				 (int) cmd->subtype);
			break;
	}

	/*
	 * Bump the command counter to ensure the next subcommand in the sequence
	 * can see the changes so far
	 */
	CommandCounterIncrement();
}

/*
 * ATRewriteTables: ALTER TABLE phase 3
 */
static void
ATRewriteTables(List **wqueue)
{
	ListCell   *ltab;
	char        relstorage;

	/* Go through each table that needs to be checked or rewritten */
	foreach(ltab, *wqueue)
	{
		AlteredTableInfo *tab = (AlteredTableInfo *) lfirst(ltab);
		/*
		 * 'OldHeap' can be an AO or external table, but kept the upstream variable name
		 * to minimize the diff.
		 */
		Relation	OldHeap;
		Oid			newTableSpace;
		Oid 		oldTableSpace;
		bool		hasIndexes;

		/* We will lock the table iff we decide to actually rewrite it */
		OldHeap = relation_open(tab->relid, NoLock);
		if (RelationIsExternal(OldHeap))
		{
			heap_close(OldHeap, NoLock);
			continue;
		}

		oldTableSpace = OldHeap->rd_rel->reltablespace;
		newTableSpace = tab->newTableSpace ? tab->newTableSpace : oldTableSpace;
		relstorage    = OldHeap->rd_rel->relstorage;
		{
			List	   *indexIds;

			indexIds = RelationGetIndexList(OldHeap);
			hasIndexes = (indexIds != NIL);
            list_free(indexIds);
		}

		/*
		 * There are two cases where we will rewrite the table, for these cases
		 * run the necessary sanity checks.
		 */
		if (tab->newvals != NIL || tab->newTableSpace)
		{
			/*
			 * We can never allow rewriting of shared or nailed-in-cache
			 * relations, because we can't support changing their relfilenode
			 * values.
			 */
			if (OldHeap->rd_rel->relisshared || OldHeap->rd_isnailed)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot rewrite system relation \"%s\"",
								RelationGetRelationName(OldHeap))));

			/*
			 * Don't allow rewrite on temp tables of other backends ... their
			 * local buffer manager is not going to cope.
			 */
			if (isOtherTempNamespace(RelationGetNamespace(OldHeap)))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("cannot rewrite temporary tables of other sessions")));
		}
		heap_close(OldHeap, NoLock);

		if (tab->newvals && relstorage == RELSTORAGE_AOCOLS)
		{
			/*
			 * ADD COLUMN for CO can be optimized only if it is the
			 * only subcommand being performed.
			 */
			bool canOptimize = true;
			for (int i=0; i < AT_NUM_PASSES && canOptimize; ++i)
			{
				if (i != AT_PASS_ADD_COL && tab->subcmds[i])
					canOptimize = false;
			}
			if (canOptimize && ATAocsNoRewrite(tab))
				continue;
		}
		/*
		 * We only need to rewrite the table if at least one column needs to
		 * be recomputed, or we are removing the OID column.
		 */
		if (tab->newvals != NIL || tab->new_dropoids)
		{
			/* Build a temporary relation and copy data */
			char		NewHeapName[NAMEDATALEN];
			Oid         OIDNewHeap;
			ObjectAddress object;

			/*
			 * Create the new heap, using a temporary name in the same
			 * namespace as the existing table.  NOTE: there is some risk of
			 * collision with user relnames.  Working around this seems more
			 * trouble than it's worth; in particular, we can't create the new
			 * heap in a different namespace from the old, or we will have
			 * problems with the TEMP status of temp tables.
			 */
			snprintf(NewHeapName, sizeof(NewHeapName),
					 "pg_temp_%u", tab->relid);

            OIDNewHeap = make_new_heap(tab->relid, NewHeapName, newTableSpace,
									   hasIndexes);

			/*
			 * Copy the heap data into the new table with the desired
			 * modifications, and test the current data within the table
			 * against new constraints generated by ALTER TABLE commands.
			 */
			ATRewriteTable(tab, OIDNewHeap);

			/*
			 * Swap the physical files of the old and new heaps.  Since we are
			 * generating a new heap, we can use RecentXmin for the table's
			 * new relfrozenxid because we rewrote all the tuples on
			 * ATRewriteTable, so no older Xid remains on the table.
			 *
			 * MPP-17516 - The fourth argument to swap_relation_files is 
			 * "swap_stats", and it dictates whether the relpages and 
			 * reltuples of the fake relfile should be copied over to our 
			 * original pg_class tuple. We do not want to do this in the case 
			 * of ALTER TABLE rewrites as the temp relfile will not have correct 
			 * stats.
			 */
			swap_relation_files(tab->relid, OIDNewHeap, RecentXmin, false);

			CommandCounterIncrement();

			/* Destroy new heap with old filenode */
			object.classId = RelationRelationId;
			object.objectId = OIDNewHeap;
			object.objectSubId = 0;

			/*
			 * The new relation is local to our transaction and we know
			 * nothing depends on it, so DROP_RESTRICT should be OK.
			 */
			performDeletion(&object, DROP_RESTRICT);
			/* performDeletion does CommandCounterIncrement at end */

			/*
			 * Rebuild each index on the relation (but not the toast table,
			 * which is all-new anyway).  We do not need
			 * CommandCounterIncrement() because reindex_relation does it.
			 */
			reindex_relation(tab->relid, false);
		}
		else
		{
			/*
			 * Test the current data within the table against new constraints
			 * generated by ALTER TABLE commands, but don't rebuild data.
			 */
			if (tab->constraints != NIL || tab->new_notnull)
				ATRewriteTable(tab, InvalidOid);

			/*
			 * If we had SET TABLESPACE but no reason to reconstruct tuples,
			 * just do a block-by-block copy.
			 */
			if (tab->newTableSpace)
				ATExecSetTableSpace(tab->relid, tab->newTableSpace);
		}
	}

	/*
	 * Foreign key constraints are checked in a final pass, since (a) it's
	 * generally best to examine each one separately, and (b) it's at least
	 * theoretically possible that we have changed both relations of the
	 * foreign key, and we'd better have finished both rewrites before we try
	 * to read the tables.
	 */
	foreach(ltab, *wqueue)
	{
		AlteredTableInfo *tab = (AlteredTableInfo *) lfirst(ltab);
		Relation	rel = NULL;
		ListCell   *lcon;

		foreach(lcon, tab->constraints)
		{
			NewConstraint *con = lfirst(lcon);

			if (con->contype == CONSTR_FOREIGN)
			{
				FkConstraint *fkconstraint = (FkConstraint *) con->qual;
				Relation	refrel;

				if (rel == NULL)
				{
					/* Long since locked, no need for another */
					rel = heap_open(tab->relid, NoLock);
				}

				refrel = heap_open(con->refrelid, RowShareLock);

				validateForeignKeyConstraint(fkconstraint, rel, refrel,
											 con->conid);

				heap_close(refrel, NoLock);
			}
		}

		if (rel)
			heap_close(rel, NoLock);
	}
}

/*
 * Scan an existing column for varblock headers, write one new segfile
 * each for new columns.  newvals is a list of NewColumnValue items.
 */
static void
ATAocsWriteNewColumns(
		AOCSAddColumnDesc idesc, AOCSHeaderScanDesc sdesc,
		AlteredTableInfo *tab, ExprContext *econtext, TupleTableSlot *slot)
{
	NewColumnValue *newval;
	TupleDesc tupdesc = RelationGetDescr(idesc->rel);
	Form_pg_attribute attr;
	Datum *values = slot_get_values(slot);
	bool *isnull = slot_get_isnull(slot);
	int64 expectedFRN = -1; /* expected firstRowNum of the next varblock */
	ListCell *l;
	int i;

	/* Start index in values and isnull array for newly added columns. */
	AttrNumber newcol = tupdesc->natts - idesc->num_newcols;

	/* Loop over each varblock in an appendonly segno. */
	while (aocs_get_nextheader(sdesc))
	{
		if (sdesc->ao_read.current.hasFirstRowNum)
		{
			if (expectedFRN == -1)
			{
				/*
				 * Initialize expected firstRowNum for each appendonly
				 * segment.  Initializing it to 1 may not always be
				 * good.  E.g. if the first insert into an appendonly
				 * segment is aborted.  A subsequent successful insert
				 * creates the first varblock having firstRowNum
				 * greater than 1.
				 */
				expectedFRN = sdesc->ao_read.current.firstRowNum;
			}
			else
			{
				Assert(expectedFRN <= sdesc->ao_read.current.firstRowNum);
				if (expectedFRN < sdesc->ao_read.current.firstRowNum)
				{
					elogif(Debug_appendonly_print_storage_headers, LOG,
						   "hole in %s: exp FRN: " INT64_FORMAT ", actual FRN: "
						   INT64_FORMAT, sdesc->ao_read.segmentFileName,
						   expectedFRN, sdesc->ao_read.current.firstRowNum);
					/*
					 * We encountered a break in sequence of row
					 * numbers (hole), replicate it in the new
					 * segfiles.
					 */
					aocs_addcol_endblock(
							idesc, sdesc->ao_read.current.firstRowNum);
				}
			}
			for (i = 0; i < sdesc->ao_read.current.rowCount; ++i)
			{
				foreach (l, tab->newvals)
				{
					newval = lfirst(l);
					values[newval->attnum-1] =
							ExecEvalExprSwitchContext(newval->exprstate,
													  econtext,
													  &isnull[newval->attnum-1],
													  NULL);
					/*
					 * Ensure that NOT NULL constraint for the newly
					 * added columns is not being violated.  This
					 * covers the case when explicit "CHECK()"
					 * constraint is not specified but only "NOT NULL"
					 * is specified in the new column's definition.
					 */
					attr = tupdesc->attrs[newval->attnum-1];
					if (attr->attnotnull &&	isnull[newval->attnum-1])
					{
						ereport(ERROR,
								(errcode(ERRCODE_NOT_NULL_VIOLATION),
								 errmsg("column \"%s\" contains null values",
										NameStr(attr->attname))));
					}
				}
				foreach (l, tab->constraints)
				{
					NewConstraint *con = lfirst(l);
					switch(con->contype)
					{
						case CONSTR_CHECK:
							if(!ExecQual(con->qualstate, econtext, true))
								ereport(ERROR,
										(errcode(ERRCODE_CHECK_VIOLATION),
										 errmsg("check constraint \"%s\" is "
												"violated",	con->name)));
							break;
						case CONSTR_FOREIGN:
							/* Nothing to do */
							break;
						default:
							elog(ERROR, "Unrecognized constraint type: %d",
								 (int) con->contype);
					}
				}
				aocs_addcol_insert_datum(idesc, values+newcol, isnull+newcol);
				ResetExprContext(econtext);
				CHECK_FOR_INTERRUPTS();
			}
			expectedFRN = sdesc->ao_read.current.firstRowNum +
					sdesc->ao_read.current.rowCount;
		}
	}
}

/*
 * Choose the column that has the smallest segfile size so as to
 * minimize disk I/O in subsequent varblock header scan. The natts arg
 * includes only existing columns and not the ones being added. Once
 * we find a segfile with nonzero tuplecount and find the column with
 * the smallest eof to return, we continue the loop but skip over all
 * segfiles except for those in AOSEG_STATE_AWAITING_DROP state which
 * we need to append to our drop list.
 */
static int
column_to_scan(AOCSFileSegInfo **segInfos, int nseg, int natts, Relation aocsrel)
{
	int scancol = -1;
	int segi;
	int i;
	AOCSVPInfoEntry *vpe;
	int64 min_eof = 0;

	for (segi = 0; segi < nseg; ++segi)
	{
		/*
		 * Skip if state is in AOSEG_STATE_AWAITING_DROP. But
		 * try to drop the segfile first, since we currently have the
		 * AccessExclusiveLock. If we don't do this, aocssegfiles in
		 * this state will have vpinfo size containing info for less
		 * number of columns compared to the relation's relnatts in
		 * its pg_class entry (e.g. in calls to getAOCSVPEntry).
		 */
		if (segInfos[segi]->state == AOSEG_STATE_AWAITING_DROP)
		{
			AOCSDrop(aocsrel, segInfos[segi]->segno);
			continue;
		}

		/*
		 * Skip over appendonly segments with no tuples (caused by VACUUM)
		 */
		if (segInfos[segi]->total_tupcount > 0 && scancol == -1)
		{
			for (i = 0; i < natts; ++i)
			{
				vpe = getAOCSVPEntry(segInfos[segi], i);
				if (vpe->eof > 0 && (!min_eof || vpe->eof < min_eof))
				{
					min_eof = vpe->eof;
					scancol = i;
				}
			}
		}
	}

	return scancol;
}

/*
 * ATAocsNoRewrite: Leverage column orientation to avoid rewrite.
 *
 * Return true if rewrite can be avoided for this command.  Return
 * false to go the usual ATRewriteTable way.
 */
static bool
ATAocsNoRewrite(AlteredTableInfo *tab)
{
	AOCSFileSegInfo **segInfos;
	AOCSHeaderScanDesc sdesc;
	AOCSAddColumnDesc idesc;
	NewColumnValue *newval;
	NewConstraint *con;
	TupleTableSlot *slot;
	EState *estate;
	ExprContext *econtext;
	Relation rel; /* Relation being altered */
	int32 nseg;
	int32 segi;
	char *basepath;
	int32 scancol; /* chosen column number to scan from */
	ListCell *l;

	/*
	 * only ADD COLUMN subcommand is supported at this time
	 */
	List *addColCmds = tab->subcmds[AT_PASS_ADD_COL];
	if (addColCmds == NULL)
		return false;

	estate = CreateExecutorState();
	foreach(l, tab->constraints)
	{
		con = lfirst(l);
		switch (con->contype)
		{
			case CONSTR_CHECK:
				con->qualstate = (List *)
					ExecPrepareExpr((Expr *) con->qual, estate);
				break;
			case CONSTR_FOREIGN:
				/* Nothing to do here */
				break;
			default:
				elog(ERROR, "unrecognized constraint type: %d",
					 (int) con->contype);
		}
	}
	foreach(l, tab->newvals)
	{
		newval = lfirst(l);
		newval->exprstate = ExecPrepareExpr((Expr *) newval->expr, estate);
	}

	rel = heap_open(tab->relid, NoLock);
	segInfos = GetAllAOCSFileSegInfo(rel, SnapshotNow, &nseg);
	basepath = relpath(rel->rd_node);
	if (nseg > 0)
	{
		aocs_addcol_emptyvpe(rel, segInfos, nseg,
							 list_length(tab->newvals));
	}

	scancol = column_to_scan(segInfos, nseg, tab->oldDesc->natts, rel);
	elogif(Debug_appendonly_print_storage_headers, LOG,
		   "using column %d of relation %s for alter table scan",
		   scancol, RelationGetRelationName(rel));

	/*
	 * Continue only if a non-empty existing segfile was found above.
	 */
	if (Gp_role != GP_ROLE_DISPATCH && scancol != -1)
	{
		slot = MakeSingleTupleTableSlot(RelationGetDescr(rel));

		/*
		 * Initialize expression context for evaluating values and
		 * constraints of the newly added columns.
		 */
		econtext = GetPerTupleExprContext(estate);
		/*
		 * The slot's data will be populated for each newly added
		 * column by ExecEvalExpr().
		 */
		econtext->ecxt_scantuple = slot;

		/*
		 * Mark all attributes including newly added columns as valid.
		 * Used for per tuple constraint evaluation.
		 */
		TupSetVirtualTupleNValid(slot, RelationGetDescr(rel)->natts);

		memset(slot_get_isnull(slot), true,
			   RelationGetDescr(rel)->natts * sizeof(bool));

		sdesc = aocs_begin_headerscan(rel, scancol);
		idesc = aocs_addcol_init(rel, list_length(addColCmds));

		/* Loop over all appendonly segments */
		for (segi = 0; segi < nseg; ++segi)
		{
			if (segInfos[segi]->total_tupcount <= 0 ||
				segInfos[segi]->state == AOSEG_STATE_AWAITING_DROP)
			{
				/*
				 * VACUUM may cause appendonly segments with eof=0.
				 * We only need to add new rows in pg_aocsseg_* in
				 * this case for each newly added column.  This is
				 * accomplished by aocs_addcol_emptyvpe() above.
				 *
				 * Compaction leaves redundant segments in
				 * AOSEG_STATE_AWAITING_DROP.  We skip over them too.
				 */
				elogif(Debug_appendonly_print_storage_headers, LOG,
					   "Skipping over empty segno %d relation %s",
					   segInfos[segi]->segno, RelationGetRelationName(rel));
				continue;
			}
			/*
			 * Open aocs segfile for chosen column for current
			 * appendonly segment.
			 */
			aocs_headerscan_opensegfile(sdesc, segInfos[segi], basepath);

			/*
			 * Create new segfiles for new columns for current
			 * appendonly segment.
			 */
			aocs_addcol_newsegfile(idesc, segInfos[segi],
								   basepath, rel->rd_node);

			ATAocsWriteNewColumns(idesc, sdesc, tab, econtext, slot);
		}
		aocs_end_headerscan(sdesc);
		aocs_addcol_finish(idesc);
		ExecDropSingleTupleTableSlot(slot);
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/*
		 * We remove the hash entry for this relation even though
		 * there is no rewrite because we may have dropped some
		 * segfiles that were in AOSEG_STATE_AWAITING_DROP state in
		 * column_to_scan(). The cost of recreating the entry later on
		 * is cheap so this should be fine. If we don't remove the
		 * hash entry and we had done any segfile drops, master will
		 * continue to see those segfiles as unavailable for use.
		 *
		 * Note that ALTER already took an exclusive lock on the
		 * relation so we are guaranteed to not drop the hash
		 * entry from under any concurrent operation.
		 */
		AORelRemoveHashEntry(RelationGetRelid(rel));
	}

	FreeExecutorState(estate);
	heap_close(rel, NoLock);
	return true;
}

/*
 * ATRewriteTable: scan or rewrite one table
 *
 * OIDNewHeap is InvalidOid if we don't need to rewrite
 */
static void
ATRewriteTable(AlteredTableInfo *tab, Oid OIDNewHeap)
{
	Relation	oldrel;
	Relation	newrel;
	TupleDesc	oldTupDesc;
	TupleDesc	newTupDesc;
	bool		needscan = false;
	List	   *notnull_attrs;
	int			i;
	ListCell   *l;
	EState	   *estate;

	/*
	 * Open the relation(s).  We have surely already locked the existing
	 * table.
	 *
	 * In EXCHANGE of partition case, we only need to validate the content
	 * based on new constraints.  oldTupDesc should point to the oldrel's tuple
	 * descriptor since tab->oldDesc comes from the parent partition.
	 */
	if (OidIsValid(tab->exchange_relid))
	{
		oldrel = heap_open(tab->exchange_relid, NoLock);
		oldTupDesc = RelationGetDescr(oldrel);
	}
	else
	{
		oldrel = heap_open(tab->relid, NoLock);
		oldTupDesc = tab->oldDesc;
	}

	newTupDesc = RelationGetDescr(oldrel);		/* includes all mods */

	if (OidIsValid(OIDNewHeap))
		newrel = heap_open(OIDNewHeap, AccessExclusiveLock);
	else
		newrel = NULL;

	/*
	 * If we need to rewrite the table, the operation has to be propagated to
	 * tables that use this table's rowtype as a column type.
	 *
	 * (Eventually this will probably become true for scans as well, but at
	 * the moment a composite type does not enforce any constraints, so it's
	 * not necessary/appropriate to enforce them just during ALTER.)
	 */
	if (newrel)
		find_composite_type_dependencies(oldrel->rd_rel->reltype,
										 RelationGetRelationName(oldrel),
										 NULL);

	/*
	 * Generate the constraint and default execution states
	 */

	estate = CreateExecutorState();

	/* Build the needed expression execution states */
	foreach(l, tab->constraints)
	{
		NewConstraint *con = lfirst(l);

		switch (con->contype)
		{
			case CONSTR_CHECK:
				needscan = true;
				con->qualstate = (List *)
					ExecPrepareExpr((Expr *) con->qual, estate);
				break;
			case CONSTR_FOREIGN:
				/* Nothing to do here */
				break;
			default:
				elog(ERROR, "unrecognized constraint type: %d",
					 (int) con->contype);
		}
	}

	foreach(l, tab->newvals)
	{
		NewColumnValue *ex = lfirst(l);

		needscan = true;

		ex->exprstate = ExecPrepareExpr((Expr *) ex->expr, estate);
	}

	notnull_attrs = NIL;
	if (newrel || tab->new_notnull)
	{
		/*
		 * If we are rebuilding the tuples OR if we added any new NOT NULL
		 * constraints, check all not-null constraints.  This is a bit of
		 * overkill but it minimizes risk of bugs, and heap_attisnull is a
		 * pretty cheap test anyway.
		 */
		for (i = 0; i < newTupDesc->natts; i++)
		{
			if (newTupDesc->attrs[i]->attnotnull &&
				!newTupDesc->attrs[i]->attisdropped)
				notnull_attrs = lappend_int(notnull_attrs, i);
		}
		if (notnull_attrs)
			needscan = true;
	}

	if (newrel || needscan)
	{
		ExprContext *econtext;
		Datum	   *values;
		bool	   *isnull;
		TupleTableSlot *oldslot;
		TupleTableSlot *newslot;
		HeapScanDesc heapscan = NULL;
		AppendOnlyScanDesc aoscan = NULL;
		HeapTuple	htuple = NULL;
		MemTuple	mtuple = NULL;
		MemoryContext oldCxt;
		List	   *dropped_attrs = NIL;
		ListCell   *lc;
		char		relstorage = oldrel->rd_rel->relstorage;

		econtext = GetPerTupleExprContext(estate);

		/*
		 * Make tuple slots for old and new tuples.  Note that even when the
		 * tuples are the same, the tupDescs might not be (consider ADD COLUMN
		 * without a default).
		 */
		oldslot = MakeSingleTupleTableSlot(oldTupDesc);
		newslot = MakeSingleTupleTableSlot(newTupDesc);

		/* Preallocate values/isnull arrays */
		i = Max(newTupDesc->natts, oldTupDesc->natts);
		values = (Datum *) palloc0(i * sizeof(Datum));
		isnull = (bool *) palloc(i * sizeof(bool));
		memset(isnull, true, i * sizeof(bool));

		/*
		 * Any attributes that are dropped according to the new tuple
		 * descriptor can be set to NULL. We precompute the list of dropped
		 * attributes to avoid needing to do so in the per-tuple loop.
		 */
		for (i = 0; i < newTupDesc->natts; i++)
		{
			if (newTupDesc->attrs[i]->attisdropped)
				dropped_attrs = lappend_int(dropped_attrs, i);
		}

		/*
		 * Scan through the rows, generating a new row if needed and then
		 * checking all the constraints.
		 */
		if(relstorage == RELSTORAGE_HEAP) 
		{
			heapscan = heap_beginscan(oldrel, SnapshotNow, 0, NULL);

			/*
			 * Switch to per-tuple memory context and reset it for each tuple
			 * produced, so we don't leak memory.
			 */
			oldCxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

			while ((htuple = heap_getnext(heapscan, ForwardScanDirection)) != NULL)
			{
				if (newrel)
				{
					Oid			tupOid = InvalidOid;

					/* Extract data from old tuple */
					heap_deform_tuple(htuple, oldTupDesc, values, isnull);
					if (oldTupDesc->tdhasoid)
						tupOid = HeapTupleGetOid(htuple);

					/* Set dropped attributes to null in new tuple */
					foreach(lc, dropped_attrs)
						isnull[lfirst_int(lc)] = true;

					/*
					 * Process supplied expressions to replace selected columns.
					 * Expression inputs come from the old tuple.
					 */
					ExecStoreHeapTuple(htuple, oldslot, InvalidBuffer, false);
					econtext->ecxt_scantuple = oldslot;

					foreach(l, tab->newvals)
					{
						NewColumnValue *ex = lfirst(l);

						values[ex->attnum - 1] = ExecEvalExpr(ex->exprstate,
															  econtext,
															  &isnull[ex->attnum - 1],
															  NULL);
					}

					/*
					 * Form the new tuple. Note that we don't explicitly pfree it,
					 * since the per-tuple memory context will be reset shortly.
					 */
					htuple = heap_form_tuple(newTupDesc, values, isnull);

					/* Preserve OID, if any */
					if (newTupDesc->tdhasoid)
						HeapTupleSetOid(htuple, tupOid);
				}

				/* Now check any constraints on the possibly-changed tuple */
				ExecStoreHeapTuple(htuple, newslot, InvalidBuffer, false);
				econtext->ecxt_scantuple = newslot;

				foreach(l, notnull_attrs)
				{
					int			attn = lfirst_int(l);

					if (heap_attisnull(htuple, attn + 1))
						ereport(ERROR,
								(errcode(ERRCODE_NOT_NULL_VIOLATION),
								 errmsg("column \"%s\" contains null values",
										NameStr(newTupDesc->attrs[attn]->attname))));
				}

				foreach(l, tab->constraints)
				{
					NewConstraint *con = lfirst(l);

					switch (con->contype)
					{
						case CONSTR_CHECK:
							if (!ExecQual(con->qualstate, econtext, true))
							{
								if (OidIsValid(tab->exchange_relid))
									ereport(ERROR,
											(errcode(ERRCODE_CHECK_VIOLATION),
											 errmsg("exchange table contains "
													"a row which violates the "
													"partitioning "
													"specification of \"%s\"",
													get_rel_name(tab->relid))));
								else
									ereport(ERROR,
											(errcode(ERRCODE_CHECK_VIOLATION),
											 errmsg("check constraint \"%s\" is violated by some row",
												con->name)));
							}
							break;
						case CONSTR_FOREIGN:
							/* Nothing to do here */
							break;
						default:
							elog(ERROR, "unrecognized constraint type: %d",
								 (int) con->contype);
					}
				}

				/* Write the tuple out to the new relation */
				if (newrel)
					simple_heap_insert(newrel, htuple);

				ResetExprContext(econtext);

				CHECK_FOR_INTERRUPTS();
			}

			MemoryContextSwitchTo(oldCxt);
			heap_endscan(heapscan);

		}
		else if(relstorage == RELSTORAGE_AOROWS && Gp_role != GP_ROLE_DISPATCH)
		{
			/*
			 * When rewriting an appendonly table we choose to always insert
			 * into the segfile reserved for special purposes (segno #0).
			 */
			int 					segno = RESERVED_SEGNO;
			AppendOnlyInsertDesc 	aoInsertDesc = NULL;
			MemTupleBinding*		mt_bind;

			if(newrel)
				aoInsertDesc = appendonly_insert_init(newrel, segno, false);

			mt_bind = (newrel ? aoInsertDesc->mt_bind : create_memtuple_binding(newTupDesc));

			aoscan = appendonly_beginscan(oldrel, SnapshotNow, SnapshotNow, 0, NULL);

			/*
			 * Switch to per-tuple memory context and reset it for each tuple
			 * produced, so we don't leak memory.
			 */
			oldCxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

			while ((mtuple = appendonly_getnext(aoscan, ForwardScanDirection, oldslot)) != NULL)
			{
				if (newrel)
				{
					Oid			tupOid = InvalidOid;

					TupClearShouldFree(oldslot);
					econtext->ecxt_scantuple = oldslot;

					/* Extract data from old tuple */
					for(i = 0; i < oldslot->tts_tupleDescriptor->natts ; i++)
						values[i] = slot_getattr(oldslot, i+1, &isnull[i]);

					if (oldTupDesc->tdhasoid)
						tupOid = MemTupleGetOid(mtuple, mt_bind);

					/* Set dropped attributes to null in new tuple */
					foreach(lc, dropped_attrs)
						isnull[lfirst_int(lc)] = true;

					/*
					 * Process supplied expressions to replace selected columns.
					 * Expression inputs come from the old tuple.
					 */
					foreach(l, tab->newvals)
					{
						NewColumnValue *ex = lfirst(l);

						values[ex->attnum - 1] = ExecEvalExpr(ex->exprstate,
															  econtext,
															  &isnull[ex->attnum - 1],
															  NULL);
					}

					/*
					 * Form the new tuple. Note that we don't explicitly pfree it,
					 * since the per-tuple memory context will be reset shortly.
					 */
					mtuple = memtuple_form_to(mt_bind,
											  values, isnull,
											  NULL, NULL, false);


					/* Preserve OID, if any */
					if (newTupDesc->tdhasoid)
						MemTupleSetOid(mtuple, mt_bind, tupOid);
				}

				/* Now check any constraints on the possibly-changed tuple */
				ExecStoreMinimalTuple(mtuple, newslot, false);
				econtext->ecxt_scantuple = newslot;

				foreach(l, notnull_attrs)
				{
					int					attn = lfirst_int(l);
					
					if (memtuple_attisnull(mtuple, mt_bind, attn + 1))
						ereport(ERROR,
								(errcode(ERRCODE_NOT_NULL_VIOLATION),
								 errmsg("column \"%s\" contains null values",
										NameStr(newTupDesc->attrs[attn]->attname))));
				}

				foreach(l, tab->constraints)
				{
					NewConstraint *con = lfirst(l);

					switch (con->contype)
					{
						case CONSTR_CHECK:
							if (!ExecQual(con->qualstate, econtext, true))
								ereport(ERROR,
										(errcode(ERRCODE_CHECK_VIOLATION),
										 errmsg("check constraint \"%s\" is violated by some row",
												con->name)));
							break;
						case CONSTR_FOREIGN:
							/* Nothing to do here */
							break;
						default:
							elog(ERROR, "unrecognized constraint type: %d",
								 (int) con->contype);
					}
				}

				/* Write the tuple out to the new relation */
				if (newrel)
				{
					Oid 		tupleOid;
					AOTupleId	aoTupleId;
					
					appendonly_insert(aoInsertDesc, mtuple, &tupleOid, &aoTupleId);
				}

				ResetExprContext(econtext);

				CHECK_FOR_INTERRUPTS();

			}

			MemoryContextSwitchTo(oldCxt);
			appendonly_endscan(aoscan);

			if(newrel)
				appendonly_insert_finish(aoInsertDesc);
		}
		else if(relstorage == RELSTORAGE_AOCOLS && Gp_role != GP_ROLE_DISPATCH)
		{
			int segno = RESERVED_SEGNO;
			AOCSInsertDesc idesc = NULL;
			AOCSScanDesc sdesc = NULL;
			int nvp = oldrel->rd_att->natts;
			bool *proj = palloc0(sizeof(bool) * nvp);

			/*
			 * We use the old tuple descriptor instead of oldrel's tuple descriptor,
			 * which may already contain altered column.
			 */
			if (oldTupDesc)
			{
				Assert(oldTupDesc->natts <= nvp);
				memset(proj, true, oldTupDesc->natts);
			}
			else
				memset(proj, true, nvp);

			if(newrel)
				idesc = aocs_insert_init(newrel, segno, false);

			sdesc = aocs_beginscan(oldrel, SnapshotNow, SnapshotNow, oldTupDesc, proj);

			aocs_getnext(sdesc, ForwardScanDirection, oldslot);

			while(!TupIsNull(oldslot))
			{
				oldCxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
				econtext->ecxt_scantuple = oldslot;

				if(newrel)
				{
					Datum *slotvalues = slot_get_values(oldslot);
					bool  *slotnulls = slot_get_isnull(oldslot);
					Datum *newslotvalues = slot_get_values(newslot);
					bool *newslotnulls = slot_get_isnull(newslot);

					int nv = Min(oldslot->tts_tupleDescriptor->natts, newslot->tts_tupleDescriptor->natts);

					/* aocs does not do oid yet */
					Assert(!oldTupDesc->tdhasoid);
					Assert(!newTupDesc->tdhasoid);

					/* Dropped att should be set correctly by aocs_getnext */
					/* transfer values from old to new slot */

					memcpy(newslotvalues, slotvalues, sizeof(Datum) * nv);
					memcpy(newslotnulls, slotnulls, sizeof(bool) * nv);
					TupSetVirtualTupleNValid(newslot, nv);

					/* Process supplied expressions */
					foreach(l, tab->newvals)
					{
						NewColumnValue *ex = lfirst(l);

						newslotvalues[ex->attnum-1] =
							ExecEvalExpr(ex->exprstate,
										 econtext,
										 &newslotnulls[ex->attnum-1],
										 NULL
								);

						if (ex->attnum > nv)
							TupSetVirtualTupleNValid(newslot, ex->attnum);
					}

					/* Use the modified tuple to check constraints below */
					econtext->ecxt_scantuple = newslot;
				}

				/* check if any NOT NULL constraint is violated */
				foreach(l, notnull_attrs)
				{
					int					attn = lfirst_int(l);
					
					if (slot_attisnull(econtext->ecxt_scantuple, attn + 1))
						ereport(ERROR,
								(errcode(ERRCODE_NOT_NULL_VIOLATION),
								 errmsg("column \"%s\" contains null values",
										NameStr(newTupDesc->attrs[attn]->attname))));
				}

				/* Check constraints */
				foreach (l, tab->constraints)
				{
					NewConstraint *con = lfirst(l);
					switch(con->contype)
					{
						case CONSTR_CHECK:
							if(!ExecQual(con->qualstate, econtext, true))
								ereport(ERROR,
										(errcode(ERRCODE_CHECK_VIOLATION),
										 errmsg("check constraint \"%s\" is violated",
												con->name
											 )));
							break;
						case CONSTR_FOREIGN:
							/* Nothing to do */
							break;
						default:
							elog(ERROR, "Unrecognized constraint type: %d",
								 (int) con->contype);
					}
				}

				if(newrel)
				{
					MemoryContextSwitchTo(oldCxt);
					aocs_insert(idesc, newslot);
				}

				ResetExprContext(econtext);
				CHECK_FOR_INTERRUPTS();

				MemoryContextSwitchTo(oldCxt);
				aocs_getnext(sdesc, ForwardScanDirection, oldslot);
			}

			aocs_endscan(sdesc);

			if(newrel)
				aocs_insert_finish(idesc);

			pfree(proj);
		}
		else if(relstorage_is_ao(relstorage) && Gp_role == GP_ROLE_DISPATCH)
		{
			/*
			 * All we want to do on the QD during an AO table rewrite
			 * is to drop the shared memory hash table entry for this
			 * table if it exists. We must do so since before the
			 * rewrite we probably have few non-zero segfile entries
			 * for this table while after the rewrite only segno zero
			 * will be full and the others will be empty. By dropping
			 * the hash entry we force refreshing the entry from the
			 * catalog the next time a write into this AO table comes
			 * along.
			 *
			 * Note that ALTER already took an exclusive lock on the
			 * old relation so we are guaranteed to not drop the hash
			 * entry from under any concurrent operation.
			 */
			LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);
			AORelRemoveHashEntry(RelationGetRelid(oldrel));
			LWLockRelease(AOSegFileLock);
		}
		else
		{
			Assert(!"Invalid relstorage type");
		}

		ExecDropSingleTupleTableSlot(oldslot);
		ExecDropSingleTupleTableSlot(newslot);
	}

	FreeExecutorState(estate);

	heap_close(oldrel, NoLock);
	if (newrel)
		heap_close(newrel, NoLock);
}

/*
 * ATGetQueueEntry: find or create an entry in the ALTER TABLE work queue
 */
static AlteredTableInfo *
ATGetQueueEntry(List **wqueue, Relation rel)
{
	Oid			relid = RelationGetRelid(rel);
	AlteredTableInfo *tab;
	ListCell   *ltab;

	foreach(ltab, *wqueue)
	{
		tab = (AlteredTableInfo *) lfirst(ltab);
		if (tab->relid == relid)
			return tab;
	}

	/*
	 * Not there, so add it.  Note that we make a copy of the relation's
	 * existing descriptor before anything interesting can happen to it.
	 */
	tab = (AlteredTableInfo *) palloc0(sizeof(AlteredTableInfo));
	tab->relid = relid;
	tab->relkind = rel->rd_rel->relkind;
	tab->oldDesc = CreateTupleDescCopy(RelationGetDescr(rel));

	*wqueue = lappend(*wqueue, tab);

	return tab;
}

/*
 * Issue an error, if this is a part of a partitioned table or, in case 
 * rejectroot is true, if this is a partitioned table itself.
 *
 * Usually called from ATPrepCmd to validate a subcommand of an ALTER
 * TABLE command.  Many commands may be invoked on an entire partitioned
 * table, but not on just a part.  These specify rejectroot as false.
 */
static void
ATPartitionCheck(AlterTableType subtype, Relation rel, bool rejectroot, bool recursing)
{
	if (recursing)
		return;

	if (rejectroot &&
		(rel_is_child_partition(RelationGetRelid(rel)) ||
		 rel_is_partitioned(RelationGetRelid(rel))))
	{
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("can't %s \"%s\"; it is a partitioned table or part thereof",
						alterTableCmdString(subtype),
						RelationGetRelationName(rel))));
	}
	else if (rel_is_child_partition(RelationGetRelid(rel)))
	{
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("can't %s \"%s\"; it is part of a partitioned table",
						alterTableCmdString(subtype),
						RelationGetRelationName(rel)),
				 errhint("You may be able to perform the operation on the partitioned table as a whole.")));
	}
}

/*
 * ATExternalPartitionCheck
 * Reject certain operations if the partitioned table has external partition.
 */
static void
ATExternalPartitionCheck(AlterTableType subtype, Relation rel, bool recursing)
{
	if (recursing)
	{
		return;
	}

	Oid relid = RelationGetRelid(rel);
	if (rel_has_external_partition(relid))
	{
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("Cannot %s \"%s\"; it has external partition(s)",
						alterTableCmdString(subtype),
						RelationGetRelationName(rel))));
	}
}

/*
 * ATSimplePermissions
 *
 * - Ensure that it is a relation (or possibly a view)
 * - Ensure this user is the owner
 * - Ensure that it is not a system table
 */
static void
ATSimplePermissions(Relation rel, bool allowView)
{
	if (rel->rd_rel->relkind != RELKIND_RELATION)
	{
		if (allowView)
		{
			if (rel->rd_rel->relkind != RELKIND_VIEW)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("\"%s\" is not a table or view",
								RelationGetRelationName(rel))));
		}
		else if (!IsUnderPostmaster &&
				 (rel->rd_rel->relkind == RELKIND_AOSEGMENTS ||
				  rel->rd_rel->relkind == RELKIND_AOBLOCKDIR ||
				  rel->rd_rel->relkind == RELKIND_AOVISIMAP))
		{
			/*
			 * Allow ALTER TABLE operations in standard alone mode on
			 * AO segment tables.
			 */
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a table",
							RelationGetRelationName(rel))));
	}

	/* Permissions checks */
	if (!pg_class_ownercheck(RelationGetRelid(rel), GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));

	if (!allowSystemTableModsDDL && IsSystemRelation(rel))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						RelationGetRelationName(rel))));
}

/*
 * ATSimplePermissionsRelationOrIndex
 *
 * - Ensure that it is a relation or an index
 * - Ensure this user is the owner
 * - Ensure that it is not a system table
 */
static void
ATSimplePermissionsRelationOrIndex(Relation rel)
{
	if (rel->rd_rel->relkind != RELKIND_RELATION &&
		rel->rd_rel->relkind != RELKIND_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table or index",
						RelationGetRelationName(rel))));

	/* Permissions checks */
	if (!pg_class_ownercheck(RelationGetRelid(rel), GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));

	if (!allowSystemTableModsDDL && IsSystemRelation(rel))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						RelationGetRelationName(rel))));
}

/*
 * ATSimpleRecursion
 *
 * Simple table recursion sufficient for most ALTER TABLE operations.
 * All direct and indirect children are processed in an unspecified order.
 * Note that if a child inherits from the original table via multiple
 * inheritance paths, it will be visited just once.
 */
static void
ATSimpleRecursion(List **wqueue, Relation rel,
				  AlterTableCmd *cmd, bool recurse)
{
	/*
	 * Propagate to children if desired.  Non-table relations never have
	 * children, so no need to search in that case.
	 */
	if (recurse && rel->rd_rel->relkind == RELKIND_RELATION)
	{
		Oid			relid = RelationGetRelid(rel);
		ListCell   *child;
		List	   *children;

		/* this routine is actually in the planner */
		children = find_all_inheritors(relid);

		/*
		 * find_all_inheritors does the recursive search of the inheritance
		 * hierarchy, so all we have to do is process all of the relids in the
		 * list that it returns.
		 */
		foreach(child, children)
		{
			Oid			childrelid = lfirst_oid(child);
			Relation	childrel;

			if (childrelid == relid)
				continue;
			childrel = relation_open(childrelid, AccessExclusiveLock);
			CheckTableNotInUse(childrel, "ALTER TABLE");
			ATPrepCmd(wqueue, childrel, cmd, false, true);
			relation_close(childrel, NoLock);
		}
	}
}

/*
 * ATOneLevelRecursion
 *
 * Here, we visit only direct inheritance children.  It is expected that
 * the command's prep routine will recurse again to find indirect children.
 * When using this technique, a multiply-inheriting child will be visited
 * multiple times.
 */
#if 0 /* unused in GPDB */
static void
ATOneLevelRecursion(List **wqueue, Relation rel,
					AlterTableCmd *cmd)
{
	Oid			relid = RelationGetRelid(rel);
	ListCell   *child;
	List	   *children;

	/* this routine is actually in the planner */
	children = find_inheritance_children(relid);

	foreach(child, children)
	{
		Oid			childrelid = lfirst_oid(child);
		Relation	childrel;

		childrel = relation_open(childrelid, AccessExclusiveLock);
		CheckTableNotInUse(childrel, "ALTER TABLE");
		ATPrepCmd(wqueue, childrel, cmd, true, true);
		relation_close(childrel, NoLock);
	}
}
#endif


/*
 * find_composite_type_dependencies
 *
 * Check to see if a composite type is being used as a column in some
 * other table (possibly nested several levels deep in composite types!).
 * Eventually, we'd like to propagate the check or rewrite operation
 * into other such tables, but for now, just error out if we find any.
 *
 * Caller should provide either a table name or a type name (not both) to
 * report in the error message, if any.
 *
 * We assume that functions and views depending on the type are not reasons
 * to reject the ALTER.  (How safe is this really?)
 */
void
find_composite_type_dependencies(Oid typeOid,
								 const char *origTblName,
								 const char *origTypeName)
{
	Relation	depRel;
	ScanKeyData key[2];
	SysScanDesc depScan;
	HeapTuple	depTup;
	Oid			arrayOid;

	/*
	 * We scan pg_depend to find those things that depend on the rowtype. (We
	 * assume we can ignore refobjsubid for a rowtype.)
	 */
	depRel = heap_open(DependRelationId, AccessShareLock);

	ScanKeyInit(&key[0],
				Anum_pg_depend_refclassid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(TypeRelationId));
	ScanKeyInit(&key[1],
				Anum_pg_depend_refobjid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(typeOid));

	depScan = systable_beginscan(depRel, DependReferenceIndexId, true,
								 SnapshotNow, 2, key);

	while (HeapTupleIsValid(depTup = systable_getnext(depScan)))
	{
		Form_pg_depend pg_depend = (Form_pg_depend) GETSTRUCT(depTup);
		Relation	rel;
		Form_pg_attribute att;

		/* Ignore dependees that aren't user columns of relations */
		/* (we assume system columns are never of rowtypes) */
		if (pg_depend->classid != RelationRelationId ||
			pg_depend->objsubid <= 0)
			continue;

		rel = relation_open(pg_depend->objid, AccessShareLock);
		att = rel->rd_att->attrs[pg_depend->objsubid - 1];

		if (rel->rd_rel->relkind == RELKIND_RELATION)
		{
			if (origTblName)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter table \"%s\" because column \"%s\".\"%s\" uses its rowtype",
								origTblName,
								RelationGetRelationName(rel),
								NameStr(att->attname))));
			else
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter type \"%s\" because column \"%s\".\"%s\" uses it",
								origTypeName,
								RelationGetRelationName(rel),
								NameStr(att->attname))));
		}
		else if (OidIsValid(rel->rd_rel->reltype))
		{
			/*
			 * A view or composite type itself isn't a problem, but we must
			 * recursively check for indirect dependencies via its rowtype.
			 */
			find_composite_type_dependencies(rel->rd_rel->reltype,
											 origTblName, origTypeName);
		}

		relation_close(rel, AccessShareLock);
	}

	systable_endscan(depScan);

	relation_close(depRel, AccessShareLock);

	/*
	 * If there's an array type for the rowtype, must check for uses of it,
	 * too.
	 */
	arrayOid = get_array_type(typeOid);
	if (OidIsValid(arrayOid))
		find_composite_type_dependencies(arrayOid, origTblName, origTypeName);
}


/*
 * ALTER TABLE ADD COLUMN
 *
 * Adds an additional attribute to a relation making the assumption that
 * CHECK, NOT NULL, and FOREIGN KEY constraints will be removed from the
 * AT_AddColumn AlterTableCmd by parse_utilcmd.c and added as independent
 * AlterTableCmd's.
 */
static void
ATPrepAddColumn(List **wqueue, Relation rel, bool recurse,
				AlterTableCmd *cmd)
{
	/* 
	 * If there's an encoding clause, this better be an append only
	 * column oriented table.
	 */
	ColumnDef *def = (ColumnDef *)cmd->def;
	if (def->encoding && !RelationIsAoCols(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("ENCODING clause supplied for table that is "
						"not column oriented")));

	if (def->encoding)
		def->encoding = transformStorageEncodingClause(def->encoding);

	/*
	 * Recurse to add the column to child classes, if requested.
	 *
	 * We must recurse one level at a time, so that multiply-inheriting
	 * children are visited the right number of times and end up with the
	 * right attinhcount.
	 */
	if (recurse)
	{
		/*
		 * We are the master and the table has child(ren):
		 * 		internally create and execute new AlterTableStmt(s) on child(ren)
		 * 		before dispatching the original AlterTableStmt
		 * This is to ensure that pg_constraint oid is consistent across segments for
		 * 		ALTER TABLE ... ADD COLUMN ... CHECK ...
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			List		*children;
			ListCell	*lchild;

			children = find_inheritance_children(RelationGetRelid(rel));
			DestReceiver *dest = None_Receiver;
			foreach(lchild, children)
			{
				Oid 			childrelid = lfirst_oid(lchild);
				Relation 		childrel;

				RangeVar 		*rv;
				AlterTableCmd 	*atc;
				AlterTableStmt 	*ats;

				if (childrelid == RelationGetRelid(rel))
					continue;

				childrel = heap_open(childrelid, AccessShareLock);
				CheckTableNotInUse(childrel, "ALTER TABLE");

				/* Recurse to child */
				atc = copyObject(cmd);
				atc->subtype = AT_AddColumnRecurse;

				/* Child should see column as singly inherited */
				((ColumnDef *) atc->def)->inhcount = 1;
				((ColumnDef *) atc->def)->is_local = false;

				rv = makeRangeVar(get_namespace_name(RelationGetNamespace(childrel)),
								  get_rel_name(childrelid), -1);

				ats = makeNode(AlterTableStmt);
				ats->relation = rv;
				ats->cmds = list_make1(atc);
				ats->relkind = OBJECT_TABLE;

				heap_close(childrel, NoLock);

				ProcessUtility((Node *)ats,
							   synthetic_sql,
							   NULL,
							   false, /* not top level */
							   dest,
							   NULL);
			}
		}
	}
	else
	{
		/*
		 * If we are told not to recurse, there had better not be any child
		 * tables; else the addition would put them out of step.
		 */
		if (find_inheritance_children(RelationGetRelid(rel)) != NIL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("column must be added to child tables too")));
	}
}

static void
ATExecAddColumn(AlteredTableInfo *tab, Relation rel,
				ColumnDef *colDef)
{
	Oid			myrelid = RelationGetRelid(rel);
	Relation	pgclass,
				attrdesc;
	HeapTuple	reltup;
	HeapTuple	attributeTuple;
	Form_pg_attribute attribute;
	FormData_pg_attribute attributeD;
	int			i;
	int			minattnum,
				maxatts;
	HeapTuple	typeTuple;
	Oid			typeOid;
	int32		typmod;
	Form_pg_type tform;
	Expr	   *defval;

	attrdesc = heap_open(AttributeRelationId, RowExclusiveLock);

	/*
	 * Are we adding the column to a recursion child?  If so, check whether to
	 * merge with an existing definition for the column.
	 */
	if (colDef->inhcount > 0)
	{
		HeapTuple	tuple;

		/* Does child already have a column by this name? */
		tuple = SearchSysCacheCopyAttName(myrelid, colDef->colname);
		if (HeapTupleIsValid(tuple))
		{
			Form_pg_attribute childatt = (Form_pg_attribute) GETSTRUCT(tuple);
			Oid			ctypeId;
			int32		ctypmod;

			/* Okay if child matches by type */
			ctypeId = typenameTypeId(NULL, colDef->typeName, &ctypmod);

			if (ctypeId != childatt->atttypid ||
				ctypmod != childatt->atttypmod)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("child table \"%s\" has different type for column \"%s\"",
							RelationGetRelationName(rel), colDef->colname)));

			/* Bump the existing child att's inhcount */
			childatt->attinhcount++;
			simple_heap_update(attrdesc, &tuple->t_self, tuple);
			CatalogUpdateIndexes(attrdesc, tuple);

			heap_freetuple(tuple);

			/* Inform the user about the merge */
			ereport((Gp_role == GP_ROLE_EXECUTE) ? DEBUG1 : NOTICE,
			  (errmsg("merging definition of column \"%s\" for child \"%s\"",
					  colDef->colname, RelationGetRelationName(rel))));

			heap_close(attrdesc, RowExclusiveLock);
			return;
		}
	}

	pgclass = heap_open(RelationRelationId, RowExclusiveLock);

	reltup = SearchSysCacheCopy(RELOID,
								ObjectIdGetDatum(myrelid),
								0, 0, 0);
	if (!HeapTupleIsValid(reltup))
		elog(ERROR, "cache lookup failed for relation %u", myrelid);

	/*
	 * this test is deliberately not attisdropped-aware, since if one tries to
	 * add a column matching a dropped column name, it's gonna fail anyway.
	 */
	if (SearchSysCacheExists(ATTNAME,
							 ObjectIdGetDatum(myrelid),
							 PointerGetDatum(colDef->colname),
							 0, 0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" already exists",
						colDef->colname, RelationGetRelationName(rel))));
	}

	minattnum = ((Form_pg_class) GETSTRUCT(reltup))->relnatts;
	maxatts = minattnum + 1;
	if (maxatts > MaxHeapAttributeNumber)
		ereport(ERROR,
				(errcode(ERRCODE_TOO_MANY_COLUMNS),
				 errmsg("tables can have at most %d columns",
						MaxHeapAttributeNumber)));
	i = minattnum + 1;

	typeTuple = typenameType(NULL, colDef->typeName, &typmod);
	tform = (Form_pg_type) GETSTRUCT(typeTuple);
	typeOid = HeapTupleGetOid(typeTuple);

	/* make sure datatype is legal for a column */
	CheckAttributeType(colDef->colname, typeOid,
					   list_make1_oid(rel->rd_rel->reltype));

	attributeTuple = heap_addheader(Natts_pg_attribute,
									false,
									ATTRIBUTE_TUPLE_SIZE,
									(void *) &attributeD);

	attribute = (Form_pg_attribute) GETSTRUCT(attributeTuple);

	attribute->attrelid = myrelid;
	namestrcpy(&(attribute->attname), colDef->colname);
	attribute->atttypid = typeOid;
	attribute->attstattarget = -1;
	attribute->attlen = tform->typlen;
	attribute->attcacheoff = -1;
	attribute->atttypmod = typmod;
	attribute->attnum = i;
	attribute->attbyval = tform->typbyval;
	attribute->attndims = list_length(colDef->typeName->arrayBounds);
	attribute->attstorage = tform->typstorage;
	attribute->attalign = tform->typalign;
	attribute->attnotnull = colDef->is_not_null;
	attribute->atthasdef = false;
	attribute->attisdropped = false;
	attribute->attislocal = colDef->is_local;
	attribute->attinhcount = colDef->inhcount;

	ReleaseSysCache(typeTuple);

	simple_heap_insert(attrdesc, attributeTuple);

	/* Update indexes on pg_attribute */
	CatalogUpdateIndexes(attrdesc, attributeTuple);

	heap_close(attrdesc, RowExclusiveLock);

	/*
	 * Update number of attributes in pg_class tuple
	 */
	((Form_pg_class) GETSTRUCT(reltup))->relnatts = maxatts;

	simple_heap_update(pgclass, &reltup->t_self, reltup);

	/* keep catalog indexes current */
	CatalogUpdateIndexes(pgclass, reltup);

	heap_freetuple(reltup);

	heap_close(pgclass, RowExclusiveLock);

	/* Make the attribute's catalog entry visible */
	CommandCounterIncrement();

	/*
	 * Store the DEFAULT, if any, in the catalogs
	 */
	if (colDef->raw_default)
	{
		colDef->attnum = attribute->attnum;
		/*
		 * This function is intended for CREATE TABLE, so it processes a
		 * _list_ of defaults, but we just do one.
		 */
		AddRelationConstraints(rel, list_make1(colDef), NIL);

		/* Make the additional catalog changes visible */
		CommandCounterIncrement();
	}

	/*
	 * Tell Phase 3 to fill in the default expression, if there is one.
	 *
	 * If there is no default, Phase 3 doesn't have to do anything, because
	 * that effectively means that the default is NULL.  The heap tuple access
	 * routines always check for attnum > # of attributes in tuple, and return
	 * NULL if so, so without any modification of the tuple data we will get
	 * the effect of NULL values in the new column.
	 *
	 * An exception occurs when the new column is of a domain type: the domain
	 * might have a NOT NULL constraint, or a check constraint that indirectly
	 * rejects nulls.  If there are any domain constraints then we construct
	 * an explicit NULL default value that will be passed through
	 * CoerceToDomain processing.  (This is a tad inefficient, since it causes
	 * rewriting the table which we really don't have to do, but the present
	 * design of domain processing doesn't offer any simple way of checking
	 * the constraints more directly.)
	 *
	 * Note: we use build_column_default, and not just the cooked default
	 * returned by AddRelationConstraints, so that the right thing happens
	 * when a datatype's default applies.
	 */
	defval = (Expr *) build_column_default(rel, attribute->attnum);

	if (!defval && GetDomainConstraints(typeOid) != NIL)
	{
		Oid			baseTypeId;
		int32		baseTypeMod;

		baseTypeMod = typmod;
		baseTypeId = getBaseTypeAndTypmod(typeOid, &baseTypeMod);
		defval = (Expr *) makeNullConst(baseTypeId, baseTypeMod);
		defval = (Expr *) coerce_to_target_type(NULL,
												(Node *) defval,
												baseTypeId,
												typeOid,
												typmod,
												COERCION_ASSIGNMENT,
												COERCE_IMPLICIT_CAST,
												-1);
		if (defval == NULL)		/* should not happen */
			elog(ERROR, "failed to coerce base type to domain");
	}

	/*
	 * MPP-14367/MPP-19664 - Handling of default NULL for AO/CO tables. Currently
	 * memtuples cannot deal with the scenario where the number of
  	 * attributes in the tuple data don't match the attnum. We will generate
	 * an explicit NULL default value and force a rewrite of the table below.
	 * Note: This is inefficient and there is already another JIRA MPP-5419 open
	 * to track adding columns without default values to AO tables efficiently.
	 * When that JIRA is fixed, the following piece of code may be removed
	 */
	if (!defval && (RelationIsAoRows(rel) || RelationIsAoCols(rel)))
		defval = (Expr *) makeNullConst(typeOid, -1);

	if (defval)
	{
		NewColumnValue *newval;

		newval = (NewColumnValue *) palloc0(sizeof(NewColumnValue));
		newval->attnum = attribute->attnum;
		newval->expr = defval;

		/* tab is null if this is called by "create or replace view"
		 * which can't have any default value.
		 */
		Assert(tab);
		tab->newvals = lappend(tab->newvals, newval);
	}

	/*
	 * If the new column is NOT NULL, tell Phase 3 it needs to test that.
	 * Also, "create or replace view" won't have constraint on the column.
	 */
	Assert(!colDef->is_not_null || tab);
	if (tab)
		tab->new_notnull |= colDef->is_not_null;

	/*
	 * Add needed dependency entries for the new column.
	 */
	add_column_datatype_dependency(myrelid, i, attribute->atttypid);

	if (!RelationIsAoCols(rel) && colDef->encoding)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("ENCODING clause not supported on non "
						"column orientated table")));
						

	/* 
	 * For AO/CO tables, always store an encoding clause. If no encoding
	 * clause was provided, store the default encoding clause.
	 */
	if (RelationIsAoCols(rel))
	{
		ColumnReferenceStorageDirective *c =
			makeNode(ColumnReferenceStorageDirective);

		c->column = colDef->colname;

		if (colDef->encoding)
			c->encoding = colDef->encoding;
		else
		{
			/* Use the type specific storage directive, if one exists */
			c->encoding = TypeNameGetStorageDirective(colDef->typeName);
			
			if (!c->encoding)
				c->encoding = default_column_encoding_clause();
		}

		AddRelationAttributeEncodings(rel, list_make1(c));
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ADD COLUMN"
				);

}

/*
 * Install a column's dependency on its datatype.
 */
static void
add_column_datatype_dependency(Oid relid, int32 attnum, Oid typid)
{
	ObjectAddress myself,
				referenced;

	myself.classId = RelationRelationId;
	myself.objectId = relid;
	myself.objectSubId = attnum;
	referenced.classId = TypeRelationId;
	referenced.objectId = typid;
	referenced.objectSubId = 0;
	recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
}

/*
 * ALTER TABLE ALTER COLUMN DROP NOT NULL
 */
static void
ATExecDropNotNull(Relation rel, const char *colName)
{
	HeapTuple	tuple;
	AttrNumber	attnum;
	Relation	attr_rel;
	List	   *indexoidlist;
	ListCell   *indexoidscan;

	/*
	 * lookup the attribute
	 */
	attr_rel = heap_open(AttributeRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));

	attnum = ((Form_pg_attribute) GETSTRUCT(tuple))->attnum;

	/* Prevent them from altering a system attribute */
	if (attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot alter system column \"%s\"",
						colName)));

	/*
	 * Check that the attribute is not in a primary key
	 *
	 * Note: we'll throw error even if the pkey index is not valid.
	 */

	/* Loop over all indexes on the relation */
	indexoidlist = RelationGetIndexList(rel);

	foreach(indexoidscan, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(indexoidscan);
		HeapTuple	indexTuple;
		Form_pg_index indexStruct;
		int			i;

		indexTuple = SearchSysCache(INDEXRELID,
									ObjectIdGetDatum(indexoid),
									0, 0, 0);
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexoid);
		indexStruct = (Form_pg_index) GETSTRUCT(indexTuple);

		/* If the index is not a primary key, skip the check */
		if (indexStruct->indisprimary)
		{
			/*
			 * Loop over each attribute in the primary key and see if it
			 * matches the to-be-altered attribute
			 */
			for (i = 0; i < indexStruct->indnatts; i++)
			{
				if (indexStruct->indkey.values[i] == attnum)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("column \"%s\" is in a primary key",
									colName)));
			}
		}

		ReleaseSysCache(indexTuple);
	}

	list_free(indexoidlist);

	/*
	 * Okay, actually perform the catalog change ... if needed
	 */
	if (((Form_pg_attribute) GETSTRUCT(tuple))->attnotnull)
	{
		((Form_pg_attribute) GETSTRUCT(tuple))->attnotnull = FALSE;

		simple_heap_update(attr_rel, &tuple->t_self, tuple);

		/* keep the system catalog indexes current */
		CatalogUpdateIndexes(attr_rel, tuple);
	}

	heap_close(attr_rel, RowExclusiveLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ALTER COLUMN DROP NOT NULL"
				);

}

/*
 * ALTER TABLE ALTER COLUMN SET NOT NULL
 */
static void
ATExecSetNotNull(AlteredTableInfo *tab, Relation rel,
				 const char *colName)
{
	HeapTuple	tuple;
	AttrNumber	attnum;
	Relation	attr_rel;

	/*
	 * lookup the attribute
	 */
	attr_rel = heap_open(AttributeRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));

	attnum = ((Form_pg_attribute) GETSTRUCT(tuple))->attnum;

	/* Prevent them from altering a system attribute */
	if (attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot alter system column \"%s\"",
						colName)));

	/*
	 * Okay, actually perform the catalog change ... if needed
	 */
	if (!((Form_pg_attribute) GETSTRUCT(tuple))->attnotnull)
	{
		((Form_pg_attribute) GETSTRUCT(tuple))->attnotnull = TRUE;

		simple_heap_update(attr_rel, &tuple->t_self, tuple);

		/* keep the system catalog indexes current */
		CatalogUpdateIndexes(attr_rel, tuple);

		/* Tell Phase 3 it needs to test the constraint */
		tab->new_notnull = true;
	}

	heap_close(attr_rel, RowExclusiveLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ALTER COLUMN SET NOT NULL"
				);

}

/*
 * ALTER TABLE ALTER COLUMN SET/DROP DEFAULT
 *
 * ATPrepColumnDefault: Find child relations
 * 	and create synthetic Alter Table Alter Column Set Default query
 * 	to add default to them.
 */
static void
ATPrepColumnDefault(Relation rel, bool recurse, AlterTableCmd *cmd)
{
	/*
	 * If we are told not to recurse, there had better not be any child
	 * tables; else the addition would put them out of step.
	 */
	if (!recurse && find_inheritance_children(RelationGetRelid(rel)) != NIL)
	{
		/*
		 * In binary upgrade we are handling the children manually when dumping
		 * the schema so not recursing is allowed
		 */
		if (IsBinaryUpgrade)
			return;

		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Column default of relation \"%s\" must be added to its child relation(s)",
						 RelationGetRelationName(rel))));
	}
	/*
	 * We are the master and the table has child(ren):
	 * 		internally create and execute new AlterTableStmt(s) on child(ren)
	 * 		before dispatching the original AlterTableStmt
	 * This is to ensure that pg_attrdef oid is consistent across segments for
	 * 		ALTER TABLE ... ALTER COLUMN ... SET DEFAULT ...
	 * If we are in utility mode, we also need add column default to child(ren).
	 */
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
	{
		/*
		 * Recurse to add column default to child classes.
		 */
		List		*children;
		ListCell	*lchild;

		children = find_inheritance_children(RelationGetRelid(rel));
		DestReceiver *dest = None_Receiver;
		foreach(lchild, children)
		{
			Oid 			childrelid = lfirst_oid(lchild);
			Relation 		childrel;

			RangeVar 		*rv;
			AlterTableCmd 	*atc;
			AlterTableStmt 	*ats;

			if (childrelid == RelationGetRelid(rel))
				continue;

			childrel = heap_open(childrelid, AccessShareLock);
			if (RelationIsExternal(childrel))
			{
				heap_close(childrel, NoLock);
				continue;
			}
			CheckTableNotInUse(childrel, "ALTER TABLE");

			/* Recurse to child */
			atc = copyObject(cmd);
			atc->subtype = AT_ColumnDefaultRecurse;

			rv = makeRangeVar(get_namespace_name(RelationGetNamespace(childrel)),
							  get_rel_name(childrelid), -1);

			ats = makeNode(AlterTableStmt);
			ats->relation = rv;
			ats->cmds = list_make1(atc);
			ats->relkind = OBJECT_TABLE;

			heap_close(childrel, NoLock);

			ProcessUtility((Node *)ats,
							synthetic_sql,
							NULL,
							false, /* not top level */
							dest,
							NULL);
		}
	}
}

static void
ATExecColumnDefault(Relation rel, const char *colName,
					ColumnDef *colDef)
{
	AttrNumber	attnum;

	/*
	 * get the number of the attribute
	 */
	attnum = get_attnum(RelationGetRelid(rel), colName);
	if (attnum == InvalidAttrNumber)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));

	/* Prevent them from altering a system attribute */
	if (attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot alter system column \"%s\"",
						colName)));

	/*
	 * Remove any old default for the column.  We use RESTRICT here for
	 * safety, but at present we do not expect anything to depend on the
	 * default.
	 */
	RemoveAttrDefault(RelationGetRelid(rel), attnum, DROP_RESTRICT, false);

	if (colDef->raw_default)
	{
		/* SET DEFAULT */
		colDef->attnum = attnum;

		/*
		 * This function is intended for CREATE TABLE, so it processes a
		 * _list_ of defaults, but we just do one.
		 */
		AddRelationConstraints(rel, list_make1(colDef), NIL);
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ALTER COLUMN DEFAULT"
				);


}

/*
 * ALTER TABLE ALTER COLUMN SET STATISTICS
 */
static void
ATPrepSetStatistics(Relation rel, const char *colName, Node *flagValue)
{
	/*
	 * We do our own permission checking because (a) we want to allow SET
	 * STATISTICS on indexes (for expressional index columns), and (b) we want
	 * to allow SET STATISTICS on system catalogs without requiring
	 * allowSystemTableModsDDL to be turned on.
	 */
	if (rel->rd_rel->relkind != RELKIND_RELATION &&
		rel->rd_rel->relkind != RELKIND_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table or index",
						RelationGetRelationName(rel))));

	/* Permissions checks */
	if (!pg_class_ownercheck(RelationGetRelid(rel), GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));
}

static void
ATExecSetStatistics(Relation rel, const char *colName, Node *newValue)
{
	int			newtarget;
	Relation	attrelation;
	HeapTuple	tuple;
	Form_pg_attribute attrtuple;

	Assert(IsA(newValue, Integer));
	newtarget = intVal(newValue);

	/*
	 * Limit target to a sane range
	 */
	if (newtarget < -1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("statistics target %d is too low",
						newtarget)));
	}
	else if (newtarget > 10000)
	{
		newtarget = 10000;
		ereport(WARNING,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("lowering statistics target to %d",
						newtarget)));
	}

	attrelation = heap_open(AttributeRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));
	attrtuple = (Form_pg_attribute) GETSTRUCT(tuple);

	if (attrtuple->attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot alter system column \"%s\"",
						colName)));

	attrtuple->attstattarget = newtarget;

	simple_heap_update(attrelation, &tuple->t_self, tuple);

	/* keep system catalog indexes current */
	CatalogUpdateIndexes(attrelation, tuple);

	heap_freetuple(tuple);

	heap_close(attrelation, RowExclusiveLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ALTER COLUMN SET STATISTICS"
				);
}

/*
 * ALTER TABLE ALTER COLUMN SET STORAGE
 */
static void
ATExecSetStorage(Relation rel, const char *colName, Node *newValue)
{
	char	   *storagemode;
	char		newstorage;
	Relation	attrelation;
	HeapTuple	tuple;
	Form_pg_attribute attrtuple;

	Assert(IsA(newValue, String));
	storagemode = strVal(newValue);

	if (pg_strcasecmp(storagemode, "plain") == 0)
		newstorage = 'p';
	else if (pg_strcasecmp(storagemode, "external") == 0)
		newstorage = 'e';
	else if (pg_strcasecmp(storagemode, "extended") == 0)
		newstorage = 'x';
	else if (pg_strcasecmp(storagemode, "main") == 0)
		newstorage = 'm';
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid storage type \"%s\"",
						storagemode)));
		newstorage = 0;			/* keep compiler quiet */
	}

	attrelation = heap_open(AttributeRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));
	attrtuple = (Form_pg_attribute) GETSTRUCT(tuple);

	if (attrtuple->attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot alter system column \"%s\"",
						colName)));

	/*
	 * safety check: do not allow toasted storage modes unless column datatype
	 * is TOAST-aware.
	 */
	if (newstorage == 'p' || TypeIsToastable(attrtuple->atttypid))
		attrtuple->attstorage = newstorage;
	else
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("column data type %s can only have storage PLAIN",
						format_type_be(attrtuple->atttypid))));

	simple_heap_update(attrelation, &tuple->t_self, tuple);

	/* keep system catalog indexes current */
	CatalogUpdateIndexes(attrelation, tuple);

	heap_freetuple(tuple);

	heap_close(attrelation, RowExclusiveLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ALTER COLUMN SET STORAGE"
				);
}


/*
 * ALTER TABLE DROP COLUMN
 *
 * DROP COLUMN cannot use the normal ALTER TABLE recursion mechanism,
 * because we have to decide at runtime whether to recurse or not depending
 * on whether attinhcount goes to zero or not.	(We can't check this in a
 * static pre-pass because it won't handle multiple inheritance situations
 * correctly.)
 */
static void
ATExecDropColumn(List **wqueue, Relation rel, const char *colName,
				 DropBehavior behavior,
				 bool recurse, bool recursing)
{
	HeapTuple	tuple;
	Form_pg_attribute targetatt;
	AttrNumber	attnum;
	List	   *children;
	ObjectAddress object;
	GpPolicy  *policy;
	PartitionNode *pn;

	/* At top level, permission check was done in ATPrepCmd, else do it */
	if (recursing)
		ATSimplePermissions(rel, false);

	/*
	 * get the number of the attribute
	 */
	tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));
	targetatt = (Form_pg_attribute) GETSTRUCT(tuple);

	attnum = targetatt->attnum;

	/* Can't drop a system attribute, except OID */
	if (attnum <= 0 && attnum != ObjectIdAttributeNumber)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot drop system column \"%s\"",
						colName)));

	/* Don't drop inherited columns */
	if (targetatt->attinhcount > 0 && !recursing)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot drop inherited column \"%s\"",
						colName)));

	/* better not be a column we partition on */
	pn = RelationBuildPartitionDesc(rel, false);
	if (pn)
	{
		List *patts = get_partition_attrs(pn);

		if (list_member_int(patts, attnum))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot drop partitioning column \"%s\"",
							colName)));

		/*
		 * Remove any partition encoding entry
		 */
		RemovePartitionEncodingByRelidAttribute(RelationGetRelid(rel), attnum);
	}

	ReleaseSysCache(tuple);

	policy = rel->rd_cdbpolicy;
	if (policy != NULL && policy->ptype == POLICYTYPE_PARTITIONED)
	{
		int			ia = 0;

		for (ia = 0; ia < policy->nattrs; ia++)
		{
			if (attnum == policy->attrs[ia])
			{
				policy->nattrs = 0;
				rel->rd_cdbpolicy = GpPolicyCopy(GetMemoryChunkContext(rel), policy);
				GpPolicyReplace(RelationGetRelid(rel), policy);
				if (Gp_role != GP_ROLE_EXECUTE)
				    ereport(NOTICE,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("Dropping a column that is part of the "
								"distribution policy forces a "
								"NULL distribution policy")));
			}
		}
	}

	/*
	 * Propagate to children as appropriate.  Unlike most other ALTER
	 * routines, we have to do this one level of recursion at a time; we can't
	 * use find_all_inheritors to do it in one pass.
	 */
	children = find_inheritance_children(RelationGetRelid(rel));

	if (children)
	{
		Relation	attr_rel;
		ListCell   *child;

		attr_rel = heap_open(AttributeRelationId, RowExclusiveLock);
		foreach(child, children)
		{
			Oid			childrelid = lfirst_oid(child);
			Relation	childrel;
			Form_pg_attribute childatt;

			childrel = heap_open(childrelid, AccessExclusiveLock);
			CheckTableNotInUse(childrel, "ALTER TABLE");

			tuple = SearchSysCacheCopyAttName(childrelid, colName);
			if (!HeapTupleIsValid(tuple))		/* shouldn't happen */
				elog(ERROR, "cache lookup failed for attribute \"%s\" of relation %u",
					 colName, childrelid);
			childatt = (Form_pg_attribute) GETSTRUCT(tuple);

			if (childatt->attinhcount <= 0)		/* shouldn't happen */
				elog(ERROR, "relation %u has non-inherited attribute \"%s\"",
					 childrelid, colName);

			if (recurse)
			{
				/*
				 * If the child column has other definition sources, just
				 * decrement its inheritance count; if not or if this is part
				 * of a partition configuration, recurse to delete it.
				 */
				if ((childatt->attinhcount == 1 && !childatt->attislocal) ||
					pn)
				{
					/* Time to delete this child column, too */
					ATExecDropColumn(wqueue, childrel, colName, behavior, true, true);
				}
				else
				{
					/* Child column must survive my deletion */
					childatt->attinhcount--;

					simple_heap_update(attr_rel, &tuple->t_self, tuple);

					/* keep the system catalog indexes current */
					CatalogUpdateIndexes(attr_rel, tuple);

					/* Make update visible */
					CommandCounterIncrement();
				}
			}
			else
			{
				/*
				 * If we were told to drop ONLY in this table (no recursion),
				 * we need to mark the inheritors' attribute as locally
				 * defined rather than inherited.
				 */
				childatt->attinhcount--;
				childatt->attislocal = true;

				simple_heap_update(attr_rel, &tuple->t_self, tuple);

				/* keep the system catalog indexes current */
				CatalogUpdateIndexes(attr_rel, tuple);

				/* Make update visible */
				CommandCounterIncrement();
			}

			heap_freetuple(tuple);

			heap_close(childrel, NoLock);
		}
		heap_close(attr_rel, RowExclusiveLock);
	}

	/*
	 * Perform the actual column deletion
	 */
	object.classId = RelationRelationId;
	object.objectId = RelationGetRelid(rel);
	object.objectSubId = attnum;

	performDeletion(&object, behavior);

	/*
	 * If we dropped the OID column, must adjust pg_class.relhasoids and
	 * tell Phase 3 to physically get rid of the column.
	 */
	if (attnum == ObjectIdAttributeNumber)
	{
		Relation	class_rel;
		Form_pg_class tuple_class;
		AlteredTableInfo *tab;

		class_rel = heap_open(RelationRelationId, RowExclusiveLock);

		tuple = SearchSysCacheCopy(RELOID,
								   ObjectIdGetDatum(RelationGetRelid(rel)),
								   0, 0, 0);
		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for relation %u",
				 RelationGetRelid(rel));
		tuple_class = (Form_pg_class) GETSTRUCT(tuple);

		tuple_class->relhasoids = false;
		simple_heap_update(class_rel, &tuple->t_self, tuple);

		/* Keep the catalog indexes up to date */
		CatalogUpdateIndexes(class_rel, tuple);

		heap_close(class_rel, RowExclusiveLock);

		/* Find or create work queue entry for this table */
		tab = ATGetQueueEntry(wqueue, rel);

		/* Tell Phase 3 to physically remove the OID column */
		tab->new_dropoids = true;
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "DROP COLUMN"
				);

}

/*
 * ALTER TABLE ADD INDEX
 *
 * There is no such command in the grammar, but parse_utilcmd.c converts
 * UNIQUE and PRIMARY KEY constraints into AT_AddIndex subcommands.  This lets
 * us schedule creation of the index at the appropriate time during ALTER.
 */
static void
ATExecAddIndex(AlteredTableInfo *tab, Relation rel,
			   IndexStmt *stmt, bool is_rebuild)
{
	bool		check_rights;
	bool		skip_build;
	bool		quiet;

	Assert(IsA(stmt, IndexStmt));

	/* The index should already be built if we are a QE */
	if (Gp_role == GP_ROLE_EXECUTE)
		return;

	/* suppress schema rights check when rebuilding existing index */
	check_rights = !is_rebuild;
	/* skip index build if phase 3 will have to rewrite table anyway */
	skip_build = (tab->newvals != NIL);
	/* suppress notices when rebuilding existing index */
	quiet = is_rebuild;

	/* The IndexStmt has already been through transformIndexStmt */

	DefineIndex(stmt->relation, /* relation */
				stmt->idxname,	/* index name */
				InvalidOid,		/* no predefined OID */
				stmt->accessMethod,		/* am name */
				stmt->tableSpace,
				stmt->indexParams,		/* parameters */
				(Expr *) stmt->whereClause,
				stmt->options,
				stmt->unique,
				stmt->primary,
				stmt->isconstraint,
				true,			/* is_alter_table */
				check_rights,
				skip_build,
				quiet,
				false,
				stmt);

	/* 
	 * If we're the master and this is a partitioned table, cascade index
	 * creation for all children.
	 */
	if (Gp_role == GP_ROLE_DISPATCH &&
		rel_is_partitioned(RelationGetRelid(rel)))
	{
		List *children = find_all_inheritors(RelationGetRelid(rel));
		ListCell *lc;
		bool prefix_match = false;
		char *pname = RelationGetRelationName(rel);
		char *iname = stmt->idxname ? stmt->idxname : "idx";
		DestReceiver *dest = None_Receiver;

		/* is the parent relation name a prefix of the index name? */
		if (strlen(iname) > strlen(pname) &&
			strncmp(pname, iname, strlen(pname)) == 0)
			prefix_match = true;

		foreach(lc, children)
		{
			Oid relid = lfirst_oid(lc);
			Relation crel;
			IndexStmt *istmt;
			AlterTableStmt *ats;
			AlterTableCmd *atc;
			RangeVar *rv;
			char *idxname;

			if (relid == RelationGetRelid(rel))
				continue;

 			crel = heap_open(relid, AccessShareLock);
			istmt = copyObject(stmt);
			istmt->is_part_child = true;

			ats = makeNode(AlterTableStmt);
			atc = makeNode(AlterTableCmd);

			rv = makeRangeVar(get_namespace_name(RelationGetNamespace(rel)),
							  get_rel_name(relid), -1);
			istmt->relation = rv;
			if (prefix_match)
			{
				/* 
				 * If the next character in the index name is '_', absorb
				 * it, as ChooseRelationName() will add another.
				 */
				int off = 0;
				if (iname[strlen(pname)] == '_')
					off = 1;
				idxname = ChooseRelationName(RelationGetRelationName(crel),
											 NULL,
											 (iname + strlen(pname) + off),
											 RelationGetNamespace(crel));
			}
			else
				idxname = ChooseRelationName(RelationGetRelationName(crel),
											 NULL,
											 iname,
											 RelationGetNamespace(crel));
			istmt->idxname = idxname;
			atc->subtype = AT_AddIndex;
			atc->def = (Node *)istmt;
			atc->part_expanded = true;

			ats->relation = copyObject(istmt->relation);
			ats->cmds = list_make1(atc);
			ats->relkind = OBJECT_TABLE;

			heap_close(crel, AccessShareLock); /* already locked master */

			ProcessUtility((Node *)ats,
						   synthetic_sql,
						   NULL,
						   false, /* not top level */
						   dest,
						   NULL);
		}
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ADD INDEX"
				);

}

/*
 * ALTER TABLE ADD CONSTRAINT
 */

static void
ATExecAddConstraint(AlteredTableInfo *tab, Relation rel, Node *newConstraint, bool recurse)
{
	switch (nodeTag(newConstraint))
	{
		case T_Constraint:
			{
				Constraint *constr = (Constraint *) newConstraint;

				/*
				 * Currently, we only expect to see CONSTR_CHECK nodes
				 * arriving here (see the preprocessing done in
				 * parse_utilcmd.c).  Use a switch anyway to make it easier to
				 * add more code later.
				 */
				switch (constr->contype)
				{
					case CONSTR_CHECK:
						{
							ATAddCheckConstraint(tab, rel, constr, recurse);
							break;
						}
					default:
						elog(ERROR, "unrecognized constraint type: %d",
							 (int) constr->contype);
				}
				break;
			}
		case T_FkConstraint:
			{
				FkConstraint *fkconstraint = (FkConstraint *) newConstraint;

				/*
				 * Assign or validate constraint name
				 */
				if (fkconstraint->constr_name)
				{
					if (ConstraintNameIsUsed(CONSTRAINT_RELATION,
											 RelationGetRelid(rel),
											 RelationGetNamespace(rel),
											 fkconstraint->constr_name))
						ereport(ERROR,
								(errcode(ERRCODE_DUPLICATE_OBJECT),
								 errmsg("constraint \"%s\" for relation \"%s\" already exists",
										fkconstraint->constr_name,
										RelationGetRelationName(rel))));
				}
				else
					fkconstraint->constr_name =
						ChooseConstraintName(RelationGetRelationName(rel),
									strVal(linitial(fkconstraint->fk_attrs)),
											 "fkey",
											 RelationGetNamespace(rel),
											 NIL);

				ATAddForeignKeyConstraint(tab, rel, fkconstraint);

				break;
			}
		default:
			elog(ERROR, "unrecognized node type: %d",
				 (int) nodeTag(newConstraint));
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ADD CONSTRAINT"
				);

}

static void
ATAddCheckConstraint(AlteredTableInfo *tab, Relation rel, Constraint *constr, bool recurse)
{
	List		*newcons;
	ListCell	*lcon;
	List 		*children;
	ListCell	*lchild;

	/*
	 * Call AddRelationNewConstraints to do the work.
	 * It returns a list of cooked constraints.
	 */
	newcons = AddRelationConstraints(rel, NIL, list_make1(constr));

	/* Add each constraint to Phase 3's queue */
	foreach(lcon, newcons)
	{
		CookedConstraint *ccon = (CookedConstraint *) lfirst(lcon);
		NewConstraint *newcon;

		newcon = (NewConstraint *) palloc0(sizeof(NewConstraint));
		newcon->name = ccon->name;
		newcon->contype = ccon->contype;
		/* ExecQual wants implicit-AND format */
		newcon->qual = (Node *) make_ands_implicit((Expr *) ccon->expr);

		tab->constraints = lappend(tab->constraints, newcon);

		/* Save the actually assigned name if it was defaulted on partitioned table */
		if (rel_is_partitioned(RelationGetRelid(rel)) && constr->name == NULL)
			constr->name = ccon->name;
	}

	/*
	 * If this constraint needs to be recursed, this is a base/parent table,
	 * and we are the master, then cascade check constraint creation for all children.
	 * We do it here to synchronize pg_constraint oid.
	 */
	if (Gp_role == GP_ROLE_DISPATCH && recurse)
	{
		/* Propagate to children as appropriate. */
		children = find_inheritance_children(RelationGetRelid(rel));
		if (children == NIL)
			return;

		DestReceiver *dest = None_Receiver;
		foreach(lchild, children)
		{
			Oid 			childrelid = lfirst_oid(lchild);
			Relation 		childrel;
			Constraint 		*childconstr;

			RangeVar 		*rv;
			AlterTableCmd 	*atc;
			AlterTableStmt 	*ats;

			if (childrelid == RelationGetRelid(rel))
				continue;

 			childrel = heap_open(childrelid, AccessShareLock);
 			CheckTableNotInUse(childrel, "ALTER TABLE");

 			/* Recurse to child */
 			childconstr = copyObject(constr);
 			//Insist(childconstr->name != NULL);

			rv = makeRangeVar(get_namespace_name(RelationGetNamespace(childrel)),
							  get_rel_name(childrelid), -1);

			atc = makeNode(AlterTableCmd);
			atc->subtype = AT_AddConstraintRecurse;
			atc->def = (Node *) childconstr;

			ats = makeNode(AlterTableStmt);
			ats->relation = rv;
			ats->cmds = list_make1(atc);
			ats->relkind = OBJECT_TABLE;

			heap_close(childrel, AccessShareLock);

			ProcessUtility((Node *)ats,
						   synthetic_sql,
						   NULL,
						   false, /* not top level */
						   dest,
						   NULL);
		}
	}
}


/*
 * Add a foreign-key constraint to a single table
 *
 * Subroutine for ATExecAddConstraint.	Must already hold exclusive
 * lock on the rel, and have done appropriate validity/permissions checks
 * for it.
 */
static void
ATAddForeignKeyConstraint(AlteredTableInfo *tab, Relation rel,
						  FkConstraint *fkconstraint)
{
	Relation	pkrel;
	AclResult	aclresult;
	int16		pkattnum[INDEX_MAX_KEYS];
	int16		fkattnum[INDEX_MAX_KEYS];
	Oid			pktypoid[INDEX_MAX_KEYS];
	Oid			fktypoid[INDEX_MAX_KEYS];
	Oid			opclasses[INDEX_MAX_KEYS];
	Oid			pfeqoperators[INDEX_MAX_KEYS];
	Oid			ppeqoperators[INDEX_MAX_KEYS];
	Oid			ffeqoperators[INDEX_MAX_KEYS];
	int			i;
	int			numfks,
				numpks;
	Oid			indexOid;
	Oid			constrOid;

	/*
	 * Grab an exclusive lock on the pk table, so that someone doesn't delete
	 * rows out from under us. (Although a lesser lock would do for that
	 * purpose, we'll need exclusive lock anyway to add triggers to the pk
	 * table; trying to start with a lesser lock will just create a risk of
	 * deadlock.)
	 */
	pkrel = heap_openrv(fkconstraint->pktable, AccessExclusiveLock);

	/*
	 * Validity and permissions checks
	 *
	 * Note: REFERENCES permissions checks are redundant with CREATE TRIGGER,
	 * but we may as well error out sooner instead of later.
	 */
	if (pkrel->rd_rel->relkind != RELKIND_RELATION)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("referenced relation \"%s\" is not a table",
						RelationGetRelationName(pkrel))));

	aclresult = pg_class_aclcheck(RelationGetRelid(pkrel), GetUserId(),
								  ACL_REFERENCES);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_CLASS,
					   RelationGetRelationName(pkrel));

	if (!allowSystemTableModsDDL && IsSystemRelation(pkrel))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						RelationGetRelationName(pkrel))));

	aclresult = pg_class_aclcheck(RelationGetRelid(rel), GetUserId(),
								  ACL_REFERENCES);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));

	/*
	 * Disallow reference from permanent table to temp table or vice versa.
	 * (The ban on perm->temp is for fairly obvious reasons.  The ban on
	 * temp->perm is because other backends might need to run the RI triggers
	 * on the perm table, but they can't reliably see tuples the owning
	 * backend has created in the temp table, because non-shared buffers are
	 * used for temp tables.)
	 */
	if (isTempNamespace(RelationGetNamespace(pkrel)))
	{
		if (!isTempNamespace(RelationGetNamespace(rel)))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot reference temporary table from permanent table constraint")));
	}
	else
	{
		if (isTempNamespace(RelationGetNamespace(rel)))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot reference permanent table from temporary table constraint")));
	}
	
	/*
	 * Disallow reference to a part of a partitioned table.  A foreign key 
	 * must reference the whole partitioned table or none of it.
	 */
	if ( rel_is_child_partition(RelationGetRelid(pkrel)) )
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot reference just part of a partitioned table")));
	}

	/*
	 * Look up the referencing attributes to make sure they exist, and record
	 * their attnums and type OIDs.
	 */
	MemSet(pkattnum, 0, sizeof(pkattnum));
	MemSet(fkattnum, 0, sizeof(fkattnum));
	MemSet(pktypoid, 0, sizeof(pktypoid));
	MemSet(fktypoid, 0, sizeof(fktypoid));
	MemSet(opclasses, 0, sizeof(opclasses));
	MemSet(pfeqoperators, 0, sizeof(pfeqoperators));
	MemSet(ppeqoperators, 0, sizeof(ppeqoperators));
	MemSet(ffeqoperators, 0, sizeof(ffeqoperators));

	numfks = transformColumnNameList(RelationGetRelid(rel),
									 fkconstraint->fk_attrs,
									 fkattnum, fktypoid);

	/*
	 * If the attribute list for the referenced table was omitted, lookup the
	 * definition of the primary key and use it.  Otherwise, validate the
	 * supplied attribute list.  In either case, discover the index OID and
	 * index opclasses, and the attnums and type OIDs of the attributes.
	 */
	if (fkconstraint->pk_attrs == NIL)
	{
		numpks = transformFkeyGetPrimaryKey(pkrel, &indexOid,
											&fkconstraint->pk_attrs,
											pkattnum, pktypoid,
											opclasses);
	}
	else
	{
		numpks = transformColumnNameList(RelationGetRelid(pkrel),
										 fkconstraint->pk_attrs,
										 pkattnum, pktypoid);
		/* Look for an index matching the column list */
		indexOid = transformFkeyCheckAttrs(pkrel, numpks, pkattnum,
										   opclasses);
	}

	/*
	 * Look up the equality operators to use in the constraint.
	 *
	 * Note that we have to be careful about the difference between the actual
	 * PK column type and the opclass' declared input type, which might be
	 * only binary-compatible with it.	The declared opcintype is the right
	 * thing to probe pg_amop with.
	 */
	if (numfks != numpks)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FOREIGN_KEY),
				 errmsg("number of referencing and referenced columns for foreign key disagree")));

	for (i = 0; i < numpks; i++)
	{
		Oid			pktype = pktypoid[i];
		Oid			fktype = fktypoid[i];
		Oid			fktyped;
		HeapTuple	cla_ht;
		Form_pg_opclass cla_tup;
		Oid			amid;
		Oid			opfamily;
		Oid			opcintype;
		Oid			pfeqop;
		Oid			ppeqop;
		Oid			ffeqop;
		int16		eqstrategy;

		/* We need several fields out of the pg_opclass entry */
		cla_ht = SearchSysCache(CLAOID,
								ObjectIdGetDatum(opclasses[i]),
								0, 0, 0);
		if (!HeapTupleIsValid(cla_ht))
			elog(ERROR, "cache lookup failed for opclass %u", opclasses[i]);
		cla_tup = (Form_pg_opclass) GETSTRUCT(cla_ht);
		amid = cla_tup->opcmethod;
		opfamily = cla_tup->opcfamily;
		opcintype = cla_tup->opcintype;
		ReleaseSysCache(cla_ht);

		/*
		 * Check it's a btree; currently this can never fail since no other
		 * index AMs support unique indexes.  If we ever did have other types
		 * of unique indexes, we'd need a way to determine which operator
		 * strategy number is equality.  (Is it reasonable to insist that
		 * every such index AM use btree's number for equality?)
		 */
		if (amid != BTREE_AM_OID)
			elog(ERROR, "only b-tree indexes are supported for foreign keys");
		eqstrategy = BTEqualStrategyNumber;

		/*
		 * There had better be a primary equality operator for the index.
		 * We'll use it for PK = PK comparisons.
		 */
		ppeqop = get_opfamily_member(opfamily, opcintype, opcintype,
									 eqstrategy);

		if (!OidIsValid(ppeqop))
			elog(ERROR, "missing operator %d(%u,%u) in opfamily %u",
				 eqstrategy, opcintype, opcintype, opfamily);

		/*
		 * Are there equality operators that take exactly the FK type? Assume
		 * we should look through any domain here.
		 */
		fktyped = getBaseType(fktype);

		pfeqop = get_opfamily_member(opfamily, opcintype, fktyped,
									 eqstrategy);
		if (OidIsValid(pfeqop))
			ffeqop = get_opfamily_member(opfamily, fktyped, fktyped,
										 eqstrategy);
		else
			ffeqop = InvalidOid;	/* keep compiler quiet */

		if (!(OidIsValid(pfeqop) && OidIsValid(ffeqop)))
		{
			/*
			 * Otherwise, look for an implicit cast from the FK type to the
			 * opcintype, and if found, use the primary equality operator.
			 * This is a bit tricky because opcintype might be a polymorphic
			 * type such as ANYARRAY or ANYENUM; so what we have to test is
			 * whether the two actual column types can be concurrently cast to
			 * that type.  (Otherwise, we'd fail to reject combinations such
			 * as int[] and point[].)
			 */
			Oid			input_typeids[2];
			Oid			target_typeids[2];

			input_typeids[0] = pktype;
			input_typeids[1] = fktype;
			target_typeids[0] = opcintype;
			target_typeids[1] = opcintype;
			if (can_coerce_type(2, input_typeids, target_typeids,
								COERCION_IMPLICIT))
				pfeqop = ffeqop = ppeqop;
		}

		if (!(OidIsValid(pfeqop) && OidIsValid(ffeqop)))
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("foreign key constraint \"%s\" "
							"cannot be implemented",
							fkconstraint->constr_name),
					 errdetail("Key columns \"%s\" and \"%s\" "
							   "are of incompatible types: %s and %s.",
							   strVal(list_nth(fkconstraint->fk_attrs, i)),
							   strVal(list_nth(fkconstraint->pk_attrs, i)),
							   format_type_be(fktype),
							   format_type_be(pktype))));

		pfeqoperators[i] = pfeqop;
		ppeqoperators[i] = ppeqop;
		ffeqoperators[i] = ffeqop;
	}

	/*
	 * Record the FK constraint in pg_constraint.
	 */
	constrOid = CreateConstraintEntry(fkconstraint->constr_name,
									  RelationGetNamespace(rel),
									  CONSTRAINT_FOREIGN,
									  fkconstraint->deferrable,
									  fkconstraint->initdeferred,
									  RelationGetRelid(rel),
									  fkattnum,
									  numfks,
									  InvalidOid,		/* not a domain
														 * constraint */
									  RelationGetRelid(pkrel),
									  pkattnum,
									  pfeqoperators,
									  ppeqoperators,
									  ffeqoperators,
									  numpks,
									  fkconstraint->fk_upd_action,
									  fkconstraint->fk_del_action,
									  fkconstraint->fk_matchtype,
									  indexOid,
									  NULL,		/* no check constraint */
									  NULL,
									  NULL);

	/*
	 * Create the triggers that will enforce the constraint.
	 */
	createForeignKeyTriggers(rel, fkconstraint, constrOid);

	/*
	 * Tell Phase 3 to check that the constraint is satisfied by existing rows
	 * (we can skip this during table creation).
	 */
	if (!fkconstraint->skip_validation)
	{
		NewConstraint *newcon;

		newcon = (NewConstraint *) palloc0(sizeof(NewConstraint));
		newcon->name = fkconstraint->constr_name;
		newcon->contype = CONSTR_FOREIGN;
		newcon->refrelid = RelationGetRelid(pkrel);
		newcon->conid = constrOid;
		newcon->qual = (Node *) fkconstraint;

		tab->constraints = lappend(tab->constraints, newcon);
	}

	/*
	 * Close pk table, but keep lock until we've committed.
	 */
	heap_close(pkrel, NoLock);
}


/*
 * transformColumnNameList - transform list of column names
 *
 * Lookup each name and return its attnum and type OID
 */
static int
transformColumnNameList(Oid relId, List *colList,
						int16 *attnums, Oid *atttypids)
{
	ListCell   *l;
	int			attnum;

	attnum = 0;
	foreach(l, colList)
	{
		char	   *attname = strVal(lfirst(l));
		HeapTuple	atttuple;

		atttuple = SearchSysCacheAttName(relId, attname);

		if (!HeapTupleIsValid(atttuple))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" referenced in foreign key constraint does not exist",
							attname)));
		if (attnum >= INDEX_MAX_KEYS)
			ereport(ERROR,
					(errcode(ERRCODE_TOO_MANY_COLUMNS),
					 errmsg("cannot have more than %d keys in a foreign key",
							INDEX_MAX_KEYS)));
		attnums[attnum] = ((Form_pg_attribute) GETSTRUCT(atttuple))->attnum;
		atttypids[attnum] = ((Form_pg_attribute) GETSTRUCT(atttuple))->atttypid;

		ReleaseSysCache(atttuple);

		attnum++;
	}

	return attnum;
}

/*
 * transformFkeyGetPrimaryKey -
 *
 *	Look up the names, attnums, and types of the primary key attributes
 *	for the pkrel.	Also return the index OID and index opclasses of the
 *	index supporting the primary key.
 *
 *	All parameters except pkrel are output parameters.	Also, the function
 *	return value is the number of attributes in the primary key.
 *
 *	Used when the column list in the REFERENCES specification is omitted.
 */
static int
transformFkeyGetPrimaryKey(Relation pkrel, Oid *indexOid,
						   List **attnamelist,
						   int16 *attnums, Oid *atttypids,
						   Oid *opclasses)
{
	List	   *indexoidlist;
	ListCell   *indexoidscan;
	HeapTuple	indexTuple = NULL;
	Form_pg_index indexStruct = NULL;
	Datum		indclassDatum;
	bool		isnull;
	oidvector  *indclass;
	int			i;

	/*
	 * Get the list of index OIDs for the table from the relcache, and look up
	 * each one in the pg_index syscache until we find one marked primary key
	 * (hopefully there isn't more than one such).  Insist it's valid, too.
	 */
	*indexOid = InvalidOid;

	indexoidlist = RelationGetIndexList(pkrel);

	foreach(indexoidscan, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(indexoidscan);

		indexTuple = SearchSysCache(INDEXRELID,
									ObjectIdGetDatum(indexoid),
									0, 0, 0);
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexoid);
		indexStruct = (Form_pg_index) GETSTRUCT(indexTuple);
		if (indexStruct->indisprimary && IndexIsValid(indexStruct))
		{
			*indexOid = indexoid;
			break;
		}
		ReleaseSysCache(indexTuple);
	}

	list_free(indexoidlist);

	/*
	 * Check that we found it
	 */
	if (!OidIsValid(*indexOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("there is no primary key for referenced table \"%s\"",
						RelationGetRelationName(pkrel))));

	/* Must get indclass the hard way */
	indclassDatum = SysCacheGetAttr(INDEXRELID, indexTuple,
									Anum_pg_index_indclass, &isnull);
	Assert(!isnull);
	indclass = (oidvector *) DatumGetPointer(indclassDatum);

	/*
	 * Now build the list of PK attributes from the indkey definition (we
	 * assume a primary key cannot have expressional elements)
	 */
	*attnamelist = NIL;
	for (i = 0; i < indexStruct->indnatts; i++)
	{
		int			pkattno = indexStruct->indkey.values[i];

		attnums[i] = pkattno;
		atttypids[i] = attnumTypeId(pkrel, pkattno);
		opclasses[i] = indclass->values[i];
		*attnamelist = lappend(*attnamelist,
			   makeString(pstrdup(NameStr(*attnumAttName(pkrel, pkattno)))));
	}

	ReleaseSysCache(indexTuple);

	return i;
}

/*
 * transformFkeyCheckAttrs -
 *
 *	Make sure that the attributes of a referenced table belong to a unique
 *	(or primary key) constraint.  Return the OID of the index supporting
 *	the constraint, as well as the opclasses associated with the index
 *	columns.
 */
Oid
transformFkeyCheckAttrs(Relation pkrel,
						int numattrs, int16 *attnums,
						Oid *opclasses) /* output parameter */
{
	Oid			indexoid = InvalidOid;
	bool		found = false;
	List	   *indexoidlist;
	ListCell   *indexoidscan;

	/*
	 * Get the list of index OIDs for the table from the relcache, and look up
	 * each one in the pg_index syscache, and match unique indexes to the list
	 * of attnums we are given.
	 */
	indexoidlist = RelationGetIndexList(pkrel);

	foreach(indexoidscan, indexoidlist)
	{
		HeapTuple	indexTuple;
		Form_pg_index indexStruct;
		int			i,
					j;

		indexoid = lfirst_oid(indexoidscan);
		indexTuple = SearchSysCache(INDEXRELID,
									ObjectIdGetDatum(indexoid),
									0, 0, 0);
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexoid);
		indexStruct = (Form_pg_index) GETSTRUCT(indexTuple);

		/*
		 * Must have the right number of columns; must be unique and not a
		 * partial index; forget it if there are any expressions, too. Invalid
		 * indexes are out as well.
		 */
		if (indexStruct->indnatts == numattrs &&
			indexStruct->indisunique &&
			IndexIsValid(indexStruct) &&
			heap_attisnull(indexTuple, Anum_pg_index_indpred) &&
			heap_attisnull(indexTuple, Anum_pg_index_indexprs))
		{
			/* Must get indclass the hard way */
			Datum		indclassDatum;
			bool		isnull;
			oidvector  *indclass;

			indclassDatum = SysCacheGetAttr(INDEXRELID, indexTuple,
											Anum_pg_index_indclass, &isnull);
			Assert(!isnull);
			indclass = (oidvector *) DatumGetPointer(indclassDatum);

			/*
			 * The given attnum list may match the index columns in any order.
			 * Check that each list is a subset of the other.
			 */
			for (i = 0; i < numattrs; i++)
			{
				found = false;
				for (j = 0; j < numattrs; j++)
				{
					if (attnums[i] == indexStruct->indkey.values[j])
					{
						found = true;
						break;
					}
				}
				if (!found)
					break;
			}
			if (found)
			{
				for (i = 0; i < numattrs; i++)
				{
					found = false;
					for (j = 0; j < numattrs; j++)
					{
						if (attnums[j] == indexStruct->indkey.values[i])
						{
							opclasses[j] = indclass->values[i];
							found = true;
							break;
						}
					}
					if (!found)
						break;
				}
			}
		}
		ReleaseSysCache(indexTuple);
		if (found)
			break;
	}

	if (!found)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FOREIGN_KEY),
				 errmsg("there is no unique constraint matching given keys for referenced table \"%s\"",
						RelationGetRelationName(pkrel))));

	list_free(indexoidlist);

	return indexoid;
}

/*
 * Scan the existing rows in a table to verify they meet a proposed FK
 * constraint.
 *
 * Caller must have opened and locked both relations.
 */
static void
validateForeignKeyConstraint(FkConstraint *fkconstraint,
							 Relation rel,
							 Relation pkrel,
							 Oid constraintOid)
{
	HeapScanDesc scan;
	HeapTuple	tuple;
	Trigger		trig;

	/*
	 * Build a trigger call structure; we'll need it either way.
	 */
	MemSet(&trig, 0, sizeof(trig));
	trig.tgoid = InvalidOid;
	trig.tgname = fkconstraint->constr_name;
	trig.tgenabled = TRIGGER_FIRES_ON_ORIGIN;
	trig.tgisconstraint = TRUE;
	trig.tgconstrrelid = RelationGetRelid(pkrel);
	trig.tgconstraint = constraintOid;
	trig.tgdeferrable = FALSE;
	trig.tginitdeferred = FALSE;
	/* we needn't fill in tgargs */

	/*
	 * See if we can do it with a single LEFT JOIN query.  A FALSE result
	 * indicates we must proceed with the fire-the-trigger method.
	 */
	if (RI_Initial_Check(&trig, rel, pkrel))
		return;

	/*
	 * Scan through each tuple, calling RI_FKey_check_ins (insert trigger) as
	 * if that tuple had just been inserted.  If any of those fail, it should
	 * ereport(ERROR) and that's that.
	 */
	scan = heap_beginscan(rel, SnapshotNow, 0, NULL);

	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		FunctionCallInfoData fcinfo;
		TriggerData trigdata;

		/*
		 * Make a call to the trigger function
		 *
		 * No parameters are passed, but we do set a context
		 */
		MemSet(&fcinfo, 0, sizeof(fcinfo));

		/*
		 * We assume RI_FKey_check_ins won't look at flinfo...
		 */
		trigdata.type = T_TriggerData;
		trigdata.tg_event = TRIGGER_EVENT_INSERT | TRIGGER_EVENT_ROW;
		trigdata.tg_relation = rel;
		trigdata.tg_trigtuple = tuple;
		trigdata.tg_newtuple = NULL;
		trigdata.tg_trigger = &trig;
		trigdata.tg_trigtuplebuf = scan->rs_cbuf;
		trigdata.tg_newtuplebuf = InvalidBuffer;

		fcinfo.context = (Node *) &trigdata;

		RI_FKey_check_ins(&fcinfo);
	}

	heap_endscan(scan);
}

static void
CreateFKCheckTrigger(RangeVar *myRel, FkConstraint *fkconstraint,
					 Oid constraintOid, bool on_insert)
{
	CreateTrigStmt *fk_trigger;
	Oid trigobj;

	fk_trigger = makeNode(CreateTrigStmt);
	fk_trigger->trigname = fkconstraint->constr_name;
	fk_trigger->relation = myRel;
	fk_trigger->before = false;
	fk_trigger->row = true;

	/* Either ON INSERT or ON UPDATE */
	if (on_insert)
	{
		fk_trigger->funcname = SystemFuncName("RI_FKey_check_ins");
		fk_trigger->actions[0] = 'i';
		fk_trigger->trigOid = fkconstraint->trig1Oid;
	}
	else
	{
		fk_trigger->funcname = SystemFuncName("RI_FKey_check_upd");
		fk_trigger->actions[0] = 'u';
		fk_trigger->trigOid = fkconstraint->trig2Oid;
	}
	fk_trigger->actions[1] = '\0';

	fk_trigger->isconstraint = true;
	fk_trigger->deferrable = fkconstraint->deferrable;
	fk_trigger->initdeferred = fkconstraint->initdeferred;
	fk_trigger->constrrel = fkconstraint->pktable;
	fk_trigger->args = NIL;

	trigobj = CreateTrigger(fk_trigger, constraintOid);

	if (on_insert)
		fkconstraint->trig1Oid = trigobj;
	else
		fkconstraint->trig2Oid = trigobj;

	/* Make changes-so-far visible */
	CommandCounterIncrement();
}

/*
 * Create the triggers that implement an FK constraint.
 */
static void
createForeignKeyTriggers(Relation rel, FkConstraint *fkconstraint,
						 Oid constraintOid)
{
	RangeVar   *myRel;
	CreateTrigStmt *fk_trigger;

	/*
	 * Special for Greenplum Database: Ignore foreign keys for now, with warning
	 */
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE)
	{
		if (Gp_role == GP_ROLE_DISPATCH)
			ereport(WARNING,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("Referential integrity (FOREIGN KEY) constraints are not supported in Greenplum Database, will not be enforced.")));
	}

	/*
	 * Reconstruct a RangeVar for my relation (not passed in, unfortunately).
	 */
	myRel = makeRangeVar(get_namespace_name(RelationGetNamespace(rel)),
						 pstrdup(RelationGetRelationName(rel)), -1);

	/* Make changes-so-far visible */
	CommandCounterIncrement();

	/*
	 * Build and execute a CREATE CONSTRAINT TRIGGER statement for the ON
	 * DELETE action on the referenced table.
	 */
	fk_trigger = makeNode(CreateTrigStmt);
	fk_trigger->trigname = fkconstraint->constr_name;
	fk_trigger->relation = fkconstraint->pktable;
	fk_trigger->before = false;
	fk_trigger->row = true;
	fk_trigger->actions[0] = 'd';
	fk_trigger->actions[1] = '\0';

	fk_trigger->isconstraint = true;
	fk_trigger->constrrel = myRel;
	switch (fkconstraint->fk_del_action)
	{
		case FKCONSTR_ACTION_NOACTION:
			fk_trigger->deferrable = fkconstraint->deferrable;
			fk_trigger->initdeferred = fkconstraint->initdeferred;
			fk_trigger->funcname = SystemFuncName("RI_FKey_noaction_del");
			break;
		case FKCONSTR_ACTION_RESTRICT:
			fk_trigger->deferrable = false;
			fk_trigger->initdeferred = false;
			fk_trigger->funcname = SystemFuncName("RI_FKey_restrict_del");
			break;
		case FKCONSTR_ACTION_CASCADE:
			fk_trigger->deferrable = false;
			fk_trigger->initdeferred = false;
			fk_trigger->funcname = SystemFuncName("RI_FKey_cascade_del");
			break;
		case FKCONSTR_ACTION_SETNULL:
			fk_trigger->deferrable = false;
			fk_trigger->initdeferred = false;
			fk_trigger->funcname = SystemFuncName("RI_FKey_setnull_del");
			break;
		case FKCONSTR_ACTION_SETDEFAULT:
			fk_trigger->deferrable = false;
			fk_trigger->initdeferred = false;
			fk_trigger->funcname = SystemFuncName("RI_FKey_setdefault_del");
			break;
		default:
			elog(ERROR, "unrecognized FK action type: %d",
				 (int) fkconstraint->fk_del_action);
			break;
	}
	fk_trigger->args = NIL;
	fk_trigger->trigOid = fkconstraint->trig3Oid;

	fkconstraint->trig3Oid = CreateTrigger(fk_trigger, constraintOid);

	/* Make changes-so-far visible */
	CommandCounterIncrement();

	/*
	 * Build and execute CREATE CONSTRAINT TRIGGER statements for the CHECK
	 * action for both INSERTs and UPDATEs on the referencing table.
	 *
	 * Note: for a self-referential FK (referencing and referenced tables are
	 * the same), it is important that the ON UPDATE action fires before the
	 * CHECK action, since both triggers will fire on the same row during an
	 * UPDATE event; otherwise the CHECK trigger will be checking a non-final
	 * state of the row.  Because triggers fire in name order, we are
	 * effectively relying on the OIDs of the triggers to sort correctly as
	 * text.  This will work except when the OID counter wraps around or adds
	 * a digit, eg "99999" sorts after "100000".  That is infrequent enough,
	 * and the use of self-referential FKs is rare enough, that we live with
	 * it for now.  There will be a real fix in PG 9.2.
	 */
	CreateFKCheckTrigger(myRel, fkconstraint, constraintOid, true);
	CreateFKCheckTrigger(myRel, fkconstraint, constraintOid, false);

	/*
	 * Build and execute a CREATE CONSTRAINT TRIGGER statement for the ON
	 * UPDATE action on the referenced table.
	 */
	fk_trigger = makeNode(CreateTrigStmt);
	fk_trigger->trigname = fkconstraint->constr_name;
	fk_trigger->relation = fkconstraint->pktable;
	fk_trigger->before = false;
	fk_trigger->row = true;
	fk_trigger->actions[0] = 'u';
	fk_trigger->actions[1] = '\0';
	fk_trigger->isconstraint = true;
	fk_trigger->constrrel = myRel;
	switch (fkconstraint->fk_upd_action)
	{
		case FKCONSTR_ACTION_NOACTION:
			fk_trigger->deferrable = fkconstraint->deferrable;
			fk_trigger->initdeferred = fkconstraint->initdeferred;
			fk_trigger->funcname = SystemFuncName("RI_FKey_noaction_upd");
			break;
		case FKCONSTR_ACTION_RESTRICT:
			fk_trigger->deferrable = false;
			fk_trigger->initdeferred = false;
			fk_trigger->funcname = SystemFuncName("RI_FKey_restrict_upd");
			break;
		case FKCONSTR_ACTION_CASCADE:
			fk_trigger->deferrable = false;
			fk_trigger->initdeferred = false;
			fk_trigger->funcname = SystemFuncName("RI_FKey_cascade_upd");
			break;
		case FKCONSTR_ACTION_SETNULL:
			fk_trigger->deferrable = false;
			fk_trigger->initdeferred = false;
			fk_trigger->funcname = SystemFuncName("RI_FKey_setnull_upd");
			break;
		case FKCONSTR_ACTION_SETDEFAULT:
			fk_trigger->deferrable = false;
			fk_trigger->initdeferred = false;
			fk_trigger->funcname = SystemFuncName("RI_FKey_setdefault_upd");
			break;
		default:
			elog(ERROR, "unrecognized FK action type: %d",
				 (int) fkconstraint->fk_upd_action);
			break;
	}
	fk_trigger->args = NIL;

	fk_trigger->trigOid = fkconstraint->trig4Oid;

	fkconstraint->trig4Oid = CreateTrigger(fk_trigger, constraintOid);
}

/*
 * ALTER TABLE DROP CONSTRAINT
 */
static void
ATPrepDropConstraint(List **wqueue, Relation rel,
					 bool recurse, AlterTableCmd *cmd)
{
	/*
	 * We don't want errors or noise from child tables, so we have to pass
	 * down a modified command.
	 *
	 * This can't result in dropping the partitioning rule because the
	 * partitioning rule exists only on the part, and ATPrepCmd guards
	 * against dropping directly from parts.
	 */
	if (recurse)
	{
		AlterTableCmd *childCmd = copyObject(cmd);

		childCmd->subtype = AT_DropConstraintQuietly;
		ATSimpleRecursion(wqueue, rel, childCmd, recurse);
	}
}

static void
ATExecDropConstraint(Relation rel, const char *constrName,
					 DropBehavior behavior, bool quiet)
{
	int			deleted;

	deleted = RemoveRelConstraints(rel, constrName, behavior);

	if (!quiet)
	{
		/* If zero constraints deleted, complain */
		if (deleted == 0)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("constraint \"%s\" does not exist",
							constrName)));
		/* Otherwise if more than one constraint deleted, notify */
		else if (deleted > 1 && Gp_role != GP_ROLE_EXECUTE)
			ereport(NOTICE,
					(errmsg("multiple constraints named \"%s\" were dropped",
							constrName)));
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "DROP CONSTRAINT"
				);

}

/*
 * ALTER COLUMN TYPE
 */
static void
ATPrepAlterColumnType(List **wqueue,
					  AlteredTableInfo *tab, Relation rel,
					  bool recurse, bool recursing,
					  AlterTableCmd *cmd)
{
	char	   *colName = cmd->name;
	TypeName   *typename = (TypeName *) cmd->def;
	HeapTuple	tuple;
	Form_pg_attribute attTup;
	AttrNumber	attnum;
	Oid			targettype;
	int32		targettypmod;
	Node	   *transform;
	NewColumnValue *newval;
	ParseState *pstate = make_parsestate(NULL);

	/* lookup the attribute so we can check inheritance status */
	tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));
	attTup = (Form_pg_attribute) GETSTRUCT(tuple);
	attnum = attTup->attnum;

	/* Can't alter a system attribute */
	if (attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot alter system column \"%s\"",
						colName)));

	/* Don't alter inherited columns */
	if (attTup->attinhcount > 0 && !recursing)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot alter inherited column \"%s\"",
						colName)));

	/* Look up the target type */
	targettype = typenameTypeId(NULL, typename, &targettypmod);

	/* make sure datatype is legal for a column */
	CheckAttributeType(colName, targettype,
					   list_make1_oid(rel->rd_rel->reltype));

	/*
	 * Set up an expression to transform the old data value to the new type.
	 * If a USING option was given, transform and use that expression, else
	 * just take the old value and try to coerce it.  We do this first so that
	 * type incompatibility can be detected before we waste effort, and
	 * because we need the expression to be parsed against the original table
	 * rowtype.
	 */

	/* GPDB: we always need the RTE */
	{
		RangeTblEntry *rte;

		/* Expression must be able to access vars of old table */
		rte = addRangeTableEntryForRelation(pstate,
											rel,
											NULL,
											false,
											true);
		addRTEtoQuery(pstate, rte, false, true, true);
	}

	if (cmd->transform)
	{
		transform = transformExpr(pstate, cmd->transform);

		/* It can't return a set */
		if (expression_returns_set(transform))
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("transform expression must not return a set")));

		/* No subplans or aggregates, either... */
		if (pstate->p_hasSubLinks)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot use subquery in transform expression")));
		if (pstate->p_hasAggs)
			ereport(ERROR,
					(errcode(ERRCODE_GROUPING_ERROR),
			errmsg("cannot use aggregate function in transform expression")));
		if (pstate->p_hasWindowFuncs)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("cannot use window function in transform expression")));

	}
	else
	{
		transform = (Node *) makeVar(1, attnum,
									 attTup->atttypid, attTup->atttypmod,
									 0);
	}

	transform = coerce_to_target_type(pstate,
									  transform, exprType(transform),
									  targettype, targettypmod,
									  COERCION_ASSIGNMENT,
									  COERCE_IMPLICIT_CAST,
									  -1);
	if (transform == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("column \"%s\" cannot be cast to type \"%s\"",
						colName, TypeNameToString(typename))));

	free_parsestate(pstate);

	/*
	 * Add a work queue item to make ATRewriteTable update the column
	 * contents.
	 */
	newval = (NewColumnValue *) palloc0(sizeof(NewColumnValue));
	newval->attnum = attnum;
	newval->expr = (Expr *) transform;

	tab->newvals = lappend(tab->newvals, newval);

	ReleaseSysCache(tuple);

	/*
	 * The recursion case is handled by ATSimpleRecursion.	However, if we are
	 * told not to recurse, there had better not be any child tables; else the
	 * alter would put them out of step.
	 */
	if (recurse)
		ATSimpleRecursion(wqueue, rel, cmd, recurse);
	else if (!recursing &&
			 find_inheritance_children(RelationGetRelid(rel)) != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("type of inherited column \"%s\" must be changed in child tables too",
						colName)));
}

static void
ATExecAlterColumnType(AlteredTableInfo *tab, Relation rel,
					  const char *colName, TypeName *typename)
{
	HeapTuple	heapTup;
	Form_pg_attribute attTup;
	AttrNumber	attnum;
	HeapTuple	typeTuple;
	Form_pg_type tform;
	Oid			targettype;
	int32		targettypmod;
	Node	   *defaultexpr;
	Relation	attrelation;
	Relation	depRel;
	ScanKeyData key[3];
	SysScanDesc scan;
	HeapTuple	depTup;
	GpPolicy   *policy = rel->rd_cdbpolicy;
	bool		sourceIsInt = false;
	bool		targetIsInt = false;
	bool		sourceIsVarlenA = false;
	bool		targetIsVarlenA = false;
	bool		hashCompatible = false;
	bool		relContainsTuples = false;

	attrelation = heap_open(AttributeRelationId, RowExclusiveLock);

	/* Look up the target column */
	heapTup = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);
	if (!HeapTupleIsValid(heapTup))		/* shouldn't happen */
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));
	attTup = (Form_pg_attribute) GETSTRUCT(heapTup);
	attnum = attTup->attnum;

	/* Check for multiple ALTER TYPE on same column --- can't cope */
	if (attTup->atttypid != tab->oldDesc->attrs[attnum - 1]->atttypid ||
		attTup->atttypmod != tab->oldDesc->attrs[attnum - 1]->atttypmod)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot alter type of column \"%s\" twice",
						colName)));

	/* Look up the target type (should not fail, since prep found it) */
	typeTuple = typenameType(NULL, typename, &targettypmod);
	tform = (Form_pg_type) GETSTRUCT(typeTuple);
	targettype = HeapTupleGetOid(typeTuple);

	if (targettype == INT4OID ||
		targettype == INT2OID ||
		targettype == INT8OID)
		sourceIsInt = true;

	if (attTup->atttypid == INT4OID ||
		attTup->atttypid == INT2OID ||
		attTup->atttypid == INT8OID)
		targetIsInt = true;

	if (targettype == VARCHAROID ||
		targettype == CHAROID ||
		targettype == TEXTOID)
		targetIsVarlenA = true;

	if (attTup->atttypid == VARCHAROID ||
		attTup->atttypid == CHAROID ||
		attTup->atttypid == TEXTOID)
		sourceIsVarlenA = true;

	if (sourceIsInt && targetIsInt)
		hashCompatible = true;
	else if (sourceIsVarlenA && targetIsVarlenA)
		hashCompatible = true;

	/*
	 * If there is a default expression for the column, get it and ensure we
	 * can coerce it to the new datatype.  (We must do this before changing
	 * the column type, because build_column_default itself will try to
	 * coerce, and will not issue the error message we want if it fails.)
	 *
	 * We remove any implicit coercion steps at the top level of the old
	 * default expression; this has been agreed to satisfy the principle of
	 * least surprise.	(The conversion to the new column type should act like
	 * it started from what the user sees as the stored expression, and the
	 * implicit coercions aren't going to be shown.)
	 */
	if (attTup->atthasdef)
	{
		defaultexpr = build_column_default(rel, attnum);
		Assert(defaultexpr);
		defaultexpr = strip_implicit_coercions(defaultexpr);
		defaultexpr = coerce_to_target_type(NULL,		/* no UNKNOWN params */
										  defaultexpr, exprType(defaultexpr),
											targettype, targettypmod,
											COERCION_ASSIGNMENT,
											COERCE_IMPLICIT_CAST,
											-1);
		if (defaultexpr == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
			errmsg("default for column \"%s\" cannot be cast to type \"%s\"",
				   colName, TypeNameToString(typename))));
	}
	else
		defaultexpr = NULL;

	if (Gp_role == GP_ROLE_DISPATCH &&
		policy != NULL &&
		policy->ptype == POLICYTYPE_PARTITIONED &&
		!hashCompatible)
	{
		relContainsTuples = cdbRelMaxSegSize(rel) > 0;
	}

	/*
	 * Find everything that depends on the column (constraints, indexes, etc),
	 * and record enough information to let us recreate the objects.
	 *
	 * The actual recreation does not happen here, but only after we have
	 * performed all the individual ALTER TYPE operations.	We have to save
	 * the info before executing ALTER TYPE, though, else the deparser will
	 * get confused.
	 *
	 * There could be multiple entries for the same object, so we must check
	 * to ensure we process each one only once.  Note: we assume that an index
	 * that implements a constraint will not show a direct dependency on the
	 * column.
	 */
	depRel = heap_open(DependRelationId, RowExclusiveLock);

	ScanKeyInit(&key[0],
				Anum_pg_depend_refclassid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationRelationId));
	ScanKeyInit(&key[1],
				Anum_pg_depend_refobjid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	ScanKeyInit(&key[2],
				Anum_pg_depend_refobjsubid,
				BTEqualStrategyNumber, F_INT4EQ,
				Int32GetDatum((int32) attnum));

	scan = systable_beginscan(depRel, DependReferenceIndexId, true,
							  SnapshotNow, 3, key);

	while (HeapTupleIsValid(depTup = systable_getnext(scan)))
	{
		Form_pg_depend foundDep = (Form_pg_depend) GETSTRUCT(depTup);
		ObjectAddress foundObject;

		/* We don't expect any PIN dependencies on columns */
		if (foundDep->deptype == DEPENDENCY_PIN)
			elog(ERROR, "cannot alter type of a pinned column");

		foundObject.classId = foundDep->classid;
		foundObject.objectId = foundDep->objid;
		foundObject.objectSubId = foundDep->objsubid;

		switch (getObjectClass(&foundObject))
		{
			case OCLASS_CLASS:
				{
					char		relKind = get_rel_relkind(foundObject.objectId);

					if (relKind == RELKIND_INDEX)
					{
						Assert(foundObject.objectSubId == 0);
						if (!list_member_oid(tab->changedIndexOids, foundObject.objectId))
						{
							char *indexdefstring = pg_get_indexdef_string(foundObject.objectId);
							tab->changedIndexOids = lappend_oid(tab->changedIndexOids,
													   foundObject.objectId);
							tab->changedIndexDefs = lappend(tab->changedIndexDefs,
															indexdefstring);

							if (indexdefstring != NULL &&
								strstr(indexdefstring," UNIQUE ") != 0 &&
								relContainsTuples)
							{
								Assert(Gp_role == GP_ROLE_DISPATCH &&
									   policy != NULL &&
									   policy->ptype == POLICYTYPE_PARTITIONED &&
									   !hashCompatible);

								for (int ia = 0; ia < policy->nattrs; ia++)
								{
									if (attnum == policy->attrs[ia])
									{
										ereport(ERROR,
												(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
												 errmsg("Changing the type of a column that is part of the "
														"distribution policy and used in a unique index is "
														"not allowed")));
									}
								}
							}
						}
					}
					else if (relKind == RELKIND_SEQUENCE)
					{
						/*
						 * This must be a SERIAL column's sequence.  We need
						 * not do anything to it.
						 */
						Assert(foundObject.objectSubId == 0);
					}
					else
					{
						/* Not expecting any other direct dependencies... */
						elog(ERROR, "unexpected object depending on column: %s",
							 getObjectDescription(&foundObject));
					}
					break;
				}

			case OCLASS_CONSTRAINT:
				Assert(foundObject.objectSubId == 0);
				if (!list_member_oid(tab->changedConstraintOids,
									 foundObject.objectId))
				{
					char	   *defstring = pg_get_constraintdef_string(foundObject.objectId);

					if (relContainsTuples &&
						(strstr(defstring," UNIQUE") != 0 || strstr(defstring,"PRIMARY KEY") != 0))
					{
						Assert(Gp_role == GP_ROLE_DISPATCH &&
							   !hashCompatible &&
							   policy != NULL &&
							   policy->ptype == POLICYTYPE_PARTITIONED);

						for (int ia = 0; ia < policy->nattrs; ia++)
						{
							if (attnum == policy->attrs[ia])
							{
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("Changing the type of a column that is used in a "
												"UNIQUE or PRIMARY KEY constraint is not allowed")));
							}
						}
					}

					/*
					 * Put NORMAL dependencies at the front of the list and
					 * AUTO dependencies at the back.  This makes sure that
					 * foreign-key constraints depending on this column will
					 * be dropped before unique or primary-key constraints of
					 * the column; which we must have because the FK
					 * constraints depend on the indexes belonging to the
					 * unique constraints.
					 */
					if (foundDep->deptype == DEPENDENCY_NORMAL)
					{
						tab->changedConstraintOids =
							lcons_oid(foundObject.objectId,
									  tab->changedConstraintOids);
						tab->changedConstraintDefs =
							lcons(defstring,
								  tab->changedConstraintDefs);
					}
					else
					{
						tab->changedConstraintOids =
							lappend_oid(tab->changedConstraintOids,
										foundObject.objectId);
						tab->changedConstraintDefs =
							lappend(tab->changedConstraintDefs,
									defstring);
					}
				}
				break;

			case OCLASS_REWRITE:
				/* XXX someday see if we can cope with revising views */
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter type of a column used by a view or rule"),
						 errdetail("%s depends on column \"%s\"",
								   getObjectDescription(&foundObject),
								   colName)));
				break;

			case OCLASS_DEFAULT:

				/*
				 * Ignore the column's default expression, since we will fix
				 * it below.
				 */
				Assert(defaultexpr);
				break;

			case OCLASS_PROC:
			case OCLASS_TYPE:
			case OCLASS_CAST:
			case OCLASS_CONVERSION:
			case OCLASS_LANGUAGE:
			case OCLASS_OPERATOR:
			case OCLASS_OPCLASS:
			case OCLASS_OPFAMILY:
			case OCLASS_TRIGGER:
			case OCLASS_SCHEMA:
			case OCLASS_TSPARSER:
			case OCLASS_TSDICT:
			case OCLASS_TSTEMPLATE:
			case OCLASS_TSCONFIG:
			case OCLASS_EXTENSION:

				/*
				 * We don't expect any of these sorts of objects to depend on
				 * a column.
				 */
				elog(ERROR, "unexpected object depending on column: %s",
					 getObjectDescription(&foundObject));
				break;

			default:
				elog(ERROR, "unrecognized object class: %u",
					 foundObject.classId);
		}
	}

	systable_endscan(scan);

	/*
	 * Now scan for dependencies of this column on other things.  The only
	 * thing we should find is the dependency on the column datatype, which we
	 * want to remove.
	 */
	ScanKeyInit(&key[0],
				Anum_pg_depend_classid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationRelationId));
	ScanKeyInit(&key[1],
				Anum_pg_depend_objid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	ScanKeyInit(&key[2],
				Anum_pg_depend_objsubid,
				BTEqualStrategyNumber, F_INT4EQ,
				Int32GetDatum((int32) attnum));

	scan = systable_beginscan(depRel, DependDependerIndexId, true,
							  SnapshotNow, 3, key);

	while (HeapTupleIsValid(depTup = systable_getnext(scan)))
	{
		Form_pg_depend foundDep = (Form_pg_depend) GETSTRUCT(depTup);

		if (foundDep->deptype != DEPENDENCY_NORMAL)
			elog(ERROR, "found unexpected dependency type '%c'",
				 foundDep->deptype);
		if (foundDep->refclassid != TypeRelationId ||
			foundDep->refobjid != attTup->atttypid)
			elog(ERROR, "found unexpected dependency for column");

		simple_heap_delete(depRel, &depTup->t_self);
	}

	systable_endscan(scan);

	heap_close(depRel, RowExclusiveLock);

	if (Gp_role == GP_ROLE_DISPATCH &&
		policy != NULL &&
		policy->ptype == POLICYTYPE_PARTITIONED &&
		!hashCompatible)
	{
		ListCell *lc;
		List *partkeys;

		partkeys = rel_partition_key_attrs(rel->rd_id);
		foreach (lc, partkeys)
		{
			if (attnum == lfirst_int(lc))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter type of a column used in "
								"a partitioning key")));
		}

		if (relContainsTuples)
		{
			for (int ia = 0; ia < policy->nattrs; ia++)
			{
				if (attnum == policy->attrs[ia])
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot alter type of a column used in "
									"a distribution policy")));
			}
		}
	}

	/*
	 * Here we go --- change the recorded column type.	(Note heapTup is a
	 * copy of the syscache entry, so okay to scribble on.)
	 */
	attTup->atttypid = targettype;
	attTup->atttypmod = targettypmod;
	attTup->attndims = list_length(typename->arrayBounds);
	attTup->attlen = tform->typlen;
	attTup->attbyval = tform->typbyval;
	attTup->attalign = tform->typalign;
	attTup->attstorage = tform->typstorage;

	ReleaseSysCache(typeTuple);

	simple_heap_update(attrelation, &heapTup->t_self, heapTup);

	/* keep system catalog indexes current */
	CatalogUpdateIndexes(attrelation, heapTup);

	heap_close(attrelation, RowExclusiveLock);

	/* Install dependency on new datatype */
	add_column_datatype_dependency(RelationGetRelid(rel), attnum, targettype);

	/*
	 * Drop any pg_statistic entry for the column, since it's now wrong type
	 */
	RemoveStatistics(RelationGetRelid(rel), attnum);

	/*
	 * Update the default, if present, by brute force --- remove and re-add
	 * the default.  Probably unsafe to take shortcuts, since the new version
	 * may well have additional dependencies.  (It's okay to do this now,
	 * rather than after other ALTER TYPE commands, since the default won't
	 * depend on other column types.)
	 */
	if (defaultexpr)
	{
		/* Must make new row visible since it will be updated again */
		CommandCounterIncrement();

		/*
		 * We use RESTRICT here for safety, but at present we do not expect
		 * anything to depend on the default.
		 */
		RemoveAttrDefault(RelationGetRelid(rel), attnum, DROP_RESTRICT, true);

		StoreAttrDefault(rel, attnum, defaultexpr);
	}

	/* Cleanup */
	heap_freetuple(heapTup);

	/* metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH) && MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ALTER COLUMN TYPE");
}

/*
 * Cleanup after we've finished all the ALTER TYPE operations for a
 * particular relation.  We have to drop and recreate all the indexes
 * and constraints that depend on the altered columns.
 */
static void
ATPostAlterTypeCleanup(List **wqueue, AlteredTableInfo *tab)
{
	ObjectAddress obj;
	ListCell   *l;

	/*
	 * Re-parse the index and constraint definitions, and attach them to the
	 * appropriate work queue entries.	We do this before dropping because in
	 * the case of a FOREIGN KEY constraint, we might not yet have exclusive
	 * lock on the table the constraint is attached to, and we need to get
	 * that before dropping.  It's safe because the parser won't actually look
	 * at the catalogs to detect the existing entry.
	 */
	foreach(l, tab->changedIndexDefs)
	{
		/*
		 * Temporary workaround for MPP-1318. INDEX CREATE is dispatched
		 * immediately, which unfortunately breaks the ALTER work queue.
		 */
		ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("cannot alter indexed column"),
			 errhint("DROP the index first, and recreate it after the ALTER")));
		/*ATPostAlterTypeParse((char *) lfirst(l), wqueue);*/
	}

	foreach(l, tab->changedConstraintDefs)
		ATPostAlterTypeParse((char *) lfirst(l), wqueue);

	/*
	 * Now we can drop the existing constraints and indexes --- constraints
	 * first, since some of them might depend on the indexes.  In fact, we
	 * have to delete FOREIGN KEY constraints before UNIQUE constraints, but
	 * we already ordered the constraint list to ensure that would happen. It
	 * should be okay to use DROP_RESTRICT here, since nothing else should be
	 * depending on these objects.
	 */
	foreach(l, tab->changedConstraintOids)
	{
		obj.classId = ConstraintRelationId;
		obj.objectId = lfirst_oid(l);
		obj.objectSubId = 0;
		performDeletion(&obj, DROP_RESTRICT);
	}

	foreach(l, tab->changedIndexOids)
	{
		obj.classId = RelationRelationId;
		obj.objectId = lfirst_oid(l);
		obj.objectSubId = 0;
		performDeletion(&obj, DROP_RESTRICT);
	}

	/*
	 * The objects will get recreated during subsequent passes over the work
	 * queue.
	 */
}

static void
ATPostAlterTypeParse(char *cmd, List **wqueue)
{
	List	   *raw_parsetree_list;
	List	   *querytree_list;
	ListCell   *list_item;

	/*
	 * We expect that we will get only ALTER TABLE and CREATE INDEX
	 * statements. Hence, there is no need to pass them through
	 * parse_analyze() or the rewriter, but instead we need to pass them
	 * through parse_utilcmd.c to make them ready for execution.
	 */
	raw_parsetree_list = raw_parser(cmd);
	querytree_list = NIL;
	foreach(list_item, raw_parsetree_list)
	{
		Node	   *stmt = (Node *) lfirst(list_item);

		if (IsA(stmt, IndexStmt))
			querytree_list = list_concat(querytree_list,
									 transformIndexStmt((IndexStmt *) stmt,
														cmd));
		else if (IsA(stmt, AlterTableStmt))
			querytree_list = list_concat(querytree_list,
							 transformAlterTableStmt((AlterTableStmt *) stmt,
													 cmd));
		else
			querytree_list = lappend(querytree_list, stmt);
	}

	/*
	 * Attach each generated command to the proper place in the work queue.
	 * Note this could result in creation of entirely new work-queue entries.
	 */
	foreach(list_item, querytree_list)
	{
		Node	   *stm = (Node *) lfirst(list_item);
		Relation	rel;
		AlteredTableInfo *tab;

		switch (nodeTag(stm))
		{
			case T_IndexStmt:
				{
					IndexStmt  *stmt = (IndexStmt *) stm;
					AlterTableCmd *newcmd;

					rel = relation_openrv(stmt->relation, AccessExclusiveLock);
					tab = ATGetQueueEntry(wqueue, rel);
					newcmd = makeNode(AlterTableCmd);
					newcmd->subtype = AT_ReAddIndex;
					newcmd->def = (Node *) stmt;
					tab->subcmds[AT_PASS_OLD_INDEX] =
						lappend(tab->subcmds[AT_PASS_OLD_INDEX], newcmd);
					relation_close(rel, NoLock);
					break;
				}
			case T_AlterTableStmt:
				{
					AlterTableStmt *stmt = (AlterTableStmt *) stm;
					ListCell   *lcmd;

					rel = relation_openrv(stmt->relation, AccessExclusiveLock);
					tab = ATGetQueueEntry(wqueue, rel);
					foreach(lcmd, stmt->cmds)
					{
						AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);

						switch (cmd->subtype)
						{
							case AT_AddIndex:
								cmd->subtype = AT_ReAddIndex;
								tab->subcmds[AT_PASS_OLD_INDEX] =
									lappend(tab->subcmds[AT_PASS_OLD_INDEX], cmd);
								break;
							case AT_AddConstraint:
								tab->subcmds[AT_PASS_OLD_CONSTR] =
									lappend(tab->subcmds[AT_PASS_OLD_CONSTR], cmd);
								break;
							default:
								elog(ERROR, "unexpected statement type: %d",
									 (int) cmd->subtype);
						}
					}
					relation_close(rel, NoLock);
					break;
				}
			default:
				elog(ERROR, "unexpected statement type: %d",
					 (int) nodeTag(stm));
		}
	}
}


/*
 * ALTER TABLE OWNER
 *
 * recursing is true if we are recursing from a table to its indexes,
 * sequences, or toast table.  We don't allow the ownership of those things to
 * be changed separately from the parent table.  Also, we can skip permission
 * checks (this is necessary not just an optimization, else we'd fail to
 * handle toast tables properly).
 *
 * recursing is also true if ALTER TYPE OWNER is calling us to fix up a
 * free-standing composite type.
 */
void
ATExecChangeOwner(Oid relationOid, Oid newOwnerId, bool recursing)
{
	Relation	target_rel;
	Relation	class_rel;
	HeapTuple	tuple;
	Form_pg_class tuple_class;

	/*
	 * Get exclusive lock till end of transaction on the target table. Use
	 * relation_open so that we can work on indexes and sequences.
	 */
	target_rel = relation_open(relationOid, AccessExclusiveLock);

	/* Get its pg_class tuple, too */
	class_rel = heap_open(RelationRelationId, RowExclusiveLock);

	tuple = SearchSysCache(RELOID,
						   ObjectIdGetDatum(relationOid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relationOid);
	tuple_class = (Form_pg_class) GETSTRUCT(tuple);

	/* Can we change the ownership of this tuple? */
	switch (tuple_class->relkind)
	{
		case RELKIND_RELATION:
		case RELKIND_VIEW:
			/* ok to change owner */
			break;
		case RELKIND_INDEX:
			if (!recursing)
			{
				/*
				 * Because ALTER INDEX OWNER used to be allowed, and in fact
				 * is generated by old versions of pg_dump, we give a warning
				 * and do nothing rather than erroring out.  Also, to avoid
				 * unnecessary chatter while restoring those old dumps, say
				 * nothing at all if the command would be a no-op anyway.
				 */
				if (tuple_class->relowner != newOwnerId)
					ereport(WARNING,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("cannot change owner of index \"%s\"",
									NameStr(tuple_class->relname)),
							 errhint("Change the ownership of the index's table, instead.")));
				/* quick hack to exit via the no-op path */
				newOwnerId = tuple_class->relowner;
			}
			break;
		case RELKIND_SEQUENCE:
			if (!recursing &&
				tuple_class->relowner != newOwnerId)
			{
				/* if it's an owned sequence, disallow changing it by itself */
				Oid			tableId;
				int32		colId;

				if (sequenceIsOwned(relationOid, &tableId, &colId))
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot change owner of sequence \"%s\"",
									NameStr(tuple_class->relname)),
					  errdetail("Sequence \"%s\" is linked to table \"%s\".",
								NameStr(tuple_class->relname),
								get_rel_name(tableId))));
			}
			break;
		case RELKIND_COMPOSITE_TYPE:
			if (recursing)
				break;
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is a composite type",
							NameStr(tuple_class->relname)),
					 errhint("Use ALTER TYPE instead.")));
			break;
		case RELKIND_TOASTVALUE:
		case RELKIND_AOSEGMENTS:
		case RELKIND_AOBLOCKDIR:
		case RELKIND_AOVISIMAP:
			if (recursing)
				break;
			/* FALL THRU */
		default:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a table, view, or sequence",
							NameStr(tuple_class->relname))));
	}

	/*
	 * If the new owner is the same as the existing owner, consider the
	 * command to have succeeded.  This is for dump restoration purposes.
	 */
	if (tuple_class->relowner != newOwnerId)
	{
		Datum		repl_val[Natts_pg_class];
		bool		repl_null[Natts_pg_class];
		bool		repl_repl[Natts_pg_class];
		Acl		   *newAcl;
		Datum		aclDatum;
		bool		isNull;
		HeapTuple	newtuple;

		/* skip permission checks when recursing to index or toast table */
		if (!recursing)
		{
			/* Superusers can always do it */
			if (!superuser())
			{
				Oid			namespaceOid = tuple_class->relnamespace;
				AclResult	aclresult;

				/* Otherwise, must be owner of the existing object */
				if (!pg_class_ownercheck(relationOid, GetUserId()))
					aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
								   RelationGetRelationName(target_rel));

				/* Must be able to become new owner */
				check_is_member_of_role(GetUserId(), newOwnerId);

				/* New owner must have CREATE privilege on namespace */
				aclresult = pg_namespace_aclcheck(namespaceOid, newOwnerId,
												  ACL_CREATE);
				if (aclresult != ACLCHECK_OK)
					aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
								   get_namespace_name(namespaceOid));
			}
		}

		memset(repl_null, false, sizeof(repl_null));
		memset(repl_repl, false, sizeof(repl_repl));

		repl_repl[Anum_pg_class_relowner - 1] = true;
		repl_val[Anum_pg_class_relowner - 1] = ObjectIdGetDatum(newOwnerId);

		/*
		 * Determine the modified ACL for the new owner.  This is only
		 * necessary when the ACL is non-null.
		 */
		aclDatum = SysCacheGetAttr(RELOID, tuple,
								   Anum_pg_class_relacl,
								   &isNull);
		if (!isNull)
		{
			newAcl = aclnewowner(DatumGetAclP(aclDatum),
								 tuple_class->relowner, newOwnerId);
			repl_repl[Anum_pg_class_relacl - 1] = true;
			repl_val[Anum_pg_class_relacl - 1] = PointerGetDatum(newAcl);
		}

		newtuple = heap_modify_tuple(tuple, RelationGetDescr(class_rel), repl_val, repl_null, repl_repl);

		simple_heap_update(class_rel, &newtuple->t_self, newtuple);
		CatalogUpdateIndexes(class_rel, newtuple);

		heap_freetuple(newtuple);

		/*
		 * Update owner dependency reference, if any.  A composite type has
		 * none, because it's tracked for the pg_type entry instead of here;
		 * indexes and TOAST tables don't have their own entries either.
		 */
		if (tuple_class->relkind != RELKIND_COMPOSITE_TYPE &&
			tuple_class->relkind != RELKIND_INDEX &&
			tuple_class->relkind != RELKIND_TOASTVALUE)
			changeDependencyOnOwner(RelationRelationId, relationOid,
									newOwnerId);

		/*
		 * Also change the ownership of the table's rowtype, if it has one
		 */
		if (tuple_class->relkind != RELKIND_INDEX)
			AlterTypeOwnerInternal(tuple_class->reltype, newOwnerId,
							 tuple_class->relkind == RELKIND_COMPOSITE_TYPE);

		/*
		 * If we are operating on a table, also change the ownership of any
		 * indexes and sequences that belong to the table, as well as the
		 * table's toast table (if it has one)
		 */
		if (tuple_class->relkind == RELKIND_RELATION ||
			tuple_class->relkind == RELKIND_TOASTVALUE ||
			tuple_class->relkind == RELKIND_AOSEGMENTS ||
			tuple_class->relkind == RELKIND_AOBLOCKDIR ||
			tuple_class->relkind == RELKIND_AOVISIMAP)
		{
			List	   *index_oid_list;
			ListCell   *i;

			/* Find all the indexes belonging to this relation */
			index_oid_list = RelationGetIndexList(target_rel);

			/* For each index, recursively change its ownership */
			foreach(i, index_oid_list)
				ATExecChangeOwner(lfirst_oid(i), newOwnerId, true);

			list_free(index_oid_list);
		}

		if (tuple_class->relkind == RELKIND_RELATION)
		{
			/* If it has a toast table, recurse to change its ownership */
			if (tuple_class->reltoastrelid != InvalidOid)
				ATExecChangeOwner(tuple_class->reltoastrelid, newOwnerId,
								  true);

			if(RelationIsAoRows(target_rel) || RelationIsAoCols(target_rel))
			{
				Oid segrelid, blkdirrelid;
				Oid visimap_relid;
				GetAppendOnlyEntryAuxOids(relationOid, SnapshotNow,
										  &segrelid,
										  &blkdirrelid, NULL,
										  &visimap_relid, NULL);
				
				/* If it has an AO segment table, recurse to change its
				 * ownership */
				if (segrelid != InvalidOid)
					ATExecChangeOwner(segrelid, newOwnerId, true);

				/* If it has an AO block directory table, recurse to change its
				 * ownership */
				if (blkdirrelid != InvalidOid)
					ATExecChangeOwner(blkdirrelid, newOwnerId, true);

				/* If it has an AO visimap table, recurse to change its 
				 * ownership */
				if (visimap_relid != InvalidOid)
				{
					ATExecChangeOwner(visimap_relid, newOwnerId, true);
				}
			}
			
			/* If it has dependent sequences, recurse to change them too */
			change_owner_recurse_to_sequences(relationOid, newOwnerId);
		}
	}

	ReleaseSysCache(tuple);
	heap_close(class_rel, RowExclusiveLock);
	relation_close(target_rel, NoLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(tuple_class)
			)
		MetaTrackUpdObject(RelationRelationId,
						   relationOid,
						   GetUserId(),
						   "ALTER", "OWNER"
				);
}

/*
 * change_owner_recurse_to_sequences
 *
 * Helper function for ATExecChangeOwner.  Examines pg_depend searching
 * for sequences that are dependent on serial columns, and changes their
 * ownership.
 */
static void
change_owner_recurse_to_sequences(Oid relationOid, Oid newOwnerId)
{
	Relation	depRel;
	SysScanDesc scan;
	ScanKeyData key[2];
	HeapTuple	tup;

	/*
	 * SERIAL sequences are those having an auto dependency on one of the
	 * table's columns (we don't care *which* column, exactly).
	 */
	depRel = heap_open(DependRelationId, AccessShareLock);

	ScanKeyInit(&key[0],
				Anum_pg_depend_refclassid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationRelationId));
	ScanKeyInit(&key[1],
				Anum_pg_depend_refobjid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relationOid));
	/* we leave refobjsubid unspecified */

	scan = systable_beginscan(depRel, DependReferenceIndexId, true,
							  SnapshotNow, 2, key);

	while (HeapTupleIsValid(tup = systable_getnext(scan)))
	{
		Form_pg_depend depForm = (Form_pg_depend) GETSTRUCT(tup);
		Relation	seqRel;

		/* skip dependencies other than auto dependencies on columns */
		if (depForm->refobjsubid == 0 ||
			depForm->classid != RelationRelationId ||
			depForm->objsubid != 0 ||
			depForm->deptype != DEPENDENCY_AUTO)
			continue;

		/* Use relation_open just in case it's an index */
		seqRel = relation_open(depForm->objid, AccessExclusiveLock);

		/* skip non-sequence relations */
		if (RelationGetForm(seqRel)->relkind != RELKIND_SEQUENCE)
		{
			/* No need to keep the lock */
			relation_close(seqRel, AccessExclusiveLock);
			continue;
		}

		/* We don't need to close the sequence while we alter it. */
		ATExecChangeOwner(depForm->objid, newOwnerId, true);

		/* Now we can close it.  Keep the lock till end of transaction. */
		relation_close(seqRel, NoLock);
	}

	systable_endscan(scan);

	relation_close(depRel, AccessShareLock);
}

/*
 * ALTER TABLE CLUSTER ON
 *
 * The only thing we have to do is to change the indisclustered bits.
 */
static void
ATExecClusterOn(Relation rel, const char *indexName)
{
	Oid			indexOid;

	if (RelationIsAoRows(rel) || RelationIsAoCols(rel))
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster append-only table \"%s\": not supported",
						RelationGetRelationName(rel))));
		return;
	}

	indexOid = get_relname_relid(indexName, rel->rd_rel->relnamespace);

	if (!OidIsValid(indexOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("index \"%s\" for table \"%s\" does not exist",
						indexName, RelationGetRelationName(rel))));

	/* Check index is valid to cluster on */
	check_index_is_clusterable(rel, indexOid, false);

	/* And do the work */
	mark_index_clustered(rel, indexOid);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "CLUSTER ON"
				);
}

/*
 * ALTER TABLE SET WITHOUT CLUSTER
 *
 * We have to find any indexes on the table that have indisclustered bit
 * set and turn it off.
 */
static void
ATExecDropCluster(Relation rel)
{
	mark_index_clustered(rel, InvalidOid);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "SET WITHOUT CLUSTER"
				);

}

/*
 * ALTER TABLE SET TABLESPACE
 */

/* 
 * Convert tablespace name to pg_tablespace Oid.  Error out if not valid and
 * settable by the current user.
 */
Oid
get_settable_tablespace_oid(char *tablespacename)
{
	Oid			tablespaceId;
	AclResult	aclresult;

	/* Check that the tablespace exists */
	tablespaceId = get_tablespace_oid(tablespacename, false);
	/* Check its permissions */
	aclresult = pg_tablespace_aclcheck(tablespaceId, GetUserId(), ACL_CREATE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_TABLESPACE, tablespacename);

	return tablespaceId;
}

static void
ATPrepSetTableSpace(AlteredTableInfo *tab, Relation rel, char *tablespacename)
{
	Oid			tablespaceId;

	tablespaceId = get_settable_tablespace_oid(tablespacename);

	/* Save info for Phase 3 to do the real work */
	if (OidIsValid(tab->newTableSpace))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot have multiple SET TABLESPACE subcommands")));
	tab->newTableSpace = tablespaceId;
}

/* 
 * ATPartsPrepSetTableSpace is like ATPrepSetTableSpace except that it generates work queue
 * entries for the command (an ALTER TABLE ... SET TABLESPACE ...)  for each part within the 
 * sub-hierarchy indicated by the oid list. 
 *
 * Designed to be called from the AT_PartAlter case of ATPrepCmd.
 */
static void
ATPartsPrepSetTableSpace(List **wqueue, Relation rel, AlterTableCmd *cmd, List *oids)
{
	ListCell *lc;
	Oid tablespaceId;
	int pass = AT_PASS_MISC;
	
	Assert( cmd && cmd->subtype == AT_SetTableSpace );
	Assert( oids );
	
	tablespaceId = get_settable_tablespace_oid(cmd->name);
	Assert(tablespaceId);
	
	ereport(DEBUG1,
			(errmsg("Expanding ALTER TABLE %s SET TABLESPACE...", 
					RelationGetRelationName(rel))
			 ));
	
	foreach(lc, oids)
	{
		Oid partrelid = lfirst_oid(lc);
		Relation partrel = relation_open(partrelid, NoLock); 
		/* NoLock because we should be holding AccessExclusiveLock on parent */
		AlterTableCmd *partcmd = copyObject(cmd);
		AlteredTableInfo *parttab = ATGetQueueEntry(wqueue, partrel);
		if( OidIsValid(parttab->newTableSpace) )
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("conflicting SET TABLESPACE subcommands for \"%s\"",
							RelationGetRelationName(partrel))));
		
		parttab->newTableSpace = tablespaceId;
		parttab->subcmds[pass] = lappend(parttab->subcmds[pass], partcmd);
		
		ereport(DEBUG1,
				(errmsg("Will SET TABLESPACE on \"%s\"", 
						RelationGetRelationName(partrel))));
		
		relation_close(partrel, NoLock);
	}
}

/*
 * ALTER TABLE/INDEX SET (...) or RESET (...)
 */
static void
ATExecSetRelOptions(Relation rel, List *defList, bool isReset)
{
	Oid			relid;
	Relation	pgclass;
	HeapTuple	tuple;
	HeapTuple	newtuple;
	Datum		datum;
	bool		isnull;
	Datum		newOptions;
	Datum		repl_val[Natts_pg_class];
	bool		repl_null[Natts_pg_class];
	bool		repl_repl[Natts_pg_class];

	if (defList == NIL)
		return;					/* nothing to do */

	pgclass = heap_open(RelationRelationId, RowExclusiveLock);

	/* Get the old reloptions */
	relid = RelationGetRelid(rel);
	tuple = SearchSysCache(RELOID,
						   ObjectIdGetDatum(relid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relid);

	datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions, &isnull);

	/* MPP-5777: disallow all options except fillfactor.
	 * Future work: could convert from SET to SET WITH codepath which
	 * can support additional reloption types
	 */
	if ((defList != NIL)
/*		&& ((rel->rd_rel->relkind == RELKIND_RELATION)
		|| (rel->rd_rel->relkind == RELKIND_TOASTVALUE)) */
			)
	{
		ListCell   *cell;

		foreach(cell, defList)
		{
			DefElem    *def = lfirst(cell);
			int			kw_len = strlen(def->defname);
			char	   *text_str = "fillfactor";
			int			text_len = strlen(text_str);

			if ((text_len != kw_len) ||
				(pg_strncasecmp(text_str, def->defname, kw_len) != 0))
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot SET reloption \"%s\"",
								def->defname)));

		}
	}


	/* Generate new proposed reloptions (text array) */
	newOptions = transformRelOptions(isnull ? (Datum) 0 : datum,
									 defList, false, isReset);

	/* Validate */
	switch (rel->rd_rel->relkind)
	{
		case RELKIND_RELATION:
		case RELKIND_TOASTVALUE:
		case RELKIND_AOSEGMENTS:
		case RELKIND_AOBLOCKDIR:
		case RELKIND_AOVISIMAP:
			if(RelationIsAoRows(rel) || RelationIsAoCols(rel))
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("altering reloptions for append only tables"
								" is not permitted")));

			(void) heap_reloptions(rel->rd_rel->relkind, newOptions, true);
			break;
		case RELKIND_INDEX:
			(void) index_reloptions(rel->rd_am->amoptions, newOptions, true);
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a table, index, or TOAST table",
							RelationGetRelationName(rel))));
			break;
	}

	/*
	 * All we need do here is update the pg_class row; the new options will be
	 * propagated into relcaches during post-commit cache inval.
	 */
	memset(repl_val, 0, sizeof(repl_val));
	memset(repl_null, false, sizeof(repl_null));
	memset(repl_repl, false, sizeof(repl_repl));

	if (newOptions != (Datum) 0)
		repl_val[Anum_pg_class_reloptions - 1] = newOptions;
	else
		repl_null[Anum_pg_class_reloptions - 1] = true;

	repl_repl[Anum_pg_class_reloptions - 1] = true;

	newtuple = heap_modify_tuple(tuple, RelationGetDescr(pgclass),
								 repl_val, repl_null, repl_repl);

	simple_heap_update(pgclass, &newtuple->t_self, newtuple);

	CatalogUpdateIndexes(pgclass, newtuple);

	heap_freetuple(newtuple);

	ReleaseSysCache(tuple);

	heap_close(pgclass, RowExclusiveLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", isReset ? "RESET" : "SET" 
				);
}

static void 
copy_append_only_data(
	RelFileNode		*oldRelFileNode,
	RelFileNode		*newRelFileNode,
	int32			segmentFileNum,
	char			*relationName,
	int64			eof,
	ItemPointer		persistentTid,
	int64			persistentSerialNum,
	char			*buffer)
{
	MIRRORED_LOCK_DECLARE;

	char		   *basepath;
	char srcFileName[MAXPGPATH];
	char dstFileName[MAXPGPATH];

	File		srcFile;

	MirroredAppendOnlyOpen mirroredDstOpen;

	int64	endOffset;
	int64	readOffset;
	int32	bufferLen;
	int 	retval;

	int 		primaryError;

	bool mirrorDataLossOccurred;

	bool mirrorCatchupRequired;

	MirrorDataLossTrackingState 	originalMirrorDataLossTrackingState;
	int64 							originalMirrorDataLossTrackingSessionNum;

	/*
	 * Open the files
	 */
	basepath = relpath(*oldRelFileNode);
	if (segmentFileNum > 0)
		snprintf(srcFileName, sizeof(srcFileName), "%s.%u", basepath, segmentFileNum);
	else
		snprintf(srcFileName, sizeof(srcFileName), "%s", basepath);
	pfree(basepath);

	srcFile = PathNameOpenFile(srcFileName, O_RDONLY | PG_BINARY, 0);
	if (srcFile < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\": %m", srcFileName)));

	basepath = relpath(*newRelFileNode);
	if (segmentFileNum > 0)
		snprintf(dstFileName, sizeof(srcFileName), "%s.%u", basepath, segmentFileNum);
	else
		snprintf(dstFileName, sizeof(srcFileName), "%s", basepath);
	pfree(basepath);

	MirroredAppendOnly_OpenReadWrite(
								&mirroredDstOpen,
								newRelFileNode,
								segmentFileNum,
								relationName,
								/* logicalEof */ 0, // NOTE: This is the START EOF.  Since we are copying, we start at 0.
								/* traceOpenFlags */ false,
								persistentTid,
								persistentSerialNum,
								&primaryError);
	if (primaryError != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for relation '%s': %s",
						dstFileName,
						relationName,
						strerror(primaryError))));
	
	/*
	 * Do the data copying.
	 */
	endOffset = eof;
	readOffset = 0;
	bufferLen = (Size) Min(2*BLCKSZ, endOffset);
	while (readOffset < endOffset)
	{
		CHECK_FOR_INTERRUPTS();
		
		retval = FileRead(srcFile, buffer, bufferLen);
		if (retval != bufferLen) 
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read from position: " INT64_FORMAT " in file '%s' : %m",
							readOffset, 
							srcFileName)));
			
			break;
		}						
		
		MirroredAppendOnly_Append(
							  &mirroredDstOpen,
							  buffer,
							  bufferLen,
							  &primaryError,
							  &mirrorDataLossOccurred);
		if (primaryError != 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write file \"%s\": %s",
							dstFileName,
							strerror(primaryError))));
		
		readOffset += bufferLen;
		
		bufferLen = (Size) Min(2*BLCKSZ, endOffset - readOffset); 					
	}
	
	/*
	 * Use the MirroredLock here to cover the flush (and close) and evaluation below whether
	 * we must catchup the mirror.
	 */
	MIRRORED_LOCK;

	MirroredAppendOnly_FlushAndClose(
							&mirroredDstOpen,
							&primaryError,
							&mirrorDataLossOccurred,
							&mirrorCatchupRequired,
							&originalMirrorDataLossTrackingState,
							&originalMirrorDataLossTrackingSessionNum);
	if (primaryError != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not flush (fsync) file \"%s\" for relation '%s': %s",
						dstFileName,
						relationName,
						strerror(primaryError))));

	FileClose(srcFile);

	if (eof > 0)
	{
		/* 
		 * This routine will handle both updating the persistent information about the
		 * new EOFs and copy data to the mirror if we are now in synchronized state.
		 */
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "copy_append_only_data: %u/%u/%u, segment file #%d, serial number " INT64_FORMAT ", TID %s, mirror catchup required %s, "
				 "mirror data loss tracking (state '%s', session num " INT64_FORMAT "), mirror new EOF " INT64_FORMAT,
				 newRelFileNode->spcNode,
				 newRelFileNode->dbNode,
				 newRelFileNode->relNode,
				 segmentFileNum,
				 persistentSerialNum,
				 ItemPointerToString(persistentTid),
				 (mirrorCatchupRequired ? "true" : "false"),
				 MirrorDataLossTrackingState_Name(originalMirrorDataLossTrackingState),
				 originalMirrorDataLossTrackingSessionNum,
				 eof);
		MirroredAppendOnly_AddMirrorResyncEofs(
										newRelFileNode,
										segmentFileNum,
										relationName,
										persistentTid,
										persistentSerialNum,
										&mirroredLockLocalVars,
										mirrorCatchupRequired,
										originalMirrorDataLossTrackingState,
										originalMirrorDataLossTrackingSessionNum,
										eof);

	}
	
	MIRRORED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "ALTER TABLE SET TABLESPACE: Copied Append-Only segment file #%d EOF " INT64_FORMAT,
			 segmentFileNum,
			 eof);
}

static void
ATExecSetTableSpace_AppendOnly(
	Oid				tableOid,
	Relation		rel,
	Relation		gp_relation_node,
	RelFileNode		*newRelFileNode)
{
	char *buffer;

	GpRelationNodeScan 	gpRelationNodeScan;

	HeapTuple tuple;

	int32 segmentFileNum;

	ItemPointerData oldPersistentTid;
	int64 oldPersistentSerialNum;

	ItemPointerData newPersistentTid;
	int64 newPersistentSerialNum;

	HeapTuple tupleCopy;

	Snapshot	appendOnlyMetaDataSnapshot = SnapshotNow;
				/*
				 * We can use SnapshotNow since we have an exclusive lock on the source.
				 */
					
	int segmentCount;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "ALTER TABLE SET TABLESPACE: Append-Only "
			 "relation id %u, old path %u/%u/%u, new path %u/%u/%u",
			 RelationGetRelid(rel),
			 rel->rd_node.spcNode,
			 rel->rd_node.dbNode,
			 rel->rd_node.relNode,
			 newRelFileNode->spcNode,
			 newRelFileNode->dbNode,
			 newRelFileNode->relNode);

	/* Use palloc to ensure we get a maxaligned buffer */		
	buffer = palloc(2*BLCKSZ);

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "ALTER TABLE SET TABLESPACE: pg_appendonly entry for Append-Only %u/%u/%u, "
			 "segrelid %u, "
			 "persistent TID %s",
			 rel->rd_node.spcNode,
			 rel->rd_node.dbNode,
			 rel->rd_node.relNode,
			 rel->rd_appendonly->segrelid,
			 ItemPointerToString(&oldPersistentTid));

	/*
	 * Loop through segment files
	 *    Create segment file in new tablespace under transaction,
	 *    Copy Append-Only segment file data and fsync,
	 *    Update old gp_relation_node with new relfilenode and persistent information,
	 *    Schedule old segment file for drop under transaction,
	 */
	GpRelationNodeBeginScan(
					SnapshotNow,
					gp_relation_node,
					tableOid,
					rel->rd_rel->reltablespace,
					rel->rd_rel->relfilenode,
					&gpRelationNodeScan);
	segmentCount = 0;
	while ((tuple = GpRelationNodeGetNext(
							&gpRelationNodeScan,
							&segmentFileNum,
							&oldPersistentTid,
							&oldPersistentSerialNum)))
	{
		int64 eof = 0;

		/*
		 * Create segment file in new tablespace under transaction.
		 */
		MirroredFileSysObj_TransactionCreateAppendOnlyFile(
											newRelFileNode,
											segmentFileNum,
											rel->rd_rel->relname.data,
											/* doJustInTimeDirCreate */ true,
											&newPersistentTid,
											&newPersistentSerialNum);

		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "ALTER TABLE SET TABLESPACE: Create for Append-Only %u/%u/%u, segment file #%d "
				 "persistent TID %s and serial number " INT64_FORMAT,
				 newRelFileNode->spcNode,
				 newRelFileNode->dbNode,
				 newRelFileNode->relNode,
				 segmentFileNum,
				 ItemPointerToString(&newPersistentTid),
				 newPersistentSerialNum);


		/*
		 * Update gp_relation_node with new persistent information.
		 */
		tupleCopy = heap_copytuple(tuple);

		UpdateGpRelationNodeTuple(
							gp_relation_node,
							tupleCopy,
							(newRelFileNode->spcNode == MyDatabaseTableSpace) ? 0:newRelFileNode->spcNode,
							newRelFileNode->relNode,
							segmentFileNum,
							&newPersistentTid,
							newPersistentSerialNum);

		/*
		 * Schedule old segment file for drop under transaction.
		 */
		MirroredFileSysObj_ScheduleDropAppendOnlyFile(
											&rel->rd_node,
											segmentFileNum,
											rel->rd_rel->relname.data,
											&oldPersistentTid,
											oldPersistentSerialNum);

		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "ALTER TABLE SET TABLESPACE: Drop for Append-Only %u/%u/%u, segment file #%d "
				 "persistent TID %s and serial number " INT64_FORMAT,
				 rel->rd_node.spcNode,
				 rel->rd_node.dbNode,
				 rel->rd_node.relNode,
				 segmentFileNum,
				 ItemPointerToString(&oldPersistentTid),
				 oldPersistentSerialNum);

		/*
		 * Copy Append-Only segment file data and fsync.
		 */
		if (RelationIsAoRows(rel))
		{
			FileSegInfo *fileSegInfo;

			fileSegInfo = GetFileSegInfo(rel, appendOnlyMetaDataSnapshot, segmentFileNum);
			if (fileSegInfo == NULL)
			{
				/*
				 * Segment file #0 is special and can exist without an entry.
				 */
				if (segmentFileNum == 0)
					eof = 0;
				else
					ereport(ERROR,
							(errmsg("Append-only %u/%u/%u row segment file #%d information missing",
									rel->rd_node.spcNode,
									rel->rd_node.dbNode,
									rel->rd_node.relNode,
									segmentFileNum)));

			}
			else
			{
				eof = fileSegInfo->eof;

				pfree(fileSegInfo);
			}
		}
		else 
		{
			int32 actualSegmentNum;
			int columnNum;

			AOCSFileSegInfo *aocsFileSegInfo;

			Assert(RelationIsAoCols(rel));

			// UNDONE: This is inefficient.

			columnNum = segmentFileNum / AOTupleId_MultiplierSegmentFileNum;

			actualSegmentNum = segmentFileNum % AOTupleId_MultiplierSegmentFileNum;
			
			aocsFileSegInfo = GetAOCSFileSegInfo(rel, appendOnlyMetaDataSnapshot, actualSegmentNum);
			if (aocsFileSegInfo == NULL)
			{
				/*
				 * Segment file #0 is special and can exist without an entry.
				 */
				if (segmentFileNum == 0)
					eof = 0;
				else
					ereport(ERROR,
							(errmsg("Append-only %u/%u/%u column logical segment file #%d information for missing (physical segment #%d, column %d)",
									rel->rd_node.spcNode,
									rel->rd_node.dbNode,
									rel->rd_node.relNode,
									actualSegmentNum,
									segmentFileNum,
									columnNum)));

			}
			else
			{
				eof = aocsFileSegInfo->vpinfo.entry[columnNum].eof;
			
				pfree(aocsFileSegInfo);
			}
		}

		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "ALTER TABLE SET TABLESPACE: Going to copy Append-Only %u/%u/%u to %u/%u/%u, segment file #%d, EOF " INT64_FORMAT ", "
				 "persistent TID %s and serial number " INT64_FORMAT,
				 rel->rd_node.spcNode,
				 rel->rd_node.dbNode,
				 rel->rd_node.relNode,
				 newRelFileNode->spcNode,
				 newRelFileNode->dbNode,
				 newRelFileNode->relNode,
				 segmentFileNum,
				 eof,
				 ItemPointerToString(&newPersistentTid),
				 newPersistentSerialNum);
		
		copy_append_only_data(
						&rel->rd_node,
						newRelFileNode,
						segmentFileNum,
						rel->rd_rel->relname.data,
						eof,
						&newPersistentTid,
						newPersistentSerialNum,
						buffer);

		segmentCount++;
	}

	GpRelationNodeEndScan(&gpRelationNodeScan);

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "ALTER TABLE SET TABLESPACE: Copied %d Append-Only segments from %u/%u/%u to %u/%u/%u, "
			 "persistent TID %s and serial number " INT64_FORMAT,
			 segmentCount,
			 rel->rd_node.spcNode,
			 rel->rd_node.dbNode,
			 rel->rd_node.relNode,
			 newRelFileNode->spcNode,
			 newRelFileNode->dbNode,
			 newRelFileNode->relNode,
			 ItemPointerToString(&newPersistentTid),
			 newPersistentSerialNum);

	pfree(buffer);
}

static void
ATExecSetTableSpace_BufferPool(
	Oid				tableOid,
	Relation		rel,
	Relation		gp_relation_node,
	RelFileNode		*newRelFileNode)
{
	Oid			oldTablespace;
	HeapTuple	nodeTuple;
	ItemPointerData newPersistentTid;
	int64 newPersistentSerialNum;
	SMgrRelation dstrel;
	bool useWal;
	ItemPointerData oldPersistentTid;
	int64 oldPersistentSerialNum;

	PersistentFileSysRelStorageMgr localRelStorageMgr;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	
	oldTablespace = rel->rd_rel->reltablespace ? rel->rd_rel->reltablespace : MyDatabaseTableSpace;

	/*
	 * We need to log the copied data in WAL enabled AND
	 * it's not a temp rel.
	 */
	useWal = !XLog_CanBypassWal() && !rel->rd_istemp;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "ALTER TABLE SET TABLESPACE: Buffer Pool managed "
			 "relation id %u, old path '%s', old relfilenode %u, old reltablespace %u, "
			 "new path '%s', new relfilenode %u, new reltablespace %u, "
			 "bulk load %s",
			 RelationGetRelid(rel),
			 relpath(rel->rd_node),
			 rel->rd_rel->relfilenode,
			 oldTablespace,
			 relpath(*newRelFileNode),
			 newRelFileNode->relNode,
			 newRelFileNode->spcNode,
			 (!useWal ? "true" : "false"));
	
	/* Fetch relation's gp_relation_node row */
	nodeTuple = FetchGpRelationNodeTuple(
					gp_relation_node,
					rel->rd_rel->reltablespace,
					rel->rd_rel->relfilenode,
					/* segmentFileNum */ 0,
					&oldPersistentTid,
					&oldPersistentSerialNum);
	if (!HeapTupleIsValid(nodeTuple))
		elog(ERROR, "cache lookup failed for relation %u, tablespace %u, relation file node %u", 
		     tableOid,
		     oldTablespace,
		     rel->rd_rel->relfilenode);

	GpPersistentRelationNode_GetRelationInfo(
										rel->rd_rel->relkind,
										rel->rd_rel->relstorage,
										rel->rd_rel->relam,
										&localRelStorageMgr,
										&relBufpoolKind);
	Assert(localRelStorageMgr == PersistentFileSysRelStorageMgr_BufferPool);
	
	dstrel = smgropen(*newRelFileNode);
	MirroredFileSysObj_TransactionCreateBufferPoolFile(
											dstrel,
											relBufpoolKind,
											rel->rd_isLocalBuf,
											rel->rd_rel->relname.data,
											/* doJustInTimeDirCreate */ true,
											/* bufferPoolBulkLoad */ !useWal,
											&newPersistentTid,
											&newPersistentSerialNum);

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
		     "ALTER TABLE SET TABLESPACE: Create for Buffer Pool managed '%s' "
			 "persistent TID %s and serial number " INT64_FORMAT,
			 relpath(*newRelFileNode),
			 ItemPointerToString(&newPersistentTid),
			 newPersistentSerialNum);

	/* copy relation data to the new physical file */
	copy_buffer_pool_data(
					rel, 
					dstrel,
					&newPersistentTid,
					newPersistentSerialNum,
					useWal);

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "ALTER TABLE SET TABLESPACE: Scheduling drop for '%s' "
			 "persistent TID %s and serial number " INT64_FORMAT,
			 relpath(*newRelFileNode),
			 ItemPointerToString(&rel->rd_segfile0_relationnodeinfo.persistentTid),
			 rel->rd_segfile0_relationnodeinfo.persistentSerialNum);


	/* schedule unlinking old physical file */
	MirroredFileSysObj_ScheduleDropBufferPoolRel(rel);

	/*
	 * Now drop smgr references.  The source was already dropped by
	 * smgrscheduleunlink.
	 */
	smgrclose(dstrel);

	/* Update gp_relation_node row. */
	UpdateGpRelationNodeTuple(
						gp_relation_node,
						nodeTuple,
						(newRelFileNode->spcNode == MyDatabaseTableSpace) ? 0:newRelFileNode->spcNode,
						newRelFileNode->relNode,
						/* segmentFileNum */ 0,
						&newPersistentTid,
						newPersistentSerialNum);

}

static bool
ATExecSetTableSpace_Relation(Oid tableOid, Oid newTableSpace)
{
	Relation    rel;
	Relation	pg_class;
	Oid			oldTableSpace;
	Oid			newrelfilenode;
	HeapTuple	tuple;
	Form_pg_class rd_rel;
	Relation	gp_relation_node;
	RelFileNode newrnode;

	rel = relation_open(tableOid, AccessExclusiveLock);

	/*
	 * We can never allow moving of shared or nailed-in-cache relations,
	 * because we can't support changing their reltablespace values.
	 */
	if (rel->rd_rel->relisshared || rel->rd_isnailed)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot move system relation \"%s\"",
						RelationGetRelationName(rel))));

	/* Can't move a non-shared relation into pg_global */
	if (newTableSpace == GLOBALTABLESPACE_OID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("only shared relations can be placed in pg_global tablespace")));

	/*
	 * Don't allow moving temp tables of other backends ... their local buffer
	 * manager is not going to cope.
	 */
	if (isOtherTempNamespace(RelationGetNamespace(rel)))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot move temporary tables of other sessions")));

	/*
	 * No work if no change in tablespace.
	 */
	oldTableSpace = rel->rd_rel->reltablespace;
	if (newTableSpace == oldTableSpace ||
		(newTableSpace == MyDatabaseTableSpace && oldTableSpace == 0))
	{
		/* XXX - Why do we hold the lock if we aren't changing anything? */
		relation_close(rel, NoLock);
		return false;
	}

	/* Get a modifiable copy of the relation's pg_class row */
	pg_class = heap_open(RelationRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopy(RELOID,
							   ObjectIdGetDatum(tableOid),
							   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", tableOid);
	rd_rel = (Form_pg_class) GETSTRUCT(tuple);

	/*
	 * Relfilenodes are not unique across tablespaces, so we need to allocate
	 * a new one in the new tablespace.
	 */
	newrelfilenode = GetNewRelFileNode(newTableSpace,
									   rel->rd_rel->relisshared);

	gp_relation_node = heap_open(GpRelationNodeRelationId, RowExclusiveLock);

	/* create another storage file. Is it a little ugly ? */
	/* NOTE: any conflict in relfilenode value will be caught here */
	newrnode = rel->rd_node;
	newrnode.relNode = newrelfilenode;
	newrnode.spcNode = newTableSpace;

	/* update the pg_class row */
	rd_rel->reltablespace = (newTableSpace == MyDatabaseTableSpace) ? InvalidOid : newTableSpace;
	rd_rel->relfilenode = newrelfilenode;
	
	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "ALTER TABLE SET TABLESPACE: Update pg_class for relation id %u --> relfilenode to %u, reltablespace to %u",
			 tableOid,
			 rd_rel->relfilenode,
			 rd_rel->reltablespace);

	simple_heap_update(pg_class, &tuple->t_self, tuple);
	CatalogUpdateIndexes(pg_class, tuple);

	heap_freetuple(tuple);

	if (RelationIsAoRows(rel) || RelationIsAoCols(rel))
		ATExecSetTableSpace_AppendOnly(
								tableOid,
								rel, 
								gp_relation_node, 
								&newrnode);
	else
		ATExecSetTableSpace_BufferPool(
								tableOid,
								rel, 
								gp_relation_node, 
								&newrnode);


	heap_close(pg_class, RowExclusiveLock);

	heap_close(gp_relation_node, RowExclusiveLock);

	/* Make sure the reltablespace change is visible */
	CommandCounterIncrement();

	/* Hold the lock until commit */
	relation_close(rel, NoLock);

	return true;
}

/*
 * Execute ALTER TABLE SET TABLESPACE for cases where there is no tuple
 * rewriting to be done, so we just want to copy the data as fast as possible.
 */
static void
ATExecSetTableSpace(Oid tableOid, Oid newTableSpace)
{
	Relation	rel;
	Oid			reltoastrelid = InvalidOid;
	Oid			reltoastidxid = InvalidOid;
	Oid			relaosegrelid = InvalidOid;
	Oid			relaoblkdirrelid = InvalidOid;
	Oid			relaoblkdiridxid = InvalidOid;
	Oid         relaovisimaprelid = InvalidOid;
	Oid         relaovisimapidxid = InvalidOid;
	Oid			relbmrelid = InvalidOid;
	Oid			relbmidxid = InvalidOid;


	/* Ensure valid input */
	Assert(OidIsValid(tableOid));
	Assert(OidIsValid(newTableSpace));

	/*
	 * Need lock here in case we are recursing to toast table or index
	 */
	rel = relation_open(tableOid, AccessExclusiveLock);

	reltoastrelid = rel->rd_rel->reltoastrelid;

	/* Find the toast relation index */
	if (reltoastrelid)
	{
		Relation toastrel;

		toastrel = relation_open(reltoastrelid, NoLock);
		reltoastidxid = toastrel->rd_rel->reltoastidxid;
		relation_close(toastrel, NoLock);
	}

	/* Get the ao sub objects */
	if (RelationIsAoRows(rel) || RelationIsAoCols(rel))
		GetAppendOnlyEntryAuxOids(tableOid, SnapshotNow,
								  &relaosegrelid,
								  &relaoblkdirrelid, &relaoblkdiridxid,
								  &relaovisimaprelid, &relaovisimapidxid);

	/* Get the bitmap sub objects */
	if (RelationIsBitmapIndex(rel))
		GetBitmapIndexAuxOids(rel, &relbmrelid, &relbmidxid);

	/* Move the main table */
	if (!ATExecSetTableSpace_Relation(tableOid, newTableSpace))
	{
		/* XXX - Why do we hold the lock if we aren't changing anything? */
		relation_close(rel, NoLock);
		return;
	}

	/* Move associated toast/toast_index/ao subobjects */
	if (OidIsValid(reltoastrelid))
		ATExecSetTableSpace_Relation(reltoastrelid, newTableSpace);
	if (OidIsValid(reltoastidxid))
		ATExecSetTableSpace_Relation(reltoastidxid, newTableSpace);
	if (OidIsValid(relaosegrelid))
		ATExecSetTableSpace_Relation(relaosegrelid, newTableSpace);
	if (OidIsValid(relaoblkdirrelid))
		ATExecSetTableSpace_Relation(relaoblkdirrelid, newTableSpace);
	if (OidIsValid(relaoblkdiridxid))
		ATExecSetTableSpace_Relation(relaoblkdiridxid, newTableSpace);
	if (OidIsValid(relaovisimaprelid))
		ATExecSetTableSpace_Relation(relaovisimaprelid, newTableSpace);
	if (OidIsValid(relaovisimapidxid))
		ATExecSetTableSpace_Relation(relaovisimapidxid, newTableSpace);

	/* 
	 * MPP-7996 - bitmap index subobjects w/Alter Table Set tablespace
	 */
	if (OidIsValid(relbmrelid))
	{
		Assert(!relaosegrelid);
		ATExecSetTableSpace_Relation(relbmrelid, newTableSpace);
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH) && MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "SET TABLESPACE"
			);

	/* Hold the lock until commit */
	relation_close(rel, NoLock);
}

/*
 * Copy data, block by block
 */
static void
copy_buffer_pool_data(Relation rel, SMgrRelation dst,
					  ItemPointer persistentTid, int64 persistentSerialNum,
					  bool useWal)
{
	SMgrRelation src;
	char	   *buf;
	Page		page;
	BlockNumber nblocks;
	BlockNumber blkno;

	MirroredBufferPoolBulkLoadInfo bulkLoadInfo;

	/*
	 * Since we copy the file directly without looking at the shared buffers,
	 * we'd better first flush out any pages of the source relation that are
	 * in shared buffers.  We assume no new changes will be made while we are
	 * holding exclusive lock on the rel.
	 */
	FlushRelationBuffers(rel);

	/*
	 * palloc the buffer so that it's MAXALIGN'd.  If it were just a local
	 * char[] array, the compiler might align it on any byte boundary, which
	 * can seriously hurt transfer speed to and from the kernel; not to
	 * mention possibly making log_newpage's accesses to the page header fail.
	 */
	buf = (char *) palloc(BLCKSZ);
	page = (Page) buf;

	nblocks = RelationGetNumberOfBlocks(rel);

	/* RelationGetNumberOfBlocks will certainly have opened rd_smgr */
	src = rel->rd_smgr;

	if (!useWal)
	{
		MirroredBufferPool_BeginBulkLoad(
								&rel->rd_node,
								persistentTid,
								persistentSerialNum,
								&bulkLoadInfo);
	}
	else
	{
		if (Debug_persistent_print)
		{
			elog(Persistent_DebugPrintLevel(),
				 "copy_buffer_pool_data %u/%u/%u: not bypassing the WAL -- not using bulk load, persistent serial num " INT64_FORMAT ", TID %s",
				 rel->rd_node.spcNode,
				 rel->rd_node.dbNode,
				 rel->rd_node.relNode,
				 persistentSerialNum,
				 ItemPointerToString(persistentTid));
		}
		MemSet(&bulkLoadInfo, 0, sizeof(MirroredBufferPoolBulkLoadInfo));
	}
	
	for (blkno = 0; blkno < nblocks; blkno++)
	{
		/* If we got a cancel signal during the copy of the data, quit */
		CHECK_FOR_INTERRUPTS();

		smgrread(src, blkno, buf);

		if (!PageIsVerified(page, blkno))
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("invalid page in block %u of relation %s",
							blkno, relpath(src->smgr_rnode))));

		/* XLOG stuff */
		if (useWal)
		{
			log_newpage_relFileNode(&dst->smgr_rnode, blkno, page, persistentTid,
						persistentSerialNum);
		}

		PageSetChecksumInplace(page, blkno);

		// -------- MirroredLock ----------
		LWLockAcquire(MirroredLock, LW_SHARED);
		
		/*
		 * Now write the page.	We say isTemp = true even if it's not a temp
		 * rel, because there's no need for smgr to schedule an fsync for this
		 * write; we'll do it ourselves below.
		 */
		smgrextend(dst, blkno, buf, true);
		
		LWLockRelease(MirroredLock);
		// -------- MirroredLock ----------
	}

	pfree(buf);

	/*
	 * If the rel isn't temp, we must fsync it down to disk before it's safe
	 * to commit the transaction.  (For a temp rel we don't care since the rel
	 * will be uninteresting after a crash anyway.)
	 *
	 * It's obvious that we must do this when not WAL-logging the copy. It's
	 * less obvious that we have to do it even if we did WAL-log the copied
	 * pages. The reason is that since we're copying outside shared buffers, a
	 * CHECKPOINT occurring during the copy has no way to flush the previously
	 * written data to disk (indeed it won't know the new rel even exists).  A
	 * crash later on would replay WAL from the checkpoint, therefore it
	 * wouldn't replay our earlier WAL entries. If we do not fsync those pages
	 * here, they might still not be on disk when the crash occurs.
	 */
	
	// -------- MirroredLock ----------
	LWLockAcquire(MirroredLock, LW_SHARED);
	
	if (!rel->rd_istemp)
		smgrimmedsync(dst);
	
	LWLockRelease(MirroredLock);
	// -------- MirroredLock ----------

	if (!useWal)
	{
		bool mirrorDataLossOccurred;
	
		/*
		 * We may have to catch-up the mirror since bulk loading of data is
		 * ignored by resynchronize.
		 */
		while (true)
		{
			bool bulkLoadFinished;
	
			bulkLoadFinished = 
				MirroredBufferPool_EvaluateBulkLoadFinish(
												&bulkLoadInfo);
	
			if (bulkLoadFinished)
			{
				/*
				 * The flush was successful to the mirror (or the mirror is
				 * not configured).
				 *
				 * We have done a state-change from 'Bulk Load Create Pending'
				 * to 'Create Pending'.
				 */
				break;
			}
	
			/*
			 * Copy primary data to mirror and flush.
			 */
			MirroredBufferPool_CopyToMirror(
									&rel->rd_node,
									rel->rd_rel->relname.data,
									persistentTid,
									persistentSerialNum,
									bulkLoadInfo.mirrorDataLossTrackingState,
									bulkLoadInfo.mirrorDataLossTrackingSessionNum,
									nblocks,
									&mirrorDataLossOccurred);
		}
	}
	else
	{
		if (Debug_persistent_print)
		{
			elog(Persistent_DebugPrintLevel(),
				 "copy_buffer_pool_data %u/%u/%u: did not bypass the WAL -- did not use bulk load, persistent serial num " INT64_FORMAT ", TID %s",
				 rel->rd_node.spcNode,
				 rel->rd_node.dbNode,
				 rel->rd_node.relNode,
				 persistentSerialNum,
				 ItemPointerToString(persistentTid));
		}
	}
}

/*
 * ALTER TABLE ENABLE/DISABLE TRIGGER
 *
 * We just pass this off to trigger.c.
 */
static void
ATExecEnableDisableTrigger(Relation rel, char *trigname,
						   char fires_when, bool skip_system)
{
	EnableDisableTrigger(rel, trigname, fires_when, skip_system);

	/* MPP-6929: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH && MetaTrackValidKindNsp(rel->rd_rel))
	{
		char	   *subtype;
		switch (fires_when)
		{
			case TRIGGER_FIRES_ON_ORIGIN:
				subtype = "ENABLE TRIGGER";
				break;
			case TRIGGER_FIRES_ALWAYS:
				subtype = "ENABLE ALWAYS TRIGGER";
				break;
			case TRIGGER_FIRES_ON_REPLICA:
				subtype = "ENABLE REPLICA TRIGGER";
				break;
			case TRIGGER_DISABLED:
				subtype = "DISBLE TRIGGER";
				break;
			default:
				subtype = "unknown trigger mode";
				break;
		}
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", 
						   subtype);
	}
}

/*
 * ALTER TABLE ENABLE/DISABLE RULE
 *
 * We just pass this off to rewriteDefine.c.
 */
static void
ATExecEnableDisableRule(Relation rel, char *trigname,
						char fires_when)
{
	EnableDisableRule(rel, trigname, fires_when);
}

static void
inherit_parent(Relation parent_rel, Relation child_rel, bool is_partition, List *inhAttrNameList)
{
	Relation	catalogRelation;
	SysScanDesc scan;
	ScanKeyData key;
	HeapTuple	inheritsTuple;
	int32		inhseqno;
	List	   *children;

	/*
	 * Check for duplicates in the list of parents, and determine the highest
	 * inhseqno already present; we'll use the next one for the new parent.
	 * (Note: get RowExclusiveLock because we will write pg_inherits below.)
	 *
	 * Note: we do not reject the case where the child already inherits from
	 * the parent indirectly; CREATE TABLE doesn't reject comparable cases.
	 */
	catalogRelation = heap_open(InheritsRelationId, RowExclusiveLock);
	ScanKeyInit(&key,
				Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(child_rel)));
	scan = systable_beginscan(catalogRelation, InheritsRelidSeqnoIndexId,
							  true, SnapshotNow, 1, &key);

	/* inhseqno sequences start at 1 */
	inhseqno = 0;
	while (HeapTupleIsValid(inheritsTuple = systable_getnext(scan)))
	{
		Form_pg_inherits inh = (Form_pg_inherits) GETSTRUCT(inheritsTuple);
		if (inh->inhparent == RelationGetRelid(parent_rel))
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_TABLE),
			 errmsg("relation \"%s\" would be inherited from more than once",
					RelationGetRelationName(parent_rel))));
		if (inh->inhseqno > inhseqno)
			inhseqno = inh->inhseqno;
	}
	systable_endscan(scan);

	/*
	 * Prevent circularity by seeing if proposed parent inherits from child.
	 * (In particular, this disallows making a rel inherit from itself.)
	 *
	 * This is not completely bulletproof because of race conditions: in
	 * multi-level inheritance trees, someone else could concurrently be
	 * making another inheritance link that closes the loop but does not join
	 * either of the rels we have locked.  Preventing that seems to require
	 * exclusive locks on the entire inheritance tree, which is a cure worse
	 * than the disease.  find_all_inheritors() will cope with circularity
	 * anyway, so don't sweat it too much.
	 */
	children = find_all_inheritors(RelationGetRelid(child_rel));

	if (list_member_oid(children, RelationGetRelid(parent_rel)))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_TABLE),
				 errmsg("circular inheritance not allowed"),
				 errdetail("\"%s\" is already a child of \"%s\".",
						   RelationGetRelationName(parent_rel),
						   RelationGetRelationName(child_rel))));

	/* If parent has OIDs then child must have OIDs */
	if (parent_rel->rd_rel->relhasoids && !child_rel->rd_rel->relhasoids)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("table \"%s\" without OIDs cannot inherit from table \"%s\" with OIDs",
						RelationGetRelationName(child_rel),
						RelationGetRelationName(parent_rel))));

	/* Match up the columns and bump attinhcount and attislocal */
	MergeAttributesIntoExisting(child_rel, parent_rel, inhAttrNameList, is_partition);

	/* Match up the constraints and make sure they're present in child */
	MergeConstraintsIntoExisting(child_rel, parent_rel);

	/*
	 * OK, it looks valid.	Make the catalog entries that show inheritance.
	 */
	StoreCatalogInheritance1(RelationGetRelid(child_rel),
							 RelationGetRelid(parent_rel),
							 inhseqno + 1,
							 catalogRelation, is_partition);

	/* Now we're done with pg_inherits */
	heap_close(catalogRelation, RowExclusiveLock);

}

/*
 * ALTER TABLE INHERIT
 *
 * Add a parent to the child's parents. This verifies that all the columns and
 * check constraints of the parent appear in the child and that they have the
 * same data types and expressions.
 */
static void
ATExecAddInherit(Relation child_rel, Node *node)
{
	Relation	parent_rel;
	bool		is_partition;
	RangeVar   *parent;
	List	   *inhAttrNameList = NIL;

	Assert(PointerIsValid(node));

	if (IsA(node, InheritPartitionCmd))
	{
		parent = ((InheritPartitionCmd *) node)->parent;
		is_partition = true;
	}
	else
	{
		parent = (RangeVar *) node;
		is_partition = false;
	}

	/*
	 * AccessShareLock on the parent is what's obtained during normal CREATE
	 * TABLE ... INHERITS ..., so should be enough here.
	 */
	parent_rel = heap_openrv(parent, AccessShareLock);

	/*
	 * Must be owner of both parent and child -- child was checked by
	 * ATSimplePermissions call in ATPrepCmd
	 */
	ATSimplePermissions(parent_rel, false);

	/* Permanent rels cannot inherit from temporary ones */
	if (!isTempNamespace(RelationGetNamespace(child_rel)) &&
		isTempNamespace(RelationGetNamespace(parent_rel)))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot inherit from temporary relation \"%s\"",
						RelationGetRelationName(parent_rel))));

	if (is_partition)
	{
		/* lookup all attrs */
		int attno;

		for (attno = 0; attno < parent_rel->rd_att->natts; attno++)
		{
			Form_pg_attribute	 attribute = parent_rel->rd_att->attrs[attno];
			char				*attributeName = NameStr(attribute->attname);

			/* MPP-5397: ignore dropped cols */
			if (!attribute->attisdropped)
				inhAttrNameList =
						lappend(inhAttrNameList,
								makeString(attributeName));
		}

	}
	inherit_parent(parent_rel, child_rel, is_partition, inhAttrNameList);

	/*
	 * Keep our lock on the parent relation until commit, unless we're
	 * doing partitioning, in which case the parent is sufficiently locked.
	 * We want to unlock here in case we're doing deep sub partitioning. We do
	 * not want to acquire too many locks since we're overflow the lock buffer.
	 * An exclusive lock on the parent table is sufficient to guard against
	 * concurrency issues.
	 */
	if (is_partition)
		heap_close(parent_rel, AccessShareLock);
	else
		heap_close(parent_rel, NoLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(child_rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(child_rel),
						   GetUserId(),
						   "ALTER", "INHERIT"
				);
}

/*
 * Obtain the source-text form of the constraint expression for a check
 * constraint, given its pg_constraint tuple
 */
static char *
decompile_conbin(HeapTuple contup, TupleDesc tupdesc)
{
	Form_pg_constraint con;
	bool		isnull;
	Datum		attr;
	Datum		expr;

	con = (Form_pg_constraint) GETSTRUCT(contup);
	attr = heap_getattr(contup, Anum_pg_constraint_conbin, tupdesc, &isnull);
	if (isnull)
		elog(ERROR, "null conbin for constraint %u", HeapTupleGetOid(contup));

	expr = DirectFunctionCall2(pg_get_expr, attr,
							   ObjectIdGetDatum(con->conrelid));
	return TextDatumGetCString(expr);
}

/*
 * Check columns in child table match up with columns in parent, and increment
 * their attinhcount.
 *
 * Called by ATExecAddInherit
 *
 * Currently all parent columns must be found in child. Missing columns are an
 * error.  One day we might consider creating new columns like CREATE TABLE
 * does.  However, that is widely unpopular --- in the common use case of
 * partitioned tables it's a foot-gun.
 *
 * The data type must match exactly. If the parent column is NOT NULL then
 * the child must be as well. Defaults are not compared, however.
 */
static void
MergeAttributesIntoExisting(Relation child_rel, Relation parent_rel, List *inhAttrNameList,
							bool is_partition)
{
	Relation	attrrel;
	AttrNumber	parent_attno;
	int			parent_natts;
	TupleDesc	tupleDesc;
	TupleConstr *constr;
	HeapTuple	tuple;
	ListCell	*attNameCell;

	attrrel = heap_open(AttributeRelationId, RowExclusiveLock);

	tupleDesc = RelationGetDescr(parent_rel);
	parent_natts = tupleDesc->natts;
	constr = tupleDesc->constr;

	/*
	 * If we have an inherited column list, ensure all named columns exist in parent
	 * and that the list excludes system columns.
	 */
	foreach( attNameCell, inhAttrNameList )
	{
		bool columnDefined = false;
		char *inhAttrName = strVal((Value *)lfirst(attNameCell));

		for (parent_attno = 1; parent_attno <= parent_natts && !columnDefined; parent_attno++)
		{
			char *attributeName;
			Form_pg_attribute attribute = tupleDesc->attrs[parent_attno - 1];
			if (attribute->attisdropped)
				continue;
			attributeName = NameStr(attribute->attname);
			columnDefined = (strcmp(inhAttrName, attributeName) == 0);
		}

		if (!columnDefined)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					errmsg("column \"%s\" does not exist in parent table \"%s\"",
							inhAttrName,
							RelationGetRelationName(parent_rel))));
	}

	for (parent_attno = 1; parent_attno <= parent_natts; parent_attno++)
	{
		Form_pg_attribute attribute = tupleDesc->attrs[parent_attno - 1];
		char	   *attributeName = NameStr(attribute->attname);

		/* Ignore dropped columns in the parent. */
		if (attribute->attisdropped)
			continue;

		/* Find same column in child (matching on column name). */
		tuple = SearchSysCacheCopyAttName(RelationGetRelid(child_rel),
										  attributeName);
		if (HeapTupleIsValid(tuple))
		{
			/* Check they are same type and typmod */
			Form_pg_attribute childatt = (Form_pg_attribute) GETSTRUCT(tuple);

			if (attribute->atttypid != childatt->atttypid ||
				attribute->atttypmod != childatt->atttypmod)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("child table \"%s\" has different type for column \"%s\"",
								RelationGetRelationName(child_rel),
								attributeName)));

			if (attribute->attnotnull && !childatt->attnotnull)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
				errmsg("column \"%s\" in child table must be marked NOT NULL",
					   attributeName)));

			/*
			 * OK, bump the child column's inheritance count.  (If we fail
			 * later on, this change will just roll back.)
			 */
			childatt->attinhcount++;

			/*
			 * For locally-defined attributes, check to see if the attribute is named
			 * in the "fully-inherited" list.  If so, mark the child attribute as
			 * not locally defined.  (Default/standard behaviour is to leave the
			 * attribute locally defined.)
			 */
			if (childatt->attislocal)
			{
				/* never local when we're doing partitioning */
				if (is_partition)
					childatt->attislocal = false;
				else
				{
					foreach( attNameCell, inhAttrNameList )
					{
						char *inhAttrName = strVal((Value *)lfirst(attNameCell));
						if (strcmp(attributeName, inhAttrName) == 0)
						{
							childatt->attislocal = false;
							break;
						}
					}
				}
			}

			simple_heap_update(attrrel, &tuple->t_self, tuple);
			CatalogUpdateIndexes(attrrel, tuple);
			heap_freetuple(tuple);
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("child table is missing column \"%s\"",
							attributeName)));
		}
	}

	heap_close(attrrel, RowExclusiveLock);
}

/*
 * Check constraints in child table match up with constraints in parent
 *
 * Called by ATExecAddInherit and exchange_part_inheritance
 *
 * Currently all constraints in parent must be present in the child. One day we
 * may consider adding new constraints like CREATE TABLE does. We may also want
 * to allow an optional flag on parent table constraints indicating they are
 * intended to ONLY apply to the master table, not to the children. That would
 * make it possible to ensure no records are mistakenly inserted into the
 * master in partitioned tables rather than the appropriate child.
 *
 * XXX This is O(N^2) which may be an issue with tables with hundreds of
 * constraints. As long as tables have more like 10 constraints it shouldn't be
 * a problem though. Even 100 constraints ought not be the end of the world.
 */
static void
MergeConstraintsIntoExisting(Relation child_rel, Relation parent_rel)
{
	Relation	catalogRelation;
	TupleDesc	tupleDesc;
	SysScanDesc scan;
	ScanKeyData key;
	HeapTuple	constraintTuple;
	ListCell   *elem;
	List	   *constraints;

	/* First gather up the child's constraint definitions */
	catalogRelation = heap_open(ConstraintRelationId, AccessShareLock);
	tupleDesc = RelationGetDescr(catalogRelation);

	ScanKeyInit(&key,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(child_rel)));
	scan = systable_beginscan(catalogRelation, ConstraintRelidIndexId,
							  true, SnapshotNow, 1, &key);

	constraints = NIL;
	while (HeapTupleIsValid(constraintTuple = systable_getnext(scan)))
	{
		Form_pg_constraint con = (Form_pg_constraint) GETSTRUCT(constraintTuple);

		if (con->contype != CONSTRAINT_CHECK)
			continue;

		constraints = lappend(constraints, heap_copytuple(constraintTuple));
	}

	systable_endscan(scan);

	/* Then scan through the parent's constraints looking for matches */
	ScanKeyInit(&key,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(parent_rel)));
	scan = systable_beginscan(catalogRelation, ConstraintRelidIndexId, true,
							  SnapshotNow, 1, &key);

	while (HeapTupleIsValid(constraintTuple = systable_getnext(scan)))
	{
		Form_pg_constraint parent_con = (Form_pg_constraint) GETSTRUCT(constraintTuple);
		bool		found = false;
		Form_pg_constraint child_con = NULL;
		HeapTuple	child_contuple = NULL;

		if (parent_con->contype != CONSTRAINT_CHECK)
			continue;

		foreach(elem, constraints)
		{
			child_contuple = (HeapTuple) lfirst(elem);
			child_con = (Form_pg_constraint) GETSTRUCT(child_contuple);
			if (strcmp(NameStr(parent_con->conname),
					   NameStr(child_con->conname)) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("child table is missing constraint \"%s\"",
							NameStr(parent_con->conname))));

		if (parent_con->condeferrable != child_con->condeferrable ||
			parent_con->condeferred != child_con->condeferred ||
			strcmp(decompile_conbin(constraintTuple, tupleDesc),
				   decompile_conbin(child_contuple, tupleDesc)) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("constraint definition for check constraint \"%s\" does not match",
							NameStr(parent_con->conname))));

		/*
		 * TODO: add conislocal,coninhcount to constraints. This is where we
		 * would have to bump them just like attributes
		 */
	}

	systable_endscan(scan);
	heap_close(catalogRelation, AccessShareLock);
}

/*
 * ALTER TABLE NO INHERIT
 *
 * Drop a parent from the child's parents. This just adjusts the attinhcount
 * and attislocal of the columns and removes the pg_inherit and pg_depend
 * entries.
 *
 * If attinhcount goes to 0 then attislocal gets set to true. If it goes back
 * up attislocal stays true, which means if a child is ever removed from a
 * parent then its columns will never be automatically dropped which may
 * surprise. But at least we'll never surprise by dropping columns someone
 * isn't expecting to be dropped which would actually mean data loss.
 */
static void
ATExecDropInherit(Relation rel, RangeVar *parent, bool is_partition)
{
	Relation	parent_rel;
	Relation	catalogRelation;
	SysScanDesc scan;
	ScanKeyData key[3];
	HeapTuple	inheritsTuple,
				attributeTuple,
				depTuple;
	bool		found = false;

	/*
	 * AccessShareLock on the parent is probably enough, seeing that DROP
	 * TABLE doesn't lock parent tables at all.  We need some lock since we'll
	 * be inspecting the parent's schema.
	 */
	parent_rel = heap_openrv(parent, AccessShareLock);

	/*
	 * We don't bother to check ownership of the parent table --- ownership of
	 * the child is presumed enough rights.
	 */

	/*
	 * Find and destroy the pg_inherits entry linking the two, or error out if
	 * there is none.
	 */
	catalogRelation = heap_open(InheritsRelationId, RowExclusiveLock);
	ScanKeyInit(&key[0],
				Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	scan = systable_beginscan(catalogRelation, InheritsRelidSeqnoIndexId,
							  true, SnapshotNow, 1, key);

	while (HeapTupleIsValid(inheritsTuple = systable_getnext(scan)))
	{
		Oid			inhparent;

		inhparent = ((Form_pg_inherits) GETSTRUCT(inheritsTuple))->inhparent;
		if (inhparent == RelationGetRelid(parent_rel))
		{
			simple_heap_delete(catalogRelation, &inheritsTuple->t_self);
			found = true;
			break;
		}
	}

	systable_endscan(scan);
	heap_close(catalogRelation, RowExclusiveLock);

	if (!found)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_TABLE),
				 errmsg("relation \"%s\" is not a parent of relation \"%s\"",
						RelationGetRelationName(parent_rel),
						RelationGetRelationName(rel))));

	/*
	 * Search through child columns looking for ones matching parent rel
	 */
	catalogRelation = heap_open(AttributeRelationId, RowExclusiveLock);
	ScanKeyInit(&key[0],
				Anum_pg_attribute_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	scan = systable_beginscan(catalogRelation, AttributeRelidNumIndexId,
							  true, SnapshotNow, 1, key);
	while (HeapTupleIsValid(attributeTuple = systable_getnext(scan)))
	{
		Form_pg_attribute att = (Form_pg_attribute) GETSTRUCT(attributeTuple);

		/* Ignore if dropped or not inherited */
		if (att->attisdropped)
			continue;
		if (att->attinhcount <= 0)
			continue;

		if (SearchSysCacheExistsAttName(RelationGetRelid(parent_rel),
										NameStr(att->attname)))
		{
			/* Decrement inhcount and possibly set islocal to true */
			HeapTuple	copyTuple = heap_copytuple(attributeTuple);
			Form_pg_attribute copy_att = (Form_pg_attribute) GETSTRUCT(copyTuple);

			copy_att->attinhcount--;
			if (copy_att->attinhcount == 0)
				copy_att->attislocal = true;

			simple_heap_update(catalogRelation, &copyTuple->t_self, copyTuple);
			CatalogUpdateIndexes(catalogRelation, copyTuple);
			heap_freetuple(copyTuple);
		}
	}
	systable_endscan(scan);
	heap_close(catalogRelation, RowExclusiveLock);

	/*
	 * Drop the dependency
	 *
	 * There's no convenient way to do this, so go trawling through pg_depend
	 */
	catalogRelation = heap_open(DependRelationId, RowExclusiveLock);

	ScanKeyInit(&key[0],
				Anum_pg_depend_classid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationRelationId));
	ScanKeyInit(&key[1],
				Anum_pg_depend_objid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	ScanKeyInit(&key[2],
				Anum_pg_depend_objsubid,
				BTEqualStrategyNumber, F_INT4EQ,
				Int32GetDatum(0));

	scan = systable_beginscan(catalogRelation, DependDependerIndexId, true,
							  SnapshotNow, 3, key);

	while (HeapTupleIsValid(depTuple = systable_getnext(scan)))
	{
		Form_pg_depend dep = (Form_pg_depend) GETSTRUCT(depTuple);

		if (dep->refclassid == RelationRelationId &&
			dep->refobjid == RelationGetRelid(parent_rel) &&
			dep->refobjsubid == 0 &&
			((dep->deptype == DEPENDENCY_NORMAL && !is_partition) ||
			 (dep->deptype == DEPENDENCY_AUTO && is_partition)))
		{
			simple_heap_delete(catalogRelation, &depTuple->t_self);
		}
	}

	systable_endscan(scan);
	heap_close(catalogRelation, RowExclusiveLock);

	/* keep our lock on the parent relation until commit */
	heap_close(parent_rel, NoLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "NO INHERIT"
				);
}

/*
 * deparse pg_class.reloptions into a list.
 */
static List *
reloptions_list(Oid relid)
{
	Datum		reloptions;
	HeapTuple	tuple;
	bool		isNull = true;
	List	   *opts = NIL;

	tuple = SearchSysCache1(RELOID,
							ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tuple))
        elog(ERROR, "cache lookup failed for relation %u", relid);

    reloptions = SysCacheGetAttr(RELOID, tuple,
								 Anum_pg_class_reloptions,
								 &isNull);
    if (!isNull)
		opts = untransformRelOptions(reloptions);

	ReleaseSysCache(tuple);

	return opts;
}

/*
 * Update the pg_attribute entries of dropped columns in given relation,
 * as if they were of type int4.
 *
 * This is used by ALTER TABLE SET DISTRIBUTED BY, which swaps the
 * relation file with a newly constructed temp table. The temp table is
 * constructed with int4 columns standing in for the dropped columns,
 * and this function is used to update the original table's definition
 * to match that.
 */
static void
change_dropped_col_datatypes(Relation rel)
{
	Relation	catalogRelation;
	SysScanDesc scan;
	ScanKeyData key;
	HeapTuple	tuple;

	/*
	 * Loop through all dropped columns.
	 */
	catalogRelation = heap_open(AttributeRelationId, RowExclusiveLock);
	ScanKeyInit(&key,
				Anum_pg_attribute_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	scan = systable_beginscan(catalogRelation, AttributeRelidNumIndexId,
							  true, SnapshotNow, 1, &key);
	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
	{
		Form_pg_attribute att = (Form_pg_attribute) GETSTRUCT(tuple);
		HeapTuple copyTuple;

		if (att->attisdropped)
		{
			Assert(att->attnum > 0 && att->attnum <= RelationGetNumberOfAttributes(rel));

			copyTuple = heap_copytuple(tuple);
			att = (Form_pg_attribute) GETSTRUCT(copyTuple);

			att->attlen = sizeof(int32);
			att->attndims = 0;
			att->atttypmod = -1;
			att->attbyval = true;
			att->attstorage = 'p';
			att->attalign = 'i';

			simple_heap_update(catalogRelation, &tuple->t_self, copyTuple);
			CatalogUpdateIndexes(catalogRelation, copyTuple);
		}
	}
	systable_endscan(scan);

	heap_close(catalogRelation, RowExclusiveLock);
}

/*
 * Build:
 *
 * CREATE TABLE pg_temp_<NNNN> AS SELECT * FROM rel
 *   DISTRIBUTED BY dist_clause
 */
static QueryDesc *
build_ctas_with_dist(Relation rel, List *dist_clause,
					 List *storage_opts, RangeVar **tmprv,
					 bool useExistingColumnAttributes)
{
	Query *q;
	SelectStmt *s = makeNode(SelectStmt);
	Node *n;
	RangeVar *from_tbl;
	List *rewritten;
	PlannedStmt *stmt;
	DestReceiver *dest;
	QueryDesc *queryDesc;
	RangeVar *tmprel = make_temp_table_name(rel, MyBackendId);
	TupleDesc	tupdesc;
	int			attno;
	bool		pre_built;

	tupdesc = RelationGetDescr(rel);

	for (attno = 0; attno < tupdesc->natts; attno++)
	{
		Form_pg_attribute att = tupdesc->attrs[attno];
		ResTarget  *t;

		t = makeNode(ResTarget);

		if (!att->attisdropped)
		{
			ColumnRef 		   *c;

			c = makeNode(ColumnRef);
			c->location = -1;
			c->fields = lappend(c->fields, makeString(pstrdup(get_namespace_name(RelationGetNamespace(rel)))));
			c->fields = lappend(c->fields, makeString(pstrdup(RelationGetRelationName(rel))));
			c->fields = lappend(c->fields, makeString(pstrdup(NameStr(att->attname))));
			t->val = (Node *) c;
		}
		else
		{
			/* Use a dummy NULL::int4 column to stand in for any dropped columns. */
			t->val = (Node *) makeConst(INT4OID, -1, sizeof(int32), (Datum) 0, true, true);
		}
		t->location = -1;

		s->targetList = lappend(s->targetList, t);
	}

	from_tbl = makeRangeVar(get_namespace_name(RelationGetNamespace(rel)),
							pstrdup(RelationGetRelationName(rel)), -1);
	from_tbl->inhOpt = INH_NO; /* MPP-5300: turn off inheritance -
								* Otherwise, the data from the child
								* tables is added to the parent!
								*/
	s->fromClause = list_make1(from_tbl);

	/* Handle tables with OIDS */
	if (rel->rd_rel->relhasoids)
		storage_opts = lappend(storage_opts, defWithOids(true));

	pre_built = prebuild_temp_table(rel, tmprel, dist_clause,
									storage_opts,
									(RelationIsAoRows(rel) ||
									 RelationIsAoCols(rel)),
									useExistingColumnAttributes);
	if (pre_built)
	{
		InsertStmt *i = makeNode(InsertStmt);

		i->relation = tmprel;
		i->selectStmt = (Node *)s;
		n = (Node *)i;
	}
	else
	{
		Oid tblspc = rel->rd_rel->reltablespace;

		s->intoClause = makeNode(IntoClause);
		s->intoClause->rel = tmprel;
		s->intoClause->options = storage_opts;
		s->intoClause->tableSpaceName = get_tablespace_name(tblspc);
		s->distributedBy = dist_clause;
		n = (Node *)s;
	}
	*tmprv = tmprel;

	q = parse_analyze((Node *)n, NULL, NULL, 0);

	AcquireRewriteLocks(q);

	/* Rewrite through rule system */
	rewritten = QueryRewrite(q);

	/* We don't expect more or less than one result query */
	Assert(list_length(rewritten) == 1);

	q = (Query *) linitial(rewritten);
	Assert(q->commandType == CMD_SELECT || q->commandType == CMD_INSERT);

	/* plan the query */
	stmt = planner(q, 0, NULL);

	/*
	 * Update snapshot command ID to ensure this query sees results of any
	 * previously executed queries.
	 */
	//ActiveSnapshot->curcid = GetCurrentCommandId();

	/* Create dest receiver for COPY OUT */
	dest = CreateDestReceiver(DestIntoRel, NULL);

	/* Create a QueryDesc requesting no output */
	queryDesc = CreateQueryDesc(stmt,  pstrdup("(internal SELECT INTO query)"),
								ActiveSnapshot, InvalidSnapshot,
								dest, NULL, false);
	return queryDesc;
}

static Datum
new_rel_opts(Relation rel, List *lwith)
{
	Datum newOptions = PointerGetDatum(NULL);
	bool make_heap = false;
	if (lwith && list_length(lwith))
	{
		ListCell *lc;

		/*
		 * See if user has specified appendonly = false. If so, we
		 * have no use for the existing reloptions
		 */
		foreach(lc, lwith)
		{
			DefElem *e = lfirst(lc);
			if (pg_strcasecmp(e->defname, "appendonly") == 0 &&
				pg_strcasecmp(defGetString(e), "false") == 0)
			{
				make_heap = true;
				break;
			}
		}
	}

	if (!make_heap)
	{
		/* Get the old reloptions */
		bool isnull;
		Oid relid = RelationGetRelid(rel);
		HeapTuple optsTuple;

		optsTuple = SearchSysCache1(RELOID,
									ObjectIdGetDatum(relid));
		if (!HeapTupleIsValid(optsTuple))
				elog(ERROR, "cache lookup failed for relation %u", relid);

		newOptions = SysCacheGetAttr(RELOID, optsTuple,
									 Anum_pg_class_reloptions, &isnull);

		/* take a copy since we're using it after ReleaseSysCache() */
		if (!isnull)
			newOptions = datumCopy(newOptions, false, -1);

		ReleaseSysCache(optsTuple);
	}

	/* Generate new proposed reloptions (text array) */
	newOptions = transformRelOptions(newOptions, lwith, false, false);

	return newOptions;
}

static RangeVar *
make_temp_table_name(Relation rel, BackendId id)
{
	char	   *nspname;
	char		tmpname[NAMEDATALEN];

	/* temporary enough */
	snprintf(tmpname, NAMEDATALEN, "pg_temp_%u_%i", RelationGetRelid(rel), id);
	nspname = get_namespace_name(RelationGetNamespace(rel));

	return makeRangeVar(nspname, pstrdup(tmpname), -1);
}

/*
 * If the table has dropped columns, we must create the table and
 * drop the columns before we can dispatch the select statement.
 * Return true if we do it, false if we do not. If we return false,
 * there are no dropped columns and we can do a SELECT INTO later.
 * If we need to do it, but fail, issue an error. (See make_type.)
 *
 * Specifically for build_ctas_with_dist.
 *
 * Note that the caller should guarantee that isTmpTableAo has
 * a value that matches 'opts'.
 */
static bool
prebuild_temp_table(Relation rel, RangeVar *tmpname, List *distro, List *opts,
					bool isTmpTableAo, bool useExistingColumnAttributes)
{
	bool need_rebuild = false;
	int attno = 0;
	TupleDesc tupdesc = RelationGetDescr(rel);

	/* 
	 * We cannot CTAS and do per column compression for AOCO tables so we need
	 * to CREATE and then INSERT.
	 */
	if (RelationIsAoCols(rel))
		need_rebuild = true;

	if (!need_rebuild)
	{
		for (attno = 0; attno < tupdesc->natts; attno++)
		{
			if (tupdesc->attrs[attno]->attisdropped)
			{
				need_rebuild = true;
				break;
			}
		}
	}

	/*
	 * If the new table is an AO table with indexes, always use
	 * Create Table + Insert Into. During Create Table phase,
	 * we determine whether to create the block directory
	 * depending on whether the original table has indexes. It is
	 * important to create the block directory to support the reindex
	 * later. See MPP-9545 for more info.
	 */
	if (isTmpTableAo &&
		rel->rd_rel->relhasindex)
		need_rebuild = true;

	if (need_rebuild)
	{
		CreateStmt *cs = makeNode(CreateStmt);
		Query *q;
		DestReceiver *dest = None_Receiver;
		List **col_encs = NULL;

		cs->relKind = RELKIND_RELATION;
		cs->distributedBy = distro;
		cs->relation = tmpname;
		cs->ownerid = rel->rd_rel->relowner;
		cs->tablespacename = get_tablespace_name(rel->rd_rel->reltablespace);
		cs->buildAoBlkdir = false;

		if (isTmpTableAo &&
			rel->rd_rel->relhasindex)
			cs->buildAoBlkdir = true;

		if (RelationIsAoCols(rel))
		{
			if (useExistingColumnAttributes)
			{
				/* 
				 * Need to remove table level compression settings for the
				 * AOCO case since they're set at the column level.
				 */
				ListCell *lc;

				foreach(lc, opts)
				{
					DefElem *de = lfirst(lc);

					if (de->defname &&
						(strcmp("compresstype", de->defname) == 0 ||
						 strcmp("compresslevel", de->defname) == 0 ||
						 strcmp("blocksize", de->defname) == 0))
						continue;
					else
						cs->options = lappend(cs->options, de);
				}
				col_encs = RelationGetUntransformedAttributeOptions(rel);
			}
			else
			{
				ListCell *lc;

				foreach(lc, opts)
				{
					DefElem *de = lfirst(lc);
					cs->options = lappend(cs->options, de);
				}
			}
		}
		else
			cs->options = opts;

		for (attno = 0; attno < tupdesc->natts; attno++)
		{
			ColumnDef *cd = makeNode(ColumnDef);
			TypeName *tname = NULL;
			Form_pg_attribute att = tupdesc->attrs[attno];

			cd->is_local = true;

			if (att->attisdropped)
			{
				/*
				 * Use dummy int4 columns to stand in for dropped columns.
				 * We cannot easily reconstruct the original layout, because
				 * we don't know what the original datatype was, and it might
				 * not even exist anymore. This means that the temp table is
				 * not binary-compatible with the old table. We will fix that
				 * by updating the catalogs of the original table, to match
				 * the temp table we build here, before swapping the relation
				 * files.
				 */
				tname = makeTypeNameFromOid(INT4OID, -1);
				cd->colname = pstrdup(NameStr(att->attname));
			}
			else
			{
				Type typ = typeidType(att->atttypid);
				Oid typnamespace = ((Form_pg_type) GETSTRUCT(typ))->typnamespace;
				char *nspname = get_namespace_name(typnamespace);
				int arno;
				char *typstr;
				int4 ndims = att->attndims;

				tname = makeNode(TypeName);

				if (!PointerIsValid(nspname))
					elog(ERROR, "could not lookup namespace %d", typnamespace);
				cd->colname = pstrdup(NameStr(att->attname));
				typstr = typeTypeName(typ);
				tname->names = list_make2(makeString(nspname),
										  makeString(typstr));
				ReleaseSysCache(typ);
				tname->typemod = att->atttypmod;

				/*
				 * If this is a built in array type, like _int4, then reduce
				 * the array dimensions by 1. This is an annoying postgres
				 * hack which I wish would go away.
				 */
				if (typstr && typstr[0] == '_' && ndims > 0)
					ndims--;

				for (arno = 0; arno < ndims; arno++)
					/* bound of -1 are fine because this has no effect on data */
					tname->arrayBounds = lappend(tname->arrayBounds,
												 makeInteger(-1));

				/* Per column encoding settings */
				if (col_encs)
					cd->encoding = col_encs[attno];
			}

			tname->location = -1;
			cd->typeName = tname;
			cs->tableElts = lappend(cs->tableElts, cd);
		}
		q = parse_analyze((Node *)cs, NULL, NULL, 0);
		ProcessUtility((Node *)q->utilityStmt,
					   synthetic_sql,
					   NULL,
					   false, /* not top level */
					   dest,
					   NULL);
		CommandCounterIncrement();
	}
	return need_rebuild;
}

/* Build a human readable tag for what we're doing */
static char *
make_distro_str(List *lwith, List *ldistro)
{
    char       *distro_str = "SET WITH DISTRIBUTED BY";

	if (lwith && ldistro)
		distro_str = "SET WITH DISTRIBUTED BY";
	else
	{
		if (lwith)
			distro_str = "SET WITH";
		else if (ldistro)
			distro_str = "SET DISTRIBUTED BY";
	}
	return pstrdup(distro_str); /* don't return a stack address */
}

/*
 * Check if distribution key compatible with unique index
 */
static void checkUniqueIndexCompatible(Relation rel, GpPolicy *pol)
{
	List *indexoidlist = RelationGetIndexList(rel);
	ListCell *indexoidscan = NULL;
	Bitmapset *polbm = NULL;
	int i = 0;

	if(pol == NULL || pol->nattrs == 0)
		return;

	for (i = 0; i < pol->nattrs; i++)
		polbm = bms_add_member(polbm, pol->attrs[i]);

	/* Loop over all indexes on the relation */
	foreach(indexoidscan, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(indexoidscan);
		HeapTuple	indexTuple;
		Form_pg_index indexStruct;
		int			i;
		Bitmapset  *indbm = NULL;

		indexTuple = SearchSysCache1(INDEXRELID,
									 ObjectIdGetDatum(indexoid));
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexoid);
		indexStruct = (Form_pg_index) GETSTRUCT(indexTuple);

		/* If the index is not a unique key, skip the check */
		if (indexStruct->indisunique)
		{
			for (i = 0; i < indexStruct->indnatts; i++)
			{
				indbm = bms_add_member(indbm, indexStruct->indkey.values[i]);
			}

			if (!bms_is_subset(polbm, indbm))
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("UNIQUE INDEX and DISTRIBUTED BY definitions incompatible"),
						 errhint("the DISTRIBUTED BY columns must be equal to "
								 "or a left-subset of the UNIQUE INDEX columns.")));
			}

			bms_free(indbm);
		}

		ReleaseSysCache(indexTuple);
	}

	list_free(indexoidlist);
	bms_free(polbm);
}

/*
 * ALTER TABLE SET DISTRIBUTED BY
 *
 * set distribution policy for rel
 */
static void
ATExecSetDistributedBy(Relation rel, Node *node, AlterTableCmd *cmd)
{
	List 	   *lprime;
	List 	   *lwith, *ldistro;
	List	   *cols = NIL;
	ListCell   *lc;
	GpPolicy   *policy = NULL;
	QueryDesc  *queryDesc;
	RangeVar   *tmprv;
	Oid			tmprelid;
	Oid			tarrelid = RelationGetRelid(rel);
	List	   *oid_map = NIL;
	bool        rand_pol = false;
	bool        force_reorg = false;
	Datum		newOptions = PointerGetDatum(NULL);
	bool		change_policy = false;
	bool		is_ao = false;
	bool        is_aocs = false;
	char        relstorage = RELSTORAGE_HEAP;
	int         nattr; /* number of attributes */
	bool useExistingColumnAttributes = true;
	SetDistributionCmd *qe_data = NULL; 

	/* Permissions checks */
	if (!pg_class_ownercheck(RelationGetRelid(rel), GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));

	/* Can't ALTER TABLE SET system catalogs */
	if (IsSystemRelation(rel))
		ereport(ERROR,
			(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
			errmsg("permission denied: \"%s\" is a system catalog", RelationGetRelationName(rel))));

	Assert(PointerIsValid(node));
	Assert(IsA(node, List));

	lprime = (List *)node;

	/* 
	 * First element is the WITH clause, second element is the actual
	 * distribution clause.
	 */
	lwith   = (List *)linitial(lprime);
	ldistro = (List *)lsecond(lprime);

	if (Gp_role == GP_ROLE_UTILITY)
		ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("SET DISTRIBUTED BY not supported in utility mode")));

	/* we only support fully distributed tables */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (rel->rd_cdbpolicy->ptype != POLICYTYPE_PARTITIONED)
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("%s not supported on non-distributed tables",
					ldistro ? "SET DISTRIBUTED BY" : "SET WITH")));
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (lwith)
		{
			bool		 seen_reorg = false;
			ListCell	*lc;
			char		*reorg_str = "reorganize";
			List		*nlist = NIL;

			/* remove the "REORGANIZE=true/false" from the WITH clause */
			foreach(lc, lwith)
			{
				DefElem	*def = lfirst(lc);

				if (pg_strcasecmp(reorg_str, def->defname) != 0)
				{
					/* MPP-7770: disable changing storage options for now */
					if (!gp_setwith_alter_storage)
						ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("option \"%s\" not supported", def->defname)));

					if (pg_strcasecmp(def->defname, "appendonly") == 0)
					{
						if (IsA(def->arg, String) && pg_strcasecmp(strVal(def->arg), "true") == 0)
						{
							is_ao = true;
							relstorage = RELSTORAGE_AOROWS;
						}
						else
							is_ao = false;
					}
					else if (pg_strcasecmp(def->defname, "orientation") == 0)
					{
						if (IsA(def->arg, String) && pg_strcasecmp(strVal(def->arg), "column") == 0)
						{
							is_aocs = true;
							relstorage = RELSTORAGE_AOCOLS;
						}
						else
						{
							if (!IsA(def->arg, String) || pg_strcasecmp(strVal(def->arg), "row") != 0)
								ereport(ERROR,
									(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
									errmsg("invalid orientation option"),
									errhint("valid orientation option is \"column\" or \"row\"")));
						}
					}
					else
					{
						useExistingColumnAttributes=false;
					}

					nlist = lappend(nlist, def);
				}
				else
				{
					/* have we been here before ? */
					if (seen_reorg)
						ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("\"%s\" specified more than once", reorg_str)));

					seen_reorg = true;
					if (!def->arg)
						force_reorg = true;
					else
					{
						if (IsA(def->arg, String) && pg_strcasecmp("TRUE", strVal(def->arg)) == 0)
							force_reorg = true;
						else if (IsA(def->arg, String) && pg_strcasecmp("FALSE", strVal(def->arg)) == 0)
							force_reorg = false;
						else
							ereport(ERROR,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
								errmsg("Invalid REORGANIZE option"),
								errhint("valid REORGANIZE option is \"true\" or \"false\"")));
					}
				}
			}

			if (is_aocs && !is_ao)
			{
				ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("specify orientation requires appendonly")));
			}

			lwith = nlist;

			/*
			 * If there are other storage options, but REORGANIZE is not
			 * specified, then the storage must be re-org'd.  But if
			 * REORGANIZE was specified use that setting.
			 *
			 * If the user specified we not force a reorg but there are other
			 * WITH clause items, then we cannot honour what the user has
			 * requested.
			 */
			if (!seen_reorg && list_length(lwith))
				force_reorg = true;
			else if (seen_reorg && force_reorg == false && list_length(lwith))
				ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("%s must be set to true when changing storage "
						"type", reorg_str)));

			newOptions = new_rel_opts(rel, lwith);

			/* ensure that the options parse */
			if (newOptions)
				(void)heap_reloptions(rel->rd_rel->relkind, newOptions, true);
		}
		else
		{
			newOptions = new_rel_opts(rel, NIL);
		}

		if (ldistro)
			change_policy = true;

		if (ldistro && linitial(ldistro) == NULL)
		{
			Insist(list_length(ldistro) == 1);

			rand_pol = true;

			if (!force_reorg)
			{
				if (rel->rd_cdbpolicy->nattrs == 0)
					ereport(WARNING,
						(errcode(ERRCODE_DUPLICATE_OBJECT),
						errmsg("distribution policy of relation \"%s\" "
							"already set to DISTRIBUTED RANDOMLY",
							RelationGetRelationName(rel)),
						errhint("Use ALTER TABLE \"%s\" "
							"SET WITH (REORGANIZE=TRUE) "
							"DISTRIBUTED RANDOMLY "
							"to force a random redistribution",
							RelationGetRelationName(rel))));
			}

			policy = createRandomDistribution();
			rel->rd_cdbpolicy = GpPolicyCopy(GetMemoryChunkContext(rel), policy);
			GpPolicyReplace(RelationGetRelid(rel), policy);

			/* only need to rebuild if have new storage options */
			if (!(DatumGetPointer(newOptions) || force_reorg))
			{
				/*
				 * caller expects ATExecSetDistributedBy() to close rel
				 * (see the non-random distribution case below for why.
				 */
				heap_close(rel, NoLock);
				lsecond(lprime) = makeNode(SetDistributionCmd); 
				goto l_distro_fini;
			}
		}
	}

	/*
	 * Changing a table from random distribution to a specific distribution
	 * policy is the hard bit. For that, we must do the following:
	 *
	 * a) Ensure that the proposed policy is sensible
	 * b) Create a temporary table and reorganise data according to
	 * our desired distribution policy. To do this, we build a Query
	 * node which expresses the query CREATE TABLE tmp_table_name AS
	 * SELECT * FROM cur_table DISTRIBUTED BY (policy)
	 * c) Execute the query across all nodes
	 * d) Update our parse tree to include the details of the newly created
	 * table
	 * e) Update the ownership of the temporary table
	 * f) Swap the relfilenodes of the existing table and the temporary table
	 * g) Update the policy on the QD to reflect the underlying data
	 * h) Drop the temporary table -- and with it, the old copy of the data
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		volatile Snapshot saveSnapshot = NULL;
		if (change_policy)
		{
			policy = palloc(sizeof(GpPolicy) + sizeof(policy->attrs[0]) * list_length(ldistro));
			policy->ptype = POLICYTYPE_PARTITIONED;
			policy->nattrs = 0;

			/* Step (a) */
			if (!rand_pol)
			{
				foreach(lc, ldistro)
				{
					char	   *colName = strVal((Value *)lfirst(lc));
					HeapTuple	tuple;
					AttrNumber	attnum;

					tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);

					if (!HeapTupleIsValid(tuple))
						ereport(ERROR,
								(errcode(ERRCODE_UNDEFINED_COLUMN),
								 errmsg("column \"%s\" of relation \"%s\" does not exist",
										colName,
										RelationGetRelationName(rel))));

					attnum = ((Form_pg_attribute) GETSTRUCT(tuple))->attnum;

					/* Prevent them from altering a system attribute */
					if (attnum <= 0)
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("cannot distribute by system column \"%s\"",
										colName)));

					policy->attrs[policy->nattrs++] = attnum;

					ReleaseSysCache(tuple);
					cols = lappend(cols, lfirst(lc));
				} /* end foreach */

				Assert(policy->nattrs > 0);

				/*
				 * See if the the old policy is the same as the new one but
				 * remember, we still might have to rebuild if there are new
				 * storage options.
				 */
				if (!DatumGetPointer(newOptions) && !force_reorg &&
					(policy->nattrs == rel->rd_cdbpolicy->nattrs))
				{
					int i;
					bool diff = false;

					for (i = 0; i < policy->nattrs; i++)
					{
						if (policy->attrs[i] != rel->rd_cdbpolicy->attrs[i])
						{
							diff = true;
							break;
						}
					}
					if (!diff)
					{
						/*
						 * This string length calculation relies on that we add
						 * a comma after each column entry except the last one,
						 * at which point the string should be NULL terminated
						 * instead.
						 */
						char *dist = palloc(list_length(ldistro) * (NAMEDATALEN + 1));

						dist[0] = '\0';

						foreach(lc, ldistro)
						{
							if (lc != list_head(ldistro))
								strcat(dist, ",");
							strcat(dist, strVal(lfirst(lc)));
						}
						ereport(WARNING,
							(errcode(ERRCODE_DUPLICATE_OBJECT),
							 errmsg("distribution policy of relation \"%s\" "
								"already set to (%s)",
								RelationGetRelationName(rel),
								dist),
							 errhint("Use ALTER TABLE \"%s\" "
								"SET WITH (REORGANIZE=TRUE) "
								"DISTRIBUTED BY (%s) "
								"to force redistribution",
								RelationGetRelationName(rel),
								dist)));
						heap_close(rel, NoLock);
						/* Tell QEs to do nothing */
						linitial(lprime) = NULL;
						lsecond(lprime) = makeNode(SetDistributionCmd); 

						return;
						/* don't goto l_distro_fini -- didn't do anything! */
					}
				}
			}

			checkUniqueIndexCompatible(rel, policy);
		}

		if (!ldistro)
			ldistro = make_dist_clause(rel);

		/*
		 * Force the use of legacy query optimizer, since PQO will not
		 * redistribute the tuples if the current and required distributions
		 * are both RANDOM even when reorganize is set to "true"
		 */
		bool saveOptimizerGucValue = optimizer;
		optimizer = false;

		if (saveOptimizerGucValue)
		{
			elog(LOG, "ALTER SET DISTRIBUTED BY: falling back to legacy query optimizer to ensure re-distribution of tuples.");
		}

		GpPolicy * original_policy = NULL;

		if (force_reorg && !rand_pol)
		{
			/*
			 * since we force the reorg, we don't care about the original
			 * distribution policy of the source table hence, we can set the
			 * policy to random, which will force it to redistribute if the new
			 * distribution policy is partitioned, even the new partition policy
			 * is same as the original one, the query optimizer will generate
			 * redistribute plan.
			 */
			GpPolicy * random_policy = createRandomDistribution();

			original_policy = rel->rd_cdbpolicy;
			rel->rd_cdbpolicy = GpPolicyCopy(GetMemoryChunkContext(rel),
											 random_policy);
			GpPolicyReplace(RelationGetRelid(rel), random_policy);
		}

		/* Step (b) - build CTAS */
		queryDesc = build_ctas_with_dist(rel, ldistro,
						untransformRelOptions(newOptions),
						&tmprv,
						useExistingColumnAttributes);

		PG_TRY();
		{
			/* 
			 * We need to update our snapshot here to make sure we see all
			 * committed work. We have an exclusive lock on the table so no one
			 * will be able to access the table now.
			 */
			saveSnapshot = ActiveSnapshot;
			ActiveSnapshot = CopySnapshot(GetLatestSnapshot());

	
			/* Step (c) - run on all nodes */
			ExecutorStart(queryDesc, 0);
			ExecutorRun(queryDesc, ForwardScanDirection, 0L);
			ExecutorEnd(queryDesc);
			FreeQueryDesc(queryDesc);

			/* Restore the old snapshot */
			ActiveSnapshot = saveSnapshot;
			optimizer = saveOptimizerGucValue;
		}
		PG_CATCH();
		{
			ActiveSnapshot = saveSnapshot;
			optimizer = saveOptimizerGucValue;

			PG_RE_THROW();
		}
		PG_END_TRY();

		CommandCounterIncrement(); /* see the effects of the command */

		if (original_policy)
		{
			rel->rd_cdbpolicy = original_policy;
			GpPolicyReplace(RelationGetRelid(rel), original_policy);
		}

		/*
		 * Step (d) - tell the seg nodes about the temporary relation. This
		 * involves stomping on the node we've been given
		 */
		qe_data = makeNode(SetDistributionCmd);
		qe_data->backendId = MyBackendId;
		qe_data->relids = list_make1_oid(tarrelid); 
	}
	else
	{
		int backend_id;
		bool reorg = false;
		ListCell *lc = NULL;
		DefElem	*def = NULL; 

		Assert(list_length(lprime) >= 2);

		lwith = linitial(lprime);
		qe_data = lsecond(lprime);

		/* Remove "reorganize" since we don't want it in reloptions of pg_class */	
		foreach(lc, lwith)
		{
			def = lfirst(lc);
			if (pg_strcasecmp(def->defname, "reorganize") == 0)
			{
				if (pg_strcasecmp(strVal(def->arg), "true") == 0)
					reorg = true;
				lwith = list_delete(lwith, def);
				break;
			}
		}

		/* Set to random distribution on master with no reorganisation */
		if (!reorg && qe_data->backendId == 0)
		{
			/* caller expects rel to be closed for this AT type */
			heap_close(rel, NoLock);
			goto l_distro_fini;			
		}

		if (!list_member_oid(qe_data->relids, tarrelid))
		{
			heap_close(rel, NoLock);
			goto l_distro_fini;			
		}

		backend_id = qe_data->backendId;
		tmprv = make_temp_table_name(rel, backend_id);
		oid_map = qe_data->indexOidMap;

		if (list_length(lprime) == 3)
		{
			Value *v = lthird(lprime);
			if (intVal(v) == 1)
			{
				is_ao = true;
				relstorage = RELSTORAGE_AOROWS;
			}
			else if (intVal(v) == 2)
			{
				is_ao = true;
				is_aocs = true;
				relstorage = RELSTORAGE_AOCOLS;
			}
			else
			{
				is_ao = false;
				relstorage = RELSTORAGE_HEAP;
			}
		}

		newOptions = new_rel_opts(rel, lwith);
	}

	/*
	 * Step (e) - Correct ownership on temporary table:
	 *   necessary so that the toast tables/indices have the correct
	 *   owner after we swap them.
	 *
	 * Note: ATExecChangeOwner does NOT dispatch, so this does not
	 * belong in the dispatch block above (MPP-9663).
	 */
	ATExecChangeOwner(RangeVarGetRelid(tmprv, false),
					  rel->rd_rel->relowner, true);
	CommandCounterIncrement(); /* see the effects of the command */

	/*
	 * Update pg_attribute for dropped columns. The temp table we built
	 * uses int4 to stand in for any dropped columns, so we need to update
	 * the original table's definition to match the new contents.
	 */
	change_dropped_col_datatypes(rel);

	/*
	 * Step (f) - swap relfilenodes and MORE !!!
	 *
	 * Just lookup the Oid and pass it to swap_relation_files(). To do
	 * this we must close the rel, since it needs to be forgotten by
	 * the cache, we keep the lock though. ATRewriteCatalogs() knows
	 * that we've closed the relation here.
	 */
	nattr = RelationGetNumberOfAttributes(rel);
	heap_close(rel, NoLock);
	rel = NULL;
	tmprelid = RangeVarGetRelid(tmprv, false);
	swap_relation_files(tarrelid, tmprelid, RecentXmin, false);

	if (DatumGetPointer(newOptions))
	{
		Datum		repl_val[Natts_pg_class];
		bool		repl_null[Natts_pg_class];
		bool		repl_repl[Natts_pg_class];
		HeapTuple	newOptsTuple;
		HeapTuple	tuple;
		Relation	relationRelation;

		/*
		 * All we need do here is update the pg_class row; the new
		 * options will be propagated into relcaches during
		 * post-commit cache inval.
		 */
		MemSet(repl_val, 0, sizeof(repl_val));
		MemSet(repl_null, false, sizeof(repl_null));
		MemSet(repl_repl, false, sizeof(repl_repl));

		if (newOptions != (Datum) 0)
			repl_val[Anum_pg_class_reloptions - 1] = newOptions;
		else
			repl_null[Anum_pg_class_reloptions - 1] = true;

		repl_repl[Anum_pg_class_reloptions - 1] = true;

		relationRelation = heap_open(RelationRelationId, RowExclusiveLock);
		tuple = SearchSysCache(RELOID,
							   ObjectIdGetDatum(tarrelid),
							   0, 0, 0);

		Insist(HeapTupleIsValid(tuple));
		newOptsTuple = heap_modify_tuple(tuple, RelationGetDescr(relationRelation),
										 repl_val, repl_null, repl_repl);

		simple_heap_update(relationRelation, &tuple->t_self, newOptsTuple);
		CatalogUpdateIndexes(relationRelation, newOptsTuple);

		heap_freetuple(newOptsTuple);

		ReleaseSysCache(tuple);

		heap_close(relationRelation, RowExclusiveLock);

		/*
		 * Increment cmd counter to make updates visible; this is
		 * needed because the same tuple has to be updated again
		 */
		CommandCounterIncrement();
	}

	if (gp_setwith_alter_storage)
	{
		RemoveAttributeEncodingsByRelid(tarrelid);
		cloneAttributeEncoding(tmprelid, tarrelid, nattr);
	}

	/* now, reindex */
	reindex_relation(tarrelid, false);

	/* Step (g) */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (change_policy)
			GpPolicyReplace(tarrelid, policy);

		qe_data->indexOidMap = oid_map;

		linitial(lprime) = lwith;
		lsecond(lprime) = qe_data;
		lprime = lappend(lprime, makeInteger(is_ao ? (is_aocs ? 2 : 1) : 0));
	}

	/* Step (h) Drop the table */
	{
		ObjectAddress object;
		object.classId = RelationRelationId;
		object.objectId = tmprelid;
		object.objectSubId = 0;

		performDeletion(&object, DROP_RESTRICT);
	}

l_distro_fini:

	/* MPP-6929: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		char *distro_str = make_distro_str(lwith, ldistro);
		/* don't check relkind - must be a table */
		MetaTrackUpdObject(RelationRelationId,
					tarrelid,
					GetUserId(),
					"ALTER", distro_str);
	}

}


/*
 * rel could be a toast table, toast table index or index on a
 * table. Get that table's OID.
 */
static Oid
rel_get_table_oid(Relation rel)
{
	Oid toid = RelationGetRelid(rel);
	Relation thisrel = NULL;

	if (rel->rd_rel->relkind == RELKIND_INDEX)
	{
		HeapTuple	indexTuple;
		Form_pg_index indexStruct;

		indexTuple = SearchSysCache1(INDEXRELID,
									 ObjectIdGetDatum(toid));
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failure: cannot find pg_index entry for OID %u",
				 toid);
		indexStruct = (Form_pg_index) GETSTRUCT(indexTuple);

		toid = indexStruct->indrelid;

		ReleaseSysCache(indexTuple);

		thisrel = relation_open(toid, NoLock);
		toid = rel_get_table_oid(thisrel); /* **RECURSIVE** */
		relation_close(thisrel, NoLock);

		return toid;
	}
	else if (rel->rd_rel->relkind == RELKIND_AOSEGMENTS ||
			 rel->rd_rel->relkind == RELKIND_AOBLOCKDIR ||
			 rel->rd_rel->relkind == RELKIND_AOVISIMAP ||
			 rel->rd_rel->relkind == RELKIND_TOASTVALUE)
	{
		/* use pg_depend to find parent */
		Relation	deprel;
		HeapTuple	tup;
		ScanKeyData scankey[2];
		SysScanDesc sscan;

		deprel = heap_open(DependRelationId, AccessShareLock);

		/* SELECT * FROM pg_depend WHERE classid = :1 AND objid = :2 */
		ScanKeyInit(&scankey[0],
					Anum_pg_depend_classid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(RelationRelationId));
		ScanKeyInit(&scankey[1],
					Anum_pg_depend_objid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(toid));

		/*
		 * We use SnapshotAny because the ordering of the dependency code means
		 * that some times we've already deleted the pg_depend tuple. So, we do
		 * an extra test below to see that, if this tuple is deleted, it was
		 * done so by our xid, otherwise we overlook it.
		 */
		sscan = systable_beginscan(deprel, DependDependerIndexId, true,
								   SnapshotAny, 2, scankey);

		while (HeapTupleIsValid(tup = systable_getnext(sscan)))
		{
			Form_pg_depend foundDep = (Form_pg_depend) GETSTRUCT(tup);
			HeapTupleHeader htup = tup->t_data;

			if (!TransactionIdIsNormal(HeapTupleHeaderGetXmax(htup)) ||
				TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(htup)))
			{
				if (foundDep->deptype == DEPENDENCY_INTERNAL)
				{
					toid = foundDep->refobjid;
					break;
				}
			}
		}
		systable_endscan(sscan);
		heap_close(deprel, AccessShareLock);
	}
	return toid;
}

/*
 * partition children, toast tables and indexes, and indexes on partition
 * children do not long lived locks because the lock on the partition master
 * protects us.
 */
bool
rel_needs_long_lock(Oid relid)
{
	bool needs_lock = true;
	Relation rel = relation_open(relid, NoLock);

	relid = rel_get_table_oid(rel);

	relation_close(rel, NoLock);

	if (Gp_role == GP_ROLE_DISPATCH)
		needs_lock = !rel_is_child_partition(relid);
	else
	{
		Relation inhrel;
		ScanKeyData scankey[2];
		SysScanDesc sscan;

		ScanKeyInit(&scankey[0],
					Anum_pg_inherits_inhrelid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(relid));
		ScanKeyInit(&scankey[1],
					Anum_pg_inherits_inhseqno,
					BTEqualStrategyNumber, F_INT4EQ,
					Int32GetDatum(1));

		inhrel = heap_open(InheritsRelationId, AccessShareLock);

		sscan = systable_beginscan(inhrel, InheritsRelidSeqnoIndexId,
								   true, SnapshotNow, 2, scankey);

		if (systable_getnext(sscan))
			needs_lock = false;

		systable_endscan(sscan);
		heap_close(inhrel, AccessShareLock);
	}
	return needs_lock;
}


/*
 * ALTER TABLE ... ADD PARTITION
 *
 */

static 	AlterPartitionId *
wack_pid_relname(AlterPartitionId 		 *pid,
				 PartitionNode  		**ppNode,
				 Relation 				  rel,
				 PgPartRule 			**ppar_prule,
				 char 					**plrelname,
				 char 					 *lRelNameBuf)
{
	AlterPartitionId 	*locPid = pid;	/* local pid if IDRule */

	if (!pid)
		return NULL;

	if (AT_AP_IDRule == locPid->idtype)
	{
		List 		*l1		   = (List *)pid->partiddef;
		ListCell 	*lc;
		PgPartRule 	*par_prule = NULL;

		lc = list_head(l1);
		*ppar_prule = (PgPartRule*) lfirst(lc);

		par_prule = *ppar_prule;

		*plrelname = par_prule->relname;

		if (par_prule && par_prule->topRule && par_prule->topRule->children)
			*ppNode = par_prule->topRule->children;

		lc = lnext(lc);

		locPid = (AlterPartitionId *)lfirst(lc);

		Assert(locPid);
	}
	else
	{
		*ppNode = RelationBuildPartitionDesc(rel, false);

		snprintf(lRelNameBuf, (NAMEDATALEN*2),
					 "relation \"%s\"",
					 RelationGetRelationName(rel));
		*plrelname = lRelNameBuf;
	}

	return locPid;
}

static void
ATPExecPartAdd(AlteredTableInfo *tab,
			   Relation rel,
               AlterPartitionCmd *pc,
			   AlterTableType att)
{
	AlterPartitionId 	*pid 		= (AlterPartitionId *)pc->partid;
	PgPartRule   		*prule 		= NULL;
	PartitionNode  		*pNode 		= NULL;
	char           		*parTypName = NULL;
	char			 	 namBuf[NAMEDATALEN];
	AlterPartitionId 	*locPid 	= NULL;	/* local pid if IDRule */
	PgPartRule* 		 par_prule 	= NULL;	/* prule for parent if IDRule */
	char 		 		 lRelNameBuf[(NAMEDATALEN*2)];
	char 				*lrelname   = NULL;
	Node 				*pSubSpec 	= NULL;
	bool				 is_split = false;
	bool				 bSetTemplate = (att == AT_PartSetTemplate);
	PartitionElem *pelem;
	List	   *colencs = NIL;

	/* This whole function is QD only. */
	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	if (att == AT_PartAddForSplit)
	{
		is_split = true;
		colencs = (List *) pc->arg2;
	}
	pelem = (PartitionElem *) pc->arg1;

	locPid =
			wack_pid_relname(pid,
							 &pNode,
							 rel,
							 &par_prule,
							 &lrelname,
							 lRelNameBuf);

	if (!pNode)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("%s is not partitioned", lrelname)));

	switch (pNode->part->parkind)
	{
		case 'h': /* hash */
			parTypName = "HASH";
			break;
		case 'r': /* range */
			parTypName = "RANGE";
			break;
		case 'l': /* list */
			parTypName = "LIST";
			break;
		default:
			elog(ERROR, "unrecognized partitioning kind '%c'",
				 pNode->part->parkind);
			Assert(false);
			break;
	} /* end switch */


	if (locPid->idtype == AT_AP_IDName)
			snprintf(namBuf, sizeof(namBuf), " \"%s\"",
					 strVal(locPid->partiddef));
	else
			namBuf[0] = '\0';

	if ('h' == pNode->part->parkind)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot add HASH partition%s to %s",
						namBuf,
						lrelname),
				 errhint("recreate the table to increase the "
						 "number of partitions")));


	/* partition must have a valid name */
	if ((locPid->idtype != AT_AP_IDName)
		&& (locPid->idtype != AT_AP_IDNone))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot ADD partition%s to %s by rank or value",
						namBuf,
						lrelname),
					 errhint("use a named partition")));

	/* don't check if splitting or setting a subpartition template */
	if (!is_split && !bSetTemplate)
	{
		/* We complain if partition already exists, so prule should be NULL */
		prule = get_part_rule(rel, pid, true, false, NULL, false);
	}

	if (!prule)
	{
		bool isDefault = pelem->isDefault;

		/* DEFAULT checks */
		if (!isDefault && (pNode->default_part) &&
			!is_split && !bSetTemplate) /* MPP-6093: ok to reset template */
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot add %s partition%s to "
						"%s with DEFAULT partition \"%s\"",
						parTypName,
						namBuf,
						lrelname,
						pNode->default_part->parname),
				 errhint("need to SPLIT partition \"%s\"",
						 pNode->default_part->parname)));

		if (isDefault && !is_split)
		{
			if ('h' == pNode->part->parkind)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("cannot add a DEFAULT partition%s "
								"to %s of type HASH",
								namBuf,
								lrelname)));

			/* MPP-6093: ok to reset template */
			if (pNode->default_part && !bSetTemplate)
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_OBJECT),
						 errmsg("DEFAULT partition \"%s\" for "
								"%s already exists",
								pNode->default_part->parname,
								lrelname)));

			/* XXX XXX: move this check to gram.y ? */
			if (pelem && pelem->boundSpec)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("invalid use of boundary specification "
								"for DEFAULT partition%s of %s",
								namBuf,
								lrelname)));
		}

		/* Do the real work for add ... */

		if ('r' == pNode->part->parkind)
		{
			pSubSpec =
			atpxPartAddList(rel, is_split, colencs, pNode,
							(locPid->idtype == AT_AP_IDName) ?
							strVal(locPid->partiddef) : NULL, /* partition name */
							isDefault, pelem,
							PARTTYP_RANGE,
							par_prule,
							lrelname,
							bSetTemplate,
							rel->rd_rel->relowner);
		}
		else if ('l' == pNode->part->parkind)
		{
			pSubSpec =
			atpxPartAddList(rel, is_split, colencs, pNode,
							(locPid->idtype == AT_AP_IDName) ?
							strVal(locPid->partiddef) : NULL, /* partition name */
							isDefault, pelem,
							PARTTYP_LIST,
							par_prule,
							lrelname,
							bSetTemplate,
							rel->rd_rel->relowner);
		}

	}

	/* MPP-6929: metadata tracking */
	if (!is_split && !bSetTemplate)
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "PARTITION", "ADD"
				);

} /* end ATPExecPartAdd */


/*
 * Add partition hierarchy to catalogs.
 *
 * parts is a list, with a partitioning rule and potentially sub-partitions.
 */
static void
ATExecPartAddInternal(Relation rel, Node *def)
{
	PartitionBy *part = (PartitionBy *)def;
	add_part_to_catalog(RelationGetRelid(rel), part, false);
}


/* ALTER TABLE ... ALTER PARTITION */
static void
ATPExecPartAlter(List **wqueue, AlteredTableInfo *tab, Relation rel,
                 AlterPartitionCmd *pc)
{
	AlterPartitionId 	*pid		   = (AlterPartitionId *)pc->partid;
	AlterTableCmd 		*atc		   = (AlterTableCmd *)pc->arg1;
	PgPartRule   		*prule		   = NULL;
	List 				*pidlst		   = NIL;
	AlterPartitionId 	*pid2		   = makeNode(AlterPartitionId);
	AlterPartitionCmd 	*pc2		   = NULL;
	bool				 bPartitionCmd = true;	/* true if a "partition" cmd */
	Relation			 rel2		   = rel;

	while (1)
	{
		pidlst = lappend(pidlst, pid);

		if (atc->subtype != AT_PartAlter)
			break;
		pc2 = (AlterPartitionCmd *)atc->def;
		pid = (AlterPartitionId *)pc2->partid;
		atc = (AlterTableCmd *)pc2->arg1;
	}

	/* let split, exchange through */
	if (!(atc->subtype == AT_PartExchange ||
		  atc->subtype == AT_PartSplit ||
		  atc->subtype == AT_SetDistributedBy) &&
		Gp_role != GP_ROLE_DISPATCH)
		return;

	switch (atc->subtype)
	{
		case AT_PartAdd:				/* Add */
		case AT_PartAddForSplit:		/* Add, as part of a split */
		case AT_PartCoalesce:			/* Coalesce */
		case AT_PartDrop:				/* Drop */
		case AT_PartSplit:				/* Split */
		case AT_PartMerge:				/* Merge */
		case AT_PartModify:				/* Modify */
		case AT_PartSetTemplate:		/* Set Subpartition Template */
				if (!gp_allow_non_uniform_partitioning_ddl)
				{
					ereport(ERROR,
						   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("Cannot modify multi-level partitioned table to have non-uniform partitioning hierarchy.")));
				}
				break;
				/* XXX XXX: treat set subpartition template special:

				need to pass the pNode to ATPExecPartSetTemplate and bypass
				ATExecCmd ...

				*/
		case AT_PartRename:	 			/* Rename */
		case AT_PartExchange:			/* Exchange */
		case AT_PartTruncate:			/* Truncate */
				break;

		/* Next, list of ALTER TABLE commands applicable to a child table */
		case AT_SetTableSpace:			/* Set Tablespace */
		case AT_SetDistributedBy:	/* SET DISTRIBUTED BY */
				bPartitionCmd = false;
				break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot ALTER PARTITION for relation \"%s\"",
							RelationGetRelationName(rel))));
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		pid2->idtype = AT_AP_IDList;
		pid2->partiddef = (Node *)pidlst;
		pid2->location  = -1;

		prule = get_part_rule(rel, pid2, true, true, NULL, false);

		if (!prule)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot ALTER PARTITION for relation \"%s\"",
							RelationGetRelationName(rel))));
		if (bPartitionCmd)
		{
			/* build the IDRule for the nested ALTER PARTITION cmd ... */
			Assert(IsA(atc->def, AlterPartitionCmd));

			pc2 = (AlterPartitionCmd *)atc->def;
			pid = (AlterPartitionId *)pc2->partid;

			pid2->idtype = AT_AP_IDRule;
			pid2->partiddef = (Node *)list_make2((Node *)prule, pid);
			pid2->location  = -1;

			pc2->partid = (Node *)pid2;
		}
		else /* treat as a table */
		{
			/* Update PID for use on QEs */
			pid2->idtype = AT_AP_IDRule;
			pid2->partiddef = (Node *)list_make2((Node *)prule, pid);
			pid2->location  = -1;
			pc->partid = (Node *)pid2;

			/* get the child table relid */
			rel2 = heap_open(prule->topRule->parchildrelid,
							 AccessExclusiveLock);

			/* MPP-5524: check if can change distribution policy */
			if (atc->subtype == AT_SetDistributedBy)
			{
				List *dist_cnames = NIL;
				Assert(IsA(atc->def, List));

				dist_cnames = lsecond((List*)atc->def);

				/*	might be null if no policy set, e.g. just a change
				 *	of storage options...
				 */
				if (dist_cnames)
				{
					Assert(IsA(dist_cnames, List));

					if (! can_implement_dist_on_part(rel, dist_cnames) )
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("cannot ALTER PARTITION ... SET "
										"DISTRIBUTED BY for %s",
										prule->relname),
										errhint("distribution policy of "
												"partition must match parent")
										));
				}

			}

			/*
			 * Give the notice first, because it looks weird if it
			 * comes after a failure message
			 */
			ereport(NOTICE,
					(errmsg("altering table \"%s\" "
							"(%s)",
							RelationGetRelationName(rel2),
							prule->relname)));
		}
	}
	else if (Gp_role == GP_ROLE_EXECUTE && atc->subtype == AT_SetDistributedBy)
	{
		pid = (AlterPartitionId *)pc->partid;
		Assert(IsA(pid->partiddef, List));
		prule = (PgPartRule *)linitial((List *)pid->partiddef);
		/* get the child table relid */
		rel2 = heap_open(prule->topRule->parchildrelid,
						 AccessExclusiveLock);
		bPartitionCmd = false;
	}
	/* execute the command */
	ATExecCmd(wqueue, tab, &rel2, atc);

	if (!bPartitionCmd)
	{
		/* NOTE: for the case of Set Distro,
		 * ATExecSetDistributedBy rebuilds the relation, so rel2
		 * is already gone!
		 */
		if (atc->subtype != AT_SetDistributedBy)
			heap_close(rel2, NoLock);
	}

	/* MPP-6929: metadata tracking - don't track this! */

} /* end ATPExecPartAlter */


/* ALTER TABLE ... COALESCE PARTITION */
static void
ATPExecPartCoalesce(Relation rel,
                    AlterPartitionCmd *pc)
{
	if (Gp_role != GP_ROLE_DISPATCH)
		return;

    ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
             errmsg("cannot COALESCE PARTITION for relation \"%s\"",
                    RelationGetRelationName(rel))));
}

/* ALTER TABLE ... DROP PARTITION */

static void
ATPExecPartDrop(Relation rel,
                AlterPartitionCmd *pc)
{
	AlterPartitionId 	*pid 		 = (AlterPartitionId *)pc->partid;
	PgPartRule   		*prule 		 = NULL;
	PartitionNode  		*pNode  	 = NULL;
	DropStmt 			*ds 		 = (DropStmt *)pc->arg1;
	bool 				 bCheckMaybe = !(ds->missing_ok);
	AlterPartitionId 	*locPid 	 = pid;  /* local pid if IDRule */
	PgPartRule* 		 par_prule 	 = NULL; /* prule for parent if IDRule */
	char 		 		 lRelNameBuf[(NAMEDATALEN*2)];
	char 				*lrelname	= NULL;
	bool 				 bForceDrop	= false;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	if (pc->arg2 &&
		IsA(pc->arg2, AlterPartitionCmd))
	{
		/* NOTE: Ugh, I hate this hack.  Normally, PartDrop has a null
		 * pc->arg2 (the DropStmt is on arg1).  However, for SPLIT, we
		 * have the case where we may need to DROP that last partition
		 * of a table, which in only ok because we will re-ADD two
		 * partitions to replace it.  So allow bForceDrop only for
		 * this case.  We need a better way to decorate the ALTER cmd
		 * structs to annotate these special cases.
		 */
		bForceDrop = true;
	}

	/* missing partition id only ok for range partitions -- just get
	 * first one */

	locPid =
			wack_pid_relname(pid,
							 &pNode,
							 rel,
							 &par_prule,
							 &lrelname,
							 lRelNameBuf);

	if (AT_AP_IDNone == locPid->idtype)
	{
		if (pNode && pNode->part && (pNode->part->parkind != 'r'))
		{
			if (strlen(lrelname))
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("missing name or value for DROP for %s",
								lrelname)));
			else
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("missing name or value for DROP")));
		}

		/* if a range partition, and not specified, just get the first one */
		locPid->idtype = AT_AP_IDRank;
		locPid->partiddef = (Node *)makeInteger(1);
	}

	prule = get_part_rule(rel, pid, bCheckMaybe, true, NULL, false);

	/* MPP-3722: complain if for(value) matches the default partition */
	if ((locPid->idtype == AT_AP_IDValue)
		&& prule &&
		(prule->topRule == prule->pNode->default_part))
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("FOR expression matches "
							"DEFAULT partition%s of %s",
							prule->partIdStr,
							prule->relname),
					 errhint("FOR expression may only specify "
							 "a non-default partition in this context.")));

	if (!prule)
	{
		Assert(ds->missing_ok);
		switch (locPid->idtype)
		{
			case AT_AP_IDNone:				/* no ID */
				/* should never happen */
				Assert(false);
				break;
			case AT_AP_IDName:				/* IDentify by Name */
				ereport(NOTICE,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("partition \"%s\" of %s does not "
							"exist, skipping",
							strVal(locPid->partiddef),
							lrelname
							)));

				break;
			case AT_AP_IDValue:				/* IDentifier FOR Value */
				ereport(NOTICE,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("partition for specified value of "
							"%s does not exist, skipping",
							lrelname
							 )));

				break;
			case AT_AP_IDRank:				/* IDentifier FOR Rank */
				ereport(NOTICE,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("partition for specified rank of "
							"%s does not exist, skipping",
							lrelname
							 )));

				break;
			case AT_AP_ID_oid:				/* IDentifier by oid */
				ereport(NOTICE,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("partition for specified oid of "
							"%s does not exist, skipping",
							lrelname
							 )));
				break;
			case AT_AP_IDDefault:			/* IDentify DEFAULT partition */
				ereport(NOTICE,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("DEFAULT partition for "
							"%s does not exist, skipping",
							lrelname
							 )));
				break;
			default: /* XXX XXX */
				Assert(false);
		}
		return;
	}
	else
	{
		char* prelname;
		int   numParts = list_length(prule->pNode->rules);
		DestReceiver 	*dest = None_Receiver;
		Relation rel2;
		RangeVar *relation;
		char *namespace_name;

		/* add the default partition to the count of partitions */
		if (prule->pNode->default_part)
			numParts++;

		/* maybe ERRCODE_INVALID_TABLE_DEFINITION ? */

		/* cannot drop a hash partition */
		if ('h' == prule->pNode->part->parkind)
			ereport(ERROR,
					(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
					 errmsg("cannot drop HASH partition%s of %s",
							prule->partIdStr,
							prule->relname),
					 errhint("recreate the table to reduce the "
							 "number of partitions")));

		/* cannot drop last partition of table */
		if (!bForceDrop && (numParts <= 1))
		{
			if (AT_AP_IDRule != pid->idtype)
				ereport(ERROR,
						(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
						 errmsg("cannot drop partition%s of "
								"%s -- only one remains",
								prule->partIdStr,
								prule->relname),
						 errhint("Use DROP TABLE \"%s\" to remove the "
								 "table and the final partition ",
								 RelationGetRelationName(rel))));
			else
				ereport(ERROR,
						(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
						 errmsg("cannot drop partition%s of "
								"%s -- only one remains",
								prule->partIdStr,
								prule->relname),
						 errhint("DROP the parent partition to remove the "
								 "final partition ")));

		}
		rel2 = heap_open(prule->topRule->parchildrelid, NoLock);

		elog(DEBUG5, "dropping partition oid %u", prule->topRule->parchildrelid);
		prelname = pstrdup(RelationGetRelationName(rel2));
		namespace_name = get_namespace_name(rel2->rd_rel->relnamespace);

		/* XXX XXX : don't need "relation" unless fix to use removerelation */
		relation = makeRangeVar(namespace_name, prelname, -1);
		relation->location = pc->location;

		heap_close(rel2, NoLock);

		ds->removeType = OBJECT_TABLE;
		ds->bAllowPartn = true; /* allow drop of partitions */

		if (prule->topRule->children)
		{
			List *l1 = atpxDropList(rel2, prule->topRule->children);

			ds->objects = lappend(l1,
								  list_make2(makeString(namespace_name),
											 makeString(prelname)));
		}
		else
			ds->objects = list_make1(list_make2(makeString(namespace_name),
												makeString(prelname)));

		ProcessUtility((Node *) ds,
					   synthetic_sql,
					   NULL,
					   false, /* not top level */
					   dest,
					   NULL);

		/* Notify of name if did not use name for partition id spec */
		if (prule && prule->topRule && prule->topRule->children
			&& (ds->behavior != DROP_CASCADE ))
		{
			ereport(NOTICE,
					(errmsg("dropped partition%s for %s and its children",
							prule->partIdStr,
							prule->relname)));
		}
		else if ((pid->idtype != AT_AP_IDName)
				 && prule->isName)
				ereport(NOTICE,
						(errmsg("dropped partition%s for %s",
								prule->partIdStr,
								prule->relname)));


		/* MPP-6929: metadata tracking */
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "PARTITION", "DROP"
				);
	}

} /* end ATPExecPartDrop */

static void
exchange_part_inheritance(Oid oldrelid, Oid newrelid)
{
	Oid			parentid;
	Relation	oldrel;
	Relation	newrel;
	Relation	parent;
	Relation	catalogRelation;
	ScanKeyData scankey;
	SysScanDesc scan;
	HeapTuple	tuple;

	oldrel = heap_open(oldrelid, AccessExclusiveLock);
	newrel = heap_open(newrelid, AccessExclusiveLock);

	/* SELECT inhparent FROM pg_inherits WHERE inhrelid = :1 */
	catalogRelation = heap_open(InheritsRelationId, AccessShareLock);
	ScanKeyInit(&scankey,
				Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(oldrelid));
	scan = systable_beginscan(catalogRelation, InheritsRelidSeqnoIndexId,
							  true, SnapshotNow, 1, &scankey);

	/* should be one and only one parent when it comes to inheritance */
	tuple = systable_getnext(scan);
	if (!tuple)
		elog(ERROR, "could not find pg_inherits row for rel %u", oldrelid);

	parentid = ((Form_pg_inherits) GETSTRUCT(tuple))->inhparent;

	Assert(systable_getnext(scan) == NULL);

	systable_endscan(scan);
	heap_close(catalogRelation, AccessShareLock);

	parent = heap_open(parentid, AccessShareLock); /* should be enough */
	ATExecDropInherit(oldrel,
			makeRangeVar(get_namespace_name(parent->rd_rel->relnamespace),
					     RelationGetRelationName(parent), -1),
			true);

	inherit_parent(parent, newrel, true /* it's a partition */, NIL);
	heap_close(parent, NoLock);
	heap_close(oldrel, NoLock);
	heap_close(newrel, NoLock);
}

/* ALTER TABLE ... EXCHANGE PARTITION 
 * 
 * Do the exchange that was validated earlier (in ATPrepExchange).
 */
static void
ATPExecPartExchange(AlteredTableInfo *tab, Relation rel, AlterPartitionCmd *pc)
{
	Oid					 oldrelid 	= InvalidOid;
	Oid					 newrelid 	= InvalidOid;
	PgPartRule   	    *orig_prule = NULL;
	AlterPartitionCmd	*pc2 = NULL;
	bool is_split = false;
	List				*pcols = NIL; /* partitioned attributes of rel */
	AlterPartitionIdType orig_pid_type = AT_AP_IDNone;	/* save for NOTICE msg at end... */

	if (Gp_role == GP_ROLE_UTILITY)
		return;
	
	/* Exchange for SPLIT is different from user-requested EXCHANGE.  The special
	 * coding to indicate SPLIT is obscure. */
	is_split = ((AlterPartitionCmd *)pc->arg2)->arg2 != NULL;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		AlterPartitionId   *pid   = (AlterPartitionId *)pc->partid;
		PgPartRule   	   *prule = NULL;
		RangeVar       	   *newrelrv  = (RangeVar *)pc->arg1;
		RangeVar		   *oldrelrv;
		PartitionNode 	   *pn;
		Relation			oldrel;
		Relation			newrel;

		pn = RelationBuildPartitionDesc(rel, false);
		pcols = get_partition_attrs(pn);

		prule = get_part_rule(rel, pid, true, true, NULL, false);

		if (!prule)
			return;

		if (prule && prule->topRule && prule->topRule->children)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot EXCHANGE PARTITION for "
							"%s -- partition has children",
							prule->relname
							 )));

		newrelid = RangeVarGetRelid(newrelrv, false);
		Assert(OidIsValid(newrelid));
		newrel = heap_open(newrelid, NoLock);
		if (RelationIsExternal(newrel))
		{
			if (prule && prule->topRule && prule->topRule->parparentoid)
			{
				heap_close(newrel, NoLock);
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot exchange sub partition with external table")));
			}
		}
		heap_close(newrel, NoLock);

		orig_pid_type = pid->idtype;
		orig_prule = prule;
		oldrelid = prule->topRule->parchildrelid;

		/*
		 * We are here because we are either doing an EXCHANGE PARTITION, or SPLIT PARTITION.
		 * We do not allow EXCHANGE PARTITION for the default partition, so let's check for that
		 * and error out.
		 */
		bool fExchangeDefaultPart = !is_split && rel_is_default_partition(oldrelid);

		if (fExchangeDefaultPart && !gp_enable_exchange_default_partition)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("cannot exchange DEFAULT partition")));
		}

		pfree(pc->partid);

		oldrel = heap_open(oldrelid, NoLock);
		oldrelrv =
			makeRangeVar(get_namespace_name(RelationGetNamespace(oldrel)),
						 get_rel_name(oldrelid), -1);
		heap_close(oldrel, NoLock);

		pc->partid = (Node *)oldrelrv;
		pc2 = (AlterPartitionCmd *)pc->arg2;
		pc2->arg2 = (Node *)pcols; /* for execute nodes */

		if (fExchangeDefaultPart)
		{
			elog(WARNING, "Exchanging default partition may result in unexpected query results if "
					"the data being exchanged should have been inserted into a different partition");
		}

		/* MPP-6929: metadata tracking */
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "PARTITION", "EXCHANGE"
				);
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		RangeVar *oldrelrv;
		RangeVar *newrelrv;

		Assert(IsA(pc->partid, RangeVar));
		Assert(IsA(pc->arg1, RangeVar));
		oldrelrv = (RangeVar *)pc->partid;
		newrelrv = (RangeVar *)pc->arg1;
		oldrelid = RangeVarGetRelid(oldrelrv, false);
		newrelid = RangeVarGetRelid(newrelrv, false);
		pc2 = (AlterPartitionCmd *)pc->arg2;
		pcols = (List *)pc2->arg2;
	}

	Assert(OidIsValid(oldrelid));
	Assert(OidIsValid(newrelid));

#if IF_ONLY_IT_WAS_THAT_SIMPLE
	swap_relation_files(oldrelid, newrelid, false);
	CommandCounterIncrement();
#else
	/*
	 * It would be nice to just swap the relfilenodes. In fact, we could
	 * do that in most cases, the exceptions being tables with dropped 
	 * columns and append only tables.
	 *
	 * So instead, we swap the names of the tables, the type names, the 
	 * constraints, inheritance. We do not swap indexes, ao information
	 * or statistics.
	 *
	 * Note that the state, whether QD or QE, at this point is
	 * - rel -- relid of partitioned table
	 * - oldrelid -- relid of part currently in place
	 * - newrelid -- relid of candidate part to exchange in
	 * - orig_pid_type -- what kind of AlterTablePartitionId id'd the part
	 * - orig_prule -- Used in issuing notice in occasional case.
	 * - pc2->arg1 -- integer Value node: 1 validate, else 0.
	 * - pcols -- integer list of master partitioning columns
	 */
	{
		char			 tmpname1[NAMEDATALEN];
		char			 tmpname2[NAMEDATALEN];
		char			*newname;
		char			*oldname;
		Relation		 newrel;
		Relation		 oldrel;
		AttrMap			*newmap;
		AttrMap			*oldmap;
		List			*newcons;
		bool			 ok;
		bool			 validate	= intVal(pc2->arg1) ? true : false;
		Oid				 oldnspid	= InvalidOid;
		Oid				 newnspid	= InvalidOid;
		char			*newNspName = NULL;
		char			*oldNspName = NULL;

		newrel = heap_open(newrelid, AccessExclusiveLock);
		if (RelationIsExternal(newrel) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("validation of external tables not supported"),
					 errhint("Use WITHOUT VALIDATION.")));

		oldrel = heap_open(oldrelid, AccessExclusiveLock);

		oldnspid = RelationGetNamespace(oldrel);
		newnspid = RelationGetNamespace(newrel);

		if (oldnspid != newnspid)
		{
			newNspName = pstrdup(get_namespace_name(newnspid));
			oldNspName = pstrdup(get_namespace_name(oldnspid));
		}

		newname = pstrdup(RelationGetRelationName(newrel));
		oldname = pstrdup(RelationGetRelationName(oldrel));
		
		ok = map_part_attrs(rel, newrel, &newmap, FALSE);
		Assert(ok);
		ok = map_part_attrs(rel, oldrel, &oldmap, FALSE);
		Assert(ok);

		newcons = cdb_exchange_part_constraints(
				rel, oldrel, newrel, validate, is_split, pc);
		tab->constraints = list_concat(tab->constraints, newcons);
		CommandCounterIncrement();

		exchange_part_inheritance(RelationGetRelid(oldrel), RelationGetRelid(newrel));
		CommandCounterIncrement();

		snprintf(tmpname1, sizeof(tmpname1), "pg_temp_%u", oldrelid);
		snprintf(tmpname2, sizeof(tmpname2), "pg_temp_%u", newrelid);

		exchange_permissions(RelationGetRelid(oldrel),
							 RelationGetRelid(newrel));
		CommandCounterIncrement();

		heap_close(newrel, NoLock);
		heap_close(oldrel, NoLock);

		/* RenameRelation renames the type too */
		RenameRelation(oldrelid, tmpname1, OBJECT_TABLE, NULL);
		CommandCounterIncrement();
		RelationForgetRelation(oldrelid);

		/* MPP-6979: if the namespaces are different, switch them */
		if (newNspName)
		{
			ObjectAddresses *objsMoved = new_object_addresses();

			/* move the old partition (which has a temporary name) to
			 * the new namespace 
			 */
			oldrel = heap_open(oldrelid, AccessExclusiveLock);
			AlterTableNamespaceInternal(oldrel, oldnspid, newnspid, objsMoved);
			heap_close(oldrel, NoLock);
			CommandCounterIncrement();
			RelationForgetRelation(oldrelid);

			/* before we move the new table to the old namespace,
			 * rename it to a temporary name to avoid a name
			 * collision.  It would be nice to have an atomic
			 * operation to rename and renamespace a relation... 
			 */
			RenameRelation(newrelid, tmpname2, OBJECT_TABLE, NULL);
			CommandCounterIncrement();
			RelationForgetRelation(newrelid);

			newrel = heap_open(newrelid, AccessExclusiveLock);
			AlterTableNamespaceInternal(newrel, newnspid, oldnspid, objsMoved);
			heap_close(newrel, NoLock);
			CommandCounterIncrement();
			RelationForgetRelation(newrelid);

			free_object_addresses(objsMoved);
		}

		RenameRelation(newrelid, oldname, OBJECT_TABLE, NULL);
		CommandCounterIncrement();
		RelationForgetRelation(newrelid);

		RenameRelation(oldrelid, newname, OBJECT_TABLE, NULL);
		CommandCounterIncrement();
		RelationForgetRelation(oldrelid);

		CommandCounterIncrement();

		/* fix up partitioning rule if we're on the QD*/
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			exchange_part_rule(oldrelid, newrelid);
			CommandCounterIncrement();

			/* Notify of name if did not use name for partition id spec */
			if ( orig_pid_type != AT_AP_IDName && orig_prule->isName )
				ereport(NOTICE,
						(errmsg("exchanged partition%s of "
								"%s with relation \"%s\"",
								orig_prule->partIdStr,
								orig_prule->relname,
								newname)));
		}
	}
	tab->exchange_relid = newrelid;
#endif
}

/* ALTER TABLE ... MERGE PARTITION */
static void
ATPExecPartMerge(Relation rel,
                 AlterPartitionCmd *pc)
{
	Relation source = NULL;
	Relation target = NULL;

	if (Gp_role == GP_ROLE_UTILITY)
		return;
	else if (Gp_role == GP_ROLE_DISPATCH)
	{
		AlterPartitionId   *pid   = (AlterPartitionId *)pc->partid;
		PgPartRule   	   *prule1 = NULL;
		PgPartRule   	   *prule2 = NULL;
		PgPartRule   	   *prule3 = NULL;
		RangeVar		   *relrv1, *relrv2;
		Relation			r1, r2;

		prule1 = get_part_rule(rel, pid, true, true, NULL, false);

		/*
		 * XXX: get_namespace_name(prule1->topRule->parchildrelid) is wrong
		 * need the relnamespace not relid
		 */
		relrv1 = makeRangeVar(
					get_namespace_name(prule1->topRule->parchildrelid),
					get_rel_name(prule1->topRule->parchildrelid), -1);

		r1 = heap_openrv(relrv1, AccessExclusiveLock);

		prule2 = get_part_rule(rel, (AlterPartitionId *)pc->arg1,
							   true, true, NULL, false);

		relrv2 = makeRangeVar(
					get_namespace_name(prule2->topRule->parchildrelid),
					get_rel_name(prule2->topRule->parchildrelid), -1);

		r2 = heap_openrv(relrv2, AccessExclusiveLock);

		if (pc->arg2)
		{
			AlterPartitionCmd *pc2 = (AlterPartitionCmd *)pc->arg2;

			prule3 = get_part_rule(rel, (AlterPartitionId *)pc2->partid,
								   true, true, NULL, false);

			Assert(PointerIsValid(prule3));

			if (prule3->topRule->parchildrelid == prule1->topRule->parchildrelid)
			{
				source = r1;
				pc->partid = (Node *)relrv1;
				target = r2;
				pc->arg1 = (Node *)relrv2;
			}
			else if (prule3->topRule->parchildrelid == prule2->topRule->parchildrelid)
			{
				source = r2;
				pc->partid = (Node *)relrv2;
				target = r1;
				pc->arg1 = (Node *)relrv1;
			}
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("target of MERGE must be either \"%s\" or "
								"\"%s\"",
								get_rel_name(prule1->topRule->parchildrelid),
								get_rel_name(prule2->topRule->parchildrelid))));

			}
		}
		else
		{
			source = r1;
			target = r2;
			pc->partid = (Node *)relrv1;
			pc->arg1 = (Node *)relrv2;
		}

		/* MPP-6929: metadata tracking */
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "PARTITION", "MERGE"
				);
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		RangeVar *relrv;

		Assert(IsA(pc->partid, RangeVar));
		Assert(IsA(pc->arg1, RangeVar));

		relrv = (RangeVar *)pc->partid;
		source = heap_openrv(relrv, AccessExclusiveLock);

		relrv = (RangeVar *)pc->arg1;
		target = heap_openrv(relrv, AccessExclusiveLock);
	}

	elog(NOTICE, "merging");
	elog(NOTICE, "%s", RelationGetRelationName(source));

	elog(NOTICE, "with %s", RelationGetRelationName(target));

    ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
             errmsg("cannot MERGE PARTITION for relation \"%s\"",
                    RelationGetRelationName(rel))));

}


/* ALTER TABLE ... MODIFY PARTITION */
static void
ATPExecPartModify(Relation rel,
                  AlterPartitionCmd *pc)
{
	AlterPartitionId *pid = (AlterPartitionId *)pc->partid;
	PgPartRule   *prule = NULL;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	prule = get_part_rule(rel, pid, true, true, NULL, false);

	if (prule)
	{
		AlterPartitionCmd 		*pc2 	= (AlterPartitionCmd *)pc->arg1;
		AlterPartitionCmd 		*pc3 	= (AlterPartitionCmd *)pc2;
		DefElem    				*def 	= (DefElem *)pc3->arg1;
		bool 					 bStart = false;
		bool 					 bEnd 	= false;
		bool 					 bAdd 	= false;
		bool 					 bDrop 	= false;
		char           *parTypName = NULL;

		if (0 == strcmp(def->defname, "START"))
			bStart = true;
		else if (0 == strcmp(def->defname, "END"))
			bEnd = true;
		else if (0 == strcmp(def->defname, "ADD"))
			bAdd = true;
		else if (0 == strcmp(def->defname, "DROP"))
			bDrop = true;
		else Assert(false);

		switch (prule->pNode->part->parkind)
		{
			case 'h': /* hash */
				parTypName = "HASH";
				break;
			case 'r': /* range */
				parTypName = "RANGE";
				break;
			case 'l': /* list */
				parTypName = "LIST";
				break;
			default:
				elog(ERROR, "unrecognized partitioning kind '%c'",
					 prule->pNode->part->parkind);
				Assert(false);
				break;
		} /* end switch */

		/* cannot modify default partitions */
		if (prule->pNode->default_part == prule->topRule)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot MODIFY DEFAULT %s partition%s "
							"for relation \"%s\"",
							parTypName,
							((prule && prule->isName) ? prule->partIdStr : ""),
							RelationGetRelationName(rel))));

		if ((bStart || bEnd) && ('r' != prule->pNode->part->parkind))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid use of %s boundary specification "
							"in partition%s of type %s",
							"RANGE",
							((prule && prule->isName) ? prule->partIdStr : ""),
							parTypName
							 )));

		if ((bAdd || bDrop) && ('l' != prule->pNode->part->parkind))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid use of %s boundary specification "
							"in partition%s of type %s",
							"LIST",
							((prule && prule->isName) ? prule->partIdStr : ""),
							parTypName
							 )));

		if (bAdd || bDrop)
		{
			bool stat = atpxModifyListOverlap(rel, pid, prule,
											  (PartitionElem *)pc3->arg2,
											  bAdd);
			stat = false;
		}
		if (bStart || bEnd)
		{
			bool stat = atpxModifyRangeOverlap(rel, pid, prule,
											   (PartitionElem *)pc3->arg2);
			stat = false;
		}

		/* MPP-6929: metadata tracking */
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "PARTITION", "MODIFY"
				);
	}

	if (0)
    ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
             errmsg("cannot MODIFY PARTITION for relation \"%s\"",
                    RelationGetRelationName(rel))));

}

/* ALTER TABLE ... RENAME PARTITION */

static void
ATPExecPartRename(Relation rel,
                  AlterPartitionCmd *pc)
{
	AlterPartitionId 	*pid 	   = (AlterPartitionId *)pc->partid;
	PgPartRule   		*prule 	   = NULL;
	PartitionNode  		*pNode     = NULL;
	AlterPartitionId 	*locPid    = pid;	/* local pid if IDRule */
	PgPartRule* 		 par_prule = NULL;	/* prule for parent if IDRule */
	char 		 		 lRelNameBuf[(NAMEDATALEN*2)];
	char 				*lrelname=NULL;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	locPid =
			wack_pid_relname(pid,
							 &pNode,
							 rel,
							 &par_prule,
							 &lrelname,
							 lRelNameBuf);

	prule = get_part_rule(rel, pid, true, true, NULL, false);

	if (prule)
	{
		AlterPartitionId		 newpid;
		PgPartRule   		   	*prule2 	 = NULL;
		Relation			 	 targetrelation;
		char        		 	 targetrelname[NAMEDATALEN];
		Relation			 	 parentrelation;
		Oid						 namespaceId;
		char	   			   	*newpartname = strVal(pc->arg1);
		char	   			   	*relname;
		char	  			 	 parentname[NAMEDATALEN];
		int 				 	 partDepth 	 = prule->pNode->part->parlevel;
		RenameStmt 			   	*renStmt 	 = makeNode(RenameStmt);
		DestReceiver 		   	*dest  		 = None_Receiver;
		Relation				 part_rel;
		HeapTuple				 tuple;
		Form_pg_partition_rule	 pgrule;
		List 					*renList 	 = NIL;
		int 					 skipped 	 = 0;
		int 					 renamed 	 = 0;

		newpid.idtype = AT_AP_IDName;
		newpid.partiddef = pc->arg1;
		newpid.location = -1;

		/* ERROR if exists */
		prule2 = get_part_rule1(rel, &newpid, true, false,
								NULL,
								pNode,
								lrelname,
								NULL);

		targetrelation = relation_open(prule->topRule->parchildrelid,
									   AccessExclusiveLock);

		StrNCpy(targetrelname, RelationGetRelationName(targetrelation),
				NAMEDATALEN);

		namespaceId = RelationGetNamespace(targetrelation);

		relation_close(targetrelation, AccessExclusiveLock);

		if (0 == prule->topRule->parparentoid)
		{
			StrNCpy(parentname,
					RelationGetRelationName(rel), NAMEDATALEN);
		}
		else
		{
			Assert(par_prule);
			if (par_prule)
			{
				/* look in the parent prule */
				parentrelation =
					RelationIdGetRelation(par_prule->topRule->parchildrelid);
				StrNCpy(parentname,
						RelationGetRelationName(parentrelation), NAMEDATALEN);
				RelationClose(parentrelation);
			}
		}

		/* MPP-3523: the "label" portion of the new relation is
		 * prt_`newpartname', and makeObjectName won't truncate this
		 * portion of the partition name -- it will assert instead.
		 */
		if (strlen(newpartname) > (NAMEDATALEN - 8))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("name \"%s\" for child partition "
							"is too long",
							newpartname)));

		relname = ChoosePartitionName(parentname,
									  partDepth,
									  newpartname,
									  namespaceId);
		/* does CommandCounterIncrement */

		renStmt->renameType = OBJECT_TABLE;
		renStmt->relation = makeRangeVar(get_namespace_name(namespaceId),
										 pstrdup(targetrelname), -1);

		renStmt->subname = NULL;
		renStmt->newname = relname;
		renStmt->bAllowPartn = true; /* allow rename of partitions */

		if (prule && prule->topRule && prule->topRule->children)
				pNode = prule->topRule->children;
		else
				pNode = NULL;

		/* rename the children as well */
		renList = atpxRenameList(pNode, targetrelname, relname, &skipped);

		part_rel = heap_open(PartitionRuleRelationId, RowExclusiveLock);

		tuple = SearchSysCacheCopy1(PARTRULEOID,
									ObjectIdGetDatum(prule->topRule->parruleid));
		Insist(HeapTupleIsValid(tuple));

		pgrule = (Form_pg_partition_rule)GETSTRUCT(tuple);
		namestrcpy(&(pgrule->parname), newpartname);

		simple_heap_update(part_rel, &tuple->t_self, tuple);
		CatalogUpdateIndexes(part_rel, tuple);

		heap_freetuple(tuple);
		heap_close(part_rel, NoLock);

		CommandCounterIncrement();

		ProcessUtility((Node *) renStmt,
					   synthetic_sql,
					   NULL,
					   false, /* not top level */
					   dest,
					   NULL);

		/* process children if there are any */
		if (renList)
		{
			ListCell 		*lc;

			foreach(lc, renList)
			{
				ListCell 		*lc2;
				List  			*lpair = lfirst(lc);

				lc2 = list_head(lpair);

				renStmt->relation = (RangeVar *)lfirst(lc2);
				lc2 = lnext(lc2);
				renStmt->newname = (char *)lfirst(lc2);

				ProcessUtility((Node *) renStmt,
							   synthetic_sql,
							   NULL,
							   false, /* not top level */
							   dest,
							   NULL);
				renamed++;
			}
		}

		/* MPP-6929: metadata tracking */
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "PARTITION", "RENAME"
				);

		/* Notify of name if did not use name for partition id spec */
		if ((pid->idtype != AT_AP_IDName)
			&& prule->isName)
			ereport(NOTICE,
					(errmsg("renamed partition%s to \"%s\" for %s",
							prule->partIdStr,
							newpartname,
							prule->relname)));

		/* MPP-3542: warn when skip child partitions */
		if (skipped)
		{
			ereport(WARNING,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("renamed %d partitions, skipped %d child partitions due to name truncation",
							renamed, skipped)));
		}

	}

} /* end ATPExecPartRename */


/* ALTER TABLE ... SET SUBPARTITION TEMPLATE  */
static void
ATPExecPartSetTemplate(AlteredTableInfo *tab,
					   Relation rel,
                       AlterPartitionCmd *pc)
{
	AlterPartitionId	*pid   = (AlterPartitionId *)pc->partid;
	PgPartRule			*prule = NULL;
	int					 lvl   = 1;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	/* set template for top level table */
    if (pid && (pid->idtype != AT_AP_IDName))
    {
		Assert((pid->idtype == AT_AP_IDRule) && IsA(pid->partiddef, List));

		/* MPP-5941: work correctly with many levels of templates */
		/* wah! the idrule is invalid, so can't use get_part_rule.
		 * So pull the pgpartrule directly from the idrule (yuck!)
		 */
		prule = (PgPartRule *)linitial((List*)pid->partiddef);

		Assert(prule);

		/* current pnode level is one below current (our parent), and
		 * we want the one above us (our subpartition), so add 2
		 */
		lvl = prule->pNode->part->parlevel + 2;

		Assert (lvl > 1);
	}

	{
		/* relid, level, no parent */
		switch (del_part_template(RelationGetRelid(rel), lvl, 0))
		{
			case 0:
				if (pc->arg1)
				{
					/* no prior template - just add new one */
					ereport(NOTICE,
							(errmsg("%s level %d "
									"subpartition template specification "
									"for relation \"%s\"",
									"adding",
									lvl,
									RelationGetRelationName(rel))));
				}
				else
				{
					/* tried to drop non-existent template */
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_OBJECT),
							 errmsg("relation \"%s\" does not have a "
									"level %d "
									"subpartition template specification",
									RelationGetRelationName(rel),
									lvl)));
				}
				break;
			case 1:
					/* if have new spec,
					 * note old spec is being replaced,
					 * else just note it is dropped
					 */
					ereport(NOTICE,
							(errmsg("%s level %d "
									"subpartition template specification "
									"for relation \"%s\"",
									(pc->arg1) ? "replacing" : "dropped",
									lvl,
									RelationGetRelationName(rel))));
				break;
			default:
					elog(ERROR,
						 "could not drop "
						 "level %d "
						 "subpartition template specification "
						 "for relation \"%s\"",
						 lvl,
						 RelationGetRelationName(rel));
					break;
		}

	}

	if (pc->arg1)
		ATPExecPartAdd(tab, rel, pc, AT_PartSetTemplate);

	/* MPP-6929: metadata tracking */
	MetaTrackUpdObject(RelationRelationId,
					   RelationGetRelid(rel),
					   GetUserId(),
					   "ALTER", "SET SUBPARTITION TEMPLATE"
			);


} /* end ATPExecPartSetTemplate */


/* part rule update */
typedef struct part_rule_cxt
{
	Oid			old_oid;
	Oid			new_oid;
} part_rule_cxt;

static bool
partrule_walker(Node *node, void *context)
{
	part_rule_cxt *p = (part_rule_cxt *)context;

	if (node == NULL)
		return false;
	if (IsA(node, PgPartRule))
	{
		PgPartRule *pg = (PgPartRule *)node;

		partrule_walker((Node *)pg->topRule, p);
		partrule_walker((Node *)pg->pNode, p);
		return false;
	}
	else if (IsA(node, Partition))
	{
		return false;
	}
	else if (IsA(node, PartitionRule))
	{
		PartitionRule *pr = (PartitionRule *)node;

		if (pr->parchildrelid == p->old_oid)
			pr->parchildrelid = p->new_oid;

		return partrule_walker((Node *)pr->children, p);

	}
	else if (IsA(node, PartitionNode))
	{
		PartitionNode *pn = (PartitionNode *)node;
		ListCell *lc;

		partrule_walker((Node *)pn->default_part, p);
		foreach(lc, pn->rules)
		{
			PartitionRule *r = lfirst(lc);
			partrule_walker((Node *)r, p);
		}
		return false;
	}

	return expression_tree_walker(node, partrule_walker, p);
}

/* 
 * Build a basic ResultRelInfo for executing split. We only need
 * the relation descriptor and index information.
 */
static ResultRelInfo *
make_split_resultrel(Relation rel)
{
	ResultRelInfo *rri;

	rri = palloc0(sizeof(ResultRelInfo));
	rri->type = T_ResultRelInfo;
	rri->ri_RelationDesc = rel;
	rri->ri_NumIndices = 0;

	ExecOpenIndices(rri);
	return rri;
}

/*
 * Close indexes and free memory
 */
static void
destroy_split_resultrel(ResultRelInfo *rri)
{
	ExecCloseIndices(rri);

	/* 
	 * Don't do anything with the relation descriptor, that's our caller's job
	 */
	pfree(rri);

}

/*
 * Scan tuples from the temprel (origin, T) and route them to split parts (A, B)
 * based on the constraints.  It is important that the origin may have dropped
 * columns while the new relations will not.
 *
 * This also covers index tuple population.  Note this doesn't handle row OID
 * as it's not allowed in partition.
 */
static void
split_rows(Relation intoa, Relation intob, Relation temprel)
{
	ResultRelInfo *rria = make_split_resultrel(intoa);
	ResultRelInfo *rrib = make_split_resultrel(intob);
	EState *estate = CreateExecutorState();
	TupleDesc		tupdescT = temprel->rd_att;
	TupleTableSlot *slotT = MakeSingleTupleTableSlot(tupdescT);
	HeapScanDesc heapscan = NULL;
	AppendOnlyScanDesc aoscan = NULL;
    AOCSScanDesc aocsscan = NULL;
    bool *aocsproj = NULL;
	MemoryContext oldCxt;
	AppendOnlyInsertDesc aoinsertdesc_a = NULL;
	AppendOnlyInsertDesc aoinsertdesc_b = NULL;
    AOCSInsertDesc aocsinsertdesc_a = NULL;
    AOCSInsertDesc aocsinsertdesc_b = NULL;
	ExprState *achk = NULL;
	ExprState *bchk = NULL;

	/*
	 * Set up for reconstructMatchingTupleSlot.  In split operation,
	 * slot/tupdesc should look same between A and B, but here we don't
	 * assume so just in case, to be safe.
	 */
	rria->ri_partSlot = MakeSingleTupleTableSlot(RelationGetDescr(intoa));
	rrib->ri_partSlot = MakeSingleTupleTableSlot(RelationGetDescr(intob));
	map_part_attrs(temprel, intoa, &rria->ri_partInsertMap, true);
	map_part_attrs(temprel, intob, &rrib->ri_partInsertMap, true);
	Assert(NULL != rria->ri_RelationDesc);
	rria->ri_resultSlot = MakeSingleTupleTableSlot(rria->ri_RelationDesc->rd_att);
	Assert(NULL != rrib->ri_RelationDesc);
	rrib->ri_resultSlot = MakeSingleTupleTableSlot(rrib->ri_RelationDesc->rd_att);

	/* constr might not be defined if this is a default partition */
	if (intoa->rd_att->constr && intoa->rd_att->constr->num_check)
	{
		Node *bin = (Node *)make_ands_implicit(
				(Expr *)stringToNode(intoa->rd_att->constr->check[0].ccbin));
		achk = ExecPrepareExpr((Expr *)bin, estate);
	}

	if (intob->rd_att->constr && intob->rd_att->constr->num_check)
	{
		Node *bin = (Node *)make_ands_implicit(
				(Expr *)stringToNode(intob->rd_att->constr->check[0].ccbin));
		bchk = ExecPrepareExpr((Expr *)bin, estate);
	}

	/* be careful about AO vs. normal heap tables */
	if (RelationIsHeap(temprel))
		heapscan = heap_beginscan(temprel, SnapshotNow, 0, NULL);
	else if (RelationIsAoRows(temprel))
		aoscan = appendonly_beginscan(temprel, SnapshotNow, SnapshotNow, 0, NULL);
	else if (RelationIsAoCols(temprel))
	{
		int nvp = temprel->rd_att->natts;
		int i;

		aocsproj = (bool *) palloc(sizeof(bool) * nvp);
		for(i=0; i<nvp; ++i)
			aocsproj[i] = true;

		aocsscan = aocs_beginscan(temprel, SnapshotNow, SnapshotNow, NULL /* relationTupleDesc */, aocsproj);
	}
	else
	{
		Assert(false);
	}

	oldCxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

	while (true)
	{
		ExprContext *econtext = GetPerTupleExprContext(estate);
		bool targetIsA;
		Relation targetRelation;
		AppendOnlyInsertDesc *targetAODescPtr;
		AOCSInsertDesc *targetAOCSDescPtr;
		TupleTableSlot	   *targetSlot;
		ItemPointer			tid = NULL;
		ResultRelInfo	   *targetRelInfo = NULL;
		AOTupleId			aoTupleId;

		/* read next tuple from temprel */
		if (RelationIsHeap(temprel))
		{
			HeapTuple tuple;

			tuple = heap_getnext(heapscan, ForwardScanDirection);
			if (!HeapTupleIsValid(tuple))
				break;

			tuple = heap_copytuple(tuple);
			ExecStoreHeapTuple(tuple, slotT, InvalidBuffer, false);
		}
		else if (RelationIsAoRows(temprel))
		{
			MemTuple mtuple;

			mtuple = appendonly_getnext(aoscan, ForwardScanDirection, slotT);
			if (!PointerIsValid(mtuple))
				break;

			TupClearShouldFree(slotT);
		}
		else if (RelationIsAoCols(temprel))
		{
			aocs_getnext(aocsscan, ForwardScanDirection, slotT);
			if (TupIsNull(slotT))
				break;
		}

		/* prepare for ExecQual */
		econtext->ecxt_scantuple = slotT;

		/* determine if we are inserting into a or b */
		if (achk)
		{
			targetIsA = ExecQual((List *)achk, econtext, false);
		}
		else
		{
			Assert(PointerIsValid(bchk));

			targetIsA = !ExecQual((List *)bchk, econtext, false);
		}

		/* load variables for the specific target */
		if (targetIsA)
		{
			targetRelation = intoa;
			targetAODescPtr = &aoinsertdesc_a;
			targetAOCSDescPtr = &aocsinsertdesc_a;
			targetRelInfo = rria;
		}
		else
		{
			targetRelation = intob;
			targetAODescPtr = &aoinsertdesc_b;
			targetAOCSDescPtr = &aocsinsertdesc_b;
			targetRelInfo = rrib;
		}

		/*
		 * Map attributes from origin to target.  We should consider dropped
		 * columns in the origin.
		 */
		targetSlot = reconstructMatchingTupleSlot(slotT, targetRelInfo);

		/* insert into the target table */
		if (RelationIsHeap(targetRelation))
		{
			HeapTuple tuple;

			tuple = ExecFetchSlotHeapTuple(targetSlot);
			simple_heap_insert(targetRelation, tuple);

			/* cache TID for later updating of indexes */
			tid = &(((HeapTuple) tuple)->t_self);
		}
		else if (RelationIsAoRows(targetRelation))
		{
			MemTuple	mtuple;
			Oid			tupleOid;

			if (!(*targetAODescPtr))
			{
				MemoryContextSwitchTo(oldCxt);
				*targetAODescPtr = appendonly_insert_init(targetRelation,
														  RESERVED_SEGNO, false);
				MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
			}

			mtuple = ExecFetchSlotMemTuple(targetSlot, false);
			appendonly_insert(*targetAODescPtr, mtuple, &tupleOid, &aoTupleId);

			/* cache TID for later updating of indexes */
			tid = (ItemPointer) &aoTupleId;
		}
		else if (RelationIsAoCols(targetRelation))
		{
			if (!*targetAOCSDescPtr)
			{
				MemoryContextSwitchTo(oldCxt);
				*targetAOCSDescPtr = aocs_insert_init(targetRelation,
													  RESERVED_SEGNO, false);
				MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
			}

			aocs_insert(*targetAOCSDescPtr, targetSlot);

			/* cache TID for later updating of indexes */
			tid = slot_get_ctid(targetSlot);
		}
		else
		{
			Assert(false);
		}

		/*
		 * Insert index for this tuple.
		 * TODO: for performance reason, should we call index_build() instead?
		 */
		if (targetRelInfo->ri_NumIndices > 0)
		{
			estate->es_result_relation_info = targetRelInfo;
			ExecInsertIndexTuples(targetSlot, tid, estate, false);
			estate->es_result_relation_info = NULL;
		}

		/* done, clean up context for this pass */
		ResetExprContext(econtext);
	}

	if (aoinsertdesc_a)
		appendonly_insert_finish(aoinsertdesc_a);
	if (aoinsertdesc_b)
		appendonly_insert_finish(aoinsertdesc_b);
	if (aocsinsertdesc_a)
		aocs_insert_finish(aocsinsertdesc_a);
	if (aocsinsertdesc_b)
		aocs_insert_finish(aocsinsertdesc_b);

	MemoryContextSwitchTo(oldCxt);
	ExecDropSingleTupleTableSlot(slotT);
	ExecDropSingleTupleTableSlot(rria->ri_partSlot);
	ExecDropSingleTupleTableSlot(rrib->ri_partSlot);

	/*
	 * We created our target result tuple table slots upfront.
	 * We can drop them now.
	 */
	Assert(NULL != rria->ri_resultSlot);
	Assert(NULL != rria->ri_resultSlot->tts_tupleDescriptor);
	ExecDropSingleTupleTableSlot(rria->ri_resultSlot);
	rria->ri_resultSlot = NULL;

	Assert(NULL != rrib->ri_resultSlot);
	Assert(NULL != rrib->ri_resultSlot->tts_tupleDescriptor);
	ExecDropSingleTupleTableSlot(rrib->ri_resultSlot);
	rrib->ri_resultSlot = NULL;

	if (rria->ri_partInsertMap)
		pfree(rria->ri_partInsertMap);
	if (rrib->ri_partInsertMap)
		pfree(rrib->ri_partInsertMap);

	if (RelationIsHeap(temprel))
		heap_endscan(heapscan);
	else if (RelationIsAoRows(temprel))
		appendonly_endscan(aoscan);
	else if (RelationIsAoCols(temprel))
	{
		pfree(aocsproj);
		aocs_endscan(aocsscan);
	}

	destroy_split_resultrel(rria);
	destroy_split_resultrel(rrib);
}

/* ALTER TABLE ... SPLIT PARTITION */

/* 
   atpxSplitDropRule: walk the partition rules, eliminating rules that
   match parruleid and paroid.  If topRule is supplied, walk his
   subtree as well (it may differ from pnode if the PgPartRule was
   copied).
*/
static void
atpxSplitDropRule(PartitionNode *pNode, PartitionRule *topRule,
				  Oid parruleid, Oid paroid)
{
	ListCell *lc;
	ListCell *lcprev = NULL, *lcdel = NULL;

	if (!pNode)
		return;

	foreach(lc, pNode->rules)
	{
		PartitionRule *rule = lfirst(lc);

		if (rule->children)
			atpxSplitDropRule(rule->children, NULL, 
							  parruleid, paroid);

		if ((parruleid == rule->parruleid) &&
			(paroid == rule->paroid))
		{
			lcdel = lc; /* should only be one match */
			break;
		}
		lcprev = lc;
	}
	if (lcdel)
		pNode->rules = list_delete_cell(pNode->rules, lcdel, lcprev);

	/* and the default partition */
	if (pNode->default_part)
	{
		PartitionRule *rule = pNode->default_part;

		if (rule->children)
				atpxSplitDropRule(rule->children, NULL,
								  parruleid, paroid);

		if ((parruleid == rule->parruleid) &&
			(paroid == rule->paroid))
			pNode->default_part = NULL;
	}

	/* check optional topRule */
	if (topRule && topRule->children)
		atpxSplitDropRule(topRule->children, NULL,
						  parruleid, paroid);

} /* end atpxSplitDropRule */

/* Given a Relation, make a distributed by () clause for parser consumption. */
List *
make_dist_clause(Relation rel)
{
	int i;
	List *distro = NIL;

	for (i = 0; i < rel->rd_cdbpolicy->nattrs; i++)
	{
		AttrNumber attno = rel->rd_cdbpolicy->attrs[i];
		TupleDesc tupdesc = RelationGetDescr(rel);
		Value *attstr;
		NameData attname;

		attname = tupdesc->attrs[attno - 1]->attname;
		attstr = makeString(pstrdup(NameStr(attname)));

		distro = lappend(distro, attstr);
	}

	if (!distro)
	{
		/* must be random distribution */
		distro = list_make1(NULL);
	}
	return distro;
}

/*
 * Given a relation, get all column encodings for that relation as a list of
 * ColumnReferenceStorageDirective structures.
 */
static List *
rel_get_column_encodings(Relation rel)
{
	List **colencs = RelationGetUntransformedAttributeOptions(rel);
	List *out = NIL;

	if (colencs)
	{
		AttrNumber attno;

		for (attno = 0; attno < RelationGetNumberOfAttributes(rel); attno++)
		{
			if (colencs[attno] && !rel->rd_att->attrs[attno]->attisdropped)
			{
				ColumnReferenceStorageDirective *d =
					makeNode(ColumnReferenceStorageDirective);
				d->column = pstrdup(NameStr(rel->rd_att->attrs[attno]->attname));
				d->encoding = colencs[attno];
		
				out = lappend(out, d);
			}
		}
	}
	return out;
}

/*
 * Depending on whether a table is heap, append only or append only column
 * oriented, return NIL, (appendonly=true) or (appendonly=true,
 * orientation=column) respectively.
 */
static List *
make_orientation_options(Relation rel)
{
	List *l = NIL;

	if (RelationIsAoRows(rel) ||
		RelationIsAoCols(rel))
	{
		l = lappend(l, makeDefElem("appendonly", (Node *)makeString("true")));

		if (RelationIsAoCols(rel))
		{
			l = lappend(l, makeDefElem("orientation",
									   (Node *)makeString("column")));
		}
	}
	return l;
}

static void
ATPExecPartSplit(Relation *rel,
                 AlterPartitionCmd *pc)
{
	Relation temprel = NULL;
	Relation intoa = NULL;
	Relation intob = NULL;
	Oid temprelid;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CreateStmt *ct = makeNode(CreateStmt);
		char tmpname[NAMEDATALEN];
		InhRelation *inh = makeNode(InhRelation);
		DestReceiver *dest = None_Receiver;
		Node *at = lsecond((List *)pc->arg1);
		AlterPartitionCmd *pc2 = (AlterPartitionCmd *)pc->arg2;
		AlterPartitionId *pid = (AlterPartitionId *)pc->partid;
		AlterTableStmt *ats = makeNode(AlterTableStmt);
		RangeVar *rv;
		AlterTableCmd *cmd = makeNode(AlterTableCmd);
		AlterPartitionCmd *mypc = makeNode(AlterPartitionCmd);
		AlterPartitionCmd *mypc2 = makeNode(AlterPartitionCmd);
		AlterPartitionId *idpid;
		RangeVar *tmprv;
		PgPartRule *prule;
		DropStmt *ds = makeNode(DropStmt);
		Query *q;
		char *nspname = get_namespace_name(RelationGetNamespace(*rel));
		char *relname = get_rel_name(RelationGetRelid(*rel));
		Oid relid = RelationGetRelid(*rel);
		RangeVar *rva = NULL;
		RangeVar *rvb = NULL;
		int into_exists = 0; /* which into partition exists? */
		int i;
		AlterPartitionId *intopid1 = NULL;
		AlterPartitionId *intopid2 = NULL;
		int default_pos = 0;
		Oid rel_to_drop = InvalidOid;
		AlterPartitionId *aapid = NULL; /* just for alter partition pids */
		Relation existrel;
		List *existstorage_opts;
		char *defparname = NULL; /* name of default partition (if specified) */
		List *distro = NIL;
		List *colencs = NIL;
		List *orient = NIL;

		/* Get target meta data */
		prule = get_part_rule(*rel, pid, true, true, NULL, false);

		/* Error out on external partition */
		existrel = heap_open(prule->topRule->parchildrelid, NoLock);
		if (RelationIsExternal(existrel))
		{
			heap_close(existrel, NoLock);
			elog(ERROR, "Cannot split external partition");
		}
		heap_close(existrel, NoLock);

		/*
		 * In order to implement SPLIT, we do the following:
		 *
		 * 1) Build a temporary table T on all nodes.
		 * 2) Exchange that table with the target partition P
		 *    Now, T has all the data or P
		 * 3) Drop partition P
		 * 4) Create two new partitions in the place of the old one
		 */

		/* look up INTO clause info, if the user supplied it */
		if (!pc2) /* no INTO */
		{
			if (prule->topRule->parisdefault)
			{
				defparname = pstrdup(prule->topRule->parname);
				into_exists = 2;
			}
		}
		else /* has INTO clause */
		{
			bool isdef = false;
			bool exists = false;
			char *parname = NULL;

			/*
			 * If we're working on a subpartition, the INTO partition is
			 * actually a child partition of the parent identified by
			 * prule. So, we cannot just use get_part_rule() to determine
			 * if one of them is a default.
			 */

			/* first item */
			if (pid->idtype == AT_AP_IDRule)
			{
				if (prule->pNode->default_part &&
					((AlterPartitionId *)pc2->partid)->idtype == AT_AP_IDDefault)
				{
					isdef = true;
					exists = true;
					parname = prule->pNode->default_part->parname;
					if (!defparname && isdef) 
						defparname = pstrdup(parname);
				}
				else
				{
					ListCell *rc;
					AlterPartitionId *id = (AlterPartitionId *)pc2->partid;

					if (id->idtype == AT_AP_IDDefault)
						ereport(ERROR,
								(errcode(ERRCODE_UNDEFINED_OBJECT),
								 errmsg("relation \"%s\" does not have a "
										"default partition",
										RelationGetRelationName(*rel))));

					foreach(rc, prule->pNode->rules)
					{
						PartitionRule *r = lfirst(rc);

						if (strcmp(r->parname, strVal((Value *)id->partiddef)) == 0)
						{
							isdef = false;
							exists = true;
							parname = r->parname;
						}
					}

					if (prule->pNode->default_part &&
						strcmp(prule->pNode->default_part->parname,
							   strVal((Value *)id->partiddef)) == 0)
					{
						isdef = true;
						exists = true;
						parname = prule->pNode->default_part->parname;
						if (!defparname && isdef) 
							defparname = pstrdup(parname);
					}
				}
			}
			else /* not a AT_AP_IDRule */
			{
				PgPartRule *tmprule;
				tmprule = get_part_rule(*rel, (AlterPartitionId *)pc2->partid,
										false, false, NULL, false);
				if (tmprule)
				{
					isdef = tmprule->topRule->parisdefault;
					exists = true;
					parname = tmprule->topRule->parname;
					if (!defparname && isdef) 
						defparname = pstrdup(parname);
				}
			}

			if (exists && isdef)
			{
				default_pos = 1;
				intopid2 = (AlterPartitionId *)pc2->partid;
				intopid1 = (AlterPartitionId *)pc2->arg1;
				into_exists = 2;

				if (intopid2->idtype == AT_AP_IDDefault)
					 intopid2->partiddef = (Node *)makeString(pstrdup(parname));
			}
			else
			{
				if (exists)
					into_exists = 1;

				intopid1 = (AlterPartitionId *)pc2->partid;
				intopid2 = (AlterPartitionId *)pc2->arg1;
			}

			/* second item */
			exists = false;
			isdef = false;
			parname = NULL;
			if (pid->idtype == AT_AP_IDRule)
			{
				if (prule->pNode->default_part &&
					((AlterPartitionId *)pc2->arg1)->idtype == AT_AP_IDDefault)
				{
					isdef = true;
					exists = true;
					parname = prule->pNode->default_part->parname;
					if (!defparname && isdef) 
						defparname = pstrdup(parname);
				}
				else
				{
					ListCell *rc;
					AlterPartitionId *id = (AlterPartitionId *)pc2->arg1;

					if (id->idtype == AT_AP_IDDefault)
						ereport(ERROR,
								(errcode(ERRCODE_UNDEFINED_OBJECT),
								 errmsg("relation \"%s\" does not have a "
										"default partition",
										RelationGetRelationName(*rel))));

					foreach(rc, prule->pNode->rules)
					{
						PartitionRule *r = lfirst(rc);

						if (strcmp(r->parname, strVal((Value *)id->partiddef)) == 0)
						{
							isdef = false;
							exists = true;
							parname = r->parname;
						}
					}

					if (prule->pNode->default_part &&
						strcmp(prule->pNode->default_part->parname,
							   strVal((Value *)id->partiddef)) == 0)
					{
						isdef = true;
						exists = true;
						parname = prule->pNode->default_part->parname;
						if (!defparname && isdef) 
							defparname = pstrdup(parname);
					}

				}
			}
			else
			{
				PgPartRule *tmprule;
				tmprule = get_part_rule(*rel, (AlterPartitionId *)pc2->arg1,
										false, false, NULL, false);
				if (tmprule)
				{
					isdef = tmprule->topRule->parisdefault;
					exists = true;
					parname = tmprule->topRule->parname;
					if (!defparname && isdef) 
						defparname = pstrdup(parname);
				}
			}

			if (exists)
			{
				if (into_exists != 0)
					ereport(ERROR,
							(errcode(ERRCODE_DUPLICATE_OBJECT),
							 errmsg("both INTO partitions "
									"already exist")));

				into_exists = 2;
				intopid1 = (AlterPartitionId *)pc2->partid;
				intopid2 = (AlterPartitionId *)pc2->arg1;

				if (isdef)
				{
					default_pos = 2;

					if (intopid2->idtype == AT_AP_IDDefault)
						 intopid2->partiddef = (Node *)makeString(parname);
				}
			}
		}

		existrel = heap_open(prule->topRule->parchildrelid, NoLock);
		existstorage_opts = reloptions_list(RelationGetRelid(existrel));
		distro = make_dist_clause(existrel);
		colencs = rel_get_column_encodings(existrel);
		orient = make_orientation_options(existrel);

		heap_close(existrel, NoLock);

		/* 1) Create temp table */
		rv = makeRangeVar(nspname, relname, -1);
		inh->relation = copyObject(rv);
        inh->options = list_make3_int(CREATE_TABLE_LIKE_INCLUDING_DEFAULTS,
									  CREATE_TABLE_LIKE_INCLUDING_CONSTRAINTS,
									  CREATE_TABLE_LIKE_INCLUDING_INDEXES);
		ct->tableElts = list_make1(inh);
		ct->distributedBy = list_copy(distro); /* must preserve the list for later */

		/* should be unique enough */
		snprintf(tmpname, NAMEDATALEN, "pg_temp_%u", relid);
		tmprv = makeRangeVar(nspname, tmpname, -1);
		ct->relation = tmprv;
		ct->relKind = RELKIND_RELATION;
		ct->ownerid = (*rel)->rd_rel->relowner;
		ct->is_split_part = true;
		/* No transformation happens for this stmt in parse_analyze() */
		q = parse_analyze((Node *)ct, NULL, NULL, 0);
		ProcessUtility((Node *)q->utilityStmt,
					   synthetic_sql,
					   NULL,
					   false, /* not top level */
					   dest,
					   NULL);
		CommandCounterIncrement();

		/* get the oid of the temporary table */
		temprelid = get_relname_relid(tmpname,
									  RelationGetNamespace(*rel));

		if (pid->idtype == AT_AP_IDRule)
		{
			idpid = copyObject(pid);
		}
		else
		{
			idpid = makeNode(AlterPartitionId);
			idpid->idtype = AT_AP_IDRule;
			idpid->partiddef = (Node *)list_make2((Node *)prule, pid);
			idpid->location  = -1;
		}

		/* 2) EXCHANGE temp with target */
		rel_to_drop = prule->topRule->parchildrelid;

		ats->relation = copyObject(rv);
		ats->relkind = OBJECT_TABLE;

		cmd->subtype = AT_PartExchange;

		mypc->partid = (Node *)idpid;
		mypc->arg1 = (Node *)tmprv;
		mypc->arg2 = (Node *)mypc2;

		mypc2->arg1 = (Node *)makeInteger(0);
		mypc2->arg2 = (Node *)makeInteger(1); /* tell them we're SPLIT */
		mypc2->location = -1;
		cmd->def = (Node *)mypc;
		ats->cmds = list_make1(cmd);
		/* No transformation happens for this stmt in parse_analyze() */
		q = parse_analyze((Node *)ats, NULL, NULL, 0);

		heap_close(*rel, NoLock);
		ProcessUtility((Node *)q->utilityStmt,
					   synthetic_sql,
					   NULL,
					   false, /* not top level */
					   dest,
					   NULL);
		*rel = heap_open(relid, AccessExclusiveLock);
		CommandCounterIncrement();

		/* 3) drop the old partition */
		if (pid->idtype == AT_AP_IDRule)
		{
			PgPartRule *pr;
			part_rule_cxt cxt;

			idpid = copyObject(pid);


			/* need to update the OID reference */
			pr = linitial((List *)idpid->partiddef);
			cxt.old_oid = rel_to_drop;
			cxt.new_oid = temprelid;
			partrule_walker((Node *)pr, (void *)&cxt);
			elog(DEBUG5, "dropping OID %u", temprelid);
		}
		else
		{
			/* refresh prule, out of date due to EXCHANGE */
			prule = get_part_rule(*rel, pid, true, true, NULL, false);
			idpid = makeNode(AlterPartitionId);
			idpid->idtype = AT_AP_IDRule;
			idpid->partiddef = (Node *)list_make2((Node *)prule, pid);
			idpid->location  = -1;
		}

		cmd->subtype = AT_PartDrop;

		ds->missing_ok = false;
		ds->behavior = DROP_RESTRICT;
		ds->removeType = OBJECT_TABLE;

		mypc->partid = (Node *)idpid;
		mypc->arg1 = (Node *)ds;
		/* MPP-6589: make DROP work, even if last one
		 * NOTE: hateful hackery.  Normally, the arg2 for the PartDrop
		 * cmd is null.  But since SPLIT may need to DROP the last
		 * partition before it re-ADDs two new ones, we pass a non-null
		 * arg2 as a flag to enable ForceDrop in ATPExecPartDrop().  
		*/
		mypc->arg2 = (Node *)makeNode(AlterPartitionCmd);

		cmd->def = (Node *)mypc;
		/* No transformation happens for this stmt in parse_analyze() */
		q = parse_analyze((Node *)ats, NULL, NULL, 0);
		heap_close(*rel, NoLock);

		ProcessUtility((Node *)q->utilityStmt,
					   synthetic_sql,
					   NULL,
					   false, /* not top level */
					   dest,
					   NULL);

		*rel = heap_open(relid, AccessExclusiveLock);
		CommandCounterIncrement();

		/*
		 * Now that we've dropped the partition, we need to handle updating
		 * the PgPartRule pid for the case where it is a pid representing
		 * ALTER PARTITION ... ALTER PARTITION ...
		 *
		 * For the normal case, we'll just run get_part_rule() again deeper
		 * in the code.
		 */
		if (pid->idtype == AT_AP_IDRule)
		{
			AlterPartitionId *tmppid = copyObject(pid);

			/* MPP-10223: pid contains a "stale" pNode with a
			 * partition rule for the partition we just dropped.
			 * Delve deep into pNode to eliminate the topRule.
			 */
			if (1)
			{
				AlterPartitionId		*newpid	 = tmppid;
				PgPartRule				*tmprule = NULL;

				while (tmppid->idtype == AT_AP_IDRule)
				{
					List *l = (List *)tmppid->partiddef;

					/* wipe out the topRule because the partition was
					 * dropped, or PartAdd will find it and complain!!
					 * Note that because the pid was copied,
					 * tmprule->topRule has a separate copy of the
					 * prule subtree, so we need to fix this one, too.
					 */
					tmprule = (PgPartRule *)linitial(l);

					atpxSplitDropRule(tmprule->pNode, 
									  tmprule->topRule,
									  prule->topRule->parruleid, 
									  prule->topRule->paroid);
					tmppid = lsecond(l);
				}
				
				tmppid = newpid;
			}

			aapid = tmppid;
		}

		/* Add two new partitions */
		elog(DEBUG5, "Split partition: adding two new partitions");

		/* 4) add two new partitions, via a loop to reduce code duplication */
		for (i = 1; i <= 2; i++)
		{
			/* build up commands for adding two. */
			AlterPartitionId *mypid = makeNode(AlterPartitionId);
			char *parname = NULL;
			AlterPartitionId *intopid = NULL;
			PartitionElem *pelem = makeNode(PartitionElem);
			Oid newchildrelid = InvalidOid;
			AlterPartitionCmd *storenode = NULL;

			/* use storage options for existing rel */
			if (existstorage_opts)
			{
				storenode = makeNode(AlterPartitionCmd);
				storenode->arg1 = (Node *)existstorage_opts;
				storenode->location = -1;
			}
			pelem->storeAttr = (Node *)storenode;

			if (pc2)
			{
				if (i == 1)
					intopid = intopid1;
				else
					intopid = intopid2;
			}
			else
			{
				if (into_exists == i && prule->topRule->parname)
					parname = pstrdup(prule->topRule->parname);
			}

			mypid->idtype = AT_AP_IDNone;
			mypid->location = -1;
			mypid->partiddef = NULL;

			mypc->partid = (Node *)mypid;

			if (intopid)
				parname = strVal(intopid->partiddef);

			if (prule->topRule->parisdefault && i == into_exists)
			{
				/* nothing to do */
			}
			else
			{
				if (prule->pNode->part->parkind == 'r')
				{
					PartitionBoundSpec *a = makeNode(PartitionBoundSpec);

					if (prule->topRule->parisdefault)
					{
						a->partStart = linitial((List *)pc->arg1);
						a->partEnd = lsecond((List *)pc->arg1);
					}
					else if (i == 1)
					{
						PartitionRangeItem *ri;

						/* MPP-6589: if the partition has an "open"
						 * START, pass a NULL partStart 
						 */
						if (prule->topRule &&
							prule->topRule->parrangestart)
						{
							ri = makeNode(PartitionRangeItem);
							ri->location = -1;
							ri->everycount = 0;
							ri->partRangeVal = 
									copyObject(prule->topRule->parrangestart);
							ri->partedge =
									prule->topRule->parrangestartincl ? 
									PART_EDGE_INCLUSIVE : PART_EDGE_EXCLUSIVE;

							a->partStart = (Node *)ri;
						}
						else
							a->partStart = NULL;

						ri = makeNode(PartitionRangeItem);
						ri->location = -1;
						ri->everycount = 0;
						ri->partRangeVal = copyObject(at);
						ri->partedge = PART_EDGE_EXCLUSIVE;

						a->partEnd = (Node *)ri;
					}
					else if (i == 2)
					{
						PartitionRangeItem *ri;
						ri = makeNode(PartitionRangeItem);
						ri->location = -1;
						ri->everycount = 0;
						ri->partRangeVal = copyObject(at);
						ri->partedge = PART_EDGE_INCLUSIVE;

						a->partStart = (Node *)ri;

						/* MPP-6589: if the partition has an "open"
						 * END, pass a NULL partEnd 
						 */
						if (prule->topRule &&
							prule->topRule->parrangeend)
						{
							ri = makeNode(PartitionRangeItem);
							ri->location = -1;
							ri->everycount = 0;
							ri->partRangeVal =
									copyObject(prule->topRule->parrangeend);
							ri->partedge = prule->topRule->parrangeendincl ? 
									PART_EDGE_INCLUSIVE : PART_EDGE_EXCLUSIVE;

							a->partEnd = (Node *)ri;
						}
						else
							a->partEnd = NULL;

					}
					pelem->boundSpec = (Node *)a;
				}
				else
				{
					if ((into_exists && into_exists != i &&
						 prule->topRule->parisdefault) ||
						(i == 2 && !prule->topRule->parisdefault))
					{
						PartitionValuesSpec *valuesspec =
							makeNode(PartitionValuesSpec);

						valuesspec->partValues = copyObject(at);
						valuesspec->location = -1;
						pelem->boundSpec = (Node *)valuesspec;
					}
					else
					{
						PartitionValuesSpec *valuesspec =
							makeNode(PartitionValuesSpec);
						List *lv = prule->topRule->parlistvalues;
						List *newvals = NIL;
						ListCell *lc;
						List *atlist = (List *)at;

						/* must be LIST */
						Assert(prule->pNode->part->parkind == 'l');

						/*
						 * Iterate through list of existing constraints and
						 * pick out those not in AT() clause
						 */
						foreach(lc, lv)
						{
							List *cols = lfirst(lc);
							ListCell *parvals = list_head(atlist);
							int16 nkeys = prule->pNode->part->parnatts;
							bool found = false;

							while (parvals)
							{
								/*
								 * In multilevel partitioning with
								 * lowest level as list, we don't find
								 * AlterPartitionCmd->arg1 to be what
								 * we expect.  We expect it as a list
								 * of lists of A_Const, where the
								 * A_Const is the value in AT()
								 * clause.  Instead, what we get as
								 * arg1 is a list whose member is
								 * A_Const.  In order to prevent a
								 * segfault and PANIC down the line,
								 * we abort here.  It should be
								 * removed after the issue is fixed.
								 */
								if (!IsA(lfirst(parvals), List))
								{
									ereport(ERROR,
											(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
											 errmsg("split partition is not "
													"currently supported when the "
													"lowest level is list partitioned")));
								}
								List *vals = lfirst(parvals);
								ListCell *lcv = list_head(vals);
								ListCell *lcc = list_head(cols);
								int parcol;
								bool matched = true;

								for (parcol = 0; parcol < nkeys; parcol++)
								{
									Oid			opclass;
									Oid			opfam;
									Oid			intype;
									Oid			funcid;
									Const *v = lfirst(lcv);
									Const *c = lfirst(lcc);
									Datum d;

									opclass =  prule->pNode->part->parclass[parcol];
									intype = get_opclass_input_type(opclass);
									opfam = get_opclass_family(opclass);
									funcid = get_opfamily_proc(opfam, intype, intype,
															   BTORDER_PROC);

									if (v->constisnull && c->constisnull)
										continue;
									else if (v->constisnull || c->constisnull)
									{
										matched = false;
										break;
									}

									d = OidFunctionCall2(funcid, c->constvalue,
														 v->constvalue);

									if (DatumGetInt32(d) != 0)
									{
										matched = false;
										break;
									}

									lcv = lnext(lcv);
									lcc = lnext(lcc);
								}

								if (matched)
								{
									found = true;
									break;
								}

								parvals = lnext(parvals);
							}
							if (!found)
							{
								/* Not in AT() clause, so keep it */
								newvals = lappend(newvals,
										copyObject((Node *)cols));
							}
						}
						valuesspec->partValues = newvals;
						valuesspec->location = -1;
						pelem->boundSpec = (Node *)valuesspec;
					}
				}
			}

			/*
			 * We always want to create a partition name because we
			 * need to recall the partition details below.
			 */
			if (!parname)
			{
				char n[NAMEDATALEN];

				snprintf(n, NAMEDATALEN, "r%lu", random());
				parname = pstrdup(n);

			}

			pelem->partName = parname;
			mypid->partiddef = (Node *)makeString(parname);
			mypid->idtype = AT_AP_IDName;

			pelem->location  = -1;
			/* MPP-10421: determine if new partition, re-use of old
			 * partition, and/or is default partition 
			 */
			pelem->isDefault = (defparname && 
								parname &&
								(0 == strcmp(parname, defparname)));

			mypc->arg1 = (Node *) pelem;
			/*
			 * Pass the set of column encodings to the ADD PARTITION command
			 * (if any).
			 */
			mypc->arg2 = (Node *) colencs;
			mypc->location = -1;

			cmd->subtype = AT_PartAddForSplit;
			cmd->def = (Node *) mypc;

			/* turn this into ALTER PARTITION if need be */
			if (pid->idtype == AT_AP_IDRule)
			{
				AlterTableCmd *ac = makeNode(AlterTableCmd);
				AlterPartitionCmd *ap = makeNode(AlterPartitionCmd);

				ac->subtype = AT_PartAlter;
				ac->def = (Node *)ap;
				ap->partid = (Node *)aapid;
				ap->arg1 = (Node *)cmd; /* embed the real command */
				ap->location = -1;

				cmd = ac;
			}

			ats->cmds = list_make1(cmd);
			/* No transformation happens for this stmt in parse_analyze() */
			q = parse_analyze((Node *)ats, NULL, NULL, 0);

			heap_close(*rel, NoLock);
			ProcessUtility((Node *)q->utilityStmt,
						   synthetic_sql,
						   NULL,
						   false, /* not top level */
						   dest,
						   NULL);
			*rel = heap_open(relid, AccessExclusiveLock);

			/* make our change visible */
			CommandCounterIncrement();

			/* get the rule back which ADD PARTITION just created */
			if (pid->idtype == AT_AP_IDRule)
			{
				ScanKeyData scankey[3];
				SysScanDesc scan;
				Relation	rulerel;
				HeapTuple	tuple;

				rulerel = heap_open(PartitionRuleRelationId, AccessShareLock);

				/*
				 * SELECT parchildrelid FROM pg_partition_rule
				 * WHERE paroid = :1
				 * AND parparentrule = :2
				 * AND parname = :3
				 */
				ScanKeyInit(&scankey[0], Anum_pg_partition_rule_paroid,
							BTEqualStrategyNumber, F_OIDEQ,
							ObjectIdGetDatum(prule->topRule->paroid));
				ScanKeyInit(&scankey[1], Anum_pg_partition_rule_parparentrule,
							BTEqualStrategyNumber, F_OIDEQ,
							ObjectIdGetDatum(prule->topRule->parparentoid));
				ScanKeyInit(&scankey[2], Anum_pg_partition_rule_parname,
							BTEqualStrategyNumber, F_NAMEEQ,
							CStringGetDatum(pelem->partName));

				/* XXX XXX: SnapshotSelf - but we just did a
				 * CommandCounterIncrement()
				 *
				 * XXX: No suitable index
				 */
				scan = systable_beginscan(rulerel, InvalidOid, false,
										  SnapshotSelf, 3, scankey);

				tuple = systable_getnext(scan);
				Insist(tuple);
				newchildrelid = ((Form_pg_partition_rule) GETSTRUCT(tuple))->parchildrelid;

				systable_endscan(scan);

				heap_close(rulerel, NoLock);
			}
			else
			{
				PgPartRule *tmprule;
				tmprule = get_part_rule(*rel, mypid, true, true, NULL, false);
				newchildrelid = tmprule->topRule->parchildrelid;
			}

			Assert(OidIsValid(newchildrelid));

			if (i == 1)
			{
				intoa = heap_open(newchildrelid,
								  AccessExclusiveLock);

				rva = makeRangeVar(
							get_namespace_name(RelationGetNamespace(intoa)),
						    get_rel_name(RelationGetRelid(intoa)), -1);
			}
			else
			{
				intob = heap_open(newchildrelid,
								  AccessExclusiveLock);

				rvb = makeRangeVar(
							get_namespace_name(RelationGetNamespace(intob)),
						    get_rel_name(RelationGetRelid(intob)), -1);

			}
		}

		temprel = heap_open(rel_to_drop, AccessExclusiveLock);

		/* update parse tree with info for the QEs */
		Assert(PointerIsValid(rva));
		Assert(PointerIsValid(rvb));

		/* update for consumption by QEs */
		pc->partid = (Node *)makeInteger(RelationGetRelid(temprel));
		pc->arg1 = (Node *)copyObject(rva);
		pc->arg2 = (Node *)copyObject(rvb);

		/* MPP-6929: metadata tracking */
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(*rel),
						   GetUserId(),
						   "PARTITION", "SPLIT"
				);

	} /* end if dispatch */
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		Oid temprelid;
		RangeVar *rva;
		RangeVar *rvb;

		temprelid = (Oid)intVal((Value *)pc->partid);
		temprel = heap_open(temprelid, AccessExclusiveLock);
		rva = (RangeVar *)pc->arg1;
		intoa = heap_openrv(rva, AccessExclusiveLock);
		rvb = (RangeVar *)pc->arg2;
		intob = heap_openrv(rvb, AccessExclusiveLock);
	}
	else
		return;

	/*
	 * Now, on every node, scan the exchanged out table, splitting data
	 * between two new partitions.
	 *
	 * To do this, we use the CHECK constraints we just created on the
	 * tables via ADD PARTITION.
	 */
	split_rows(intoa, intob, temprel);

	elog(DEBUG5, "dropping temp rel %s", RelationGetRelationName(temprel));
	temprelid = RelationGetRelid(temprel);
	heap_close(temprel, NoLock);

	/* drop temp table */
	{
		ObjectAddress addr;
		addr.classId = RelationRelationId;
		addr.objectId = temprelid;
		addr.objectSubId = 0;

		performDeletion(&addr, DROP_RESTRICT);
	}

	heap_close(intoa, NoLock);
	heap_close(intob, NoLock);
}

/* ALTER TABLE ... TRUNCATE PARTITION */
static List *
atpxTruncateList(Relation rel, PartitionNode *pNode)
{
	List *l1 = NIL;
	ListCell *lc;

	if (!pNode)
		return l1;

	/* add the child lists first */
	foreach(lc, pNode->rules)
	{
		PartitionRule *rule = lfirst(lc);
		List *l2 = NIL;

		if (rule->children)
			l2 = atpxTruncateList(rel, rule->children);
		else
			l2 = NIL;

		if (l2)
		{
			if (l1)
				l1 = list_concat(l1, l2);
			else
				l1 = l2;
		}
	}

	/* and the default partition */
	if (pNode->default_part)
	{
		PartitionRule *rule = pNode->default_part;
		List *l2 = NIL;

		if (rule->children)
			l2 = atpxTruncateList(rel, rule->children);
		else
			l2 = NIL;

		if (l2)
		{
			if (l1)
				l1 = list_concat(l1, l2);
			else
				l1 = l2;
		}
	}

	/* add entries for rules at current level */
	foreach(lc, pNode->rules)
	{
		PartitionRule 	*rule = lfirst(lc);
		RangeVar 		*rv;
		Relation		 rel;

		rel = heap_open(rule->parchildrelid, AccessShareLock);

		rv = makeRangeVar(get_namespace_name(RelationGetNamespace(rel)),
						  pstrdup(RelationGetRelationName(rel)), -1);

		heap_close(rel, NoLock);

		if (l1)
			l1 = lappend(l1, rv);
		else
			l1 = list_make1(rv);
	}

	/* and the default partition */
	if (pNode->default_part)
	{
		PartitionRule 	*rule = pNode->default_part;
		RangeVar 		*rv;
		Relation		 rel;

		rel = heap_open(rule->parchildrelid, AccessShareLock);
		rv = makeRangeVar(get_namespace_name(RelationGetNamespace(rel)),
						  pstrdup(RelationGetRelationName(rel)), -1);

		heap_close(rel, NoLock);

		if (l1)
			l1 = lappend(l1, rv);
		else
			l1 = list_make1(rv);
	}

	return l1;
} /* end atpxTruncateList */

static void
ATPExecPartTruncate(Relation rel,
                    AlterPartitionCmd *pc)
{
	AlterPartitionId *pid = (AlterPartitionId *)pc->partid;
	PgPartRule   *prule = NULL;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	prule = get_part_rule(rel, pid, true, true, NULL, false);

	if (prule)
	{
		RangeVar 		*rv;
		TruncateStmt 	*ts   = (TruncateStmt *)pc->arg1;
		DestReceiver 	*dest = None_Receiver;
		Relation	  	 rel2;

		rel2 = heap_open(prule->topRule->parchildrelid, AccessShareLock);
		if (RelationIsExternal(rel2))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot truncate external partition")));

		rv = makeRangeVar(get_namespace_name(RelationGetNamespace(rel2)),
						  pstrdup(RelationGetRelationName(rel2)), -1);

		rv->location = pc->location;

		if (prule->topRule->children)
		{
			List *l1 = atpxTruncateList(rel2, prule->topRule->children);

			ts->relations = lappend(l1, rv);
		}
		else
			ts->relations = list_make1(rv);

		heap_close(rel2, NoLock);

		ProcessUtility( (Node *) ts,
					   synthetic_sql,
					   NULL,
					   false, /* not top level */
					   dest,
					   NULL);

		/* Notify of name if did not use name for partition id spec */
		if (prule && prule->topRule && prule->topRule->children
			&& (ts->behavior != DROP_CASCADE ))
		{
			ereport(NOTICE,
					(errmsg("truncated partition%s for %s and its children",
							prule->partIdStr,
							prule->relname)));
		}
		else if ((pid->idtype != AT_AP_IDName)
				 && prule->isName)
				ereport(NOTICE,
						(errmsg("truncated partition%s for %s",
								prule->partIdStr,
								prule->relname)));
	}

} /* end ATPExecPartTruncate */

/*
 * Execute ALTER TABLE SET SCHEMA
 *
 * Note: caller must have checked ownership of the relation already
 */
void
AlterTableNamespace(RangeVar *relation, const char *newschema)
{
	Relation	rel;
	Oid			relid;
	Oid			oldNspOid;
	Oid			nspOid;
	ObjectAddresses *objsMoved;

	rel = relation_openrv(relation, AccessExclusiveLock);

	relid = RelationGetRelid(rel);
	oldNspOid = RelationGetNamespace(rel);

	/* Can we change the schema of this tuple? */
	switch (rel->rd_rel->relkind)
	{
		case RELKIND_RELATION:
		case RELKIND_VIEW:
			/* ok to change schema */
			break;
		case RELKIND_SEQUENCE:
			{
				/* if it's an owned sequence, disallow moving it by itself */
				Oid			tableId;
				int32		colId;

				if (sequenceIsOwned(relid, &tableId, &colId))
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot move an owned sequence into another schema"),
					  errdetail("Sequence \"%s\" is linked to table \"%s\".",
								RelationGetRelationName(rel),
								get_rel_name(tableId))));
			}
			break;
		case RELKIND_COMPOSITE_TYPE:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is a composite type",
							RelationGetRelationName(rel)),
					 errhint("Use ALTER TYPE instead.")));
			break;
		case RELKIND_INDEX:
		case RELKIND_TOASTVALUE:
		case RELKIND_AOSEGMENTS:
		case RELKIND_AOBLOCKDIR:
		case RELKIND_AOVISIMAP:
			/* FALL THRU */
		default:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a table, view, or sequence",
							RelationGetRelationName(rel))));
	}

	/* get schema OID and check its permissions */
	nspOid = LookupCreationNamespace(newschema);

	/* common checks on switching namespaces */
	CheckSetNamespace(oldNspOid, nspOid, RelationRelationId, relid);

	/* OK, modify the pg_class row and pg_depend entry */
	objsMoved = new_object_addresses();
	AlterTableNamespaceInternal(rel, oldNspOid, nspOid, objsMoved);
	free_object_addresses(objsMoved);

	/* MPP-7825, MPP-6929, MPP-7600: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "SET SCHEMA"
				);

	/* close rel, but keep lock until commit */
	relation_close(rel, NoLock);
}

/*
 * The guts of relocating a table to another namespace: besides moving
 * the table itself, its dependent objects are relocated to the new schema.
 */
void
AlterTableNamespaceInternal(Relation rel, Oid oldNspOid, Oid nspOid,
							ObjectAddresses *objsMoved)
{
	Relation        classRel;

	Assert(objsMoved != NULL);

	/* OK, modify the pg_class row and pg_depend entry */
	classRel = heap_open(RelationRelationId, RowExclusiveLock);

	AlterRelationNamespaceInternal(classRel, RelationGetRelid(rel), oldNspOid,
								   nspOid, true, objsMoved);

	/* Fix the table's row type too */
	AlterTypeNamespaceInternal(rel->rd_rel->reltype,
							   nspOid, false, false, objsMoved);

	/* Fix other dependent stuff */
	if (rel->rd_rel->relkind == RELKIND_RELATION)
	{
		AlterIndexNamespaces(classRel, rel, oldNspOid, nspOid, objsMoved);
		AlterSeqNamespaces(classRel, rel, oldNspOid, nspOid,
						   objsMoved, AccessExclusiveLock);
		AlterConstraintNamespaces(RelationGetRelid(rel), oldNspOid, nspOid,
								  false, objsMoved);
	}

	heap_close(classRel, RowExclusiveLock);
}

/*
 * The guts of relocating a relation to another namespace: fix the pg_class
 * entry, and the pg_depend entry if any.  Caller must already have
 * opened and write-locked pg_class.
 */
void
AlterRelationNamespaceInternal(Relation classRel, Oid relOid,
							   Oid oldNspOid, Oid newNspOid,
							   bool hasDependEntry, ObjectAddresses *objsMoved)
{
	HeapTuple	classTup;
	Form_pg_class classForm;
	ObjectAddress	thisobj;

	classTup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relOid));
	if (!HeapTupleIsValid(classTup))
		elog(ERROR, "cache lookup failed for relation %u", relOid);
	classForm = (Form_pg_class) GETSTRUCT(classTup);

	Assert(classForm->relnamespace == oldNspOid);

	thisobj.classId = RelationRelationId;
	thisobj.objectId = relOid;
	thisobj.objectSubId = 0;

	/*
	 * Do nothing when there's nothing to do.
	 */
	if (!object_address_present(&thisobj, objsMoved))
	{
		/* check for duplicate name (more friendly than unique-index failure) */
		if (get_relname_relid(NameStr(classForm->relname),
							  newNspOid) != InvalidOid)
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_TABLE),
					 errmsg("relation \"%s\" already exists in schema \"%s\"",
							NameStr(classForm->relname),
							get_namespace_name(newNspOid))));

		/* classTup is a copy, so OK to scribble on */
		classForm->relnamespace = newNspOid;

		simple_heap_update(classRel, &classTup->t_self, classTup);
		CatalogUpdateIndexes(classRel, classTup);

		/* Update dependency on schema if caller said so */
		if (hasDependEntry &&
			changeDependencyFor(RelationRelationId, relOid,
								NamespaceRelationId, oldNspOid, newNspOid) != 1)
			elog(ERROR, "failed to change schema dependency for relation \"%s\"",
				 NameStr(classForm->relname));

		add_exact_object_address(&thisobj, objsMoved);
	}

	heap_freetuple(classTup);
}

/*
 * Move all indexes for the specified relation to another namespace.
 *
 * Note: we assume adequate permission checking was done by the caller,
 * and that the caller has a suitable lock on the owning relation.
 */
static void
AlterIndexNamespaces(Relation classRel, Relation rel,
					 Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved)
{
	List	   *indexList;
	ListCell   *l;

	indexList = RelationGetIndexList(rel);

	foreach(l, indexList)
	{
		Oid			indexOid = lfirst_oid(l);
		ObjectAddress thisobj;

		thisobj.classId = RelationRelationId;
		thisobj.objectId = indexOid;
		thisobj.objectSubId = 0;

		/*
		 * Note: currently, the index will not have its own dependency on the
		 * namespace, so we don't need to do changeDependencyFor(). There's no
		 * rowtype in pg_type, either.
		 *
		 * XXX this objsMoved test may be pointless -- surely we have a single
		 * dependency link from a relation to each index?
		 */
		if (!object_address_present(&thisobj, objsMoved))
		{
			AlterRelationNamespaceInternal(classRel, indexOid,
										   oldNspOid, newNspOid,
										   false, objsMoved);
			add_exact_object_address(&thisobj, objsMoved);
		}
	}

	list_free(indexList);
}

/*
 * Move all SERIAL-column sequences of the specified relation to another
 * namespace.
 *
 * Note: we assume adequate permission checking was done by the caller,
 * and that the caller has a suitable lock on the owning relation.
 */
static void
AlterSeqNamespaces(Relation classRel, Relation rel,
				   Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved,
				   LOCKMODE lockmode)
{
	Relation	depRel;
	SysScanDesc scan;
	ScanKeyData key[2];
	HeapTuple	tup;

	/*
	 * SERIAL sequences are those having an auto dependency on one of the
	 * table's columns (we don't care *which* column, exactly).
	 */
	depRel = heap_open(DependRelationId, AccessShareLock);

	ScanKeyInit(&key[0],
				Anum_pg_depend_refclassid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationRelationId));
	ScanKeyInit(&key[1],
				Anum_pg_depend_refobjid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	/* we leave refobjsubid unspecified */

	scan = systable_beginscan(depRel, DependReferenceIndexId, true,
							  SnapshotNow, 2, key);

	while (HeapTupleIsValid(tup = systable_getnext(scan)))
	{
		Form_pg_depend depForm = (Form_pg_depend) GETSTRUCT(tup);
		Relation	seqRel;

		/* skip dependencies other than auto dependencies on columns */
		if (depForm->refobjsubid == 0 ||
			depForm->classid != RelationRelationId ||
			depForm->objsubid != 0 ||
			depForm->deptype != DEPENDENCY_AUTO)
			continue;

		/* Use relation_open just in case it's an index */
		seqRel = relation_open(depForm->objid, lockmode);

		/* skip non-sequence relations */
		if (RelationGetForm(seqRel)->relkind != RELKIND_SEQUENCE)
		{
			/* No need to keep the lock */
			relation_close(seqRel, lockmode);
			continue;
		}

		/* Fix the pg_class and pg_depend entries */
		AlterRelationNamespaceInternal(classRel, depForm->objid,
									   oldNspOid, newNspOid,
									   true, objsMoved);

		/*
		 * Sequences have entries in pg_type. We need to be careful to move
		 * them to the new namespace, too.
		 */
		AlterTypeNamespaceInternal(RelationGetForm(seqRel)->reltype,
								   newNspOid, false, false, objsMoved);

		/* Now we can close it.  Keep the lock till end of transaction. */
		relation_close(seqRel, NoLock);
	}

	systable_endscan(scan);

	relation_close(depRel, AccessShareLock);
}


/*
 * This code supports
 *	CREATE TEMP TABLE ... ON COMMIT { DROP | PRESERVE ROWS | DELETE ROWS }
 *
 * Because we only support this for TEMP tables, it's sufficient to remember
 * the state in a backend-local data structure.
 */

/*
 * Register a newly-created relation's ON COMMIT action.
 */
void
register_on_commit_action(Oid relid, OnCommitAction action)
{
	OnCommitItem *oc;
	MemoryContext oldcxt;

	/*
	 * We needn't bother registering the relation unless there is an ON COMMIT
	 * action we need to take.
	 */
	if (action == ONCOMMIT_NOOP || action == ONCOMMIT_PRESERVE_ROWS)
		return;

	oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

	oc = (OnCommitItem *) palloc(sizeof(OnCommitItem));
	oc->relid = relid;
	oc->oncommit = action;
	oc->creating_subid = GetCurrentSubTransactionId();
	oc->deleting_subid = InvalidSubTransactionId;

	on_commits = lcons(oc, on_commits);

	MemoryContextSwitchTo(oldcxt);
}

/*
 * Unregister any ON COMMIT action when a relation is deleted.
 *
 * Actually, we only mark the OnCommitItem entry as to be deleted after commit.
 */
void
remove_on_commit_action(Oid relid)
{
	ListCell   *l;

	foreach(l, on_commits)
	{
		OnCommitItem *oc = (OnCommitItem *) lfirst(l);

		if (oc->relid == relid)
		{
			oc->deleting_subid = GetCurrentSubTransactionId();
			break;
		}
	}
}

/*
 * Perform ON COMMIT actions.
 *
 * This is invoked just before actually committing, since it's possible
 * to encounter errors.
 */
void
PreCommit_on_commit_actions(void)
{
	ListCell   *l;
	List	   *oids_to_truncate = NIL;

	/*
	 * We skip this operation in the catchup handler, especially
	 * between prepare and commit state, as we may not see the heap
	 * that has just been created in the prepared transaction that
	 * is not visible yet.  Skipping this under the catchup handler
	 * should be ok in known cases.
	 */
	if (in_process_catchup_event)
		return;

	foreach(l, on_commits)
	{
		OnCommitItem *oc = (OnCommitItem *) lfirst(l);

		/* Ignore entry if already dropped in this xact */
		if (oc->deleting_subid != InvalidSubTransactionId)
			continue;

		switch (oc->oncommit)
		{
			case ONCOMMIT_NOOP:
			case ONCOMMIT_PRESERVE_ROWS:
				/* Do nothing (there shouldn't be such entries, actually) */
				break;
			case ONCOMMIT_DELETE_ROWS:
				oids_to_truncate = lappend_oid(oids_to_truncate, oc->relid);
				break;
			case ONCOMMIT_DROP:
				{
					ObjectAddress object;

					object.classId = RelationRelationId;
					object.objectId = oc->relid;
					object.objectSubId = 0;
					performDeletion(&object, DROP_CASCADE);

					/*
					 * Note that table deletion will call
					 * remove_on_commit_action, so the entry should get marked
					 * as deleted.
					 */
					Assert(oc->deleting_subid != InvalidSubTransactionId);
					break;
				}
		}
	}
	if (oids_to_truncate != NIL)
	{
		heap_truncate(oids_to_truncate);
		CommandCounterIncrement();		/* XXX needed? */
	}
}

/*
 * Post-commit or post-abort cleanup for ON COMMIT management.
 *
 * All we do here is remove no-longer-needed OnCommitItem entries.
 *
 * During commit, remove entries that were deleted during this transaction;
 * during abort, remove those created during this transaction.
 */
void
AtEOXact_on_commit_actions(bool isCommit)
{
	ListCell   *cur_item;
	ListCell   *prev_item;

	prev_item = NULL;
	cur_item = list_head(on_commits);

	while (cur_item != NULL)
	{
		OnCommitItem *oc = (OnCommitItem *) lfirst(cur_item);

		if (isCommit ? oc->deleting_subid != InvalidSubTransactionId :
			oc->creating_subid != InvalidSubTransactionId)
		{
			/* cur_item must be removed */
			on_commits = list_delete_cell(on_commits, cur_item, prev_item);
			pfree(oc);
			if (prev_item)
				cur_item = lnext(prev_item);
			else
				cur_item = list_head(on_commits);
		}
		else
		{
			/* cur_item must be preserved */
			oc->creating_subid = InvalidSubTransactionId;
			oc->deleting_subid = InvalidSubTransactionId;
			prev_item = cur_item;
			cur_item = lnext(prev_item);
		}
	}
}

/*
 * Post-subcommit or post-subabort cleanup for ON COMMIT management.
 *
 * During subabort, we can immediately remove entries created during this
 * subtransaction.	During subcommit, just relabel entries marked during
 * this subtransaction as being the parent's responsibility.
 */
void
AtEOSubXact_on_commit_actions(bool isCommit, SubTransactionId mySubid,
							  SubTransactionId parentSubid)
{
	ListCell   *cur_item;
	ListCell   *prev_item;

	prev_item = NULL;
	cur_item = list_head(on_commits);

	while (cur_item != NULL)
	{
		OnCommitItem *oc = (OnCommitItem *) lfirst(cur_item);

		if (!isCommit && oc->creating_subid == mySubid)
		{
			/* cur_item must be removed */
			on_commits = list_delete_cell(on_commits, cur_item, prev_item);
			pfree(oc);
			if (prev_item)
				cur_item = lnext(prev_item);
			else
				cur_item = list_head(on_commits);
		}
		else
		{
			/* cur_item must be preserved */
			if (oc->creating_subid == mySubid)
				oc->creating_subid = parentSubid;
			if (oc->deleting_subid == mySubid)
				oc->deleting_subid = isCommit ? parentSubid : InvalidSubTransactionId;
			prev_item = cur_item;
			cur_item = lnext(prev_item);
		}
	}
}


/* ALTER TABLE EXCHANGE
 *
 * NB different signature from non-partitioned table Prep functions.
 */
static void
ATPrepExchange(Relation rel, AlterPartitionCmd *pc)
{
	PgPartRule   	   	*prule 	= NULL;
	Relation			 oldrel = NULL;
	Relation			 newrel = NULL;
	
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		AlterPartitionId *pid = (AlterPartitionId *) pc->partid;
		bool				is_split;
		
		is_split = ((AlterPartitionCmd *)pc->arg2)->arg2 != NULL;
		
		if (is_split)
			return;
		
		prule = get_part_rule(rel, pid, true, true, NULL, false);
		if (prule)
		{
			/* cannot exchange a hash partition */
			if ('h' == prule->pNode->part->parkind)
				ereport(ERROR,
						(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
						 errmsg("cannot exchange HASH partition%s "
								"of relation \"%s\"", prule->partIdStr,
								RelationGetRelationName(rel))));
		}
		oldrel = heap_open(prule->topRule->parchildrelid,
						   AccessExclusiveLock);
		newrel = heap_openrv((RangeVar *)pc->arg1,
							 AccessExclusiveLock);
	}
	else
	{
		oldrel = heap_openrv((RangeVar *)pc->partid,
							 AccessExclusiveLock);
		newrel = heap_openrv((RangeVar *)pc->arg1,
							 AccessExclusiveLock);
		
	}
	ATSimplePermissions(rel, false);
	ATSimplePermissions(oldrel, false);
	ATSimplePermissions(newrel, false);
	
	/* Check that old and new look the same, error if not. */
	is_exchangeable(rel, oldrel, newrel, true);
	
	heap_close(oldrel, NoLock);
	heap_close(newrel, NoLock);
}

/*
 * Return a palloc'd string representing an AlterTableCmd type for use
 * in a message pattern like "can't %s a partitioned table".  The default
 * return string is "alter".
 *
 * This may pose a challenge for localization.  
 */
static
char *alterTableCmdString(AlterTableType subtype)
{
	char *cmdstring = NULL;
	
	switch (subtype)
	{
		case AT_AddColumn: /* add column */
		case AT_AddColumnRecurse: /* internal to command/tablecmds.c*/
			cmdstring = pstrdup("add a column to");
			break;
			
		case AT_ColumnDefault: /* alter column default */
		case AT_ColumnDefaultRecurse:
			cmdstring = pstrdup("alter a column default of");
			break;
			
		case AT_DropNotNull: /* alter column drop not null */
		case AT_SetNotNull: /* alter column set not null */
			cmdstring = pstrdup("alter a column null setting of");
			break;
			
		case AT_SetStatistics: /* alter column statistics */
		case AT_SetStorage: /* alter column storage */
			break;
			
		case AT_DropColumn: /* drop column */
		case AT_DropColumnRecurse: /* internal to commands/tablecmds.c */
			cmdstring = pstrdup("drop a column from");
			break;
			
		case AT_AddIndex: /* add index */
		case AT_ReAddIndex: /* internal to commands/tablecmds.c */
			cmdstring = pstrdup("add index or primary/unique key to");
			break;
			
		case AT_AddConstraint: /* add constraint */
		case AT_AddConstraintRecurse: /* internal to commands/tablecmds.c */
			cmdstring = pstrdup("add a constraint to");
			break;
			
		case AT_ProcessedConstraint: /* pre-processed add constraint (local in parser/analyze.c) */
			break;
			
		case AT_DropConstraint: /* drop constraint */
			cmdstring = pstrdup("drop a constraint from");
			break;
			
		case AT_DropConstraintQuietly: /* drop constraint, no error/warning (local in commands/tablecmds.c) */
			break;
			
		case AT_AlterColumnType: /* alter column type */
			cmdstring = pstrdup("alter a column datatype of");
			break;
			
		case AT_ChangeOwner: /* change owner */
			cmdstring = pstrdup("alter the owner of");
			break;
			
		case AT_ClusterOn: /* CLUSTER ON */
		case AT_DropCluster: /* SET WITHOUT CLUSTER */
			break;
			
		case AT_DropOids: /* SET WITHOUT OIDS */
			cmdstring = pstrdup("alter the oid setting of");
			break;
			
		case AT_SetTableSpace: /* SET TABLESPACE */
		case AT_SetRelOptions: /* SET (...) -- AM specific parameters */
		case AT_ResetRelOptions: /* RESET (...) -- AM specific parameters */
			break;
			
		case AT_EnableTrig: /* ENABLE TRIGGER name */
		case AT_EnableAlwaysTrig: /* ENABLE ALWAYS TRIGGER name */
		case AT_EnableReplicaTrig: /* ENABLE REPLICA TRIGGER name */
		case AT_DisableTrig: /* DISABLE TRIGGER name */
		case AT_EnableTrigAll: /* ENABLE TRIGGER ALL */
		case AT_DisableTrigAll: /* DISABLE TRIGGER ALL */
		case AT_EnableTrigUser: /* ENABLE TRIGGER USER */
		case AT_DisableTrigUser: /* DISABLE TRIGGER USER */
			cmdstring = pstrdup("enable or disable triggers on");
			break;

		case AT_EnableRule: /* ENABLE RULE name */
		case AT_EnableAlwaysRule: /* ENABLE ALWAYS RULE name */
		case AT_EnableReplicaRule: /* ENABLE REPLICA RULE  name */
		case AT_DisableRule: /* DISABLE TRIGGER name */
			cmdstring = pstrdup("enable or disable rules on");
			break;

		case AT_AddInherit: /* INHERIT parent */
		case AT_DropInherit: /* NO INHERIT parent */
			cmdstring = pstrdup("alter inheritance on");
			break;
			
		case AT_SetDistributedBy: /* SET DISTRIBUTED BY */
			cmdstring = pstrdup("set distributed on");
			break;
			
		case AT_PartAdd: /* Add */
		case AT_PartAddForSplit: /* Add */
		case AT_PartAlter: /* Alter */
		case AT_PartCoalesce: /* Coalesce */
		case AT_PartDrop: /* Drop */
			break;
			
		case AT_PartExchange: /* Exchange */
			cmdstring = pstrdup("exchange a part into");
			break;
			
		case AT_PartMerge: /* Merge */
			cmdstring = pstrdup("merge parts of");
			break;
			
		case AT_PartModify: /* Modify */
		case AT_PartRename: /* Rename */
		case AT_PartSetTemplate: /* Set Subpartition Template */
			break;
			
		case AT_PartSplit: /* Split */
			cmdstring = pstrdup("split parts of");
			break;
			
		case AT_PartTruncate: /* Truncate */
		case AT_PartAddInternal: /* CREATE TABLE time partition addition */
			break;
	}
	
	if ( cmdstring == NULL )
	{
		cmdstring = pstrdup("alter");
	}
	
	return cmdstring;
}
