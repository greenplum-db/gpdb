/*-------------------------------------------------------------------------
 *
 * tablecmds.c
 *	  Commands for creating and altering table structures and settings
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/tablecmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/aocs_compaction.h"
#include "access/aomd.h"
#include "access/appendonlywriter.h"
#include "access/bitmap.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "access/tupconvert.h"
#include "access/xact.h"
#include "catalog/aocatalog.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_attribute_encoding.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_compression.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_inherits_fn.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_partition.h"
#include "catalog/pg_partition_rule.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "catalog/pg_type_fn.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "catalog/toasting.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbappendonlyxlog.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbpartition.h"
#include "cdb/memquota.h"
#include "commands/cluster.h"
#include "commands/copy.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/sequence.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "commands/trigger.h"
#include "commands/typecmds.h"
#include "commands/user.h"
#include "executor/executor.h"
#include "executor/instrument.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/parsenodes.h"
#include "optimizer/clauses.h"
#include "optimizer/plancat.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_partition.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "parser/parser.h"
#include "postmaster/autostats.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/sinval.h"
#include "tcop/utility.h"
#include "storage/lock.h"
#include "storage/predicate.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/metrics_utils.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tqual.h"
#include "utils/typcache.h"

#include "catalog/oid_dispatch.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbrelsize.h"
#include "cdb/cdboidsync.h"

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

#define AT_PASS_UNSET			-1		/* UNSET will cause ERROR */
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
	bool		rewrite;		/* T if a rewrite is forced */
	bool		dist_opfamily_changed; /* T if changing datatype of distribution key column and new opclass is in different opfamily than old one */
	Oid			new_opclass;		/* new opclass, if changing a distribution key column */
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

/*
 * Error-reporting support for RemoveRelations
 */
struct dropmsgstrings
{
	char		kind;
	int			nonexistent_code;
	const char *nonexistent_msg;
	const char *skipping_msg;
	const char *nota_msg;
	const char *drophint_msg;
};

static const struct dropmsgstrings dropmsgstringarray[] = {
	{RELKIND_RELATION,
		ERRCODE_UNDEFINED_TABLE,
		gettext_noop("table \"%s\" does not exist"),
		gettext_noop("table \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a table"),
	gettext_noop("Use DROP TABLE to remove a table, DROP EXTERNAL TABLE if external, or DROP FOREIGN TABLE if foreign.")},
	{RELKIND_SEQUENCE,
		ERRCODE_UNDEFINED_TABLE,
		gettext_noop("sequence \"%s\" does not exist"),
		gettext_noop("sequence \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a sequence"),
	gettext_noop("Use DROP SEQUENCE to remove a sequence.")},
	{RELKIND_VIEW,
		ERRCODE_UNDEFINED_TABLE,
		gettext_noop("view \"%s\" does not exist"),
		gettext_noop("view \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a view"),
	gettext_noop("Use DROP VIEW to remove a view.")},
	{RELKIND_MATVIEW,
		ERRCODE_UNDEFINED_TABLE,
		gettext_noop("materialized view \"%s\" does not exist"),
		gettext_noop("materialized view \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a materialized view"),
	gettext_noop("Use DROP MATERIALIZED VIEW to remove a materialized view.")},
	{RELKIND_INDEX,
		ERRCODE_UNDEFINED_OBJECT,
		gettext_noop("index \"%s\" does not exist"),
		gettext_noop("index \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not an index"),
	gettext_noop("Use DROP INDEX to remove an index.")},
	{RELKIND_COMPOSITE_TYPE,
		ERRCODE_UNDEFINED_OBJECT,
		gettext_noop("type \"%s\" does not exist"),
		gettext_noop("type \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a type"),
	gettext_noop("Use DROP TYPE to remove a type.")},
	{RELKIND_FOREIGN_TABLE,
		ERRCODE_UNDEFINED_OBJECT,
		gettext_noop("foreign table \"%s\" does not exist"),
		gettext_noop("foreign table \"%s\" does not exist, skipping"),
		gettext_noop("\"%s\" is not a foreign table"),
	gettext_noop("Use DROP FOREIGN TABLE to remove a foreign table.")},
	{'\0', 0, NULL, NULL, NULL, NULL}
};

struct DropRelationCallbackState
{
	char		relkind;
	Oid			heapOid;
	bool		concurrent;
};

/* Alter table target-type flags for ATSimplePermissions */
#define		ATT_TABLE				0x0001
#define		ATT_VIEW				0x0002
#define		ATT_MATVIEW				0x0004
#define		ATT_INDEX				0x0008
#define		ATT_COMPOSITE_TYPE		0x0010
#define		ATT_FOREIGN_TABLE		0x0020

static void ATExecExpandTableCTAS(AlterTableCmd *rootCmd, Relation rel, AlterTableCmd *cmd);

static void truncate_check_rel(Relation rel);
static void MergeAttributesIntoExisting(Relation child_rel, Relation parent_rel,
						List *inhAttrNameList, bool is_partition);
static bool MergeCheckConstraint(List *constraints, char *name, Node *expr);
static void MergeConstraintsIntoExisting(Relation child_rel, Relation parent_rel);
static void StoreCatalogInheritance(Oid relationId, List *supers);
static void StoreCatalogInheritance1(Oid relationId, Oid parentOid,
						 int16 seqNumber, Relation inhRelation,
						 bool is_partition);
static int	findAttrByName(const char *attributeName, List *schema);
static void AlterIndexNamespaces(Relation classRel, Relation rel,
				   Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved);
static void AlterSeqNamespaces(Relation classRel, Relation rel,
				   Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved,
				   LOCKMODE lockmode);
static void ATExecAlterConstraint(Relation rel, AlterTableCmd *cmd,
					  bool recurse, bool recursing, LOCKMODE lockmode);
static void ATExecValidateConstraint(Relation rel, char *constrName,
						 bool recurse, bool recursing, LOCKMODE lockmode);
static int transformColumnNameList(Oid relId, List *colList,
						int16 *attnums, Oid *atttypids);
static int transformFkeyGetPrimaryKey(Relation pkrel, Oid *indexOid,
						   List **attnamelist,
						   int16 *attnums, Oid *atttypids,
						   Oid *opclasses);
static void checkFkeyPermissions(Relation rel, int16 *attnums, int natts);
static CoercionPathType findFkeyCast(Oid targetTypeId, Oid sourceTypeId,
			 Oid *funcid);
static void validateCheckConstraint(Relation rel, HeapTuple constrtup);
static void validateForeignKeyConstraint(char *conname,
							 Relation rel, Relation pkrel,
							 Oid pkindOid, Oid constraintOid);
static void createForeignKeyTriggers(Relation rel, Oid refRelOid,
						 Constraint *fkconstraint,
						 Oid constraintOid, Oid indexOid);
static void ATController(Relation rel, List *cmds, bool recurse, LOCKMODE lockmode);
static void prepSplitCmd(Relation rel, PgPartRule *prule, bool is_at);
static void ATPrepCmd(List **wqueue, Relation rel, AlterTableCmd *cmd,
		  bool recurse, bool recursing, LOCKMODE lockmode);
static void ATRewriteCatalogs(List **wqueue, LOCKMODE lockmode);
static void ATAddToastIfNeeded(List **wqueue, LOCKMODE lockmode);
static void ATExecCmd(List **wqueue, AlteredTableInfo *tab, Relation *rel_p,
		  AlterTableCmd *cmd, LOCKMODE lockmode);
static void ATRewriteTables(List **wqueue, LOCKMODE lockmode);
static void ATRewriteTable(AlteredTableInfo *tab, Oid OIDNewHeap, LOCKMODE lockmode);
static void ATAocsWriteNewColumns(
		AOCSAddColumnDesc idesc, AOCSHeaderScanDesc sdesc,
		AlteredTableInfo *tab, ExprContext *econtext, TupleTableSlot *slot);
static bool ATAocsNoRewrite(AlteredTableInfo *tab);
static AlteredTableInfo *ATGetQueueEntry(List **wqueue, Relation rel);
static void ATSimplePermissions(Relation rel, int allowed_targets);
static void ATWrongRelkindError(Relation rel, int allowed_targets);
static void ATSimpleRecursion(List **wqueue, Relation rel,
				  AlterTableCmd *cmd, bool recurse, LOCKMODE lockmode);
static void ATTypedTableRecursion(List **wqueue, Relation rel, AlterTableCmd *cmd,
					  LOCKMODE lockmode);
static List *find_typed_table_dependencies(Oid typeOid, const char *typeName,
							  DropBehavior behavior);
static void ATPrepAddColumn(List **wqueue, Relation rel, bool recurse, bool recursing,
				AlterTableCmd *cmd, LOCKMODE lockmode);
static void ATExecAddColumn(List **wqueue, AlteredTableInfo *tab, Relation rel,
				ColumnDef *colDef, bool isOid,
				bool recurse, bool recursing, LOCKMODE lockmode);
static void check_for_column_name_collision(Relation rel, const char *colname);
static void add_column_datatype_dependency(Oid relid, int32 attnum, Oid typid);
static void add_column_collation_dependency(Oid relid, int32 attnum, Oid collid);
static void ATPrepAddOids(List **wqueue, Relation rel, bool recurse,
			  AlterTableCmd *cmd, LOCKMODE lockmode);
static void ATExecDropNotNull(Relation rel, const char *colName, LOCKMODE lockmode);
static void ATExecSetNotNull(AlteredTableInfo *tab, Relation rel,
				 const char *colName, LOCKMODE lockmode);
static void ATPrepColumnDefault(Relation rel, bool recurse, AlterTableCmd *cmd);
static void ATExecColumnDefault(Relation rel, const char *colName,
					ColumnDef *newDefault, LOCKMODE lockmode);
static void ATPrepSetStatistics(Relation rel, const char *colName,
					Node *newValue, LOCKMODE lockmode);
static void ATExecSetStatistics(Relation rel, const char *colName,
					Node *newValue, LOCKMODE lockmode);
static void ATExecSetOptions(Relation rel, const char *colName,
				 Node *options, bool isReset, LOCKMODE lockmode);
static void ATExecSetStorage(Relation rel, const char *colName,
				 Node *newValue, LOCKMODE lockmode);
static void ATPrepDropColumn(List **wqueue, Relation rel, bool recurse, bool recursing,
				 AlterTableCmd *cmd, LOCKMODE lockmode);
static void ATExecDropColumn(List **wqueue, Relation rel, const char *colName,
				 DropBehavior behavior,
				 bool recurse, bool recursing,
				 bool missing_ok, LOCKMODE lockmode);
static void ATExecAddIndex(AlteredTableInfo *tab, Relation rel,
			   IndexStmt *stmt, bool is_rebuild, LOCKMODE lockmode);
static void ATExecAddConstraint(List **wqueue,
					AlteredTableInfo *tab, Relation rel,
					Constraint *newConstraint, bool recurse, bool is_readd,
					LOCKMODE lockmode);
static void ATExecAddIndexConstraint(AlteredTableInfo *tab, Relation rel,
						 IndexStmt *stmt, LOCKMODE lockmode);
static void ATAddCheckConstraint(List **wqueue,
					 AlteredTableInfo *tab, Relation rel,
					 Constraint *constr,
					 bool recurse, bool recursing, bool is_readd,
					 LOCKMODE lockmode);
static void ATAddForeignKeyConstraint(AlteredTableInfo *tab, Relation rel,
						  Constraint *fkconstraint, LOCKMODE lockmode);
static void ATExecDropConstraint(Relation rel, const char *constrName,
					 DropBehavior behavior,
					 bool recurse, bool recursing,
					 bool missing_ok, LOCKMODE lockmode);
static void ATPrepAlterColumnType(List **wqueue,
					  AlteredTableInfo *tab, Relation rel,
					  bool recurse, bool recursing,
					  AlterTableCmd *cmd, LOCKMODE lockmode);
static bool ATColumnChangeRequiresRewrite(Node *expr, AttrNumber varattno);
static void ATExecAlterColumnType(AlteredTableInfo *tab, Relation rel,
					  AlterTableCmd *cmd, LOCKMODE lockmode);
static void RememberConstraintForRebuilding(Oid conoid, AlteredTableInfo *tab,
											DependencyType deptype);
static void RememberIndexForRebuilding(Oid indoid, AlteredTableInfo *tab);
static void ATExecAlterColumnGenericOptions(Relation rel, const char *colName,
								List *options, LOCKMODE lockmode);
static void ATPostAlterTypeCleanup(List **wqueue, AlteredTableInfo *tab, LOCKMODE lockmode);
static void ATPostAlterTypeParse(Oid oldId, Oid oldRelId, Oid refRelId,
					 char *cmd, List **wqueue, LOCKMODE lockmode,
					 bool rewrite);
static void TryReuseIndex(Oid oldId, IndexStmt *stmt);
static void TryReuseForeignKey(Oid oldId, Constraint *con);
static void change_owner_fix_column_acls(Oid relationOid,
							 Oid oldOwnerId, Oid newOwnerId);
static void change_owner_recurse_to_sequences(Oid relationOid,
								  Oid newOwnerId, LOCKMODE lockmode);
static void ATExecClusterOn(Relation rel, const char *indexName, LOCKMODE lockmode);
static void ATExecDropCluster(Relation rel, LOCKMODE lockmode);
static void ATPrepSetTableSpace(AlteredTableInfo *tab, Relation rel,
					char *tablespacename, LOCKMODE lockmode);
static void ATPartsPrepSetTableSpace(List **wqueue, Relation rel, AlterTableCmd *cmd, 
									 List *oids);
static void ATExecSetTableSpace(Oid tableOid, Oid newTableSpace, LOCKMODE lockmode);
static void ATExecSetRelOptions(Relation rel, List *defList,
					AlterTableType operation,
					LOCKMODE lockmode);
static void ATExecEnableDisableTrigger(Relation rel, char *trigname,
					   char fires_when, bool skip_system, LOCKMODE lockmode);
static void ATExecEnableDisableRule(Relation rel, char *rulename,
						char fires_when, LOCKMODE lockmode);
static void ATPrepAddInherit(Relation child_rel);
static void ATExecAddInherit(Relation child_rel, Node *node, LOCKMODE lockmode);
static void ATExecDropInherit(Relation rel, RangeVar *parent, LOCKMODE lockmode);
static void drop_parent_dependency(Oid relid, Oid refclassid, Oid refobjid, bool is_partition);
static void ATExecAddOf(Relation rel, const TypeName *ofTypename, LOCKMODE lockmode);
static void ATExecDropOf(Relation rel, LOCKMODE lockmode);
static void ATExecReplicaIdentity(Relation rel, ReplicaIdentityStmt *stmt, LOCKMODE lockmode);
static void ATExecGenericOptions(Relation rel, List *options);

static void copy_relation_data(SMgrRelation rel, SMgrRelation dst,
				   ForkNumber forkNum, char relpersistence);
static const char *storage_name(char c);

static void RangeVarCallbackForDropRelation(const RangeVar *rel, Oid relOid,
								Oid oldRelOid, void *arg);
static void RangeVarCallbackForAlterRelation(const RangeVar *rv, Oid relid,
								 Oid oldrelid, void *arg);
static void RemoveInheritance(Relation child_rel, Relation parent_rel, bool is_partition);
static void ATExecExpandTable(List **wqueue, Relation rel, AlterTableCmd *cmd);

static void ATExecSetDistributedBy(Relation rel, Node *node,
								   AlterTableCmd *cmd);
static void ATPrepExchange(Relation rel, AlterPartitionCmd *pc);
static void ATPrepDropConstraint(List **wqueue, Relation rel,
					AlterTableCmd *cmd, bool recurse, bool recursing);

const char *synthetic_sql = "(internally generated SQL command)";

/* ALTER TABLE ... PARTITION */


static void AttachPartitionEnsureIndexes(Relation rel, Relation attachrel);
static void ATExecDetachPartitionInheritance(Relation rel, RangeVar *name);
static void ATExecAttachPartitionIdx(List **wqueue, Relation parentIdx,
									 AlterPartitionId *alterpartId);
static void refuseDupeIndexAttach(Relation parentIdx, Relation partIdx,
								  Relation partitionTbl);
static void ATPExecPartAdd(AlteredTableInfo *tab,
						   Relation rel,
                           AlterPartitionCmd *pc,
						   AlterTableType att);				/* Add */
static void ATPExecPartAlter(List **wqueue, AlteredTableInfo *tab, 
							 Relation *rel,
                             AlterPartitionCmd *pc);		/* Alter */
static void ATPExecPartDrop(Relation rel,
                            AlterPartitionCmd *pc);			/* Drop */
static void ATPExecPartExchange(AlteredTableInfo *tab,
								Relation rel,
                                AlterPartitionCmd *pc);		/* Exchange */
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
static bool TypeTupleExists(Oid typeId);

static void ATExecPartAddInternal(Relation rel, Node *def);

static RangeVar *make_temp_table_name(Relation rel, BackendId id);
static bool prebuild_temp_table(Relation rel, RangeVar *tmpname, DistributedBy *distro,
								List *opts, bool isTmpTableAo,
								bool useExistingColumnAttributes);
static void ATPartitionCheck(AlterTableType subtype, Relation rel, bool rejectroot, bool recursing);
static void ATExternalPartitionCheck(AlterTableType subtype, Relation rel, bool recursing);

static char *alterTableCmdString(AlterTableType subtype);

static void change_dropped_col_datatypes(Relation rel);

static void CheckDropRelStorage(RangeVar *rel, ObjectType removeType);

static void inherit_parent(Relation parent_rel, Relation child_rel,
						   bool is_partition, List *inhAttrNameList);
static inline void SetConstraints(TupleDesc tupleDesc, char *relName, List **constraints, AttrNumber *attnos);
static inline void SetSchema(TupleDesc tuple_desc, List **schema, AttrNumber **attnos);



/* ----------------------------------------------------------------
 *		DefineRelation
 *				Creates a new relation.
 *
 * stmt carries parsetree information from an ordinary CREATE TABLE statement.
 * The other arguments are used to extend the behavior for other cases:
 * relkind: relkind to assign to the new relation
 * ownerId: if not InvalidOid, use this as the new relation's owner.
 *
 * Note that permissions checks are done against current user regardless of
 * ownerId.  A nonzero ownerId is used when someone is creating a relation
 * "on behalf of" someone else, so we still want to see that the current user
 * has permissions to do it.
 *
 * If successful, returns the OID of the new relation.
 *
 * If 'dispatch' is true (and we are running in QD), the statement is
 * also dispatched to the QE nodes. Otherwise it is the caller's
 * responsibility to dispatch.
 * ----------------------------------------------------------------
 */
Oid
DefineRelation(CreateStmt *stmt, char relkind, Oid ownerId, char relstorage, bool dispatch, bool useChangedOpts, GpPolicy *intoPolicy)
{
	char		relname[NAMEDATALEN];
	Oid			namespaceId;
	List	   *schema;
	GpPolicy   *policy;
	Oid			relationId = InvalidOid;
	Oid			tablespaceId;
	Relation	rel;
	TupleDesc	descriptor;
	List	   *old_constraints;
	bool		localHasOids;
	List	   *rawDefaults;
	List	   *cookedDefaults;
	Datum		reloptions;
	ListCell   *listptr;
	AttrNumber	attnum;
	bool		isPartitioned;
	List	   *cooked_constraints;
	bool		shouldDispatch = dispatch &&
								 Gp_role == GP_ROLE_DISPATCH &&
                                 IsNormalProcessingMode();


	/*
	 * In QE mode, tableElts contain not only the normal ColumnDefs, but also
	 * pre-made CookedConstraints. Separate them into different lists.
	 */
	schema = NIL;
	cooked_constraints = NIL;
	foreach(listptr, stmt->tableElts)
	{
		Node	   *node = lfirst(listptr);

		if (IsA(node, CookedConstraint))
			cooked_constraints = lappend(cooked_constraints, node);
		else
			schema = lappend(schema, node);
	}
	Assert(cooked_constraints == NIL || Gp_role == GP_ROLE_EXECUTE);
	static char *validnsps[] = HEAP_RELOPT_NAMESPACES;
	Oid			ofTypeId;

	/*
	 * Truncate relname to appropriate length (probably a waste of time, as
	 * parser should have done this already).
	 */
	StrNCpy(relname, stmt->relation->relname, NAMEDATALEN);

	/*
	 * Check consistency of arguments
	 */
	if (stmt->oncommit != ONCOMMIT_NOOP
		&& stmt->relation->relpersistence != RELPERSISTENCE_TEMP)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("ON COMMIT can only be used on temporary tables")));
	if (stmt->constraints != NIL && relkind == RELKIND_FOREIGN_TABLE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("constraints are not supported on foreign tables")));

	/*
	 * Look up the namespace in which we are supposed to create the relation,
	 * check we have permission to create there, lock it against concurrent
	 * drop, and mark stmt->relation as RELPERSISTENCE_TEMP if a temporary
	 * namespace is selected.
	 */
	namespaceId =
		RangeVarGetAndCheckCreationNamespace(stmt->relation, NoLock, NULL);

	/*
	 * Security check: disallow creating temp tables from security-restricted
	 * code.  This is needed because calling code might not expect untrusted
	 * tables to appear in pg_temp at the front of its search path.
	 */
	if (stmt->relation->relpersistence == RELPERSISTENCE_TEMP
		&& InSecurityRestrictedOperation())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("cannot create temporary table within security-restricted operation")));

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
		 * These relkinds have no storage, and thus do not support tablespaces.
		 * We shouldn't go through the regular default case for these because
		 * we don't want to pick up the value from the default_tablespace guc.
		 * MPP-8262: unable to create sequence with default_tablespace
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
		tablespaceId = GetDefaultTablespace(stmt->relation->relpersistence);

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

	/* In all cases disallow placing user relations in pg_global */
	if (tablespaceId == GLOBALTABLESPACE_OID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("only shared relations can be placed in pg_global tablespace")));

	/* Identify user ID that will own the table */
	if (!OidIsValid(ownerId))
		ownerId = GetUserId();

	/*
	 * Parse and validate reloptions, if any.
	 */
	reloptions = transformRelOptions((Datum) 0, stmt->options, NULL, validnsps,
									 true, false);

	if (relkind == RELKIND_VIEW)
		(void) view_reloptions(reloptions, true);
	else
		(void) heap_reloptions(relkind, reloptions, true);

	if (stmt->ofTypename)
	{
		AclResult	aclresult;

		ofTypeId = typenameTypeId(NULL, stmt->ofTypename);

		aclresult = pg_type_aclcheck(ofTypeId, GetUserId(), ACL_USAGE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error_type(aclresult, ofTypeId);
	}
	else
		ofTypeId = InvalidOid;

	/*
	 * Look up inheritance ancestors and generate relation schema, including
	 * inherited attributes. Update the offsets of the distribution attributes
	 * in GpPolicy if necessary
	 */
	isPartitioned = stmt->partitionBy ? true : false;
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
	{
		schema = MergeAttributes(schema, stmt->inhRelations,
								 stmt->relation->relpersistence,
								 isPartitioned,
								 &stmt->inhOids, &old_constraints,
								 &stmt->parentOidCount);
	}
	else
	{
		/*
		 * In QE mode, we already extracted all the constraints, inherited
		 * or not, from tableElts at the beginning of the function.
		 */
		old_constraints = NIL;
	}

	/*
	 * Create a tuple descriptor from the relation schema.  Note that this
	 * deals with column names, types, and NOT NULL constraints, but not
	 * default values or CHECK constraints; we handle those below.
	 */
	descriptor = BuildDescForRelation(schema);

	localHasOids = interpretOidsOption(stmt->options,
									   (relkind == RELKIND_RELATION ||
										relkind == RELKIND_FOREIGN_TABLE));
	descriptor->tdhasoid = (localHasOids || stmt->parentOidCount > 0);


	/*
	 * now that we have the final list of attributes, interpret DISTRIBUTED BY
	 * column names into a GpPolicy
	 */
	if (intoPolicy)
	{
		Assert(!stmt->inhRelations);
		policy = intoPolicy;
	}
	else
		policy = getPolicyForDistributedBy(stmt->distributedBy, descriptor);

	/*
	 * Find columns with default values and prepare for insertion of the
	 * defaults.  Pre-cooked (that is, inherited) defaults go into a list of
	 * CookedConstraint structs that we'll pass to heap_create_with_catalog,
	 * while raw defaults go into a list of RawColumnDefault structs that will
	 * be processed by AddRelationNewConstraints.  (We can't deal with raw
	 * expressions until we can do transformExpr.)
	 *
	 * We can set the atthasdef flags now in the tuple descriptor; this just
	 * saves StoreAttrDefault from having to do an immediate update of the
	 * pg_attribute rows.
	 */
	rawDefaults = NIL;
	cookedDefaults = NIL;
	attnum = 0;

	foreach(listptr, schema)
	{
		ColumnDef  *colDef = lfirst(listptr);

		attnum++;

		if (colDef->raw_default != NULL)
		{
			RawColumnDefault *rawEnt;

			Assert(colDef->cooked_default == NULL);

			rawEnt = (RawColumnDefault *) palloc(sizeof(RawColumnDefault));
			rawEnt->attnum = attnum;
			rawEnt->raw_default = colDef->raw_default;
			rawDefaults = lappend(rawDefaults, rawEnt);
			descriptor->attrs[attnum - 1]->atthasdef = true;
		}
		else if (colDef->cooked_default != NULL)
		{
			CookedConstraint *cooked;

			cooked = (CookedConstraint *) palloc(sizeof(CookedConstraint));
			cooked->contype = CONSTR_DEFAULT;
			cooked->name = NULL;
			cooked->attnum = attnum;
			cooked->expr = colDef->cooked_default;
			cooked->skip_validation = false;
			cooked->is_local = true;	/* not used for defaults */
			cooked->inhcount = 0;		/* ditto */
			cooked->is_no_inherit = false;
			cookedDefaults = lappend(cookedDefaults, cooked);
			descriptor->attrs[attnum - 1]->atthasdef = true;
		}
	}

	/*
	 * In executor mode, we received all the defaults and constraints
	 * in pre-cooked form from the QD, so forget about the lists we
	 * constructed just above, and use the old_constraints we received
	 * from the QD.
	 */
	if (Gp_role != GP_ROLE_EXECUTE)
		cooked_constraints = list_concat(cookedDefaults, old_constraints);

	if (shouldDispatch)
	{
		Relation		pg_class_desc;
		Relation		pg_type_desc;
		MemoryContext	oldContext;

		/*
		 * We use a RowExclusiveLock but hold it till end of transaction so
		 * that two DDL operations will not deadlock between QEs
		 */
		pg_class_desc = heap_open(RelationRelationId, RowExclusiveLock);
		pg_type_desc = heap_open(TypeRelationId, RowExclusiveLock);

		LockRelationOid(DependRelationId, RowExclusiveLock);

		cdb_sync_oid_to_segments();

		heap_close(pg_class_desc, NoLock);  /* gonna update, so don't unlock */

		stmt->relKind = relkind;
		stmt->relStorage = relstorage;

		if (!OidIsValid(stmt->ownerid))
			stmt->ownerid = ownerId;

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
			stmt->ownerid = ownerId;
	}

	/* MPP-8405: disallow OIDS on partitioned tables */
	if (descriptor->tdhasoid && IsNormalProcessingMode() && Gp_role == GP_ROLE_DISPATCH)
	{
		if (stmt->partitionBy || stmt->is_part_child)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("OIDS=TRUE is not allowed on partitioned tables"),
					 errhint("Use OIDS=FALSE.")));
	}

	bool valid_opts = (relstorage == RELSTORAGE_EXTERNAL || !useChangedOpts);

	/*
	 * Create the relation.  Inherited defaults and constraints are passed in
	 * for immediate handling --- since they don't need parsing, they can be
	 * stored immediately.
	 */
	relationId = heap_create_with_catalog(relname,
										  namespaceId,
										  tablespaceId,
										  InvalidOid,
										  InvalidOid,
										  ofTypeId,
										  ownerId,
										  descriptor,
										  cooked_constraints,
										  /* relam */ InvalidOid,
										  relkind,
										  stmt->relation->relpersistence,
										  relstorage,
										  tablespaceId==GLOBALTABLESPACE_OID,
										  false,
										  localHasOids,
										  stmt->parentOidCount,
										  stmt->oncommit,
                                          policy,  /*CDB*/
                                          reloptions,
										  true,
										  allowSystemTableMods,
										  false,
										  valid_opts,
										  stmt->is_part_child,
										  stmt->is_part_parent);

	/*
	 * Give a warning if you use OIDS=TRUE on user tables. We do this after calling
	 * heap_create_with_catalog, because heap_create_with_catalog performs some extra
	 * checks, and e.g. if you try to use OIDS=TRUE on an AOCS table, you will get
	 * an error. We don't want to emit the NOTICE first, in that case.
	 */
	if (descriptor->tdhasoid && IsNormalProcessingMode() && Gp_role == GP_ROLE_DISPATCH)
	{
		ereport(NOTICE,
				(errmsg("OIDS=TRUE is not recommended for user-created tables"),
				 errhint("Use OIDS=FALSE to prevent wrap-around of the OID counter.")));
	}

	/* Store inheritance information for new rel. */
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
	 * Now add any newly specified column default values and CHECK constraints
	 * to the new relation.  These are passed to us in the form of raw
	 * parsetrees; we need to transform them  to executable expression trees
	 * before they can be added. The most convenient way to do that is to apply
	 * the parser's transformExpr routine, but transformExpr doesn't work
	 * unless we have a pre-existing relation. So, the transformation has to be
	 * postponed to this final step of CREATE TABLE.
	 */
	if (Gp_role != GP_ROLE_EXECUTE &&
		(rawDefaults || stmt->constraints))
	{
		List	   *newCookedDefaults;

		newCookedDefaults =
			AddRelationNewConstraints(rel, rawDefaults, stmt->constraints,
									  true, true, false);

		cooked_constraints = list_concat(cooked_constraints, newCookedDefaults);
	}

	if (stmt->attr_encodings)
		AddRelationAttributeEncodings(rel, stmt->attr_encodings);

	/*
	 * Transfer any inherited CHECK constraints back to the statement, so
	 * that they are dispatched to QE nodes along with the statement
	 * itself. This way, the QE nodes don't need to repeat the processing
	 * above, which reduces the risk that they would interpret the defaults
	 * or constraints somehow differently.
	 *
	 * NOTE: We do this even if !shouldDispatch, because it means that the
	 * caller will dispatch the statement later, not that we won't need to
	 * dispatch at all.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		stmt->tableElts = schema;

		foreach(listptr, cooked_constraints)
		{
			CookedConstraint *cooked = (CookedConstraint *) lfirst(listptr);

			/*
			 * In PostgreSQL, CookedConstraint is not a regular struct, not
			 * "node", so MergeAttributes and friends that above created
			 * the CookedConstraints have not set the node tag. Set it now.
			 */
			cooked->type = T_CookedConstraint;

			stmt->tableElts = lappend(stmt->tableElts, cooked);
		}
	}

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

		uquery = parse_analyze(dstmt, synthetic_sql, NULL, 0);
		Insist(uquery->commandType == CMD_UTILITY);

		ereport(DEBUG1,
				(errmsg("processing deferred utility statement")));

		ProcessUtility((Node*)uquery->utilityStmt,
					   synthetic_sql,
					   PROCESS_UTILITY_SUBCOMMAND,
					   NULL,
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
static bool
MetaTrackValidKindNsp(Form_pg_class rd_rel)
{
	Oid nsp = rd_rel->relnamespace;

	if (PG_CATALOG_NAMESPACE == nsp)
	{
		/*
		 * MPP-7773: don't track objects in system namespace
		 * if modifying system tables (eg during upgrade)  
		 */
		if (allowSystemTableMods)
			return false;
	}

	/* MPP-7599: watch out for toast indexes */
	return (METATRACK_VALIDNAMESPACE(nsp)
			&& MetaTrackValidRelkind(rd_rel->relkind)
			/* MPP-7572: not valid if in any temporary namespace */
			&& (!(isAnyTempNamespace(nsp))));
}

/*
 * Emit the right error or warning message for a "DROP" command issued on a
 * non-existent relation
 */
static void
DropErrorMsgNonExistent(RangeVar *rel, char rightkind, bool missing_ok)
{
	const struct dropmsgstrings *rentry;

	if (rel->schemaname != NULL &&
		!OidIsValid(LookupNamespaceNoError(rel->schemaname)))
	{
		if (!missing_ok)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_SCHEMA),
				   errmsg("schema \"%s\" does not exist", rel->schemaname)));
		}
		else
		{
			ereport(NOTICE,
					(errmsg("schema \"%s\" does not exist, skipping",
							rel->schemaname)));
		}
		return;
	}

	for (rentry = dropmsgstringarray; rentry->kind != '\0'; rentry++)
	{
		if (rentry->kind == rightkind)
		{
			if (!missing_ok)
			{
				ereport(ERROR,
						(errcode(rentry->nonexistent_code),
						 errmsg(rentry->nonexistent_msg, rel->relname)));
			}
			else
			{
				if (Gp_role != GP_ROLE_EXECUTE)
					ereport(NOTICE, (errmsg(rentry->skipping_msg, rel->relname)));
				break;
			}
		}
	}

	Assert(rentry->kind != '\0');		/* Should be impossible */
}

/*
 * Emit the right error message for a "DROP" command issued on a
 * relation of the wrong type
 */
static void
DropErrorMsgWrongType(const char *relname, char wrongkind, char rightkind)
{
	const struct dropmsgstrings *rentry;
	const struct dropmsgstrings *wentry;

	for (rentry = dropmsgstringarray; rentry->kind != '\0'; rentry++)
		if (rentry->kind == rightkind)
			break;
	Assert(rentry->kind != '\0');

	for (wentry = dropmsgstringarray; wentry->kind != '\0'; wentry++)
		if (wentry->kind == wrongkind)
			break;
	/* wrongkind could be something we don't have in our table... */

	ereport(ERROR,
			(errcode(ERRCODE_WRONG_OBJECT_TYPE),
			 errmsg(rentry->nota_msg, relname),
	   (wentry->kind != '\0') ? errhint("%s", _(wentry->drophint_msg)) : 0));
}

/*
 * RemoveRelations
 *		Implements DROP TABLE, DROP INDEX, DROP SEQUENCE, DROP VIEW,
 *		DROP MATERIALIZED VIEW, DROP FOREIGN TABLE
 */
void
RemoveRelations(DropStmt *drop)
{
	ObjectAddresses *objects;
	char		relkind;
	ListCell   *cell;
	int			flags = 0;
	LOCKMODE	lockmode = AccessExclusiveLock;

	/* DROP CONCURRENTLY uses a weaker lock, and has some restrictions */
	if (drop->concurrent)
	{
		flags |= PERFORM_DELETION_CONCURRENTLY;
		lockmode = ShareUpdateExclusiveLock;
		Assert(drop->removeType == OBJECT_INDEX);
		if (list_length(drop->objects) != 1)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("DROP INDEX CONCURRENTLY does not support dropping multiple objects")));
		if (drop->behavior == DROP_CASCADE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("DROP INDEX CONCURRENTLY does not support CASCADE")));
	}

	/*
	 * First we identify all the relations, then we delete them in a single
	 * performMultipleDeletions() call.  This is to avoid unwanted DROP
	 * RESTRICT errors if one of the relations depends on another.
	 */

	/* Determine required relkind */
	switch (drop->removeType)
	{
		case OBJECT_EXTTABLE:
		case OBJECT_TABLE:
			relkind = RELKIND_RELATION;
			break;

		case OBJECT_INDEX:
			relkind = RELKIND_INDEX;
			break;

		case OBJECT_SEQUENCE:
			relkind = RELKIND_SEQUENCE;
			break;

		case OBJECT_VIEW:
			relkind = RELKIND_VIEW;
			break;

		case OBJECT_MATVIEW:
			relkind = RELKIND_MATVIEW;
			break;

		case OBJECT_FOREIGN_TABLE:
			relkind = RELKIND_FOREIGN_TABLE;
			break;

		default:
			elog(ERROR, "unrecognized drop object type: %d",
				 (int) drop->removeType);
			relkind = 0;		/* keep compiler quiet */
			break;
	}

	/* Lock and validate each relation; build a list of object addresses */
	objects = new_object_addresses();

	foreach(cell, drop->objects)
	{
		RangeVar   *rel = makeRangeVarFromNameList((List *) lfirst(cell));
		Oid			relOid;
		ObjectAddress obj;
		struct DropRelationCallbackState state;

		/*
		 * These next few steps are a great deal like relation_openrv, but we
		 * don't bother building a relcache entry since we don't need it.
		 *
		 * Check for shared-cache-inval messages before trying to access the
		 * relation.  This is needed to cover the case where the name
		 * identifies a rel that has been dropped and recreated since the
		 * start of our transaction: if we don't flush the old syscache entry,
		 * then we'll latch onto that entry and suffer an error later.
		 */
		AcceptInvalidationMessages();

		/* Look up the appropriate relation using namespace search. */
		state.relkind = relkind;
		state.heapOid = InvalidOid;
		state.concurrent = drop->concurrent;
		relOid = RangeVarGetRelidExtended(rel, lockmode, true,
										  false,
										  RangeVarCallbackForDropRelation,
										  (void *) &state);

		/* Not there? */
		if (!OidIsValid(relOid))
		{
			DropErrorMsgNonExistent(rel, relkind, drop->missing_ok);
			continue;
		}

		if (drop->removeType == OBJECT_EXTTABLE || drop->removeType == OBJECT_TABLE)
			CheckDropRelStorage(rel, drop->removeType);

		/* Disallow direct DROP TABLE of a partition (MPP-3260) */
		if (rel_is_child_partition(relOid) && !drop->bAllowPartn)
		{
			Oid		 master = rel_partition_get_master(relOid);
			char	*pretty	= rel_get_part_path_pretty(relOid,
													   " ALTER PARTITION ",
													   " DROP PARTITION ");

			ereport(ERROR,
					(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
					errmsg("cannot drop partition \"%s\" directly",
						   get_rel_name(relOid)),
					errhint("Table \"%s\" is a child partition of table \"%s\". To drop it, use ALTER TABLE \"%s\"%s...",
							get_rel_name(relOid), get_rel_name(master),
							get_rel_name(master), pretty ? pretty : "" )));
		}

		/* OK, we're ready to delete this one */
		obj.classId = RelationRelationId;
		obj.objectId = relOid;
		obj.objectSubId = 0;

		add_exact_object_address(&obj, objects);
	}

	performMultipleDeletions(objects, drop->behavior, flags);

	free_object_addresses(objects);
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

	relOid = RangeVarGetRelid(relation, NoLock, false);

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
	recheckoid = RangeVarGetRelid(relation, NoLock, false);

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
		LOCKACQUIRE_ALREADY_CLEAR)
	{
		UnlockRelation(rel, AccessExclusiveLock);
		return false;
	}
	return true;
}
#endif

static void
relid_set_new_relfilenode(Oid relid, TransactionId recentXmin)
{
	if (OidIsValid(relid))
	{
		MultiXactId minmulti;
		Relation rel;

		rel = relation_open(relid, AccessExclusiveLock);

		minmulti = GetOldestMultiXactId();

		RelationSetNewRelfilenode(rel, recentXmin, minmulti);
		heap_close(rel, NoLock);
	}
}

static void
ao_aux_tables_safe_truncate(Relation rel)
{
	if (!RelationIsAppendOptimized(rel))
		return;

	Oid relid = RelationGetRelid(rel);

	Oid aoseg_relid = InvalidOid;
	Oid aoblkdir_relid = InvalidOid;
	Oid aovisimap_relid = InvalidOid;

	GetAppendOnlyEntryAuxOids(relid, NULL, &aoseg_relid,
							  &aoblkdir_relid, NULL, &aovisimap_relid,
							  NULL);

	relid_set_new_relfilenode(aoseg_relid, RecentXmin);
	relid_set_new_relfilenode(aoblkdir_relid, RecentXmin);
	relid_set_new_relfilenode(aovisimap_relid, RecentXmin);
}

/*
 * Before acquiring a table lock, check whether we have sufficient rights.
 * In the case of DROP INDEX, also try to lock the table before the index.
 */
static void
RangeVarCallbackForDropRelation(const RangeVar *rel, Oid relOid, Oid oldRelOid,
								void *arg)
{
	HeapTuple	tuple;
	struct DropRelationCallbackState *state;
	char		relkind;
	Form_pg_class classform;
	LOCKMODE	heap_lockmode;

	state = (struct DropRelationCallbackState *) arg;
	relkind = state->relkind;
	heap_lockmode = state->concurrent ?
		ShareUpdateExclusiveLock : AccessExclusiveLock;

	/*
	 * If we previously locked some other index's heap, and the name we're
	 * looking up no longer refers to that relation, release the now-useless
	 * lock.
	 */
	if (relOid != oldRelOid && OidIsValid(state->heapOid))
	{
		UnlockRelationOid(state->heapOid, heap_lockmode);
		state->heapOid = InvalidOid;
	}

	/* Didn't find a relation, so no need for locking or permission checks. */
	if (!OidIsValid(relOid))
		return;

	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relOid));
	if (!HeapTupleIsValid(tuple))
		return;					/* concurrently dropped, so nothing to do */
	classform = (Form_pg_class) GETSTRUCT(tuple);

	if (classform->relkind != relkind)
		DropErrorMsgWrongType(rel->relname, classform->relkind, relkind);

	/* Allow DROP to either table owner or schema owner */
	if (!pg_class_ownercheck(relOid, GetUserId()) &&
		!pg_namespace_ownercheck(classform->relnamespace, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   rel->relname);

	if (!allowSystemTableMods && IsSystemClass(relOid, classform))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						rel->relname)));

	ReleaseSysCache(tuple);

	/*
	 * In DROP INDEX, attempt to acquire lock on the parent table before
	 * locking the index.  index_drop() will need this anyway, and since
	 * regular queries lock tables before their indexes, we risk deadlock if
	 * we do it the other way around.  No error if we don't find a pg_index
	 * entry, though --- the relation may have been dropped.
	 */
	if (relkind == RELKIND_INDEX && relOid != oldRelOid)
	{
		state->heapOid = IndexGetRelation(relOid, true);
		if (OidIsValid(state->heapOid))
			LockRelationOid(state->heapOid, heap_lockmode);
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
	List	   *seq_relids = NIL;
	EState	   *estate;
	ResultRelInfo *resultRelInfos;
	ResultRelInfo *resultRelInfo;
	SubTransactionId mySubid;
	ListCell   *cell;
	List *partList = NIL;
	List *relationsToTruncate = NIL;

	/*
	 * Copy list to ensure we do not modify a cached plan
	 */
	relationsToTruncate = list_copy(stmt->relations);

	/*
	 * Check if table has partitions and add them too
	 */
	foreach(cell, relationsToTruncate)
	{
		RangeVar   *rv = lfirst(cell);
		Relation	rel;
		PartitionNode *pNode;

		rel = heap_openrv(rv, AccessExclusiveLock);
			
		pNode = RelationBuildPartitionDesc(rel, false);

		if (pNode)
		{
			List *plist = atpxTruncateList(rel, pNode);
			partList = list_concat(partList, plist);
		}
		heap_close(rel, NoLock);
	}

	relationsToTruncate = list_concat(partList, relationsToTruncate);

	/*
	 * Open, exclusive-lock, and check all the explicitly-specified relations
	 */
	foreach(cell, relationsToTruncate)
	{
		RangeVar   *rv = lfirst(cell);
		Relation	rel;
		bool		recurse = interpretInhOption(rv->inhOpt);
		Oid			myrelid;
		LOCKMODE	lockmode = AccessExclusiveLock;

		rel = heap_openrv(rv, lockmode);
		myrelid = RelationGetRelid(rel);
		/* don't throw error for "TRUNCATE foo, foo" */
		if (list_member_oid(relids, myrelid))
		{
			heap_close(rel, lockmode);
			continue;
		}
		truncate_check_rel(rel);
		rels = lappend(rels, rel);
		relids = lappend_oid(relids, myrelid);

		if (MetaTrackValidKindNsp(rel->rd_rel))
			meta_relids = lappend_oid(meta_relids, myrelid);

		if (recurse)
		{
			ListCell   *child;
			List	   *children;

			children = find_all_inheritors(myrelid, lockmode, NULL);

			foreach(child, children)
			{
				Oid			childrelid = lfirst_oid(child);

				if (list_member_oid(relids, childrelid))
					continue;

				/* find_all_inheritors already got lock */
				rel = heap_open(childrelid, NoLock);
				/*
				 * This check is performed outside truncate_check_rel() in
				 * order to provide a more reasonable error message.
				 */
				if (RelationIsExternal(rel))
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("cannot truncate table having external partition: \"%s\"",
								    RelationGetRelationName(rel))));

				/*
				 * It is possible that the parent table has children that are
				 * temp tables of other backends.  We cannot safely access
				 * such tables (because of buffering issues), and the best
				 * thing to do is to silently ignore them.  Note that this
				 * check is the same as one of the checks done in
				 * truncate_check_rel() called below, still it is kept
				 * here for simplicity.
				 */
				if (RELATION_IS_OTHER_TEMP(rel))
				{
					heap_close(rel, lockmode);
					continue;
				}

				truncate_check_rel(rel);
				rels = lappend(rels, rel);
				relids = lappend_oid(relids, childrelid);
			}
		}
	}

	/*
	 * In CASCADE mode, suck in all referencing relations as well.  This
	 * requires multiple iterations to find indirectly-dependent relations. At
	 * each phase, we need to exclusive-lock new rels before looking for their
	 * dependencies, else we might miss something.  Also, we check each rel as
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

	/*
	 * If we are asked to restart sequences, find all the sequences, lock them
	 * (we need AccessExclusiveLock for ResetSequence), and check permissions.
	 * We want to do this early since it's pointless to do all the truncation
	 * work only to fail on sequence permissions.
	 */
	if (stmt->restart_seqs)
	{
		foreach(cell, rels)
		{
			Relation	rel = (Relation) lfirst(cell);
			List	   *seqlist = getOwnedSequences(RelationGetRelid(rel));
			ListCell   *seqcell;

			foreach(seqcell, seqlist)
			{
				Oid			seq_relid = lfirst_oid(seqcell);
				Relation	seq_rel;

				seq_rel = relation_open(seq_relid, AccessExclusiveLock);

				/* This check must match AlterSequence! */
				if (!pg_class_ownercheck(seq_relid, GetUserId()))
					aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
								   RelationGetRelationName(seq_rel));

				seq_relids = lappend_oid(seq_relids, seq_relid);

				relation_close(seq_rel, NoLock);
			}
		}
	}

	/* Prepare to catch AFTER triggers. */
	AfterTriggerBeginQuery();

	/*
	 * To fire triggers, we'll need an EState as well as a ResultRelInfo for
	 * each relation.  We don't need to call ExecOpenIndices, though.
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
						  0,	/* dummy rangetable index */
						  0);
		resultRelInfo++;
	}
	estate->es_result_relations = resultRelInfos;
	estate->es_num_result_relations = list_length(rels);

	/*
	 * Process all BEFORE STATEMENT TRUNCATE triggers before we begin
	 * truncating (this is because one of them might throw an error). Also, if
	 * we were to allow them to prevent statement execution, that would need
	 * to be handled here.
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

	mySubid = GetCurrentSubTransactionId();

	foreach(cell, rels)
	{
		Relation	rel = (Relation) lfirst(cell);

		Assert(CheckExclusiveAccess(rel));

		/*
		 * Normally, we need a transaction-safe truncation here.  However, if
		 * the table was either created in the current (sub)transaction or has
		 * a new relfilenode in the current (sub)transaction, then we can just
		 * truncate it in-place, because a rollback would cause the whole
		 * table or the current physical file to be thrown away anyway.
		 */
		if (rel->rd_createSubid == mySubid ||
			rel->rd_newRelfilenodeSubid == mySubid)
		{
			/* Immediate, non-rollbackable truncation is OK */
			heap_truncate_one_rel(rel);
		}
		else
		{
			Oid			heap_relid;
			Oid			toast_relid;
			MultiXactId minmulti;

			/*
			 * This effectively deletes all rows in the table, and may be done
			 * in a serializable transaction.  In that case we must record a
			 * rw-conflict in to this transaction from each transaction
			 * holding a predicate lock on the table.
			 */
			CheckTableForSerializableConflictIn(rel);

			minmulti = GetOldestMultiXactId();

			/*
			 * Need the full transaction-safe pushups.
			 *
			 * Create a new empty storage file for the relation, and assign it
			 * as the relfilenode value. The old storage file is scheduled for
			 * deletion at commit.
			 */
			RelationSetNewRelfilenode(rel, RecentXmin, minmulti);
			if (rel->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED)
				heap_create_init_fork(rel);

			heap_relid = RelationGetRelid(rel);
			toast_relid = rel->rd_rel->reltoastrelid;

			/*
			 * The same for the toast table, if any.
			 */
			if (OidIsValid(toast_relid))
			{
				Relation toast_rel = relation_open(toast_relid, AccessExclusiveLock);
				RelationSetNewRelfilenode(toast_rel, RecentXmin, minmulti);
				if (toast_rel->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED)
					heap_create_init_fork(toast_rel);
				heap_close(toast_rel, NoLock);
			}

			/*
			 * The same for the ao auxiliary tables, if any.
			 */
			ao_aux_tables_safe_truncate(rel);

			/*
			 * Reconstruct the indexes to match, and we're done.
			 */
			reindex_relation(RelationGetRelid(rel), REINDEX_REL_PROCESS_TOAST);
		}
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
	 * Restart owned sequences if we were asked to.
	 */
	foreach(cell, seq_relids)
	{
		Oid			seq_relid = lfirst_oid(cell);

		ResetSequence(seq_relid);
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

		if (RelationIsAppendOptimized(rel) && IS_QUERY_DISPATCHER())
		{
			/*
			 * Drop the shared memory hash table entry for this table if it
			 * exists. We must do so since before the rewrite we probably have few
			 * non-zero segfile entries for this table while after the rewrite
			 * only segno zero will be full and the others will be empty. By
			 * dropping the hash entry we force refreshing the entry from the
			 * catalog the next time a write into this AO table comes along.
			 */
			LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);
			AORelRemoveHashEntry(RelationGetRelid(rel));
			LWLockRelease(AOSegFileLock);
		}

		heap_close(rel, NoLock);
	}
}

/*
 * Check that a given rel is safe to truncate.  Subroutine for ExecuteTruncate
 */
static void
truncate_check_rel(Relation rel)
{
	AclResult	aclresult;

	/*
	 * Only allow truncate on regular or append-only tables.
	 *
	 * In binary upgrade mode, we additionally allow TRUNCATE on AO auxiliary
	 * tables so that they can be wiped and recreated by the upgrade machinery.
	 */
	if (rel->rd_rel->relkind != RELKIND_RELATION &&
		!(IsBinaryUpgrade && IsAppendonlyMetadataRelkind(rel->rd_rel->relkind)))
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

	if (!allowSystemTableMods && IsSystemRelation(rel))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						RelationGetRelationName(rel))));

	/*
	 * Don't allow truncate on temp tables of other backends ... their local
	 * buffer manager is not going to cope.
	 */
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			  errmsg("cannot truncate temporary tables of other sessions")));

	/*
	 * Also check for active uses of the relation in the current transaction,
	 * including open scans and pending AFTER trigger events.
	 */
	CheckTableNotInUse(rel, "TRUNCATE");
}

/*
 * storage_name
 *	  returns the name corresponding to a typstorage/attstorage enum value
 */
static const char *
storage_name(char c)
{
	switch (c)
	{
		case 'p':
			return "PLAIN";
		case 'm':
			return "MAIN";
		case 'x':
			return "EXTENDED";
		case 'e':
			return "EXTERNAL";
		default:
			return "???";
	}
}

/*----------
 * MergeAttributes
 *		Returns new schema given initial schema and superclasses.
 *
 * Input arguments:
 * 'schema' is the column/attribute definition for the table. (It's a list
 *		of ColumnDef's.) It is destructively changed.
 * 'supers' is a list of names (as RangeVar nodes) of parent relations.
 * 'relpersistence' is a persistence type of the table.
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
MergeAttributes(List *schema, List *supers, char relpersistence, bool isPartitioned,
				List **supOids, List **supconstr, int *supOidCount)
{
	ListCell   *entry;
	List	   *inhSchema = NIL;
	List	   *parentOids = NIL;
	List	   *constraints = NIL;
	int			parentsWithOids = 0;
	bool		have_bogus_defaults = false;
	int			child_attno;
	static Node bogus_marker = {0};		/* marks conflicting defaults */

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
		ListCell   *rest = lnext(entry);
		ListCell   *prev = entry;

		if (coldef->typeName == NULL)

			/*
			 * Typed table column option that does not belong to a column from
			 * the type.  This works because the columns from the type come
			 * first in the list.
			 */
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" does not exist",
							coldef->colname)));

		while (rest != NULL)
		{
			ColumnDef  *restdef = lfirst(rest);
			ListCell   *next = lnext(rest);		/* need to save it in case we
												 * delete it */

			if (strcmp(coldef->colname, restdef->colname) == 0)
			{
				if (coldef->is_from_type)
				{
					/*
					 * merge the column options into the column from the type
					 */
					coldef->is_not_null = restdef->is_not_null;
					coldef->raw_default = restdef->raw_default;
					coldef->cooked_default = restdef->cooked_default;
					coldef->constraints = restdef->constraints;
					coldef->is_from_type = false;
					list_delete_cell(schema, rest, prev);
				}
				else
					ereport(ERROR,
							(errcode(ERRCODE_DUPLICATE_COLUMN),
							 errmsg("column \"%s\" specified more than once",
									coldef->colname)));
			}
			prev = rest;
			rest = next;
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

		/*
		 * A self-exclusive lock is needed here.  If two backends attempt to
		 * add children to the same parent simultaneously, and that parent has
		 * no pre-existing children, then both will attempt to update the
		 * parent's relhassubclass field, leading to a "tuple concurrently
		 * updated" error.  Also, this interlocks against a concurrent ANALYZE
		 * on the parent table, which might otherwise be attempting to clear
		 * the parent's relhassubclass field, if its previous children were
		 * recently dropped.
		 */
		relation = heap_openrv(parent, ShareUpdateExclusiveLock);

		if (relation->rd_rel->relkind != RELKIND_RELATION)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("inherited relation \"%s\" is not a table",
							parent->relname)));
		/* Permanent rels cannot inherit from temporary ones */
		if (relpersistence != RELPERSISTENCE_TEMP &&
			relation->rd_rel->relpersistence == RELPERSISTENCE_TEMP)
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

		/* If existing rel is temp, it must belong to this session */
		if (relation->rd_rel->relpersistence == RELPERSISTENCE_TEMP &&
			!relation->rd_islocaltemp)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot inherit from temporary relation of another session")));

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
				Oid			defCollId;

				/*
				 * Yes, try to merge the two column definitions. They must
				 * have the same type, typmod, and collation.
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
				typenameTypeIdAndMod(NULL, def->typeName, &defTypeId, &deftypmod);
				if (defTypeId != attribute->atttypid ||
					deftypmod != attribute->atttypmod)
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
						errmsg("inherited column \"%s\" has a type conflict",
							   attributeName),
							 errdetail("%s versus %s",
									   TypeNameToString(def->typeName),
									   format_type_be(attribute->atttypid))));
				defCollId = GetColumnDefCollation(NULL, def, defTypeId);
				if (defCollId != attribute->attcollation)
					ereport(ERROR,
							(errcode(ERRCODE_COLLATION_MISMATCH),
					errmsg("inherited column \"%s\" has a collation conflict",
						   attributeName),
							 errdetail("\"%s\" versus \"%s\"",
									   get_collation_name(defCollId),
							  get_collation_name(attribute->attcollation))));

				/* Copy storage parameter */
				if (def->storage == 0)
					def->storage = attribute->attstorage;
				else if (def->storage != attribute->attstorage)
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
							 errmsg("inherited column \"%s\" has a storage parameter conflict",
									attributeName),
							 errdetail("%s versus %s",
									   storage_name(def->storage),
									   storage_name(attribute->attstorage))));

				def->inhcount++;
				/* Merge of NOT NULL constraints = OR 'em together */
				def->is_not_null |= attribute->attnotnull;
				/* Default and other constraints are handled below */
				newattno[parent_attno - 1] = exist_attno;

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
				def->is_from_type = false;
				def->storage = attribute->attstorage;
				def->raw_default = NULL;
				def->cooked_default = NULL;
				def->collClause = NULL;
				def->collOid = attribute->attcollation;
				def->constraints = NIL;
				def->location = -1;
				inhSchema = lappend(inhSchema, def);
				newattno[parent_attno - 1] = ++child_attno;
			}

			/*
			 * Copy default if any
			 */
			if (attribute->atthasdef)
			{
				Node	   *this_default = NULL;
				AttrDefault *attrdef;
				int			i;

				/* Find default in constraint structure */
				Assert(constr != NULL);
				attrdef = constr->defval;
				for (i = 0; i < constr->num_defval; i++)
				{
					if (attrdef[i].adnum == parent_attno)
					{
						this_default = stringToNode(attrdef[i].adbin);
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
					def->cooked_default = this_default;
				else if (!equal(def->cooked_default, this_default))
				{
					def->cooked_default = &bogus_marker;
					have_bogus_defaults = true;
				}
			}
		}

		/*
		 * Now copy the CHECK constraints of this parent, adjusting attnos
		 * using the completed newattno[] map.  Identically named constraints
		 * are merged if possible, else we throw error.
		 */
		if (constr && constr->num_check > 0)
		{
			ConstrCheck *check = constr->check;
			int			i;

			for (i = 0; i < constr->num_check; i++)
			{
				char	   *name = check[i].ccname;
				Node	   *expr;
				bool		found_whole_row;

				/* ignore if the constraint is non-inheritable */
				if (check[i].ccnoinherit)
					continue;

				/* Adjust Vars to match new table's column numbering */
				expr = map_variable_attnos(stringToNode(check[i].ccbin),
										   1, 0,
										   newattno, tupleDesc->natts,
										   &found_whole_row);

				/*
				 * For the moment we have to reject whole-row variables. We
				 * could convert them, if we knew the new table's rowtype OID,
				 * but that hasn't been assigned yet.
				 */
				if (found_whole_row)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						  errmsg("cannot convert whole-row table reference"),
							 errdetail("Constraint \"%s\" contains a whole-row reference to table \"%s\".",
									   name,
									   RelationGetRelationName(relation))));

				/* check for duplicate */
				if (!MergeCheckConstraint(constraints, name, expr))
				{
					/* nope, this is a new one */
					CookedConstraint *cooked;

					cooked = (CookedConstraint *) palloc(sizeof(CookedConstraint));
					cooked->contype = CONSTR_CHECK;
					cooked->name = pstrdup(name);
					cooked->attnum = 0; /* not used for constraints */
					cooked->expr = expr;
					cooked->skip_validation = false;
					cooked->is_local = false;
					cooked->inhcount = 1;
					cooked->is_no_inherit = false;
					constraints = lappend(constraints, cooked);
				}
			}
		}

		pfree(newattno);

		/*
		 * Close the parent rel, but keep our AccessShareLock on it until xact
		 * commit.  That will prevent someone else from deleting or ALTERing
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
				Oid			defcollid,
							newcollid;

				/*
				 * Yes, try to merge the two column definitions. They must
				 * have the same type, typmod, and collation.
				 */
				ereport((Gp_role == GP_ROLE_EXECUTE) ? DEBUG1 : NOTICE,
				   (errmsg("merging column \"%s\" with inherited definition",
						   attributeName)));
				def = (ColumnDef *) list_nth(inhSchema, exist_attno - 1);
				typenameTypeIdAndMod(NULL, def->typeName, &defTypeId, &deftypmod);
				typenameTypeIdAndMod(NULL, newdef->typeName, &newTypeId, &newtypmod);
				if (defTypeId != newTypeId || deftypmod != newtypmod)
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
							 errmsg("column \"%s\" has a type conflict",
									attributeName),
							 errdetail("%s versus %s",
									   TypeNameToString(def->typeName),
									   TypeNameToString(newdef->typeName))));
				defcollid = GetColumnDefCollation(NULL, def, defTypeId);
				newcollid = GetColumnDefCollation(NULL, newdef, newTypeId);
				if (defcollid != newcollid)
					ereport(ERROR,
							(errcode(ERRCODE_COLLATION_MISMATCH),
							 errmsg("column \"%s\" has a collation conflict",
									attributeName),
							 errdetail("\"%s\" versus \"%s\"",
									   get_collation_name(defcollid),
									   get_collation_name(newcollid))));

				/* Copy storage parameter */
				if (def->storage == 0)
					def->storage = newdef->storage;
				else if (newdef->storage != 0 && def->storage != newdef->storage)
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("column \"%s\" has a storage parameter conflict",
							attributeName),
							 errdetail("%s versus %s",
									   storage_name(def->storage),
									   storage_name(newdef->storage))));

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

			if (def->cooked_default == &bogus_marker)
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
 * MergeCheckConstraint
 *		Try to merge an inherited CHECK constraint with previous ones
 *
 * If we inherit identically-named constraints from multiple parents, we must
 * merge them, or throw an error if they don't have identical definitions.
 *
 * constraints is a list of CookedConstraint structs for previous constraints.
 *
 * Returns TRUE if merged (constraint is a duplicate), or FALSE if it's
 * got a so-far-unique name, or throws error if conflict.
 */
static bool
MergeCheckConstraint(List *constraints, char *name, Node *expr)
{
	ListCell   *lc;

	foreach(lc, constraints)
	{
		CookedConstraint *ccon = (CookedConstraint *) lfirst(lc);

		Assert(ccon->contype == CONSTR_CHECK);

		/* Non-matching names never conflict */
		if (strcmp(ccon->name, name) != 0)
			continue;

		if (equal(expr, ccon->expr))
		{
			/* OK to merge */
			ccon->inhcount++;
			return true;
		}

		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("check constraint name \"%s\" appears multiple times but with different expressions",
						name)));
	}

	return false;
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
	int32		seqNumber;
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
	Datum		values[Natts_pg_inherits];
	bool		nulls[Natts_pg_inherits];
	ObjectAddress childobject,
				parentobject;
	HeapTuple	tuple;

	/*
	 * Make the pg_inherits entry
	 */
	values[Anum_pg_inherits_inhrelid - 1] = ObjectIdGetDatum(relationId);
	values[Anum_pg_inherits_inhparent - 1] = ObjectIdGetDatum(parentOid);
	values[Anum_pg_inherits_inhseqno - 1] = Int32GetDatum(seqNumber);

	memset(nulls, 0, sizeof(nulls));

	tuple = heap_form_tuple(desc, values, nulls);

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
	 * Post creation hook of this inheritance. Since object_access_hook
	 * doesn't take multiple object identifiers, we relay oid of parent
	 * relation using auxiliary_id argument.
	 */
	InvokeObjectPostAlterHookArg(InheritsRelationId,
								 relationId, 0,
								 parentOid, false);

	/*
	 * Mark the parent as having subclasses.
	 */
	SetRelationHasSubclass(parentOid, true);
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
 * SetRelationHasSubclass
 *		Set the value of the relation's relhassubclass field in pg_class.
 *
 * NOTE: caller must be holding an appropriate lock on the relation.
 * ShareUpdateExclusiveLock is sufficient.
 *
 * NOTE: an important side-effect of this operation is that an SI invalidation
 * message is sent out to all backends --- including me --- causing plans
 * referencing the relation to be rebuilt with the new list of children.
 * This must happen even if we find that no change is needed in the pg_class
 * row.
 */
void
SetRelationHasSubclass(Oid relationId, bool relhassubclass)
{
	Relation	relationRelation;
	HeapTuple	tuple;
	Form_pg_class classtuple;

	/*
	 * Fetch a modifiable copy of the tuple, modify it, update pg_class.
	 */
	relationRelation = heap_open(RelationRelationId, RowExclusiveLock);
	tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relationId));
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
 *		renameatt_check			- basic sanity checks before attribute rename
 */
static void
renameatt_check(Oid myrelid, Form_pg_class classform, bool recursing)
{
	char		relkind = classform->relkind;

	if (classform->reloftype && !recursing)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot rename column of typed table")));

	/*
	 * Renaming the columns of sequences or toast tables doesn't actually
	 * break anything from the system's point of view, since internal
	 * references are by attnum.  But it doesn't seem right to allow users to
	 * change names that are hardcoded into the system, hence the following
	 * restriction.
	 */
	if (relkind != RELKIND_RELATION &&
		relkind != RELKIND_VIEW &&
		relkind != RELKIND_MATVIEW &&
		relkind != RELKIND_COMPOSITE_TYPE &&
		relkind != RELKIND_INDEX &&
		relkind != RELKIND_FOREIGN_TABLE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table, view, materialized view, composite type, index, or foreign table",
						NameStr(classform->relname))));

	/*
	 * permissions checking.  only the owner of a class can change its schema.
	 */
	if (!pg_class_ownercheck(myrelid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   NameStr(classform->relname));
	if (!allowSystemTableMods && IsSystemClass(myrelid, classform))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						NameStr(classform->relname))));
}

/*
 *		renameatt_internal		- workhorse for renameatt
 */
static void
renameatt_internal(Oid myrelid,
				   const char *oldattname,
				   const char *newattname,
				   bool recurse,
				   bool recursing,
				   int expected_parents,
				   DropBehavior behavior)
{
	Relation	targetrelation;
	Relation	attrelation;
	HeapTuple	atttup;
	Form_pg_attribute attform;
	int			attnum;

	/*
	 * Grab an exclusive lock on the target table, which we will NOT release
	 * until end of transaction.
	 */
	targetrelation = relation_open(myrelid, AccessExclusiveLock);
	renameatt_check(myrelid, RelationGetForm(targetrelation), recursing);

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
		List	   *child_oids,
				   *child_numparents;
		ListCell   *lo,
				   *li;

		/*
		 * we need the number of parents for each child so that the recursive
		 * calls to renameatt() can determine whether there are any parents
		 * outside the inheritance hierarchy being processed.
		 */
		child_oids = find_all_inheritors(myrelid, AccessExclusiveLock,
										 &child_numparents);

		/*
		 * find_all_inheritors does the recursive search of the inheritance
		 * hierarchy, so all we have to do is process all of the relids in the
		 * list that it returns.
		 */
		forboth(lo, child_oids, li, child_numparents)
		{
			Oid			childrelid = lfirst_oid(lo);
			int			numparents = lfirst_int(li);

			if (childrelid == myrelid)
				continue;
			/* note we need not recurse again */
			renameatt_internal(childrelid, oldattname, newattname, false, true, numparents, behavior);
		}
	}
	else
	{
		/*
		 * If we are told not to recurse, there had better not be any child
		 * tables; else the rename would put them out of step.
		 *
		 * expected_parents will only be 0 if we are not already recursing.
		 */
		if (expected_parents == 0 &&
			find_inheritance_children(myrelid, NoLock) != NIL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("inherited column \"%s\" must be renamed in child tables too",
							oldattname)));
	}

	/* rename attributes in typed tables of composite type */
	if (targetrelation->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
	{
		List	   *child_oids;
		ListCell   *lo;

		child_oids = find_typed_table_dependencies(targetrelation->rd_rel->reltype,
									 RelationGetRelationName(targetrelation),
												   behavior);

		foreach(lo, child_oids)
			renameatt_internal(lfirst_oid(lo), oldattname, newattname, true, true, 0, behavior);
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
	 * if the attribute is inherited, forbid the renaming.  if this is a
	 * top-level call to renameatt(), then expected_parents will be 0, so the
	 * effect of this code will be to prohibit the renaming if the attribute
	 * is inherited at all.  if this is a recursive call to renameatt(),
	 * expected_parents will be the number of parents the current relation has
	 * within the inheritance hierarchy being processed, so we'll prohibit the
	 * renaming only if there are additional parents from elsewhere.
	 */
	if (attform->attinhcount > expected_parents)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot rename inherited column \"%s\"",
						oldattname)));

	/* new name should not already exist */
	check_for_column_name_collision(targetrelation, newattname);

	/* apply the update */
	namestrcpy(&(attform->attname), newattname);

	simple_heap_update(attrelation, &atttup->t_self, atttup);

	/* keep system catalog indexes current */
	CatalogUpdateIndexes(attrelation, atttup);

	InvokeObjectPostAlterHook(RelationRelationId, myrelid, attnum);

	heap_freetuple(atttup);

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
	Relation relrelation;    /* for RELATION relation */
	Relation fakerel;
	HeapTuple reltup;

	fakerel = palloc0(sizeof(RelationData));

	/*
	 * Find relation's pg_class tuple, and make sure newrelname isn't in use.
	 */
	relrelation = heap_open(RelationRelationId, RowExclusiveLock);

	reltup = SearchSysCacheCopy(RELOID,
								ObjectIdGetDatum(myrelid),
								0, 0, 0);
	if (!HeapTupleIsValid(reltup))        /* shouldn't happen */
		elog(ERROR, "cache lookup failed for relation %u", myrelid);
	fakerel->rd_rel = (Form_pg_class) GETSTRUCT(reltup);

	heap_close(relrelation, RowExclusiveLock);

	return fakerel;
}
/*
 * Perform permissions and integrity checks before acquiring a relation lock.
 */
static void
RangeVarCallbackForRenameAttribute(const RangeVar *rv, Oid relid, Oid oldrelid,
								   void *arg)
{
	HeapTuple	tuple;
	Form_pg_class form;

	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tuple))
		return;					/* concurrently dropped */
	form = (Form_pg_class) GETSTRUCT(tuple);
	renameatt_check(relid, form, false);
	ReleaseSysCache(tuple);
}

/*
 *		renameatt		- changes the name of a attribute in a relation
 */
Oid
renameatt(RenameStmt *stmt)
{
	Oid			relid;

	/* lock level taken here should match renameatt_internal */
	relid = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock,
									 stmt->missing_ok, false,
									 RangeVarCallbackForRenameAttribute,
									 NULL);

	if (!OidIsValid(relid))
	{
		ereport(NOTICE,
				(errmsg("relation \"%s\" does not exist, skipping",
						stmt->relation->relname)));
		return InvalidOid;
	}

	renameatt_internal(relid,
					   stmt->subname,	/* old att name */
					   stmt->newname,	/* new att name */
					   interpretInhOption(stmt->relation->inhOpt),		/* recursive? */
					   false,	/* recursing? */
					   0,		/* expected inhcount */
					   stmt->behavior);

	/* This is an ALTER TABLE command so it's about the relid */
	return relid;
}


/*
 * same logic as renameatt_internal
 */
static Oid
rename_constraint_internal(Oid myrelid,
						   Oid mytypid,
						   const char *oldconname,
						   const char *newconname,
						   bool recurse,
						   bool recursing,
						   int expected_parents)
{
	Relation	targetrelation = NULL;
	Oid			constraintOid;
	HeapTuple	tuple;
	Form_pg_constraint con;

	AssertArg(!myrelid || !mytypid);

	if (mytypid)
	{
		constraintOid = get_domain_constraint_oid(mytypid, oldconname, false);
	}
	else
	{
		targetrelation = relation_open(myrelid, AccessExclusiveLock);

		/*
		 * don't tell it whether we're recursing; we allow changing typed
		 * tables here
		 */
		renameatt_check(myrelid, RelationGetForm(targetrelation), false);

		constraintOid = get_relation_constraint_oid(myrelid, oldconname, false);
	}

	tuple = SearchSysCache1(CONSTROID, ObjectIdGetDatum(constraintOid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for constraint %u",
			 constraintOid);
	con = (Form_pg_constraint) GETSTRUCT(tuple);

	if (myrelid && con->contype == CONSTRAINT_CHECK && !con->connoinherit)
	{
		if (recurse)
		{
			List	   *child_oids,
					   *child_numparents;
			ListCell   *lo,
					   *li;

			child_oids = find_all_inheritors(myrelid, AccessExclusiveLock,
											 &child_numparents);

			forboth(lo, child_oids, li, child_numparents)
			{
				Oid			childrelid = lfirst_oid(lo);
				int			numparents = lfirst_int(li);

				if (childrelid == myrelid)
					continue;

				rename_constraint_internal(childrelid, InvalidOid, oldconname, newconname, false, true, numparents);
			}
		}
		else
		{
			if (expected_parents == 0 &&
				find_inheritance_children(myrelid, NoLock) != NIL)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("inherited constraint \"%s\" must be renamed in child tables too",
								oldconname)));
		}

		if (con->coninhcount > expected_parents)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot rename inherited constraint \"%s\"",
							oldconname)));
	}

	if (con->conindid
		&& (con->contype == CONSTRAINT_PRIMARY
			|| con->contype == CONSTRAINT_UNIQUE
			|| con->contype == CONSTRAINT_EXCLUSION))
		/* rename the index; this renames the constraint as well */
		RenameRelationInternal(con->conindid, newconname, false);
	else
		RenameConstraintById(constraintOid, newconname);

	ReleaseSysCache(tuple);

	if (targetrelation)
	{
		/*
		 * Invalidate relcache so as others can see the new constraint name.
		 */
		CacheInvalidateRelcache(targetrelation);

		relation_close(targetrelation, NoLock); /* close rel but keep lock */
	}

	return constraintOid;
}

Oid
RenameConstraint(RenameStmt *stmt)
{
	Oid			relid = InvalidOid;
	Oid			typid = InvalidOid;

	if (stmt->relationType == OBJECT_DOMAIN)
	{
		Relation	rel;
		HeapTuple	tup;

		typid = typenameTypeId(NULL, makeTypeNameFromNameList(stmt->object));
		rel = heap_open(TypeRelationId, RowExclusiveLock);
		tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
		if (!HeapTupleIsValid(tup))
			elog(ERROR, "cache lookup failed for type %u", typid);
		checkDomainOwner(tup);
		ReleaseSysCache(tup);
		heap_close(rel, NoLock);
	}
	else
	{
		/* lock level taken here should match rename_constraint_internal */
		relid = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock,
										 false, false,
										 RangeVarCallbackForRenameAttribute,
										 NULL);
	}

	return
		rename_constraint_internal(relid, typid,
								   stmt->subname,
								   stmt->newname,
		 stmt->relation ? interpretInhOption(stmt->relation->inhOpt) : false,	/* recursive? */
								   false,		/* recursing? */
								   0 /* expected inhcount */ );

}

/*
 * Execute ALTER TABLE/INDEX/SEQUENCE/VIEW/MATERIALIZED VIEW/FOREIGN TABLE
 * RENAME
 */
Oid
RenameRelation(RenameStmt *stmt)
{
	Oid			relid;
	Relation	targetrelation;
	char	   *oldrelname;

	/*
	 * Grab an exclusive lock on the target table, index, sequence, view,
	 * materialized view, or foreign table, which we will NOT release until
	 * end of transaction.
	 *
	 * Lock level used here should match RenameRelationInternal, to avoid lock
	 * escalation.
	 */
	relid = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock,
									 stmt->missing_ok, false,
									 RangeVarCallbackForAlterRelation,
									 (void *) stmt);

	if (!OidIsValid(relid))
	{
		ereport(NOTICE,
				(errmsg("relation \"%s\" does not exist, skipping",
						stmt->relation->relname)));
		return InvalidOid;
	}

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
		targetrelation = fake_relation_open(relid);
	else
		targetrelation = relation_open(relid, AccessExclusiveLock);
	oldrelname = RelationGetRelationName(targetrelation);

	/* if this is a child table of a partitioning configuration, complain */
	if (stmt && rel_is_child_partition(relid) && !stmt->bAllowPartn)
	{
		Oid		 master = rel_partition_get_master(relid);
		char	*pretty	= rel_get_part_path_pretty(relid,
													  " ALTER PARTITION ",
													  " RENAME PARTITION ");
		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
						errmsg("cannot rename partition \"%s\" directly",
							   get_rel_name(relid)),
						errhint("Table \"%s\" is a child partition of table "
								"\"%s\".  To rename it, use ALTER TABLE \"%s\"%s...",
								get_rel_name(relid), get_rel_name(master),
								get_rel_name(master), pretty ? pretty : "" )));
	}

	/* Do the work */
	RenameRelationInternal(relid, stmt->newname, false);

	/* MPP-3059: recursive rename of partitioned table */
	/* Note: the top-level RENAME has bAllowPartn=FALSE, while the
	 * generated statements from this block set it to TRUE.  That way,
	 * the rename of each partition is allowed, but this block doesn't
	 * get invoked recursively.
	 */
	if (stmt && !rel_is_child_partition(relid) && !stmt->bAllowPartn &&
		(Gp_role == GP_ROLE_DISPATCH))
	{
		PartitionNode *pNode;

		pNode = RelationBuildPartitionDescByOid(relid, false);

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
			renList = atpxRenameList(pNode, oldrelname, stmt->newname, &skipped);

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
								   PROCESS_UTILITY_QUERY,
								   NULL,
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

	return relid;
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
RenameRelationInternal(Oid myrelid, const char *newrelname, bool is_internal)
{
	Relation	targetrelation;
	Relation	relrelation;	/* for RELATION relation */
	HeapTuple	reltup;
	Form_pg_class relform;
	Oid			namespaceId;

	/*
	 * In Postgres:
	 * Grab an exclusive lock on the target table, index, sequence, view,
	 * materialized view, or foreign table, which we will NOT release until
	 * end of transaction.
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
	namespaceId = RelationGetNamespace(targetrelation);

	/*
	 * Find relation's pg_class tuple, and make sure newrelname isn't in use.
	 */
	relrelation = heap_open(RelationRelationId, RowExclusiveLock);

	reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(myrelid));
	if (!HeapTupleIsValid(reltup))		/* shouldn't happen */
		elog(ERROR, "cache lookup failed for relation %u", myrelid);
	relform = (Form_pg_class) GETSTRUCT(reltup);

	if (get_relname_relid(newrelname, namespaceId) != InvalidOid)
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_TABLE),
				 errmsg("relation \"%s\" already exists",
						newrelname)));

	/*
	 * Update pg_class tuple with new relname.  (Scribbling on reltup is OK
	 * because it's a copy...)
	 */
	namestrcpy(&(relform->relname), newrelname);

	simple_heap_update(relrelation, &reltup->t_self, reltup);

	/* keep the system catalog indexes current */
	CatalogUpdateIndexes(relrelation, reltup);

	InvokeObjectPostAlterHookArg(RelationRelationId, myrelid, 0,
								 InvalidOid, is_internal);

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
		( ! ( (PG_CATALOG_NAMESPACE == namespaceId) && (allowSystemTableMods)))
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
 * We need this check because our lock doesn't protect us against stomping
 * on our own foot, only other people's feet!
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
				case AT_PartDrop:
				case AT_PartExchange:
				case AT_PartRename:
				case AT_PartSetTemplate:
				case AT_PartSplit:
				case AT_PartTruncate:
				case AT_PartAddInternal:
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("unsupported ALTER command for external table")));
					break;

				case AT_AddColumn: /* check no constraint is added too */
					if (((ColumnDef *) cmd->def)->constraints != NIL ||
						((ColumnDef *) cmd->def)->is_not_null ||
						((ColumnDef *) cmd->def)->raw_default)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_COLUMN_DEFINITION),
								 errmsg("unsupported ALTER command for external table"),
								 errdetail("No constraints allowed.")));
					break;

				default:
					/* ALTER type supported */
					break;
			}
		}
	}
}

/*
 * AlterTableLookupRelation
 *		Look up, and lock, the OID for the relation named by an alter table
 *		statement.
 */
Oid
AlterTableLookupRelation(AlterTableStmt *stmt, LOCKMODE lockmode)
{
	return RangeVarGetRelidExtended(stmt->relation, lockmode, stmt->missing_ok, false,
									RangeVarCallbackForAlterRelation,
									(void *) stmt);
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
 * it.  The intention of this design is to allow multiple independent
 * updates of the table schema to be performed with only one pass over the
 * data.
 *
 * ATPrepCmd performs phase 1.  A "work queue" entry is created for
 * each table to be affected (there may be multiple affected tables if the
 * commands traverse a table inheritance hierarchy).  Also we do preliminary
 * validation of the subcommands, including parse transformation of those
 * expressions that need to be evaluated with respect to the old table
 * schema.
 *
 * ATRewriteCatalogs performs phase 2 for each affected table.  (Note that
 * phases 2 and 3 normally do no explicit recursion, since phase 1 already
 * did it --- although some subcommands have to recurse in phase 2 instead.)
 * Certain subcommands need to be performed before others to avoid
 * unnecessary conflicts; for example, DROP COLUMN should come before
 * ADD COLUMN.  Therefore phase 1 divides the subcommands into multiple
 * lists, one for each logical "pass" of phase 2.
 *
 * ATRewriteTables performs phase 3 for those tables that need it.
 *
 * Thanks to the magic of MVCC, an error anywhere along the way rolls back
 * the whole operation; we don't have to do anything special to clean up.
 *
 * The caller must lock the relation, with an appropriate lock level
 * for the subcommands requested, using AlterTableGetLockLevel(stmt->cmds)
 * or higher. We pass the lock level down
 * so that we can apply it recursively to inherited tables. Note that the
 * lock level we want as we recurse might well be higher than required for
 * that specific subcommand. So we pass down the overall lock requirement,
 * rather than reassess it at lower levels.
 */
void
AlterTable(Oid relid, LOCKMODE lockmode, AlterTableStmt *stmt)
{
	Relation	rel;

	/* Caller is required to provide an adequate lock. */
	rel = relation_open(relid, NoLock);

	ATVerifyObject(stmt, rel);

	CheckTableNotInUse(rel, "ALTER TABLE");

	ATController(rel, stmt->cmds, interpretInhOption(stmt->relation->inhOpt),
				 lockmode);

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
	Relation	rel;
	LOCKMODE	lockmode = AlterTableGetLockLevel(cmds);

	rel = relation_open(relid, lockmode);

	ATController(rel, cmds, recurse, lockmode);
}

/*
 * AlterTableGetLockLevel
 *
 * Sets the overall lock level required for the supplied list of subcommands.
 * Policy for doing this set according to needs of AlterTable(), see
 * comments there for overall explanation.
 *
 * Function is called before and after parsing, so it must give same
 * answer each time it is called. Some subcommands are transformed
 * into other subcommand types, so the transform must never be made to a
 * lower lock level than previously assigned. All transforms are noted below.
 *
 * Since this is called before we lock the table we cannot use table metadata
 * to influence the type of lock we acquire.
 *
 * There should be no lockmodes hardcoded into the subcommand functions. All
 * lockmode decisions for ALTER TABLE are made here only. The one exception is
 * ALTER TABLE RENAME which is treated as a different statement type T_RenameStmt
 * and does not travel through this section of code and cannot be combined with
 * any of the subcommands given here.
 *
 * Note that Hot Standby only knows about AccessExclusiveLocks on the master
 * so any changes that might affect SELECTs running on standbys need to use
 * AccessExclusiveLocks even if you think a lesser lock would do, unless you
 * have a solution for that also.
 *
 * Also note that pg_dump uses only an AccessShareLock, meaning that anything
 * that takes a lock less than AccessExclusiveLock can change object definitions
 * while pg_dump is running. Be careful to check that the appropriate data is
 * derived by pg_dump using an MVCC snapshot, rather than syscache lookups,
 * otherwise we might end up with an inconsistent dump that can't restore.
 */
LOCKMODE
AlterTableGetLockLevel(List *cmds)
{
	/*
	 * This only works if we read catalog tables using MVCC snapshots.
	 */
	ListCell   *lcmd;
	LOCKMODE	lockmode = ShareUpdateExclusiveLock;

	foreach(lcmd, cmds)
	{
		AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);
		LOCKMODE	cmd_lockmode = AccessExclusiveLock; /* default for compiler */

		switch (cmd->subtype)
		{
				/*
				 * These subcommands rewrite the heap, so require full locks.
				 */
			case AT_AddColumn:	/* may rewrite heap, in some cases and visible
								 * to SELECT */
			case AT_AddColumnRecurse:
			case AT_SetTableSpace:		/* must rewrite heap */
			case AT_AlterColumnType:	/* must rewrite heap */
			case AT_AddOids:	/* must rewrite heap */
			case AT_AddOidsRecurse:
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * These subcommands may require addition of toast tables. If
				 * we add a toast table to a table currently being scanned, we
				 * might miss data added to the new toast table by concurrent
				 * insert transactions.
				 */
			case AT_SetStorage:/* may add toast tables, see
								 * ATRewriteCatalogs() */
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * Removing constraints can affect SELECTs that have been
				 * optimised assuming the constraint holds true.
				 */
			case AT_DropConstraint:		/* as DROP INDEX */
			case AT_DropConstraintRecurse:
			case AT_DropNotNull:		/* may change some SQL plans */
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * Subcommands that may be visible to concurrent SELECTs
				 */
			case AT_DropColumn:	/* change visible to SELECT */
			case AT_DropColumnRecurse:	/* change visible to SELECT */
			case AT_AddColumnToView:	/* CREATE VIEW */
			case AT_DropOids:	/* calls AT_DropColumn */
			case AT_EnableAlwaysRule:	/* may change SELECT rules */
			case AT_EnableReplicaRule:	/* may change SELECT rules */
			case AT_EnableRule:	/* may change SELECT rules */
			case AT_DisableRule:		/* may change SELECT rules */
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * Changing owner may remove implicit SELECT privileges
				 */
			case AT_ChangeOwner:		/* change visible to SELECT */
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * Changing foreign table options may affect optimisation.
				 */
			case AT_GenericOptions:
			case AT_AlterColumnGenericOptions:
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * These subcommands affect write operations only. XXX
				 * Theoretically, these could be ShareRowExclusiveLock.
				 */
			case AT_ColumnDefault:
			case AT_ColumnDefaultRecurse:
			case AT_ProcessedConstraint:		/* becomes AT_AddConstraint */
			case AT_AddConstraintRecurse:		/* becomes AT_AddConstraint */
			case AT_ReAddConstraint:	/* becomes AT_AddConstraint */
			case AT_EnableTrig:
			case AT_EnableAlwaysTrig:
			case AT_EnableReplicaTrig:
			case AT_EnableTrigAll:
			case AT_EnableTrigUser:
			case AT_DisableTrig:
			case AT_DisableTrigAll:
			case AT_DisableTrigUser:
			case AT_AlterConstraint:
			case AT_AddIndex:	/* from ADD CONSTRAINT */
			case AT_AddIndexConstraint:
			case AT_ReplicaIdentity:
			case AT_SetNotNull:
				cmd_lockmode = AccessExclusiveLock;
				break;

			case AT_AddConstraint:
				if (IsA(cmd->def, Constraint))
				{
					Constraint *con = (Constraint *) cmd->def;

					switch (con->contype)
					{
						case CONSTR_EXCLUSION:
						case CONSTR_PRIMARY:
						case CONSTR_UNIQUE:

							/*
							 * Cases essentially the same as CREATE INDEX. We
							 * could reduce the lock strength to ShareLock if
							 * we can work out how to allow concurrent catalog
							 * updates. XXX Might be set down to
							 * ShareRowExclusiveLock but requires further
							 * analysis.
							 */
							cmd_lockmode = AccessExclusiveLock;
							break;
						case CONSTR_FOREIGN:

							/*
							 * We add triggers to both tables when we add a
							 * Foreign Key, so the lock level must be at least
							 * as strong as CREATE TRIGGER. XXX Might be set
							 * down to ShareRowExclusiveLock though trigger
							 * info is accessed by pg_get_triggerdef
							 */
							cmd_lockmode = AccessExclusiveLock;
							break;

						default:
							cmd_lockmode = AccessExclusiveLock;
					}
				}
				break;

				/*
				 * These subcommands affect inheritance behaviour. Queries
				 * started before us will continue to see the old inheritance
				 * behaviour, while queries started after we commit will see
				 * new behaviour. No need to prevent reads or writes to the
				 * subtable while we hook it up though. Changing the TupDesc
				 * may be a problem, so keep highest lock.
				 */
			case AT_AddInherit:
			case AT_DropInherit:
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * These subcommands affect implicit row type conversion. They
				 * have affects similar to CREATE/DROP CAST on queries. don't
				 * provide for invalidating parse trees as a result of such
				 * changes, so we keep these at AccessExclusiveLock.
				 */
			case AT_AddOf:
			case AT_DropOf:
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * Only used by CREATE OR REPLACE VIEW which must conflict
				 * with an SELECTs currently using the view.
				 */
			case AT_ReplaceRelOptions:
				cmd_lockmode = AccessExclusiveLock;
				break;

				/*
				 * These subcommands affect general strategies for performance
				 * and maintenance, though don't change the semantic results
				 * from normal data reads and writes. Delaying an ALTER TABLE
				 * behind currently active writes only delays the point where
				 * the new strategy begins to take effect, so there is no
				 * benefit in waiting. In this case the minimum restriction
				 * applies: we don't currently allow concurrent catalog
				 * updates.
				 */
			case AT_SetStatistics:		/* Uses MVCC in getTableAttrs() */
			case AT_ClusterOn:	/* Uses MVCC in getIndexes() */
			case AT_DropCluster:		/* Uses MVCC in getIndexes() */
			case AT_SetOptions:	/* Uses MVCC in getTableAttrs() */
			case AT_ResetOptions:		/* Uses MVCC in getTableAttrs() */
				cmd_lockmode = ShareUpdateExclusiveLock;
				break;

			case AT_ValidateConstraint: /* Uses MVCC in
												 * getConstraints() */
			case AT_ValidateConstraintRecurse:
				cmd_lockmode = ShareUpdateExclusiveLock;
				break;

				/*
				 * Rel options are more complex than first appears. Options
				 * are set here for tables, views and indexes; for historical
				 * reasons these can all be used with ALTER TABLE, so we can't
				 * decide between them using the basic grammar.
				 *
				 * XXX Look in detail at each option to determine lock level,
				 * e.g. cmd_lockmode = GetRelOptionsLockLevel((List *)
				 * cmd->def);
				 */
			case AT_SetRelOptions:		/* Uses MVCC in getIndexes() and
										 * getTables() */
			case AT_ResetRelOptions:	/* Uses MVCC in getIndexes() and
										 * getTables() */
				cmd_lockmode = AccessExclusiveLock;
				break;

				/* GPDB additions */
			case AT_ExpandTable:
			case AT_SetDistributedBy:
			case AT_PartAdd:
			case AT_PartAddForSplit:
			case AT_PartAlter:
			case AT_PartDrop:
			case AT_PartExchange:
			case AT_PartRename:
			case AT_PartSetTemplate:
			case AT_PartSplit:
			case AT_PartTruncate:
			case AT_PartAddInternal:
			case AT_PartAttachIndex:
				cmd_lockmode = AccessExclusiveLock;
				break;

			default:			/* oops */
				elog(ERROR, "unrecognized alter table type: %d",
					 (int) cmd->subtype);
				break;
		}

		/*
		 * Take the greatest lockmode from any subcommand
		 */
		if (cmd_lockmode > lockmode)
			lockmode = cmd_lockmode;
	}

	return lockmode;
}

static void
ATController(Relation rel, List *cmds, bool recurse, LOCKMODE lockmode)
{
	List	   *wqueue = NIL;
	ListCell   *lcmd;
	bool is_partition = false;
	bool is_external = RelationIsExternal(rel);

	cdb_sync_oid_to_segments();

	/* Phase 1: preliminary examination of commands, create work queue */
	foreach(lcmd, cmds)
	{
		AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);

		if (cmd->subtype == AT_PartAddInternal)
			is_partition = true;
		else if (cmd->subtype == AT_AddOids && Gp_role == GP_ROLE_DISPATCH)
		{
			if (RelationIsAoCols(rel))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("OIDS=TRUE is not allowed on tables that use column-oriented storage"),
						 errhint("Use OIDS=FALSE.")));
			else
				ereport(NOTICE,
						(errmsg("OIDS=TRUE is not recommended for user-created tables"),
						 errhint("Use OIDS=FALSE to prevent wrap-around of the OID counter.")));
		}

		ATPrepCmd(&wqueue, rel, cmd, recurse, false, lockmode);
	}

	if (is_partition)
		relation_close(rel, AccessExclusiveLock);
	else
		/* Close the relation, but keep lock until commit */
		relation_close(rel, NoLock);

	/* Phase 2: update system catalogs */
	ATRewriteCatalogs(&wqueue, lockmode);

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
						if (masterSetCmd->backendId == 0)
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
	 *
	 * We should skip this for foreign table as well, but it's handled inside of
	 * ATRewriteTables() and ATAddToastIfNeeded(), we keep it that way to align
	 * with upstream.
	 */
	if (!is_external)
	{
		ATRewriteTables(&wqueue, lockmode);

		ATAddToastIfNeeded(&wqueue, lockmode);
	}
}


/*
 * prepSplitCmd
 *
 * Do initial sanity checking for an ALTER TABLE ... SPLIT PARTITION cmd.
 * - Shouldn't have children
 * - The usual permissions checks
 */
static void
prepSplitCmd(Relation rel, PgPartRule *prule, bool is_at)
{
	PartitionNode		*pNode = NULL;
	pNode = RelationBuildPartitionDesc(rel, false);

	if (prule->topRule->children)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("cannot split partition with child "
									   "partitions"),
						errhint("Try splitting the child partitions.")));

	}

	if (prule->topRule->parisdefault &&
		prule->pNode->part->parkind == 'r' &&
		is_at)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("AT clause cannot be used when splitting "
						"a default RANGE partition")));
	}
	else if ((prule->pNode->part->parkind == 'l') && (pNode !=NULL && pNode->default_part)
			 && (pNode->default_part->children))
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("SPLIT PARTITION is not "
								"currently supported when leaf partition is "
								"list partitioned in multi level partition table")));
		}
	else if ((prule->pNode->part->parkind == 'l') && !is_at)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("cannot SPLIT DEFAULT PARTITION "
						"with LIST"),
				errhint("Use SPLIT with the AT clause instead.")));

	}
}

/*
 * ATPrepCmd
 *
 * Traffic cop for ALTER TABLE Phase 1 operations, including simple
 * recursion and permission checks.
 *
 * Caller must have acquired appropriate lock type on relation already.
 * This lock should be held until commit.
 */
static void
ATPrepCmd(List **wqueue, Relation rel, AlterTableCmd *cmd,
		  bool recurse, bool recursing, LOCKMODE lockmode)
{
	AlteredTableInfo *tab;
	int			pass = AT_PASS_UNSET;

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
			ATSimplePermissions(rel,
						 ATT_TABLE | ATT_COMPOSITE_TYPE | ATT_FOREIGN_TABLE);
			/*
			 * test that this is allowed for partitioning, but only if we aren't
			 * recursing.
			 */
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);

			/*
			 * (!recurse) indicate that we can not only add column to root
			 * partition
			 */
			ATPartitionCheck(cmd->subtype, rel, !recurse /* rejectroot */, recursing);
			/* Performs own recursion */
			ATPrepAddColumn(wqueue, rel, recurse, recursing, cmd, lockmode);
			/* Recursion occurs during execution phase */
			pass = AT_PASS_ADD_COL;
			break;
		case AT_AddColumnRecurse:		/* ADD COLUMN internal */
		case AT_AddOidsRecurse:			/* SET WITH OIDS internal */
			Assert(Gp_role == GP_ROLE_EXECUTE);
			/* No need to do ATPartitionCheck */
			ATPrepAddColumn(wqueue, rel, recurse, recursing, cmd, lockmode);
			/* Recursion occurs during execution phase */
			pass = AT_PASS_ADD_COL;
			break;
		case AT_AddColumnToView:		/* add column via CREATE OR REPLACE
										 * VIEW */
			ATSimplePermissions(rel, ATT_VIEW);
			ATPrepAddColumn(wqueue, rel, recurse, recursing, cmd, lockmode);
			/* Recursion occurs during execution phase */
			pass = AT_PASS_ADD_COL;
			break;
		case AT_ColumnDefault:	/* ALTER COLUMN DEFAULT */

			/*
			 * We allow defaults on views so that INSERT into a view can have
			 * default-ish behavior.  This works because the rewriter
			 * substitutes default values into INSERTs before it expands
			 * rules.
			 */
			ATSimplePermissions(rel, ATT_TABLE | ATT_VIEW | ATT_FOREIGN_TABLE);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			ATPrepColumnDefault(rel, recurse, cmd);
			pass = ((ColumnDef *)(cmd->def))->raw_default ?
					AT_PASS_ADD_CONSTR : AT_PASS_DROP;
			break;
		case AT_ColumnDefaultRecurse:	/* ALTER COLUMN DEFAULT */
			ATSimplePermissions(rel, ATT_TABLE | ATT_VIEW | ATT_FOREIGN_TABLE);
			/*
			 * This is an internal Alter Table command to add default into child
			 * tables. Therefore, no need to do ATPartitionCheck.
			 */
			ATPrepColumnDefault(rel, recurse, cmd);
			pass = ((ColumnDef *)(cmd->def))->raw_default ?
					AT_PASS_ADD_CONSTR : AT_PASS_DROP;
			break;
		case AT_DropNotNull:	/* ALTER COLUMN DROP NOT NULL */
			ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
			/* No command-specific prep needed */
			pass = AT_PASS_DROP;
			break;
		case AT_SetNotNull:		/* ALTER COLUMN SET NOT NULL */
			ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
			if (!cmd->part_expanded)
				ATPartitionCheck(cmd->subtype, rel, false, recursing);
			/* No command-specific prep needed */
			pass = AT_PASS_ADD_CONSTR;
			break;
		case AT_SetStatistics:	/* ALTER COLUMN SET STATISTICS */
			ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
			/* Performs own permission checks */
			ATPrepSetStatistics(rel, cmd->name, cmd->def, lockmode);
			pass = AT_PASS_MISC;
			break;
		case AT_SetOptions:		/* ALTER COLUMN SET ( options ) */
		case AT_ResetOptions:	/* ALTER COLUMN RESET ( options ) */
			ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW | ATT_INDEX | ATT_FOREIGN_TABLE);
			/* This command never recurses */
			pass = AT_PASS_MISC;
			break;
		case AT_SetStorage:		/* ALTER COLUMN SET STORAGE */
			ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW);
			ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;
		case AT_DropColumn:		/* DROP COLUMN */
		case AT_DropColumnRecurse:
			ATSimplePermissions(rel,
						 ATT_TABLE | ATT_COMPOSITE_TYPE | ATT_FOREIGN_TABLE);
			ATPrepDropColumn(wqueue, rel, recurse, recursing, cmd, lockmode);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);

			/*
			 * (!recurse) indicate that we can not only drop column to root
			 * partition
			 */
			ATPartitionCheck(cmd->subtype, rel, !recurse /* rejectroot */, recursing);
			/* Recursion occurs during execution phase */
			pass = AT_PASS_DROP;
			break;
		case AT_AddIndex:		/* ADD INDEX */
			ATSimplePermissions(rel, ATT_TABLE);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			/*
			 * Any recursion for partitioning is done in ATExecAddIndex()
			 * itself However, if the index supports a PK or UNIQUE constraint
			 * and the relation is partitioned, we need to assure the
			 * constraint is named in a reasonable way.
			 */
			{
				ConstrType contype = CONSTR_NULL; /* name picker will reject this */
				IndexStmt *topindexstmt = (IndexStmt*)cmd->def;
				Insist(IsA(topindexstmt, IndexStmt));
				if (topindexstmt->isconstraint)
				{
					switch (rel_part_status(RelationGetRelid(rel)))
					{
						case PART_STATUS_ROOT:
 							break;
 						case PART_STATUS_INTERIOR:
 						case PART_STATUS_LEAF:
							/* Don't do this check for child parts (= internal requests). */
 							if (!topindexstmt->is_part_child)
 							{
 								char	*what;

 								if (contype == CONSTR_PRIMARY)
 									what = pstrdup("PRIMARY KEY");
								else
									what = pstrdup("UNIQUE");

 								ereport(ERROR,
 										(errcode(ERRCODE_WRONG_OBJECT_TYPE),
 										 errmsg("can't place a %s constraint on just part of partitioned table \"%s\"", 
 												what, RelationGetRelationName(rel)),
 										 errhint("Constrain the whole table or create a part-wise UNIQUE index instead.")));
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
			ATSimplePermissions(rel, ATT_TABLE);
			/*
			 * (!recurse &&  !recursing) is supposed to detect the ONLY clause.
			 * We allow operations on the root of a partitioning hierarchy, but
			 * not ONLY the root.
			 */
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);
			ATPartitionCheck(cmd->subtype, rel, (!recurse && !recursing), recursing);
			/* Recursion occurs during execution phase */
			/* No command-specific prep needed except saving recurse flag */
			if (recurse)
				cmd->subtype = AT_AddConstraintRecurse;
			pass = AT_PASS_ADD_CONSTR;
			break;
		case AT_AddConstraintRecurse: /* ADD check CONSTRAINT internal */
			/* Parent/Base CHECK constraints apply to child/part tables here.
			 * No need for ATPartitionCheck
			 */
			ATSimplePermissions(rel, ATT_TABLE);
			pass = AT_PASS_ADD_CONSTR;
			break;
		case AT_AddIndexConstraint:		/* ADD CONSTRAINT USING INDEX */
			ATSimplePermissions(rel, ATT_TABLE);
			/* This command never recurses */
			/* No command-specific prep needed */
			pass = AT_PASS_ADD_CONSTR;
			break;
		case AT_DropConstraint:	/* DROP CONSTRAINT */
		case AT_DropConstraintRecurse:
			ATSimplePermissions(rel, ATT_TABLE);

			/* In GPDB, we have some extra work to do because of partitioning */
			ATPrepDropConstraint(wqueue, rel, cmd, recurse, recursing);

			/* Recursion occurs during execution phase */
			/* No command-specific prep needed except saving recurse flag */
			if (recurse)
				cmd->subtype = AT_DropConstraintRecurse;
			pass = AT_PASS_DROP;
			break;
		case AT_AlterColumnType:		/* ALTER COLUMN TYPE */
			ATSimplePermissions(rel,
						 ATT_TABLE | ATT_COMPOSITE_TYPE | ATT_FOREIGN_TABLE);
			ATExternalPartitionCheck(cmd->subtype, rel, recursing);

			/*
			 * (!recurse) indicate that we can not only alter type to root
			 * partition
			 */
			ATPartitionCheck(cmd->subtype, rel, !recurse /* rejectroot */, recursing);
			/* Performs own recursion */
			ATPrepAlterColumnType(wqueue, tab, rel, recurse, recursing, cmd, lockmode);
			pass = AT_PASS_ALTER_TYPE;
			break;
		case AT_AlterColumnGenericOptions:
			ATSimplePermissions(rel, ATT_FOREIGN_TABLE);
			/* This command never recurses */
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
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
					ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);

				pass = AT_PASS_MISC;
			}
			break;
		case AT_ClusterOn:		/* CLUSTER ON */
		case AT_DropCluster:	/* SET WITHOUT CLUSTER */
			ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW);
			/* These commands never recurse */
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;
		case AT_AddOids:		/* SET WITH OIDS */
			ATSimplePermissions(rel, ATT_TABLE);
			if (!rel->rd_rel->relhasoids || recursing)
				ATPrepAddOids(wqueue, rel, recurse, cmd, lockmode);
			/* Recursion occurs during execution phase */
			pass = AT_PASS_ADD_COL;
			break;
		case AT_DropOids:		/* SET WITHOUT OIDS */
			ATSimplePermissions(rel, ATT_TABLE);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			/* Performs own recursion */
			if (rel->rd_rel->relhasoids)
			{
				AlterTableCmd *dropCmd = makeNode(AlterTableCmd);

				dropCmd->subtype = AT_DropColumn;
				dropCmd->name = pstrdup("oid");
				dropCmd->behavior = cmd->behavior;
				ATPrepCmd(wqueue, rel, dropCmd, recurse, false, lockmode);
			}
			pass = AT_PASS_DROP;
			break;
		case AT_SetTableSpace:	/* SET TABLESPACE */
			ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW | ATT_INDEX);
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
				ATPrepSetTableSpace(tab, rel, cmd->name, lockmode);
			pass = AT_PASS_MISC;	/* doesn't actually matter */
			break;
		case AT_SetRelOptions:	/* SET (...) */
		case AT_ResetRelOptions:		/* RESET (...) */
		case AT_ReplaceRelOptions:		/* reset them all, then set just these */
			ATSimplePermissions(rel, ATT_TABLE | ATT_VIEW | ATT_MATVIEW | ATT_INDEX);
			/* This command never recurses */
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;
		case AT_SetDistributedBy:	/* SET DISTRIBUTED BY */
			ATSimplePermissions(rel, ATT_TABLE);
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
							break;
						case PART_STATUS_LEAF:
						case PART_STATUS_INTERIOR:
							/*Reject interior branches of partitioned tables.*/
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
									 errmsg("can't set the distribution policy of \"%s\"",
											RelationGetRelationName(rel)),
									 errhint("Distribution policy can be set for an entire partitioned table, not for one of its leaf parts or an interior branch.")));
							break; /* tidy */
					}
				}
				else /* ALTER TABLE ONLY */
				{
					switch (ps)
					{
						case PART_STATUS_NONE:
							break;
						case PART_STATUS_LEAF:
						case PART_STATUS_ROOT:
						case PART_STATUS_INTERIOR:
							ereport(ERROR,
									(errcode(ERRCODE_WRONG_OBJECT_TYPE),
									 errmsg("can't set the distribution policy of ONLY \"%s\"",
											RelationGetRelationName(rel)),
									 errhint("Distribution policy can be set for an entire partitioned table, not for one of its leaf parts or an interior branch.")));
							break; /* tidy */
					}
				}
			}

			ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
			pass = AT_PASS_MISC;
			break;
		case AT_ExpandTable:
			ATSimplePermissions(rel, ATT_TABLE);
			if (!recursing)
			{
				Oid relid = RelationGetRelid(rel);
				PartStatus ps = rel_part_status(relid);

				ATExternalPartitionCheck(cmd->subtype, rel, recursing);

				if (Gp_role == GP_ROLE_DISPATCH &&
					rel->rd_cdbpolicy->numsegments == getgpsegmentCount())
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("cannot expand table \"%s\"",
									RelationGetRelationName(rel)),
							 errdetail("table has already been expanded")));

				switch (ps)
				{
					case PART_STATUS_NONE:
					case PART_STATUS_ROOT:
						break;

					case PART_STATUS_INTERIOR:
					case PART_STATUS_LEAF:
						ereport(ERROR,
								(errcode(ERRCODE_WRONG_OBJECT_TYPE),
								 errmsg("cannot expand leaf or interior partition \"%s\"",
										RelationGetRelationName(rel)),
								 errdetail("root/leaf/interior partitions need to have same numsegments"),
								 errhint("use \"ALTER TABLE %s EXPAND TABLE\" instead",
										 get_rel_name(rel_partition_get_master(relid)))));
						break;
				}
			}

			ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
			pass = AT_PASS_MISC;
			break;

		case AT_AddInherit:		/* INHERIT */
			ATSimplePermissions(rel, ATT_TABLE);
			ATPartitionCheck(cmd->subtype, rel, true, recursing);
			/* This command never recurses */
			ATPrepAddInherit(rel);
			pass = AT_PASS_MISC;
			break;
		case AT_AlterConstraint:		/* ALTER CONSTRAINT */
			ATSimplePermissions(rel, ATT_TABLE);
			pass = AT_PASS_MISC;
			break;
		case AT_ValidateConstraint:		/* VALIDATE CONSTRAINT */
			ATSimplePermissions(rel, ATT_TABLE);
			/* Recursion occurs during execution phase */
			/* No command-specific prep needed except saving recurse flag */
			if (recurse)
				cmd->subtype = AT_ValidateConstraintRecurse;
			pass = AT_PASS_MISC;
			break;
		case AT_ValidateConstraintRecurse: /* ADD validate CONSTRAINT internal */
			/* Parent/Base CHECK constraints apply to child/part tables here.
			 * No need for ATPartitionCheck
			 */
			ATSimplePermissions(rel, ATT_TABLE);
			pass = AT_PASS_MISC;
			break;
		case AT_ReplicaIdentity:		/* REPLICA IDENTITY ... */
			ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW);
			pass = AT_PASS_MISC;
			/* This command never recurses */
			/* No command-specific prep needed */
			break;
		case AT_EnableTrig:		/* ENABLE TRIGGER variants */
		case AT_EnableAlwaysTrig:
		case AT_EnableReplicaTrig:
		case AT_EnableTrigAll:
		case AT_EnableTrigUser:
		case AT_DisableTrig:	/* DISABLE TRIGGER variants */
		case AT_DisableTrigAll:
		case AT_DisableTrigUser:
			ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
			pass = AT_PASS_MISC;
			break;
		case AT_EnableRule:		/* ENABLE/DISABLE RULE variants */
		case AT_EnableAlwaysRule:
		case AT_EnableReplicaRule:
		case AT_DisableRule:
		case AT_DropInherit:	/* NO INHERIT */
		case AT_AddOf:			/* OF */
		case AT_DropOf: /* NOT OF */
			ATSimplePermissions(rel, ATT_TABLE);
			ATPartitionCheck(cmd->subtype, rel, true, recursing);
			/* These commands never recurse */
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;
		case AT_GenericOptions:
			ATSimplePermissions(rel, ATT_FOREIGN_TABLE);
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
				ParseState *pstate = make_parsestate(NULL);
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
				 * 0.5) Shouldn't have children (Done in prepSplitCmd)
				 * 1) The usual permissions checks (Done in prepSplitCmd)
				 * 2) AT () parameter falls into constraint specified
				 * 3) INTO partitions don't exist except for the one being split
				 */

				/* We'll error out if it doesn't exist */
				prule1 = get_part_rule(rel, pid, true, true, NULL, false);

				target = heap_open(prule1->topRule->parchildrelid,
								   AccessExclusiveLock);

				if (linitial((List *)pc->arg1))
					is_at = false;

				prepSplitCmd(rel, prule1, is_at);

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

						vals = (List *)transformExpressionList(pstate, vals,
															   EXPR_KIND_PARTITION_EXPRESSION);

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

							n = coerce_partition_value(NULL, n, typid, typmod, t);

							lfirst(lc2) = n;

							i++;
						}
						lfirst(lc) = vals;
					}

					assign_list_collations(pstate, l1);

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

				free_parsestate(pstate);

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

									d = OidFunctionCall2Coll(funcid,
															 c->constcollid,
															 c->constvalue,
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
										 errmsg("AT clause parameter is not a member of the target partition specification")));


						}

						if (nmatches >= list_length(lv))
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("AT clause cannot contain all values in partition%s",
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
							res = OidFunctionCall2Coll(funcid,
													   atval->constcollid,
													   start->constvalue,
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
								res = OidFunctionCall2Coll(funcid,
														   atval->constcollid,
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
									 errmsg("AT clause parameter is not a member of the target partition specification")));
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
				 *   prule3 refers to the second INTO part
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
				
				ATSimplePermissions(target, ATT_TABLE);
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
						errmsg("cannot modify subpartition template for partitioned table")));
			}
			/* FALL THRU */
		case AT_PartAdd:				/* Add */
		case AT_PartAddForSplit:		/* Add, as part of a split */
		case AT_PartDrop:				/* Drop */
		case AT_PartRename:				/* Rename */

		case AT_PartTruncate:			/* Truncate */
		case AT_PartAddInternal:		/* internal partition creation */
			ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
			ATPartitionCheck(cmd->subtype, rel, false, recursing);
				/* XXX XXX XXX */
			/* This command never recurses */
			/* No command-specific prep needed */
			/* XXX: check permissions */
			pass = AT_PASS_MISC;
			break;
		case AT_PartAttachIndex:
			ATSimplePermissions(rel, ATT_INDEX);
			/* No command-specific prep needed */
			pass = AT_PASS_MISC;
			break;

		default:				/* oops */
			elog(ERROR, "unrecognized alter table type: %d",
				 (int) cmd->subtype);
			pass = AT_PASS_UNSET;		/* keep compiler quiet */
			break;
	}
	Assert(pass > AT_PASS_UNSET);

	/* Add the subcommand to the appropriate list for phase 2 */
	tab->subcmds[pass] = lappend(tab->subcmds[pass], cmd);
}

/*
 * ATRewriteCatalogs
 *
 * Traffic cop for ALTER TABLE Phase 2 operations.  Subcommands are
 * dispatched in a "safe" execution order (designed to avoid unnecessary
 * conflicts).
 */
static void
ATRewriteCatalogs(List **wqueue, LOCKMODE lockmode)
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
			 * Appropriate lock was obtained by phase 1, needn't get it again
			 */
			rel = relation_open(tab->relid, NoLock);

			foreach(lcmd, subcmds)
			{
				AlterTableCmd	*atc = (AlterTableCmd *) lfirst(lcmd);

				ATExecCmd(wqueue, tab, &rel, atc, lockmode);

				/*
				 * SET DISTRIBUTED BY() calls RelationForgetRelation(),
				 * which will scribble on rel, so we must re-open it.
				 */
				if (atc->subtype == AT_SetDistributedBy)
					rel = relation_open(tab->relid, NoLock);
				/* ATExecExpandTable() may close the relation temporarily */
				else if (atc->subtype == AT_ExpandTable)
					rel = relation_open(tab->relid, NoLock);
			}

			/*
			 * After the ALTER TYPE pass, do cleanup work (this is not done in
			 * ATExecAlterColumnType since it should be done only once if
			 * multiple columns of a table are altered).
			 */
			if (pass == AT_PASS_ALTER_TYPE)
				ATPostAlterTypeCleanup(wqueue, tab, lockmode);

			relation_close(rel, NoLock);
		}
	}

}

static void
ATAddToastIfNeeded(List **wqueue, LOCKMODE lockmode)
{
	ListCell   *ltab;

	/* Check to see if a toast table must be added. */
	foreach(ltab, *wqueue)
	{
		AlteredTableInfo *tab = (AlteredTableInfo *) lfirst(ltab);

		if (tab->relkind == RELKIND_RELATION ||
			tab->relkind == RELKIND_MATVIEW)
		{
			bool is_part = !rel_needs_long_lock(tab->relid);

			/*
			 * FIXME: we've passed false as is_part_parent to make_new_heap().
			 * So it seems ok to pass false for is_part_parent here but is that
			 * right?  E.g. what happens when a partitioned table goes through
			 * this code path?  All non-leaf nodes suddenly get a valid
			 * relfrozenxid, when they shouldn't?  How about creating a new
			 * function to scan pg_inherits to determine, given a master
			 * relation's OID, whether an auxiliary table needs valid
			 * relfrozenxid or not?
			 */
			AlterTableCreateToastTable(tab->relid, (Datum) 0, lockmode,
									   is_part, false);
		}
	}
}

/*
 * ATExecCmd: dispatch a subcommand to appropriate execution routine
 *
 * NOTE: we need to use a pointer to Relation here since the relation
 * address may be changed by ATPExecPartSplit(). This is different
 * behavior from Postgres upstream.
 */
static void
ATExecCmd(List **wqueue, AlteredTableInfo *tab, Relation *rel_p,
		  AlterTableCmd *cmd, LOCKMODE lockmode)
{
	Relation rel = *rel_p;

	switch (cmd->subtype)
	{
		case AT_AddColumn:		/* ADD COLUMN */
		case AT_AddColumnToView:		/* add column via CREATE OR REPLACE
										 * VIEW */
			ATExecAddColumn(wqueue, tab, rel, (ColumnDef *) cmd->def,
							false, false, false, lockmode);
			break;
		case AT_AddColumnRecurse:
			ATExecAddColumn(wqueue, tab, rel, (ColumnDef *) cmd->def,
							false, true, false, lockmode);
			break;
		case AT_ColumnDefault:	/* ALTER COLUMN DEFAULT */
		case AT_ColumnDefaultRecurse:
			ATExecColumnDefault(rel, cmd->name, (ColumnDef *) cmd->def, lockmode);
			break;
		case AT_DropNotNull:	/* ALTER COLUMN DROP NOT NULL */
			ATExecDropNotNull(rel, cmd->name, lockmode);
			break;
		case AT_SetNotNull:		/* ALTER COLUMN SET NOT NULL */
			ATExecSetNotNull(tab, rel, cmd->name, lockmode);
			break;
		case AT_SetStatistics:	/* ALTER COLUMN SET STATISTICS */
			ATExecSetStatistics(rel, cmd->name, cmd->def, lockmode);
			break;
		case AT_SetOptions:		/* ALTER COLUMN SET ( options ) */
			ATExecSetOptions(rel, cmd->name, cmd->def, false, lockmode);
			break;
		case AT_ResetOptions:	/* ALTER COLUMN RESET ( options ) */
			ATExecSetOptions(rel, cmd->name, cmd->def, true, lockmode);
			break;
		case AT_SetStorage:		/* ALTER COLUMN SET STORAGE */
			ATExecSetStorage(rel, cmd->name, cmd->def, lockmode);
			break;
		case AT_DropColumn:		/* DROP COLUMN */
			ATExecDropColumn(wqueue, rel, cmd->name,
					 cmd->behavior, false, false, cmd->missing_ok, lockmode);
			break;
		case AT_DropColumnRecurse:		/* DROP COLUMN with recursion */
			ATExecDropColumn(wqueue, rel, cmd->name,
					  cmd->behavior, true, false, cmd->missing_ok, lockmode);
			break;
		case AT_AddIndex:		/* ADD INDEX */
			ATExecAddIndex(tab, rel, (IndexStmt *) cmd->def, false, lockmode);
			break;
		case AT_ReAddIndex:		/* ADD INDEX */
			ATExecAddIndex(tab, rel, (IndexStmt *) cmd->def, true, lockmode);
			break;
		case AT_AddConstraint:	/* ADD CONSTRAINT */
			ATExecAddConstraint(wqueue, tab, rel, (Constraint *) cmd->def,
								false, false, lockmode);
			break;
		case AT_AddConstraintRecurse:	/* ADD CONSTRAINT with recursion */
			ATExecAddConstraint(wqueue, tab, rel, (Constraint *) cmd->def,
								true, false, lockmode);
			break;
		case AT_ReAddConstraint:		/* Re-add pre-existing check
										 * constraint */
			ATExecAddConstraint(wqueue, tab, rel, (Constraint *) cmd->def,
								true, true, lockmode);
			break;
		case AT_AddIndexConstraint:		/* ADD CONSTRAINT USING INDEX */
			ATExecAddIndexConstraint(tab, rel, (IndexStmt *) cmd->def, lockmode);
			break;
		case AT_AlterConstraint:		/* ALTER CONSTRAINT */
			ATExecAlterConstraint(rel, cmd, false, false, lockmode);
			break;
		case AT_ValidateConstraint:		/* VALIDATE CONSTRAINT */
			ATExecValidateConstraint(rel, cmd->name, false, false, lockmode);
			break;
		case AT_ValidateConstraintRecurse:		/* VALIDATE CONSTRAINT with
												 * recursion */
			ATExecValidateConstraint(rel, cmd->name, true, false, lockmode);
			break;
		case AT_DropConstraint:	/* DROP CONSTRAINT */
			ATExecDropConstraint(rel, cmd->name, cmd->behavior,
								 false, false,
								 cmd->missing_ok, lockmode);
			break;
		case AT_DropConstraintRecurse:	/* DROP CONSTRAINT with recursion */
			ATExecDropConstraint(rel, cmd->name, cmd->behavior,
								 true, false,
								 cmd->missing_ok, lockmode);
			break;
		case AT_AlterColumnType:		/* ALTER COLUMN TYPE */
			ATExecAlterColumnType(tab, rel, cmd, lockmode);
			break;
		case AT_AlterColumnGenericOptions:		/* ALTER COLUMN OPTIONS */
			ATExecAlterColumnGenericOptions(rel, cmd->name, (List *) cmd->def, lockmode);
			break;
		case AT_ChangeOwner:	/* ALTER OWNER */
			ATExecChangeOwner(RelationGetRelid(rel),
							  get_role_oid(cmd->name, false),
							  false, lockmode);
			break;
		case AT_ClusterOn:		/* CLUSTER ON */
			ATExecClusterOn(rel, cmd->name, lockmode);
			break;
		case AT_DropCluster:	/* SET WITHOUT CLUSTER */
			ATExecDropCluster(rel, lockmode);
			break;
		case AT_AddOids:		/* SET WITH OIDS */
		case AT_AddOidsRecurse:	/* SET WITH OIDS */
			/* Use the ADD COLUMN code, unless prep decided to do nothing */
			if (cmd->def != NULL)
				ATExecAddColumn(wqueue, tab, rel, (ColumnDef *) cmd->def,
								true, true, false, lockmode);
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
		case AT_ResetRelOptions:		/* RESET (...) */
		case AT_ReplaceRelOptions:		/* replace entire option list */
			ATExecSetRelOptions(rel, (List *) cmd->def, cmd->subtype, lockmode);
			break;
		case AT_EnableTrig:		/* ENABLE TRIGGER name */
			ATExecEnableDisableTrigger(rel, cmd->name,
								   TRIGGER_FIRES_ON_ORIGIN, false, lockmode);
			break;
		case AT_EnableAlwaysTrig:		/* ENABLE ALWAYS TRIGGER name */
			ATExecEnableDisableTrigger(rel, cmd->name,
									   TRIGGER_FIRES_ALWAYS, false, lockmode);
			break;
		case AT_EnableReplicaTrig:		/* ENABLE REPLICA TRIGGER name */
			ATExecEnableDisableTrigger(rel, cmd->name,
								  TRIGGER_FIRES_ON_REPLICA, false, lockmode);
			break;
		case AT_DisableTrig:	/* DISABLE TRIGGER name */
			ATExecEnableDisableTrigger(rel, cmd->name,
									   TRIGGER_DISABLED, false, lockmode);
			break;
		case AT_EnableTrigAll:	/* ENABLE TRIGGER ALL */
			ATExecEnableDisableTrigger(rel, NULL,
								   TRIGGER_FIRES_ON_ORIGIN, false, lockmode);
			break;
		case AT_DisableTrigAll:	/* DISABLE TRIGGER ALL */
			ATExecEnableDisableTrigger(rel, NULL,
									   TRIGGER_DISABLED, false, lockmode);
			break;
		case AT_EnableTrigUser:	/* ENABLE TRIGGER USER */
			ATExecEnableDisableTrigger(rel, NULL,
									TRIGGER_FIRES_ON_ORIGIN, true, lockmode);
			break;
		case AT_DisableTrigUser:		/* DISABLE TRIGGER USER */
			ATExecEnableDisableTrigger(rel, NULL,
									   TRIGGER_DISABLED, true, lockmode);
			break;

		case AT_EnableRule:		/* ENABLE RULE name */
			ATExecEnableDisableRule(rel, cmd->name,
									RULE_FIRES_ON_ORIGIN, lockmode);
			break;
		case AT_EnableAlwaysRule:		/* ENABLE ALWAYS RULE name */
			ATExecEnableDisableRule(rel, cmd->name,
									RULE_FIRES_ALWAYS, lockmode);
			break;
		case AT_EnableReplicaRule:		/* ENABLE REPLICA RULE name */
			ATExecEnableDisableRule(rel, cmd->name,
									RULE_FIRES_ON_REPLICA, lockmode);
			break;
		case AT_DisableRule:	/* DISABLE RULE name */
			ATExecEnableDisableRule(rel, cmd->name,
									RULE_DISABLED, lockmode);
			break;

		case AT_AddInherit:
			ATExecAddInherit(rel, (Node *) cmd->def, lockmode);
			break;
		case AT_DropInherit:
			ATExecDropInherit(rel, (RangeVar *) cmd->def, lockmode);
			break;
		case AT_AddOf:
			ATExecAddOf(rel, (TypeName *) cmd->def, lockmode);
			break;
		case AT_DropOf:
			ATExecDropOf(rel, lockmode);
			break;
		case AT_ReplicaIdentity:
			ATExecReplicaIdentity(rel, (ReplicaIdentityStmt *) cmd->def, lockmode);
			break;
		case AT_GenericOptions:
			ATExecGenericOptions(rel, (List *) cmd->def);
			break;

		case AT_SetDistributedBy:	/* SET DISTRIBUTED BY */
			ATExecSetDistributedBy(rel, (Node *) cmd->def, cmd);
			break;
		case AT_ExpandTable:	/* SET DISTRIBUTED BY */
			ATExecExpandTable(wqueue, rel, cmd);
			break;
			/* CDB: Partitioned Table commands */
		case AT_PartAdd:				/* Add */
		case AT_PartAddForSplit:		/* Add, as part of a Split */
			ATPExecPartAdd(tab, rel, (AlterPartitionCmd *) cmd->def,
						   cmd->subtype);
			break;
		case AT_PartAlter:				/* Alter */
			ATPExecPartAlter(wqueue, tab, rel_p, (AlterPartitionCmd *) cmd->def);
			break;
		case AT_PartDrop:				/* Drop */
			ATPExecPartDrop(rel, (AlterPartitionCmd *) cmd->def);
			break;
		case AT_PartExchange:			/* Exchange */
			ATPExecPartExchange(tab, rel, (AlterPartitionCmd *) cmd->def);
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
			ATPExecPartSplit(rel_p, (AlterPartitionCmd *) cmd->def);
			break;
		case AT_PartTruncate:			/* Truncate */
			ATPExecPartTruncate(rel, (AlterPartitionCmd *) cmd->def);
			break;
		case AT_PartAddInternal:
			ATExecPartAddInternal(rel, cmd->def);
			break;
		case AT_PartAttachIndex:
			ATExecAttachPartitionIdx(wqueue, rel,
									 (AlterPartitionId*) ((AlterPartitionCmd *) cmd->def)->partid);
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
ATRewriteTables(List **wqueue, LOCKMODE lockmode)
{
	ListCell   *ltab;
	char        relstorage;

	/* Go through each table that needs to be checked or rewritten */
	foreach(ltab, *wqueue)
	{
		AlteredTableInfo *tab = (AlteredTableInfo *) lfirst(ltab);

		/* Foreign tables have no storage. */
		if (tab->relkind == RELKIND_FOREIGN_TABLE)
			continue;

		/*
		 * If we change column data types or add/remove OIDs, the operation
		 * has to be propagated to tables that use this table's rowtype as a
		 * column type.  tab->newvals will also be non-NULL in the case where
		 * we're adding a column with a default.  We choose to forbid that
		 * case as well, since composite types might eventually support
		 * defaults.
		 *
		 * (Eventually we'll probably need to check for composite type
		 * dependencies even when we're just scanning the table without a
		 * rewrite, but at the moment a composite type does not enforce any
		 * constraints, so it's not necessary/appropriate to enforce them just
		 * during ALTER.)
		 */
		if (tab->newvals != NIL || tab->rewrite)
		{
			Relation	rel;

			rel = heap_open(tab->relid, NoLock);
			find_composite_type_dependencies(rel->rd_rel->reltype, rel, NULL);
			heap_close(rel, NoLock);
		}

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
			 * We don't support rewriting of system catalogs; there are too
			 * many corner cases and too little benefit.  In particular this
			 * is certainly not going to work for mapped catalogs.
			 */
			if (IsSystemRelation(OldHeap))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot rewrite system relation \"%s\"",
								RelationGetRelationName(OldHeap))));

			if (RelationIsUsedAsCatalogTable(OldHeap))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("cannot rewrite table \"%s\" used as a catalog table",
					   RelationGetRelationName(OldHeap))));

			/*
			 * Don't allow rewrite on temp tables of other backends ... their
			 * local buffer manager is not going to cope.
			 */
			if (RELATION_IS_OTHER_TEMP(OldHeap))
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
		if (tab->rewrite)
		{
			/* Build a temporary relation and copy data */
			Oid         OIDNewHeap;

			/* Create transient table that will receive the modified data */
			OIDNewHeap = make_new_heap(tab->relid, newTableSpace, false,
									   lockmode, hasIndexes);

			/*
			 * Copy the heap data into the new table with the desired
			 * modifications, and test the current data within the table
			 * against new constraints generated by ALTER TABLE commands.
			 */
			ATRewriteTable(tab, OIDNewHeap, lockmode);

			/*
			 * Swap the physical files of the old and new heaps, then rebuild
			 * indexes and discard the old heap.  We can use RecentXmin for
			 * the table's new relfrozenxid because we rewrote all the tuples
			 * in ATRewriteTable, so no older Xid remains in the table.  Also,
			 * we never try to swap toast tables by content, since we have no
			 * interest in letting this code work on system catalogs.
			 *
			 * MPP-17516 - The 'swap_stats' argument dictates whether the
			 * relpages and reltuples of the fake relfile should be copied
			 * over to our original pg_class tuple. We do not want to do this
			 * in the case of ALTER TABLE rewrites as the temp relfile will
			 * not have correct stats.
			 */
			finish_heap_swap(tab->relid, OIDNewHeap,
							 false, false,
							 false /* swap_stats */,
							 true,
							 !OidIsValid(tab->newTableSpace),
							 RecentXmin,
							 ReadNextMultiXactId());
		}
		else
		{
			/*
			 * Test the current data within the table against new constraints
			 * generated by ALTER TABLE commands, but don't rebuild data.
			 */
			if (tab->constraints != NIL || tab->new_notnull)
				ATRewriteTable(tab, InvalidOid, lockmode);

			/*
			 * If we had SET TABLESPACE but no reason to reconstruct tuples,
			 * just do a block-by-block copy.
			 */
			if (tab->newTableSpace)
				ATExecSetTableSpace(tab->relid, tab->newTableSpace, lockmode);
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
				Constraint *fkconstraint = (Constraint *) con->qual;
				Relation	refrel;

				if (rel == NULL)
				{
					/* Long since locked, no need for another */
					rel = heap_open(tab->relid, NoLock);
				}

				refrel = heap_open(con->refrelid, RowShareLock);

				validateForeignKeyConstraint(fkconstraint->conname, rel, refrel,
											 con->refindid,
											 con->conid);

				/*
				 * No need to mark the constraint row as validated, we did
				 * that when we inserted the row earlier.
				 */

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
				aocs_addcol_setfirstrownum(idesc, expectedFRN);
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
	List *drop_segno_list = NIL;

	for (segi = 0; segi < nseg; ++segi)
	{
		/*
		 * Append to drop_segno_list and skip if state is in
		 * AOSEG_STATE_AWAITING_DROP. At the end of the loop, we will
		 * try to drop the segfiles since we currently have the
		 * AccessExclusiveLock. If we don't do this, aocssegfiles in
		 * this state will have vpinfo size containing info for less
		 * number of columns compared to the relation's relnatts in
		 * its pg_class entry (e.g. in calls to getAOCSVPEntry).
		 */
		if (segInfos[segi]->state == AOSEG_STATE_AWAITING_DROP)
		{
			drop_segno_list = lappend_int(drop_segno_list, segInfos[segi]->segno);
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

	if (list_length(drop_segno_list) > 0 && Gp_role != GP_ROLE_DISPATCH)
		AOCSDrop(aocsrel, drop_segno_list);

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
	Snapshot snapshot;

	/*
	 * only ADD COLUMN subcommand is supported at this time
	 */
	List *addColCmds = tab->subcmds[AT_PASS_ADD_COL];
	if (addColCmds == NULL)
		return false;

	snapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));

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
	segInfos = GetAllAOCSFileSegInfo(rel, snapshot, &nseg);
	basepath = relpathbackend(rel->rd_node, rel->rd_backend, MAIN_FORKNUM);
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
			RelFileNodeBackend rnode;

			rnode.node = rel->rd_node;
			rnode.backend = rel->rd_backend;

			aocs_addcol_newsegfile(idesc, segInfos[segi],
								   basepath, rnode);

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
	UnregisterSnapshot(snapshot);
	return true;
}

/*
 * ATRewriteTable: scan or rewrite one table
 *
 * OIDNewHeap is InvalidOid if we don't need to rewrite
 */
static void
ATRewriteTable(AlteredTableInfo *tab, Oid OIDNewHeap, LOCKMODE lockmode)
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
	CommandId	mycid;
	BulkInsertState bistate;
	int			hi_options;

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
		newrel = heap_open(OIDNewHeap, lockmode);
	else
		newrel = NULL;

	/*
	 * Prepare a BulkInsertState and options for heap_insert. Because we're
	 * building a new heap, we can skip WAL-logging and fsync it to disk at
	 * the end instead (unless WAL-logging is required for archiving or
	 * streaming replication). The FSM is empty too, so don't bother using it.
	 */
	if (newrel)
	{
		mycid = GetCurrentCommandId(true);
		bistate = GetBulkInsertState();

		hi_options = HEAP_INSERT_SKIP_FSM;
		if (!XLogIsNeeded())
			hi_options |= HEAP_INSERT_SKIP_WAL;
	}
	else
	{
		/* keep compiler quiet about using these uninitialized */
		mycid = 0;
		bistate = NULL;
		hi_options = 0;
	}

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

		/* expr already planned */
		ex->exprstate = ExecInitExpr((Expr *) ex->expr, NULL);
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
		Snapshot	snapshot;
		char		relstorage = oldrel->rd_rel->relstorage;

		if (newrel)
			ereport(DEBUG1,
					(errmsg("rewriting table \"%s\"",
							RelationGetRelationName(oldrel))));
		else
			ereport(DEBUG1,
					(errmsg("verifying table \"%s\"",
							RelationGetRelationName(oldrel))));

		if (newrel)
		{
			/*
			 * All predicate locks on the tuples or pages are about to be made
			 * invalid, because we move tuples around.  Promote them to
			 * relation locks.
			 */
			TransferPredicateLocksToHeapRelation(oldrel);
		}

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
		snapshot = RegisterSnapshot(GetLatestSnapshot());
		if (relstorage == RELSTORAGE_HEAP)
		{
			heapscan = heap_beginscan(oldrel, snapshot, 0, NULL);

			/*
			 * Switch to per-tuple memory context and reset it for each tuple
			 * produced, so we don't leak memory.
			 */
			oldCxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

			while ((htuple = heap_getnext(heapscan, ForwardScanDirection)) != NULL)
			{
				if (tab->rewrite)
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
											 errmsg("exchange table contains a row which violates the partitioning specification of \"%s\"",
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

				/*
				 * Constraints might reference the tableoid column, so
				 * initialize t_tableOid before evaluating them.
				 */
				// GPDB_94_MERGE_FIXME: tableoid removed. should we put it back? It's used
				// in a lot more places in 9.4
#if 0
				tuple->t_tableOid = RelationGetRelid(oldrel);
#endif

				/* Write the tuple out to the new relation */
				if (newrel)
					heap_insert(newrel, htuple, mycid, hi_options, bistate,
								GetCurrentTransactionId());

				ResetExprContext(econtext);

				CHECK_FOR_INTERRUPTS();
			}

			MemoryContextSwitchTo(oldCxt);
			heap_endscan(heapscan);

		}
		else if (relstorage == RELSTORAGE_AOROWS && Gp_role != GP_ROLE_DISPATCH)
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

			aoscan = appendonly_beginscan(oldrel, snapshot, snapshot, 0, NULL);

			/*
			 * Switch to per-tuple memory context and reset it for each tuple
			 * produced, so we don't leak memory.
			 */
			oldCxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

			while (appendonly_getnext(aoscan, ForwardScanDirection, oldslot))
			{
				mtuple = TupGetMemTuple(oldslot);

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
					AOTupleId	aoTupleId;
					
					appendonly_insert(aoInsertDesc, mtuple, InvalidOid, &aoTupleId);
				}

				ResetExprContext(econtext);

				CHECK_FOR_INTERRUPTS();

			}

			MemoryContextSwitchTo(oldCxt);
			appendonly_endscan(aoscan);

			if (newrel)
				appendonly_insert_finish(aoInsertDesc);
		}
		else if (relstorage == RELSTORAGE_AOCOLS && Gp_role != GP_ROLE_DISPATCH)
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

			sdesc = aocs_beginscan(oldrel, snapshot, snapshot, oldTupDesc, proj);

			while (aocs_getnext(sdesc, ForwardScanDirection, oldslot))
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
							elog(ERROR, "Unrecognized constraint type: %d",
								 (int) con->contype);
					}
				}

				if (newrel)
				{
					MemoryContextSwitchTo(oldCxt);
					aocs_insert(idesc, newslot);
				}

				ResetExprContext(econtext);
				CHECK_FOR_INTERRUPTS();

				MemoryContextSwitchTo(oldCxt);
			}

			aocs_endscan(sdesc);

			if(newrel)
				aocs_insert_finish(idesc);

			pfree(proj);
		}
		else if (relstorage_is_ao(relstorage) && Gp_role == GP_ROLE_DISPATCH)
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

		UnregisterSnapshot(snapshot);

		ExecDropSingleTupleTableSlot(oldslot);
		ExecDropSingleTupleTableSlot(newslot);
	}

	FreeExecutorState(estate);

	heap_close(oldrel, NoLock);
	if (newrel)
	{
		FreeBulkInsertState(bistate);

		/* If we skipped writing WAL, then we need to sync the heap. */
		if (hi_options & HEAP_INSERT_SKIP_WAL)
			heap_sync(newrel);

		heap_close(newrel, NoLock);
	}
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
				 errmsg("cannot %s \"%s\"; it has external partition(s)",
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
ATSimplePermissions(Relation rel, int allowed_targets)
{
	int			actual_target;

	switch (rel->rd_rel->relkind)
	{
		case RELKIND_RELATION:
			actual_target = ATT_TABLE;
			break;
		case RELKIND_VIEW:
			actual_target = ATT_VIEW;
			break;
		case RELKIND_MATVIEW:
			actual_target = ATT_MATVIEW;
			break;
		case RELKIND_INDEX:
			actual_target = ATT_INDEX;
			break;
		case RELKIND_COMPOSITE_TYPE:
			actual_target = ATT_COMPOSITE_TYPE;
			break;
		case RELKIND_FOREIGN_TABLE:
			actual_target = ATT_FOREIGN_TABLE;
			break;

		case RELKIND_AOSEGMENTS:
		case RELKIND_AOBLOCKDIR:
		case RELKIND_AOVISIMAP:
			/*
			 * Allow ALTER TABLE operations in standard alone mode on
			 * AO segment tables.
			 */
			if (IsUnderPostmaster)
				actual_target = ATT_TABLE;
			else
				actual_target = 0;
			break;

		default:
			actual_target = 0;
			break;
	}

	/* Wrong target type? */
	if ((actual_target & allowed_targets) == 0)
		ATWrongRelkindError(rel, allowed_targets);

	/* Permissions checks */
	if (!pg_class_ownercheck(RelationGetRelid(rel), GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));

	if (!allowSystemTableMods && IsSystemRelation(rel))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						RelationGetRelationName(rel))));
}

/*
 * ATWrongRelkindError
 *
 * Throw an error when a relation has been determined to be of the wrong
 * type.
 */
static void
ATWrongRelkindError(Relation rel, int allowed_targets)
{
	char	   *msg;

	switch (allowed_targets)
	{
		case ATT_TABLE:
			msg = _("\"%s\" is not a table");
			break;
		case ATT_TABLE | ATT_VIEW:
			msg = _("\"%s\" is not a table or view");
			break;
		case ATT_TABLE | ATT_VIEW | ATT_MATVIEW | ATT_INDEX:
			msg = _("\"%s\" is not a table, view, materialized view, or index");
			break;
		case ATT_TABLE | ATT_MATVIEW:
			msg = _("\"%s\" is not a table or materialized view");
			break;
		case ATT_TABLE | ATT_MATVIEW | ATT_INDEX:
			msg = _("\"%s\" is not a table, materialized view, or index");
			break;
		case ATT_TABLE | ATT_FOREIGN_TABLE:
			msg = _("\"%s\" is not a table or foreign table");
			break;
		case ATT_TABLE | ATT_COMPOSITE_TYPE | ATT_FOREIGN_TABLE:
			msg = _("\"%s\" is not a table, composite type, or foreign table");
			break;
		case ATT_TABLE | ATT_MATVIEW | ATT_INDEX | ATT_FOREIGN_TABLE:
			msg = _("\"%s\" is not a table, materialized view, index, or foreign table");
			break;
		case ATT_VIEW:
			msg = _("\"%s\" is not a view");
			break;
		case ATT_FOREIGN_TABLE:
			msg = _("\"%s\" is not a foreign table");
			break;
		default:
			/* shouldn't get here, add all necessary cases above */
			msg = _("\"%s\" is of the wrong type");
			break;
	}

	ereport(ERROR,
			(errcode(ERRCODE_WRONG_OBJECT_TYPE),
			 errmsg(msg, RelationGetRelationName(rel))));
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
				  AlterTableCmd *cmd, bool recurse, LOCKMODE lockmode)
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

		children = find_all_inheritors(relid, lockmode, NULL);

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
			/* find_all_inheritors already got lock */
			childrel = relation_open(childrelid, NoLock);
			CheckTableNotInUse(childrel, "ALTER TABLE");
			ATPrepCmd(wqueue, childrel, cmd, false, true, lockmode);
			relation_close(childrel, NoLock);
		}
	}
}

/*
 * ATTypedTableRecursion
 *
 * Propagate ALTER TYPE operations to the typed tables of that type.
 * Also check the RESTRICT/CASCADE behavior.  Given CASCADE, also permit
 * recursion to inheritance children of the typed tables.
 */
static void
ATTypedTableRecursion(List **wqueue, Relation rel, AlterTableCmd *cmd,
					  LOCKMODE lockmode)
{
	ListCell   *child;
	List	   *children;

	Assert(rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE);

	children = find_typed_table_dependencies(rel->rd_rel->reltype,
											 RelationGetRelationName(rel),
											 cmd->behavior);

	foreach(child, children)
	{
		Oid			childrelid = lfirst_oid(child);
		Relation	childrel;

		childrel = relation_open(childrelid, lockmode);
		CheckTableNotInUse(childrel, "ALTER TABLE");
		ATPrepCmd(wqueue, childrel, cmd, true, true, lockmode);
		relation_close(childrel, NoLock);
	}
}


/*
 * find_composite_type_dependencies
 *
 * Check to see if the type "typeOid" is being used as a column in some table
 * (possibly nested several levels deep in composite types, arrays, etc!).
 * Eventually, we'd like to propagate the check or rewrite operation
 * into such tables, but for now, just error out if we find any.
 *
 * Caller should provide either the associated relation of a rowtype,
 * or a type name (not both) for use in the error message, if any.
 *
 * Note that "typeOid" is not necessarily a composite type; it could also be
 * another container type such as an array or range, or a domain over one of
 * these things.  The name of this function is therefore somewhat historical,
 * but it's not worth changing.
 *
 * We assume that functions and views depending on the type are not reasons
 * to reject the ALTER.  (How safe is this really?)
 */
void
find_composite_type_dependencies(Oid typeOid, Relation origRelation,
								 const char *origTypeName)
{
	Relation	depRel;
	ScanKeyData key[2];
	SysScanDesc depScan;
	HeapTuple	depTup;

	/* since this function recurses, it could be driven to stack overflow */
	check_stack_depth();

	/*
	 * We scan pg_depend to find those things that depend on the given type.
	 * (We assume we can ignore refobjsubid for a type.)
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
								 NULL, 2, key);

	while (HeapTupleIsValid(depTup = systable_getnext(depScan)))
	{
		Form_pg_depend pg_depend = (Form_pg_depend) GETSTRUCT(depTup);
		Relation	rel;
		Form_pg_attribute att;

		/* Check for directly dependent types */
		if (pg_depend->classid == TypeRelationId)
		{
			/*
			 * This must be an array, domain, or range containing the given
			 * type, so recursively check for uses of this type.  Note that
			 * any error message will mention the original type not the
			 * container; this is intentional.
			 */
			find_composite_type_dependencies(pg_depend->objid,
											 origRelation, origTypeName);
			continue;
		}

		/* Else, ignore dependees that aren't user columns of relations */
		/* (we assume system columns are never of interesting types) */
		if (pg_depend->classid != RelationRelationId ||
			pg_depend->objsubid <= 0)
			continue;

		rel = relation_open(pg_depend->objid, AccessShareLock);
		att = rel->rd_att->attrs[pg_depend->objsubid - 1];

		if (rel->rd_rel->relkind == RELKIND_RELATION ||
			rel->rd_rel->relkind == RELKIND_MATVIEW)
		{
			if (origTypeName)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter type \"%s\" because column \"%s.%s\" uses it",
								origTypeName,
								RelationGetRelationName(rel),
								NameStr(att->attname))));
			else if (origRelation->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter type \"%s\" because column \"%s.%s\" uses it",
								RelationGetRelationName(origRelation),
								RelationGetRelationName(rel),
								NameStr(att->attname))));
			else if (origRelation->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter foreign table \"%s\" because column \"%s.%s\" uses its row type",
								RelationGetRelationName(origRelation),
								RelationGetRelationName(rel),
								NameStr(att->attname))));
			else
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter table \"%s\" because column \"%s.%s\" uses its row type",
								RelationGetRelationName(origRelation),
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
											 origRelation, origTypeName);
		}

		relation_close(rel, AccessShareLock);
	}

	systable_endscan(depScan);

	relation_close(depRel, AccessShareLock);
}


/*
 * find_typed_table_dependencies
 *
 * Check to see if a composite type is being used as the type of a
 * typed table.  Abort if any are found and behavior is RESTRICT.
 * Else return the list of tables.
 */
static List *
find_typed_table_dependencies(Oid typeOid, const char *typeName, DropBehavior behavior)
{
	Relation	classRel;
	ScanKeyData key[1];
	HeapScanDesc scan;
	HeapTuple	tuple;
	List	   *result = NIL;

	classRel = heap_open(RelationRelationId, AccessShareLock);

	ScanKeyInit(&key[0],
				Anum_pg_class_reloftype,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(typeOid));

	scan = heap_beginscan_catalog(classRel, 1, key);

	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		if (behavior == DROP_RESTRICT)
			ereport(ERROR,
					(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
					 errmsg("cannot alter type \"%s\" because it is the type of a typed table",
							typeName),
			errhint("Use ALTER ... CASCADE to alter the typed tables too.")));
		else
			result = lappend_oid(result, HeapTupleGetOid(tuple));
	}

	heap_endscan(scan);
	heap_close(classRel, AccessShareLock);

	return result;
}


/*
 * check_of_type
 *
 * Check whether a type is suitable for CREATE TABLE OF/ALTER TABLE OF.  If it
 * isn't suitable, throw an error.  Currently, we require that the type
 * originated with CREATE TYPE AS.  We could support any row type, but doing so
 * would require handling a number of extra corner cases in the DDL commands.
 */
void
check_of_type(HeapTuple typetuple)
{
	Form_pg_type typ = (Form_pg_type) GETSTRUCT(typetuple);
	bool		typeOk = false;

	if (typ->typtype == TYPTYPE_COMPOSITE)
	{
		Relation	typeRelation;

		Assert(OidIsValid(typ->typrelid));
		typeRelation = relation_open(typ->typrelid, AccessShareLock);
		typeOk = (typeRelation->rd_rel->relkind == RELKIND_COMPOSITE_TYPE);

		/*
		 * Close the parent rel, but keep our AccessShareLock on it until xact
		 * commit.  That will prevent someone else from deleting or ALTERing
		 * the type before the typed table creation/conversion commits.
		 */
		relation_close(typeRelation, NoLock);
	}
	if (!typeOk)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("type %s is not a composite type",
						format_type_be(HeapTupleGetOid(typetuple)))));
}


/*
 * ALTER TABLE ADD COLUMN
 *
 * Adds an additional attribute to a relation making the assumption that
 * CHECK, NOT NULL, and FOREIGN KEY constraints will be removed from the
 * AT_AddColumn AlterTableCmd by parse_utilcmd.c and added as independent
 * AlterTableCmd's.
 *
 * ADD COLUMN cannot use the normal ALTER TABLE recursion mechanism, because we
 * have to decide at runtime whether to recurse or not depending on whether we
 * actually add a column or merely merge with an existing column.  (We can't
 * check this in a static pre-pass because it won't handle multiple inheritance
 * situations correctly.)
 */
static void
ATPrepAddColumn(List **wqueue, Relation rel, bool recurse, bool recursing,
				AlterTableCmd *cmd, LOCKMODE lockmode)
{
	/* 
	 * If there's an encoding clause, this better be an append only
	 * column oriented table.
	 */
	ColumnDef *def = (ColumnDef *)cmd->def;
	if (def->encoding && !RelationIsAoCols(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("ENCODING clause not supported on non column orientated table")));

	if (def->encoding)
		def->encoding = transformStorageEncodingClause(def->encoding);

	if (rel->rd_rel->reloftype && !recursing)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot add column to typed table")));

	if (rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
		ATTypedTableRecursion(wqueue, rel, cmd, lockmode);

	if (recurse)
	{
		if (cmd->subtype == AT_AddColumn || cmd->subtype == AT_AddColumnRecurse)
			cmd->subtype = AT_AddColumnRecurse;
		else if (cmd->subtype == AT_AddColumnToView)
			cmd->subtype = AT_AddColumnRecurse;
		else if (cmd->subtype == AT_AddOids || cmd->subtype == AT_AddOidsRecurse)
			cmd->subtype = AT_AddOidsRecurse;
		else
			elog(ERROR, "unexpected ALTER TABLE subtype %d in ADD COLUMN command",
				 cmd->subtype);
	}
}

static void
ATExecAddColumn(List **wqueue, AlteredTableInfo *tab, Relation rel,
				ColumnDef *colDef, bool isOid,
				bool recurse, bool recursing, LOCKMODE lockmode)
{
	Oid			myrelid = RelationGetRelid(rel);
	Relation	pgclass,
				attrdesc;
	HeapTuple	reltup;
	FormData_pg_attribute attribute;
	int			newattnum;
	char		relkind;
	HeapTuple	typeTuple;
	Oid			typeOid;
	int32		typmod;
	Oid			collOid;
	Form_pg_type tform;
	Expr	   *defval;
	List	   *children;
	ListCell   *child;
	AclResult	aclresult;

	/* At top level, permission check was done in ATPrepCmd, else do it */
	if (recursing)
		ATSimplePermissions(rel, ATT_TABLE);

	attrdesc = heap_open(AttributeRelationId, RowExclusiveLock);

	/*
	 * Are we adding the column to a recursion child?  If so, check whether to
	 * merge with an existing definition for the column.  If we do merge, we
	 * must not recurse.  Children will already have the column, and recursing
	 * into them would mess up attinhcount.
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
			Oid			ccollid;

			/* Child column must match on type, typmod, and collation */
			typenameTypeIdAndMod(NULL, colDef->typeName, &ctypeId, &ctypmod);
			if (ctypeId != childatt->atttypid ||
				ctypmod != childatt->atttypmod)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("child table \"%s\" has different type for column \"%s\"",
							RelationGetRelationName(rel), colDef->colname)));
			ccollid = GetColumnDefCollation(NULL, colDef, ctypeId);
			if (ccollid != childatt->attcollation)
				ereport(ERROR,
						(errcode(ERRCODE_COLLATION_MISMATCH),
						 errmsg("child table \"%s\" has different collation for column \"%s\"",
							  RelationGetRelationName(rel), colDef->colname),
						 errdetail("\"%s\" versus \"%s\"",
								   get_collation_name(ccollid),
							   get_collation_name(childatt->attcollation))));

			/* If it's OID, child column must actually be OID */
			if (isOid && childatt->attnum != ObjectIdAttributeNumber)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("child table \"%s\" has a conflicting \"%s\" column",
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

	reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(myrelid));
	if (!HeapTupleIsValid(reltup))
		elog(ERROR, "cache lookup failed for relation %u", myrelid);
	relkind = ((Form_pg_class) GETSTRUCT(reltup))->relkind;

	/* new name should not already exist */
	check_for_column_name_collision(rel, colDef->colname);

	/* Determine the new attribute's number */
	if (isOid)
		newattnum = ObjectIdAttributeNumber;
	else
	{
		newattnum = ((Form_pg_class) GETSTRUCT(reltup))->relnatts + 1;
		if (newattnum > MaxHeapAttributeNumber)
			ereport(ERROR,
					(errcode(ERRCODE_TOO_MANY_COLUMNS),
					 errmsg("tables can have at most %d columns",
							MaxHeapAttributeNumber)));
	}

	typeTuple = typenameType(NULL, colDef->typeName, &typmod);
	tform = (Form_pg_type) GETSTRUCT(typeTuple);
	typeOid = HeapTupleGetOid(typeTuple);

	aclresult = pg_type_aclcheck(typeOid, GetUserId(), ACL_USAGE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error_type(aclresult, typeOid);

	collOid = GetColumnDefCollation(NULL, colDef, typeOid);

	/* make sure datatype is legal for a column */
	CheckAttributeType(colDef->colname, typeOid, collOid,
					   list_make1_oid(rel->rd_rel->reltype),
					   false);

	/* construct new attribute's pg_attribute entry */
	attribute.attrelid = myrelid;
	namestrcpy(&(attribute.attname), colDef->colname);
	attribute.atttypid = typeOid;
	attribute.attstattarget = (newattnum > 0) ? -1 : 0;
	attribute.attlen = tform->typlen;
	attribute.attcacheoff = -1;
	attribute.atttypmod = typmod;
	attribute.attnum = newattnum;
	attribute.attbyval = tform->typbyval;
	attribute.attndims = list_length(colDef->typeName->arrayBounds);
	attribute.attstorage = tform->typstorage;
	attribute.attalign = tform->typalign;
	attribute.attnotnull = colDef->is_not_null;
	attribute.atthasdef = false;
	attribute.attisdropped = false;
	attribute.attislocal = colDef->is_local;
	attribute.attinhcount = colDef->inhcount;
	attribute.attcollation = collOid;
	/* attribute.attacl is handled by InsertPgAttributeTuple */

	ReleaseSysCache(typeTuple);

	InsertPgAttributeTuple(attrdesc, &attribute, NULL);

	heap_close(attrdesc, RowExclusiveLock);

	/*
	 * Update pg_class tuple as appropriate
	 */
	if (isOid)
		((Form_pg_class) GETSTRUCT(reltup))->relhasoids = true;
	else
		((Form_pg_class) GETSTRUCT(reltup))->relnatts = newattnum;

	simple_heap_update(pgclass, &reltup->t_self, reltup);

	/* keep catalog indexes current */
	CatalogUpdateIndexes(pgclass, reltup);

	heap_freetuple(reltup);

	/* Post creation hook for new attribute */
	InvokeObjectPostCreateHook(RelationRelationId, myrelid, newattnum);

	heap_close(pgclass, RowExclusiveLock);

	/* Make the attribute's catalog entry visible */
	CommandCounterIncrement();

	/*
	 * Store the DEFAULT, if any, in the catalogs
	 */
	if (colDef->raw_default)
	{
		RawColumnDefault *rawEnt;

		rawEnt = (RawColumnDefault *) palloc(sizeof(RawColumnDefault));
		rawEnt->attnum = attribute.attnum;
		rawEnt->raw_default = copyObject(colDef->raw_default);

		/*
		 * This function is intended for CREATE TABLE, so it processes a
		 * _list_ of defaults, but we just do one.
		 */
		AddRelationNewConstraints(rel, list_make1(rawEnt), NIL,
								  false, true, false);

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
	 * returned by AddRelationNewConstraints, so that the right thing happens
	 * when a datatype's default applies.
	 *
	 * We skip this step completely for views and foreign tables.  For a view,
	 * we can only get here from CREATE OR REPLACE VIEW, which historically
	 * doesn't set up defaults, not even for domain-typed columns.  And in any
	 * case we mustn't invoke Phase 3 on a view or foreign table, since they
	 * have no storage.
	 */
	if (relkind != RELKIND_VIEW && relkind != RELKIND_COMPOSITE_TYPE
		&& relkind != RELKIND_FOREIGN_TABLE && attribute.attnum > 0)
	{
		defval = (Expr *) build_column_default(rel, attribute.attnum);

		if (!defval && GetDomainConstraints(typeOid) != NIL)
		{
			Oid			baseTypeId;
			int32		baseTypeMod;
			Oid			baseTypeColl;

			baseTypeMod = typmod;
			baseTypeId = getBaseTypeAndTypmod(typeOid, &baseTypeMod);
			baseTypeColl = get_typcollation(baseTypeId);
			defval = (Expr *) makeNullConst(baseTypeId, baseTypeMod, baseTypeColl);
			defval = (Expr *) coerce_to_target_type(NULL,
													(Node *) defval,
													baseTypeId,
													typeOid,
													typmod,
													COERCION_ASSIGNMENT,
													COERCE_IMPLICIT_CAST,
													-1);
			if (defval == NULL) /* should not happen */
				elog(ERROR, "failed to coerce base type to domain");
		}

		/*
		 * Handling of default NULL for AO/CO tables.
		 *
		 * Currently memtuples cannot deal with the scenario where the number of
		 * attributes in the tuple data don't match the attnum. We will generate an
		 * explicit NULL default value and force a rewrite of the table below.
		 *
		 * At one point there were plans to restructure memtuples so that this
		 * rewrite did not have to occur. An optimization was added to
		 * column-oriented tables to avoid the rewrite, but it does not apply to
		 * row-oriented tables. Eventually it would be nice to remove this
		 * workaround; see GitHub issue
		 *     https://github.com/greenplum-db/gpdb/issues/3756
		 */

		if (!defval && RelationIsAppendOptimized(rel))
		{
			defval = (Expr *) makeNullConst(typeOid, -1, collOid);
		}

		if (defval)
		{
			NewColumnValue *newval;

			newval = (NewColumnValue *) palloc0(sizeof(NewColumnValue));
			newval->attnum = attribute.attnum;
			newval->expr = expression_planner(defval);

			/*
			 * tab is null if this is called by "create or replace view" which
			 * can't have any default value.
			 */
			Assert(tab);
			tab->newvals = lappend(tab->newvals, newval);
			tab->rewrite = true;
		}

		/*
		 * If the new column is NOT NULL, tell Phase 3 it needs to test that.
		 * Also, "create or replace view" won't have constraint on the column.
		 * (Note we don't do this for an OID column.  OID will be marked not
		 * null, but since it's filled specially, there's no need to test
		 * anything.)
		 */
		Assert(!colDef->is_not_null || tab);
		if (tab)
			tab->new_notnull |= colDef->is_not_null;
	}

	/*
	 * If we are adding an OID column, we have to tell Phase 3 to rewrite the
	 * table to fix that.
	 */
	if (isOid)
		tab->rewrite = true;

	/*
	 * Add needed dependency entries for the new column.
	 */
	add_column_datatype_dependency(myrelid, newattnum, attribute.atttypid);
	add_column_collation_dependency(myrelid, newattnum, attribute.attcollation);

	if (!RelationIsAoCols(rel) && colDef->encoding)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("ENCODING clause not supported on non column orientated table")));

	/* 
	 * For AO/CO tables, always store an encoding clause. If no encoding
	 * clause was provided, store the default encoding clause.
	 */
	if (RelationIsAoCols(rel))
	{
		ColumnReferenceStorageDirective *c;
		
		c = makeNode(ColumnReferenceStorageDirective);
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
	if ((Gp_role == GP_ROLE_DISPATCH) && MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ADD COLUMN");

	/*
	 * Propagate to children as appropriate.  Unlike most other ALTER
	 * routines, we have to do this one level of recursion at a time; we can't
	 * use find_all_inheritors to do it in one pass.
	 */
	children = find_inheritance_children(RelationGetRelid(rel), lockmode);

	/*
	 * If we are told not to recurse, there had better not be any child
	 * tables; else the addition would put them out of step.
	 */
	if (children && !recurse)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("column must be added to child tables too")));

	/* Children should see column as singly inherited */
	if (!recursing)
	{
		colDef = copyObject(colDef);
		colDef->inhcount = 1;
		colDef->is_local = false;
	}

	foreach(child, children)
	{
		Oid			childrelid = lfirst_oid(child);
		Relation	childrel;
		AlteredTableInfo *childtab;

		/* find_inheritance_children already got lock */
		childrel = heap_open(childrelid, NoLock);
		CheckTableNotInUse(childrel, "ALTER TABLE");

		/* Find or create work queue entry for this table */
		childtab = ATGetQueueEntry(wqueue, childrel);

		/* Recurse to child */
		ATExecAddColumn(wqueue, childtab, childrel,
						colDef, isOid, recurse, true, lockmode);

		heap_close(childrel, NoLock);
	}
}

/*
 * If a new or renamed column will collide with the name of an existing
 * column, error out.
 */
static void
check_for_column_name_collision(Relation rel, const char *colname)
{
	HeapTuple	attTuple;
	int			attnum;

	/*
	 * this test is deliberately not attisdropped-aware, since if one tries to
	 * add a column matching a dropped column name, it's gonna fail anyway.
	 */
	attTuple = SearchSysCache2(ATTNAME,
							   ObjectIdGetDatum(RelationGetRelid(rel)),
							   PointerGetDatum(colname));
	if (!HeapTupleIsValid(attTuple))
		return;

	attnum = ((Form_pg_attribute) GETSTRUCT(attTuple))->attnum;
	ReleaseSysCache(attTuple);

	/*
	 * We throw a different error message for conflicts with system column
	 * names, since they are normally not shown and the user might otherwise
	 * be confused about the reason for the conflict.
	 */
	if (attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_COLUMN),
			 errmsg("column name \"%s\" conflicts with a system column name",
					colname)));
	else
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" already exists",
						colname, RelationGetRelationName(rel))));
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
 * Install a column's dependency on its collation.
 */
static void
add_column_collation_dependency(Oid relid, int32 attnum, Oid collid)
{
	ObjectAddress myself,
				referenced;

	/* We know the default collation is pinned, so don't bother recording it */
	if (OidIsValid(collid) && collid != DEFAULT_COLLATION_OID)
	{
		myself.classId = RelationRelationId;
		myself.objectId = relid;
		myself.objectSubId = attnum;
		referenced.classId = CollationRelationId;
		referenced.objectId = collid;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}
}

/*
 * ALTER TABLE SET WITH OIDS
 *
 * Basically this is an ADD COLUMN for the special OID column.  We have
 * to cons up a ColumnDef node because the ADD COLUMN code needs one.
 */
static void
ATPrepAddOids(List **wqueue, Relation rel, bool recurse, AlterTableCmd *cmd, LOCKMODE lockmode)
{
	/* If we're recursing to a child table, the ColumnDef is already set up */
	if (cmd->def == NULL)
	{
		ColumnDef  *cdef = makeNode(ColumnDef);

		cdef->colname = pstrdup("oid");
		cdef->typeName = makeTypeNameFromOid(OIDOID, -1);
		cdef->inhcount = 0;
		cdef->is_local = true;
		cdef->is_not_null = true;
		cdef->storage = 0;
		cdef->location = -1;
		cmd->def = (Node *) cdef;
	}
	ATPrepAddColumn(wqueue, rel, recurse, false, cmd, lockmode);

	if (recurse)
		cmd->subtype = AT_AddOidsRecurse;
}

/*
 * ALTER TABLE ALTER COLUMN DROP NOT NULL
 */
static void
ATExecDropNotNull(Relation rel, const char *colName, LOCKMODE lockmode)
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

		indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexoid));
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

	InvokeObjectPostAlterHook(RelationRelationId,
							  RelationGetRelid(rel), attnum);

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
				 const char *colName, LOCKMODE lockmode)
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

	InvokeObjectPostAlterHook(RelationRelationId,
							  RelationGetRelid(rel), attnum);

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
	if (!recurse && find_inheritance_children(RelationGetRelid(rel), NoLock) != NIL)
	{
		/*
		 * In binary upgrade we are handling the children manually when dumping
		 * the schema so not recursing is allowed
		 */
		if (IsBinaryUpgrade)
			return;

		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("column default of relation \"%s\" must be added to its child relation(s)",
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

		children = find_inheritance_children(RelationGetRelid(rel), NoLock);
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
						    PROCESS_UTILITY_QUERY,
							NULL,
							dest,
							NULL);
		}
	}
}

static void
ATExecColumnDefault(Relation rel, const char *colName,
					ColumnDef *colDef, LOCKMODE lockmode)
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
	 *
	 * We treat removing the existing default as an internal operation when it
	 * is preparatory to adding a new default, but as a user-initiated
	 * operation when the user asked for a drop.
	 */
	RemoveAttrDefault(RelationGetRelid(rel), attnum, DROP_RESTRICT, false,
					  colDef->raw_default == NULL ? false : true);

	Assert(!colDef->cooked_default);
	if (colDef->raw_default)
	{
		/* SET DEFAULT */
		RawColumnDefault *rawEnt;

		rawEnt = (RawColumnDefault *) palloc(sizeof(RawColumnDefault));
		rawEnt->attnum = attnum;
		rawEnt->raw_default = colDef->raw_default;

		/*
		 * This function is intended for CREATE TABLE, so it processes a
		 * _list_ of defaults, but we just do one.
		 */
		AddRelationNewConstraints(rel, list_make1(rawEnt), NIL,
								  false, true, false);
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
ATPrepSetStatistics(Relation rel, const char *colName, Node *newValue, LOCKMODE lockmode)
{
	/*
	 * We do our own permission checking because (a) we want to allow SET
	 * STATISTICS on indexes (for expressional index columns), and (b) we want
	 * to allow SET STATISTICS on system catalogs without requiring
	 * allowSystemTableMods to be turned on.
	 */
	if (rel->rd_rel->relkind != RELKIND_RELATION &&
		rel->rd_rel->relkind != RELKIND_MATVIEW &&
		rel->rd_rel->relkind != RELKIND_INDEX &&
		rel->rd_rel->relkind != RELKIND_FOREIGN_TABLE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table, materialized view, index, or foreign table",
						RelationGetRelationName(rel))));

	/* Permissions checks */
	if (!pg_class_ownercheck(RelationGetRelid(rel), GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));
}

static void
ATExecSetStatistics(Relation rel, const char *colName, Node *newValue, LOCKMODE lockmode)
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

	InvokeObjectPostAlterHook(RelationRelationId,
							  RelationGetRelid(rel),
							  attrtuple->attnum);
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

static void
ATExecSetOptions(Relation rel, const char *colName, Node *options,
				 bool isReset, LOCKMODE lockmode)
{
	Relation	attrelation;
	HeapTuple	tuple,
				newtuple;
	Form_pg_attribute attrtuple;
	Datum		datum,
				newOptions;
	bool		isnull;
	Datum		repl_val[Natts_pg_attribute];
	bool		repl_null[Natts_pg_attribute];
	bool		repl_repl[Natts_pg_attribute];

	attrelation = heap_open(AttributeRelationId, RowExclusiveLock);

	tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);

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

	/* Generate new proposed attoptions (text array) */
	Assert(IsA(options, List));
	datum = SysCacheGetAttr(ATTNAME, tuple, Anum_pg_attribute_attoptions,
							&isnull);
	newOptions = transformRelOptions(isnull ? (Datum) 0 : datum,
									 (List *) options, NULL, NULL, false,
									 isReset);
	/* Validate new options */
	(void) attribute_reloptions(newOptions, true);

	/* Build new tuple. */
	memset(repl_null, false, sizeof(repl_null));
	memset(repl_repl, false, sizeof(repl_repl));
	if (newOptions != (Datum) 0)
		repl_val[Anum_pg_attribute_attoptions - 1] = newOptions;
	else
		repl_null[Anum_pg_attribute_attoptions - 1] = true;
	repl_repl[Anum_pg_attribute_attoptions - 1] = true;
	newtuple = heap_modify_tuple(tuple, RelationGetDescr(attrelation),
								 repl_val, repl_null, repl_repl);

	/* Update system catalog. */
	simple_heap_update(attrelation, &newtuple->t_self, newtuple);
	CatalogUpdateIndexes(attrelation, newtuple);

	InvokeObjectPostAlterHook(RelationRelationId,
							  RelationGetRelid(rel),
							  attrtuple->attnum);
	heap_freetuple(newtuple);

	ReleaseSysCache(tuple);

	heap_close(attrelation, RowExclusiveLock);
}

/*
 * ALTER TABLE ALTER COLUMN SET STORAGE
 */
static void
ATExecSetStorage(Relation rel, const char *colName, Node *newValue, LOCKMODE lockmode)
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

	InvokeObjectPostAlterHook(RelationRelationId,
							  RelationGetRelid(rel),
							  attrtuple->attnum);

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
 * on whether attinhcount goes to zero or not.  (We can't check this in a
 * static pre-pass because it won't handle multiple inheritance situations
 * correctly.)
 */
static void
ATPrepDropColumn(List **wqueue, Relation rel, bool recurse, bool recursing,
				 AlterTableCmd *cmd, LOCKMODE lockmode)
{
	if (rel->rd_rel->reloftype && !recursing)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot drop column from typed table")));

	if (rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
		ATTypedTableRecursion(wqueue, rel, cmd, lockmode);

	if (recurse)
		cmd->subtype = AT_DropColumnRecurse;
}

static void
ATExecDropColumn(List **wqueue, Relation rel, const char *colName,
				 DropBehavior behavior,
				 bool recurse, bool recursing,
				 bool missing_ok, LOCKMODE lockmode)
{
	HeapTuple	tuple;
	Form_pg_attribute targetatt;
	AttrNumber	attnum;
	List	   *children;
	ObjectAddress object;
	PartitionNode *pn;

	/* At top level, permission check was done in ATPrepCmd, else do it */
	if (recursing)
		ATSimplePermissions(rel, ATT_TABLE);

	/*
	 * get the number of the attribute
	 */
	tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);
	if (!HeapTupleIsValid(tuple))
	{
		if (!missing_ok)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" of relation \"%s\" does not exist",
							colName, RelationGetRelationName(rel))));
		}
		else
		{
			ereport(NOTICE,
					(errmsg("column \"%s\" of relation \"%s\" does not exist, skipping",
							colName, RelationGetRelationName(rel))));
			return;
		}
	}
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

	if (GpPolicyIsPartitioned(rel->rd_cdbpolicy))
	{
		int			ia = 0;

		for (ia = 0; ia < rel->rd_cdbpolicy->nattrs; ia++)
		{
			if (attnum == rel->rd_cdbpolicy->attrs[ia])
			{
				MemoryContext oldcontext;
				GpPolicy *policy;

				/* force a random distribution */
				rel->rd_cdbpolicy->nattrs = 0;

				oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));
				policy = GpPolicyCopy(rel->rd_cdbpolicy);
				MemoryContextSwitchTo(oldcontext);

				/*
				 * replace policy first in catalog and then assign to
				 * rd_cdbpolicy to make sure we have intended policy in relcache
				 * even with relcache invalidation. Otherwise rd_cdbpolicy can
				 * become invalid soon after assignment.
				 */
				GpPolicyReplace(RelationGetRelid(rel), policy);
				rel->rd_cdbpolicy = policy;
				if (Gp_role != GP_ROLE_EXECUTE)
				    ereport(NOTICE,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("dropping a column that is part of the distribution policy forces a NULL distribution policy")));
			}
		}
	}

	/*
	 * Propagate to children as appropriate.  Unlike most other ALTER
	 * routines, we have to do this one level of recursion at a time; we can't
	 * use find_all_inheritors to do it in one pass.
	 */
	children = find_inheritance_children(RelationGetRelid(rel), lockmode);

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

			/* find_inheritance_children already got lock */
			childrel = heap_open(childrelid, NoLock);
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
					ATExecDropColumn(wqueue, childrel, colName,
									 behavior, true, true,
									 false, lockmode);
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
				 * we need to mark the inheritors' attributes as locally
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

	performDeletion(&object, behavior, 0);

	/*
	 * If we dropped the OID column, must adjust pg_class.relhasoids and tell
	 * Phase 3 to physically get rid of the column.  We formerly left the
	 * column in place physically, but this caused subtle problems.  See
	 * http://archives.postgresql.org/pgsql-hackers/2009-02/msg00363.php
	 */
	if (attnum == ObjectIdAttributeNumber)
	{
		Relation	class_rel;
		Form_pg_class tuple_class;
		AlteredTableInfo *tab;

		class_rel = heap_open(RelationRelationId, RowExclusiveLock);

		tuple = SearchSysCacheCopy1(RELOID,
									ObjectIdGetDatum(RelationGetRelid(rel)));
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
		tab->rewrite = true;
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH) && MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "DROP COLUMN");
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
			   IndexStmt *stmt, bool is_rebuild, LOCKMODE lockmode)
{
	bool		check_rights;
	bool		skip_build;
	bool		quiet;
	Oid			new_index;

	Assert(IsA(stmt, IndexStmt));
	Assert(!stmt->concurrent);

	/* The index should already be built if we are a QE */
	if (Gp_role == GP_ROLE_EXECUTE)
		return;

	/* suppress schema rights check when rebuilding existing index */
	check_rights = !is_rebuild;
	/* skip index build if phase 3 will do it or we're reusing an old one */
	skip_build = tab->rewrite || OidIsValid(stmt->oldNode);
	/* suppress notices when rebuilding existing index */
	quiet = is_rebuild;

	/* The IndexStmt has already been through transformIndexStmt */

	new_index = DefineIndex(RelationGetRelid(rel),
							stmt,
							InvalidOid, /* no predefined OID */
							true,		/* is_alter_table */
							check_rights,
							skip_build,
							quiet);

	/*
	 * If TryReuseIndex() stashed a relfilenode for us, we used it for the new
	 * index instead of building from scratch.  The DROP of the old edition of
	 * this index will have scheduled the storage for deletion at commit, so
	 * cancel that pending deletion.
	 */
	if (OidIsValid(stmt->oldNode))
	{
		Relation	irel = index_open(new_index, NoLock);

		RelationPreserveStorage(irel->rd_node, true);
		index_close(irel, NoLock);
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "ADD INDEX");
}

/*
 * ALTER TABLE ADD CONSTRAINT USING INDEX
 */
static void
ATExecAddIndexConstraint(AlteredTableInfo *tab, Relation rel,
						 IndexStmt *stmt, LOCKMODE lockmode)
{
	Oid			index_oid = stmt->indexOid;
	Relation	indexRel;
	char	   *indexName;
	IndexInfo  *indexInfo;
	char	   *constraintName;
	char		constraintType;

	Assert(IsA(stmt, IndexStmt));
	Assert(OidIsValid(index_oid));
	Assert(stmt->isconstraint);

	/*
	 * Doing this on partitioned tables is not a simple feature to implement,
	 * so let's punt for now.
	 */
	Oid rel_id = RelationGetRelid(rel);
	bool is_root_or_interior_partition = rel_is_partitioned(rel_id)
		|| rel_is_interior_partition(rel_id);
	if (Gp_role != GP_ROLE_EXECUTE && is_root_or_interior_partition)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("ALTER TABLE / ADD CONSTRAINT USING INDEX is not supported on partitioned tables")));

	indexRel = index_open(index_oid, AccessShareLock);

	indexName = pstrdup(RelationGetRelationName(indexRel));

	indexInfo = BuildIndexInfo(indexRel);

	/* this should have been checked at parse time */
	if (!indexInfo->ii_Unique)
		elog(ERROR, "index \"%s\" is not unique", indexName);

	/*
	 * Determine name to assign to constraint.  We require a constraint to
	 * have the same name as the underlying index; therefore, use the index's
	 * existing name as the default constraint name, and if the user
	 * explicitly gives some other name for the constraint, rename the index
	 * to match.
	 */
	constraintName = stmt->idxname;
	if (constraintName == NULL)
		constraintName = indexName;
	else if (strcmp(constraintName, indexName) != 0)
	{
		if (Gp_role != GP_ROLE_EXECUTE)
			ereport(NOTICE,
				(errmsg("ALTER TABLE / ADD CONSTRAINT USING INDEX will rename index \"%s\" to \"%s\"",
						indexName, constraintName)));
		RenameRelationInternal(index_oid, constraintName, false);
	}

	/* Extra checks needed if making primary key */
	if (stmt->primary)
		index_check_primary_key(rel, indexInfo, true);

	/* Note we currently don't support EXCLUSION constraints here */
	if (stmt->primary)
		constraintType = CONSTRAINT_PRIMARY;
	else
		constraintType = CONSTRAINT_UNIQUE;

	/* Create the catalog entries for the constraint */
	index_constraint_create(rel,
							index_oid,
							InvalidOid,
							indexInfo,
							constraintName,
							constraintType,
							stmt->deferrable,
							stmt->initdeferred,
							stmt->primary,
							true,		/* update pg_index */
							true,		/* remove old dependencies */
							allowSystemTableMods,
							false);		/* is_internal */

	index_close(indexRel, NoLock);
}

/*
 * ALTER TABLE ADD CONSTRAINT
 */
static void
ATExecAddConstraint(List **wqueue, AlteredTableInfo *tab, Relation rel,
					Constraint *newConstraint, bool recurse, bool is_readd,
					LOCKMODE lockmode)
{
	Assert(IsA(newConstraint, Constraint));

	/*
	 * Currently, we only expect to see CONSTR_CHECK and CONSTR_FOREIGN nodes
	 * arriving here (see the preprocessing done in parse_utilcmd.c).  Use a
	 * switch anyway to make it easier to add more code later.
	 */
	switch (newConstraint->contype)
	{
		case CONSTR_CHECK:
			ATAddCheckConstraint(wqueue, tab, rel,
								 newConstraint, recurse, false, is_readd,
								 lockmode);
			break;

		case CONSTR_FOREIGN:

			/*
			 * Note that we currently never recurse for FK constraints, so the
			 * "recurse" flag is silently ignored.
			 *
			 * Assign or validate constraint name
			 */
			if (newConstraint->conname)
			{
				if (ConstraintNameIsUsed(CONSTRAINT_RELATION,
										 RelationGetRelid(rel),
										 RelationGetNamespace(rel),
										 newConstraint->conname))
					ereport(ERROR,
							(errcode(ERRCODE_DUPLICATE_OBJECT),
							 errmsg("constraint \"%s\" for relation \"%s\" already exists",
									newConstraint->conname,
									RelationGetRelationName(rel))));
			}
			else
				newConstraint->conname =
					ChooseConstraintName(RelationGetRelationName(rel),
								   strVal(linitial(newConstraint->fk_attrs)),
										 "fkey",
										 RelationGetNamespace(rel),
										 NIL);

			ATAddForeignKeyConstraint(tab, rel, newConstraint, lockmode);
			break;

		default:
			elog(ERROR, "unrecognized constraint type: %d",
				 (int) newConstraint->contype);
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

/*
 * Add a check constraint to a single table and its children
 *
 * Subroutine for ATExecAddConstraint.
 *
 * We must recurse to child tables during execution, rather than using
 * ALTER TABLE's normal prep-time recursion.  The reason is that all the
 * constraints *must* be given the same name, else they won't be seen as
 * related later.  If the user didn't explicitly specify a name, then
 * AddRelationNewConstraints would normally assign different names to the
 * child constraints.  To fix that, we must capture the name assigned at
 * the parent table and pass that down.
 */
static void
ATAddCheckConstraint(List **wqueue, AlteredTableInfo *tab, Relation rel,
					 Constraint *constr, bool recurse, bool recursing,
					 bool is_readd, LOCKMODE lockmode)
{
	List	   *newcons;
	ListCell   *lcon;
	List	   *children;
	ListCell   *child;

	/* At top level, permission check was done in ATPrepCmd, else do it */
	if (recursing)
		ATSimplePermissions(rel, ATT_TABLE);

	/*
	 * Call AddRelationNewConstraints to do the work, making sure it works on
	 * a copy of the Constraint so transformExpr can't modify the original. It
	 * returns a list of cooked constraints.
	 *
	 * If the constraint ends up getting merged with a pre-existing one, it's
	 * omitted from the returned list, which is what we want: we do not need
	 * to do any validation work.  That can only happen at child tables,
	 * though, since we disallow merging at the top level.
	 */
	newcons = AddRelationNewConstraints(rel, NIL,
										list_make1(copyObject(constr)),
										recursing | is_readd,	/* allow_merge */
										!recursing,		/* is_local */
										is_readd);		/* is_internal */

	/* Add each to-be-validated constraint to Phase 3's queue */
	foreach(lcon, newcons)
	{
		CookedConstraint *ccon = (CookedConstraint *) lfirst(lcon);

		if (!ccon->skip_validation)
		{
			NewConstraint *newcon;

			newcon = (NewConstraint *) palloc0(sizeof(NewConstraint));
			newcon->name = ccon->name;
			newcon->contype = ccon->contype;
			/* ExecQual wants implicit-AND format */
			newcon->qual = (Node *) make_ands_implicit((Expr *) ccon->expr);

			tab->constraints = lappend(tab->constraints, newcon);
		}

		/* Save the actually assigned name if it was defaulted */
		if (constr->conname == NULL)
			constr->conname = ccon->name;
	}

	/* At this point we must have a locked-down name to use */
	Assert(constr->conname != NULL);

	/* Advance command counter in case same table is visited multiple times */
	CommandCounterIncrement();

	/*
	 * If the constraint got merged with an existing constraint, we're done.
	 * We mustn't recurse to child tables in this case, because they've
	 * already got the constraint, and visiting them again would lead to an
	 * incorrect value for coninhcount.
	 */
	if (newcons == NIL)
		return;

	/*
	 * If adding a NO INHERIT constraint, no need to find our children.
	 */
	if (constr->is_no_inherit)
		return;

	/*
	 * Propagate to children as appropriate.  Unlike most other ALTER
	 * routines, we have to do this one level of recursion at a time; we can't
	 * use find_all_inheritors to do it in one pass.
	 */
	children = find_inheritance_children(RelationGetRelid(rel), lockmode);

	/*
	 * If we are told not to recurse, there had better not be any child tables;
	 * else the addition would put them out of step.
	 * Check if ONLY was specified with ALTER TABLE.  If so, allow the
	 * constraint creation only if there are no children currently.  Error out
	 * otherwise.
	 */
	if (Gp_role == GP_ROLE_DISPATCH && children && !recurse)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("constraint must be added to child tables too")));

	/*
	 * If this constraint needs to be recursed, this is a base/parent table,
	 * and we are the master, then cascade check constraint creation for all
	 * children.  We do it here to synchronize pg_constraint oid.
	 */
	if (recurse)
	{
		foreach(child, children)
		{
			Oid			childrelid = lfirst_oid(child);
			Relation	childrel;
			AlteredTableInfo *childtab;

			/* find_inheritance_children already got lock */
			childrel = heap_open(childrelid, NoLock);
			CheckTableNotInUse(childrel, "ALTER TABLE");

			/* Find or create work queue entry for this table */
			childtab = ATGetQueueEntry(wqueue, childrel);

			/* Recurse to child */
			ATAddCheckConstraint(wqueue, childtab, childrel,
								 constr, recurse, true, is_readd, lockmode);

			heap_close(childrel, NoLock);
		}
	}
}

/*
 * Add a foreign-key constraint to a single table
 *
 * Subroutine for ATExecAddConstraint.  Must already hold exclusive
 * lock on the rel, and have done appropriate validity checks for it.
 * We do permissions checks here, however.
 */
static void
ATAddForeignKeyConstraint(AlteredTableInfo *tab, Relation rel,
						  Constraint *fkconstraint, LOCKMODE lockmode)
{
	Relation	pkrel;
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
	bool		old_check_ok;
	ListCell   *old_pfeqop_item = list_head(fkconstraint->old_conpfeqop);

	/*
	 * Grab an exclusive lock on the pk table, so that someone doesn't delete
	 * rows out from under us. (Although a lesser lock would do for that
	 * purpose, we'll need exclusive lock anyway to add triggers to the pk
	 * table; trying to start with a lesser lock will just create a risk of
	 * deadlock.)
	 */
	if (OidIsValid(fkconstraint->old_pktable_oid))
		pkrel = heap_open(fkconstraint->old_pktable_oid, AccessExclusiveLock);
	else
		pkrel = heap_openrv(fkconstraint->pktable, AccessExclusiveLock);

	/*
	 * Validity checks (permission checks wait till we have the column
	 * numbers)
	 */
	if (pkrel->rd_rel->relkind != RELKIND_RELATION)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("referenced relation \"%s\" is not a table",
						RelationGetRelationName(pkrel))));

	if (!allowSystemTableMods && IsSystemRelation(pkrel))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						RelationGetRelationName(pkrel))));

	/*
	 * References from permanent or unlogged tables to temp tables, and from
	 * permanent tables to unlogged tables, are disallowed because the
	 * referenced data can vanish out from under us.  References from temp
	 * tables to any other table type are also disallowed, because other
	 * backends might need to run the RI triggers on the perm table, but they
	 * can't reliably see tuples in the local buffers of other backends.
	 */
	switch (rel->rd_rel->relpersistence)
	{
		case RELPERSISTENCE_PERMANENT:
			if (pkrel->rd_rel->relpersistence != RELPERSISTENCE_PERMANENT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("constraints on permanent tables may reference only permanent tables")));
			break;
		case RELPERSISTENCE_UNLOGGED:
			if (pkrel->rd_rel->relpersistence != RELPERSISTENCE_PERMANENT
				&& pkrel->rd_rel->relpersistence != RELPERSISTENCE_UNLOGGED)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("constraints on unlogged tables may reference only permanent or unlogged tables")));
			break;
		case RELPERSISTENCE_TEMP:
			if (pkrel->rd_rel->relpersistence != RELPERSISTENCE_TEMP)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("constraints on temporary tables may reference only temporary tables")));
			if (!pkrel->rd_islocaltemp || !rel->rd_islocaltemp)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("constraints on temporary tables must involve temporary tables of this session")));
			break;
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
	 * Now we can check permissions.
	 */
	checkFkeyPermissions(pkrel, pkattnum, numpks);
	checkFkeyPermissions(rel, fkattnum, numfks);

	/*
	 * Look up the equality operators to use in the constraint.
	 *
	 * Note that we have to be careful about the difference between the actual
	 * PK column type and the opclass' declared input type, which might be
	 * only binary-compatible with it.  The declared opcintype is the right
	 * thing to probe pg_amop with.
	 */
	if (numfks != numpks)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FOREIGN_KEY),
				 errmsg("number of referencing and referenced columns for foreign key disagree")));

	/*
	 * On the strength of a previous constraint, we might avoid scanning
	 * tables to validate this one.  See below.
	 */
	old_check_ok = (fkconstraint->old_conpfeqop != NIL);
	Assert(!old_check_ok || numfks == list_length(fkconstraint->old_conpfeqop));

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
		Oid			pfeqop_right;

		/* We need several fields out of the pg_opclass entry */
		cla_ht = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclasses[i]));
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
		{
			pfeqop_right = fktyped;
			ffeqop = get_opfamily_member(opfamily, fktyped, fktyped,
										 eqstrategy);
		}
		else
		{
			/* keep compiler quiet */
			pfeqop_right = InvalidOid;
			ffeqop = InvalidOid;
		}

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
			{
				pfeqop = ffeqop = ppeqop;
				pfeqop_right = opcintype;
			}
		}

		if (!(OidIsValid(pfeqop) && OidIsValid(ffeqop)))
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("foreign key constraint \"%s\" "
							"cannot be implemented",
							fkconstraint->conname),
					 errdetail("Key columns \"%s\" and \"%s\" "
							   "are of incompatible types: %s and %s.",
							   strVal(list_nth(fkconstraint->fk_attrs, i)),
							   strVal(list_nth(fkconstraint->pk_attrs, i)),
							   format_type_be(fktype),
							   format_type_be(pktype))));

		if (old_check_ok)
		{
			/*
			 * When a pfeqop changes, revalidate the constraint.  We could
			 * permit intra-opfamily changes, but that adds subtle complexity
			 * without any concrete benefit for core types.  We need not
			 * assess ppeqop or ffeqop, which RI_Initial_Check() does not use.
			 */
			old_check_ok = (pfeqop == lfirst_oid(old_pfeqop_item));
			old_pfeqop_item = lnext(old_pfeqop_item);
		}
		if (old_check_ok)
		{
			Oid			old_fktype;
			Oid			new_fktype;
			CoercionPathType old_pathtype;
			CoercionPathType new_pathtype;
			Oid			old_castfunc;
			Oid			new_castfunc;

			/*
			 * Identify coercion pathways from each of the old and new FK-side
			 * column types to the right (foreign) operand type of the pfeqop.
			 * We may assume that pg_constraint.conkey is not changing.
			 */
			old_fktype = tab->oldDesc->attrs[fkattnum[i] - 1]->atttypid;
			new_fktype = fktype;
			old_pathtype = findFkeyCast(pfeqop_right, old_fktype,
										&old_castfunc);
			new_pathtype = findFkeyCast(pfeqop_right, new_fktype,
										&new_castfunc);

			/*
			 * Upon a change to the cast from the FK column to its pfeqop
			 * operand, revalidate the constraint.  For this evaluation, a
			 * binary coercion cast is equivalent to no cast at all.  While
			 * type implementors should design implicit casts with an eye
			 * toward consistency of operations like equality, we cannot
			 * assume here that they have done so.
			 *
			 * A function with a polymorphic argument could change behavior
			 * arbitrarily in response to get_fn_expr_argtype().  Therefore,
			 * when the cast destination is polymorphic, we only avoid
			 * revalidation if the input type has not changed at all.  Given
			 * just the core data types and operator classes, this requirement
			 * prevents no would-be optimizations.
			 *
			 * If the cast converts from a base type to a domain thereon, then
			 * that domain type must be the opcintype of the unique index.
			 * Necessarily, the primary key column must then be of the domain
			 * type.  Since the constraint was previously valid, all values on
			 * the foreign side necessarily exist on the primary side and in
			 * turn conform to the domain.  Consequently, we need not treat
			 * domains specially here.
			 *
			 * Since we require that all collations share the same notion of
			 * equality (which they do, because texteq reduces to bitwise
			 * equality), we don't compare collation here.
			 *
			 * We need not directly consider the PK type.  It's necessarily
			 * binary coercible to the opcintype of the unique index column,
			 * and ri_triggers.c will only deal with PK datums in terms of
			 * that opcintype.  Changing the opcintype also changes pfeqop.
			 */
			old_check_ok = (new_pathtype == old_pathtype &&
							new_castfunc == old_castfunc &&
							(!IsPolymorphicType(pfeqop_right) ||
							 new_fktype == old_fktype));
		}

		pfeqoperators[i] = pfeqop;
		ppeqoperators[i] = ppeqop;
		ffeqoperators[i] = ffeqop;
	}

	/*
	 * Record the FK constraint in pg_constraint.
	 */
	constrOid = CreateConstraintEntry(fkconstraint->conname,
									  RelationGetNamespace(rel),
									  CONSTRAINT_FOREIGN,
									  fkconstraint->deferrable,
									  fkconstraint->initdeferred,
									  fkconstraint->initially_valid,
									  RelationGetRelid(rel),
									  fkattnum,
									  numfks,
									  InvalidOid,		/* not a domain
														 * constraint */
									  indexOid,
									  RelationGetRelid(pkrel),
									  pkattnum,
									  pfeqoperators,
									  ppeqoperators,
									  ffeqoperators,
									  numpks,
									  fkconstraint->fk_upd_action,
									  fkconstraint->fk_del_action,
									  fkconstraint->fk_matchtype,
									  NULL,		/* no exclusion constraint */
									  NULL,		/* no check constraint */
									  NULL,
									  NULL,
									  true,		/* islocal */
									  0,		/* inhcount */
									  true,		/* isnoinherit */
									  false);	/* is_internal */

	/*
	 * Create the triggers that will enforce the constraint.
	 */
	createForeignKeyTriggers(rel, RelationGetRelid(pkrel), fkconstraint,
							 constrOid, indexOid);

	/*
	 * Tell Phase 3 to check that the constraint is satisfied by existing
	 * rows. We can skip this during table creation, when requested explicitly
	 * by specifying NOT VALID in an ADD FOREIGN KEY command, and when we're
	 * recreating a constraint following a SET DATA TYPE operation that did
	 * not impugn its validity.
	 */
	if (!old_check_ok && !fkconstraint->skip_validation)
	{
		NewConstraint *newcon;

		newcon = (NewConstraint *) palloc0(sizeof(NewConstraint));
		newcon->name = fkconstraint->conname;
		newcon->contype = CONSTR_FOREIGN;
		newcon->refrelid = RelationGetRelid(pkrel);
		newcon->refindid = indexOid;
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
 * ALTER TABLE ALTER CONSTRAINT
 *
 * Update the attributes of a constraint.
 *
 * Currently only works for Foreign Key constraints.
 * Foreign keys do not inherit, so we purposely ignore the
 * recursion bit here, but we keep the API the same for when
 * other constraint types are supported.
 */
static void
ATExecAlterConstraint(Relation rel, AlterTableCmd *cmd,
					  bool recurse, bool recursing, LOCKMODE lockmode)
{
	Constraint *cmdcon;
	Relation	conrel;
	SysScanDesc scan;
	ScanKeyData key;
	HeapTuple	contuple;
	Form_pg_constraint currcon = NULL;
	bool		found = false;

	Assert(IsA(cmd->def, Constraint));
	cmdcon = (Constraint *) cmd->def;

	conrel = heap_open(ConstraintRelationId, RowExclusiveLock);

	/*
	 * Find and check the target constraint
	 */
	ScanKeyInit(&key,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	scan = systable_beginscan(conrel, ConstraintRelidIndexId,
							  true, NULL, 1, &key);

	while (HeapTupleIsValid(contuple = systable_getnext(scan)))
	{
		currcon = (Form_pg_constraint) GETSTRUCT(contuple);
		if (strcmp(NameStr(currcon->conname), cmdcon->conname) == 0)
		{
			found = true;
			break;
		}
	}

	if (!found)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("constraint \"%s\" of relation \"%s\" does not exist",
						cmdcon->conname, RelationGetRelationName(rel))));

	if (currcon->contype != CONSTRAINT_FOREIGN)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("constraint \"%s\" of relation \"%s\" is not a foreign key constraint",
						cmdcon->conname, RelationGetRelationName(rel))));

	if (currcon->condeferrable != cmdcon->deferrable ||
		currcon->condeferred != cmdcon->initdeferred)
	{
		HeapTuple	copyTuple;
		HeapTuple	tgtuple;
		Form_pg_constraint copy_con;
		List	   *otherrelids = NIL;
		ScanKeyData tgkey;
		SysScanDesc tgscan;
		Relation	tgrel;
		ListCell   *lc;

		/*
		 * Now update the catalog, while we have the door open.
		 */
		copyTuple = heap_copytuple(contuple);
		copy_con = (Form_pg_constraint) GETSTRUCT(copyTuple);
		copy_con->condeferrable = cmdcon->deferrable;
		copy_con->condeferred = cmdcon->initdeferred;
		simple_heap_update(conrel, &copyTuple->t_self, copyTuple);
		CatalogUpdateIndexes(conrel, copyTuple);

		InvokeObjectPostAlterHook(ConstraintRelationId,
								  HeapTupleGetOid(contuple), 0);

		heap_freetuple(copyTuple);

		/*
		 * Now we need to update the multiple entries in pg_trigger that
		 * implement the constraint.
		 */
		tgrel = heap_open(TriggerRelationId, RowExclusiveLock);

		ScanKeyInit(&tgkey,
					Anum_pg_trigger_tgconstraint,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(HeapTupleGetOid(contuple)));

		tgscan = systable_beginscan(tgrel, TriggerConstraintIndexId, true,
									NULL, 1, &tgkey);

		while (HeapTupleIsValid(tgtuple = systable_getnext(tgscan)))
		{
			Form_pg_trigger tgform = (Form_pg_trigger) GETSTRUCT(tgtuple);
			Form_pg_trigger copy_tg;

			/*
			 * Remember OIDs of other relation(s) involved in FK constraint.
			 * (Note: it's likely that we could skip forcing a relcache inval
			 * for other rels that don't have a trigger whose properties
			 * change, but let's be conservative.)
			 */
			if (tgform->tgrelid != RelationGetRelid(rel))
				otherrelids = list_append_unique_oid(otherrelids,
													 tgform->tgrelid);

			/*
			 * Update deferrability of RI_FKey_noaction_del,
			 * RI_FKey_noaction_upd, RI_FKey_check_ins and RI_FKey_check_upd
			 * triggers, but not others; see createForeignKeyTriggers and
			 * CreateFKCheckTrigger.
			 */
			if (tgform->tgfoid != F_RI_FKEY_NOACTION_DEL &&
				tgform->tgfoid != F_RI_FKEY_NOACTION_UPD &&
				tgform->tgfoid != F_RI_FKEY_CHECK_INS &&
				tgform->tgfoid != F_RI_FKEY_CHECK_UPD)
				continue;

			copyTuple = heap_copytuple(tgtuple);
			copy_tg = (Form_pg_trigger) GETSTRUCT(copyTuple);

			copy_tg->tgdeferrable = cmdcon->deferrable;
			copy_tg->tginitdeferred = cmdcon->initdeferred;
			simple_heap_update(tgrel, &copyTuple->t_self, copyTuple);
			CatalogUpdateIndexes(tgrel, copyTuple);

			InvokeObjectPostAlterHook(TriggerRelationId,
									  HeapTupleGetOid(tgtuple), 0);

			heap_freetuple(copyTuple);
		}

		systable_endscan(tgscan);

		heap_close(tgrel, RowExclusiveLock);

		/*
		 * Invalidate relcache so that others see the new attributes.  We must
		 * inval both the named rel and any others having relevant triggers.
		 * (At present there should always be exactly one other rel, but
		 * there's no need to hard-wire such an assumption here.)
		 */
		CacheInvalidateRelcache(rel);
		foreach(lc, otherrelids)
		{
			CacheInvalidateRelcacheByRelid(lfirst_oid(lc));
		}
	}

	systable_endscan(scan);

	heap_close(conrel, RowExclusiveLock);
}

/*
 * ALTER TABLE VALIDATE CONSTRAINT
 *
 * XXX The reason we handle recursion here rather than at Phase 1 is because
 * there's no good way to skip recursing when handling foreign keys: there is
 * no need to lock children in that case, yet we wouldn't be able to avoid
 * doing so at that level.
 */
static void
ATExecValidateConstraint(Relation rel, char *constrName, bool recurse,
						 bool recursing, LOCKMODE lockmode)
{
	Relation	conrel;
	SysScanDesc scan;
	ScanKeyData key;
	HeapTuple	tuple;
	Form_pg_constraint con = NULL;
	bool		found = false;

	conrel = heap_open(ConstraintRelationId, RowExclusiveLock);

	/*
	 * Find and check the target constraint
	 */
	ScanKeyInit(&key,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	scan = systable_beginscan(conrel, ConstraintRelidIndexId,
							  true, NULL, 1, &key);

	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
	{
		con = (Form_pg_constraint) GETSTRUCT(tuple);
		if (strcmp(NameStr(con->conname), constrName) == 0)
		{
			found = true;
			break;
		}
	}

	if (!found)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("constraint \"%s\" of relation \"%s\" does not exist",
						constrName, RelationGetRelationName(rel))));

	if (con->contype != CONSTRAINT_FOREIGN &&
		con->contype != CONSTRAINT_CHECK)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("constraint \"%s\" of relation \"%s\" is not a foreign key or check constraint",
						constrName, RelationGetRelationName(rel))));

	if (!con->convalidated)
	{
		HeapTuple	copyTuple;
		Form_pg_constraint copy_con;

		if (con->contype == CONSTRAINT_FOREIGN)
		{
			Oid			conid = HeapTupleGetOid(tuple);
			Relation	refrel;

			/*
			 * Triggers are already in place on both tables, so a concurrent
			 * write that alters the result here is not possible. Normally we
			 * can run a query here to do the validation, which would only
			 * require AccessShareLock. In some cases, it is possible that we
			 * might need to fire triggers to perform the check, so we take a
			 * lock at RowShareLock level just in case.
			 */
			refrel = heap_open(con->confrelid, RowShareLock);

			validateForeignKeyConstraint(constrName, rel, refrel,
										 con->conindid,
										 conid);
			heap_close(refrel, NoLock);

			/*
			 * Foreign keys do not inherit, so we purposely ignore the
			 * recursion bit here
			 */
		}
		else if (con->contype == CONSTRAINT_CHECK)
		{
			List	   *children = NIL;
			ListCell   *child;

			/*
			 * If we're recursing, the parent has already done this, so skip
			 * it.  Also, if the constraint is a NO INHERIT constraint, we
			 * shouldn't try to look for it in the children.
			 */
			if (!recursing && !con->connoinherit)
				children = find_all_inheritors(RelationGetRelid(rel),
											   lockmode, NULL);

			/*
			 * For CHECK constraints, we must ensure that we only mark the
			 * constraint as validated on the parent if it's already validated
			 * on the children.
			 *
			 * We recurse before validating on the parent, to reduce risk of
			 * deadlocks.
			 */
			foreach(child, children)
			{
				Oid			childoid = lfirst_oid(child);
				Relation	childrel;

				if (childoid == RelationGetRelid(rel))
					continue;

				/*
				 * If we are told not to recurse, there had better not be any
				 * child tables; else the addition would put them out of step.
				 */
				if (!recurse)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("constraint must be validated on child tables too")));

				/* find_all_inheritors already got lock */
				childrel = heap_open(childoid, NoLock);

				ATExecValidateConstraint(childrel, constrName, false,
										 true, lockmode);
				heap_close(childrel, NoLock);
			}

			validateCheckConstraint(rel, tuple);

			/*
			 * Invalidate relcache so that others see the new validated
			 * constraint.
			 */
			CacheInvalidateRelcache(rel);
		}

		/*
		 * Now update the catalog, while we have the door open.
		 */
		copyTuple = heap_copytuple(tuple);
		copy_con = (Form_pg_constraint) GETSTRUCT(copyTuple);
		copy_con->convalidated = true;
		simple_heap_update(conrel, &copyTuple->t_self, copyTuple);
		CatalogUpdateIndexes(conrel, copyTuple);

		InvokeObjectPostAlterHook(ConstraintRelationId,
								  HeapTupleGetOid(tuple), 0);

		heap_freetuple(copyTuple);
	}

	systable_endscan(scan);

	heap_close(conrel, RowExclusiveLock);
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
 *	for the pkrel.  Also return the index OID and index opclasses of the
 *	index supporting the primary key.
 *
 *	All parameters except pkrel are output parameters.  Also, the function
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

		indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexoid));
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexoid);
		indexStruct = (Form_pg_index) GETSTRUCT(indexTuple);
		if (indexStruct->indisprimary && IndexIsValid(indexStruct))
		{
			/*
			 * Refuse to use a deferrable primary key.  This is per SQL spec,
			 * and there would be a lot of interesting semantic problems if we
			 * tried to allow it.
			 */
			if (!indexStruct->indimmediate)
				ereport(ERROR,
						(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						 errmsg("cannot use a deferrable primary key for referenced table \"%s\"",
								RelationGetRelationName(pkrel))));

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
	bool		found_deferrable = false;
	List	   *indexoidlist;
	ListCell   *indexoidscan;
	int			i,
				j;

	/*
	 * Reject duplicate appearances of columns in the referenced-columns list.
	 * Such a case is forbidden by the SQL standard, and even if we thought it
	 * useful to allow it, there would be ambiguity about how to match the
	 * list to unique indexes (in particular, it'd be unclear which index
	 * opclass goes with which FK column).
	 */
	for (i = 0; i < numattrs; i++)
	{
		for (j = i + 1; j < numattrs; j++)
		{
			if (attnums[i] == attnums[j])
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_FOREIGN_KEY),
						 errmsg("foreign key referenced-columns list must not contain duplicates")));
		}
	}

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

		indexoid = lfirst_oid(indexoidscan);
		indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexoid));
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
			Datum		indclassDatum;
			bool		isnull;
			oidvector  *indclass;

			/* Must get indclass the hard way */
			indclassDatum = SysCacheGetAttr(INDEXRELID, indexTuple,
											Anum_pg_index_indclass, &isnull);
			Assert(!isnull);
			indclass = (oidvector *) DatumGetPointer(indclassDatum);

			/*
			 * The given attnum list may match the index columns in any order.
			 * Check for a match, and extract the appropriate opclasses while
			 * we're at it.
			 *
			 * We know that attnums[] is duplicate-free per the test at the
			 * start of this function, and we checked above that the number of
			 * index columns agrees, so if we find a match for each attnums[]
			 * entry then we must have a one-to-one match in some order.
			 */
			for (i = 0; i < numattrs; i++)
			{
				found = false;
				for (j = 0; j < numattrs; j++)
				{
					if (attnums[i] == indexStruct->indkey.values[j])
					{
						opclasses[i] = indclass->values[j];
						found = true;
						break;
					}
				}
				if (!found)
					break;
			}

			/*
			 * Refuse to use a deferrable unique/primary key.  This is per SQL
			 * spec, and there would be a lot of interesting semantic problems
			 * if we tried to allow it.
			 */
			if (found && !indexStruct->indimmediate)
			{
				/*
				 * Remember that we found an otherwise matching index, so that
				 * we can generate a more appropriate error message.
				 */
				found_deferrable = true;
				found = false;
			}
		}
		ReleaseSysCache(indexTuple);
		if (found)
			break;
	}

	if (!found)
	{
		if (found_deferrable)
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("cannot use a deferrable unique constraint for referenced table \"%s\"",
							RelationGetRelationName(pkrel))));
		else
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_FOREIGN_KEY),
					 errmsg("there is no unique constraint matching given keys for referenced table \"%s\"",
							RelationGetRelationName(pkrel))));
	}

	list_free(indexoidlist);

	return indexoid;
}

/*
 * findFkeyCast -
 *
 *	Wrapper around find_coercion_pathway() for ATAddForeignKeyConstraint().
 *	Caller has equal regard for binary coercibility and for an exact match.
*/
static CoercionPathType
findFkeyCast(Oid targetTypeId, Oid sourceTypeId, Oid *funcid)
{
	CoercionPathType ret;

	if (targetTypeId == sourceTypeId)
	{
		ret = COERCION_PATH_RELABELTYPE;
		*funcid = InvalidOid;
	}
	else
	{
		ret = find_coercion_pathway(targetTypeId, sourceTypeId,
									COERCION_IMPLICIT, funcid);
		if (ret == COERCION_PATH_NONE)
			/* A previously-relied-upon cast is now gone. */
			elog(ERROR, "could not find cast from %u to %u",
				 sourceTypeId, targetTypeId);
	}

	return ret;
}

/* Permissions checks for ADD FOREIGN KEY */
static void
checkFkeyPermissions(Relation rel, int16 *attnums, int natts)
{
	Oid			roleid = GetUserId();
	AclResult	aclresult;
	int			i;

	/* Okay if we have relation-level REFERENCES permission */
	aclresult = pg_class_aclcheck(RelationGetRelid(rel), roleid,
								  ACL_REFERENCES);
	if (aclresult == ACLCHECK_OK)
		return;
	/* Else we must have REFERENCES on each column */
	for (i = 0; i < natts; i++)
	{
		aclresult = pg_attribute_aclcheck(RelationGetRelid(rel), attnums[i],
										  roleid, ACL_REFERENCES);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_CLASS,
						   RelationGetRelationName(rel));
	}
}

/*
 * Scan the existing rows in a table to verify they meet a proposed
 * CHECK constraint.
 *
 * The caller must have opened and locked the relation appropriately.
 */
static void
validateCheckConstraint(Relation rel, HeapTuple constrtup)
{
	EState	   *estate;
	Datum		val;
	char	   *conbin;
	Expr	   *origexpr;
	List	   *exprstate;
	TupleDesc	tupdesc;
	HeapScanDesc scan;
	HeapTuple	tuple;
	ExprContext *econtext;
	MemoryContext oldcxt;
	TupleTableSlot *slot;
	Form_pg_constraint constrForm;
	bool		isnull;
	Snapshot	snapshot;

	constrForm = (Form_pg_constraint) GETSTRUCT(constrtup);

	estate = CreateExecutorState();

	/*
	 * XXX this tuple doesn't really come from a syscache, but this doesn't
	 * matter to SysCacheGetAttr, because it only wants to be able to fetch
	 * the tupdesc
	 */
	val = SysCacheGetAttr(CONSTROID, constrtup, Anum_pg_constraint_conbin,
						  &isnull);
	if (isnull)
		elog(ERROR, "null conbin for constraint %u",
			 HeapTupleGetOid(constrtup));
	conbin = TextDatumGetCString(val);
	origexpr = (Expr *) stringToNode(conbin);
	exprstate = (List *)
		ExecPrepareExpr((Expr *) make_ands_implicit(origexpr), estate);

	econtext = GetPerTupleExprContext(estate);
	tupdesc = RelationGetDescr(rel);
	slot = MakeSingleTupleTableSlot(tupdesc);
	econtext->ecxt_scantuple = slot;

	snapshot = RegisterSnapshot(GetLatestSnapshot());
	scan = heap_beginscan(rel, snapshot, 0, NULL);

	/*
	 * Switch to per-tuple memory context and reset it for each tuple
	 * produced, so we don't leak memory.
	 */
	oldcxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		ExecStoreHeapTuple(tuple, slot, InvalidBuffer, false);

		if (!ExecQual(exprstate, econtext, true))
			ereport(ERROR,
					(errcode(ERRCODE_CHECK_VIOLATION),
					 errmsg("check constraint \"%s\" is violated by some row",
							NameStr(constrForm->conname)),
					 errtableconstraint(rel, NameStr(constrForm->conname))));

		ResetExprContext(econtext);
	}

	MemoryContextSwitchTo(oldcxt);
	heap_endscan(scan);
	UnregisterSnapshot(snapshot);
	ExecDropSingleTupleTableSlot(slot);
	FreeExecutorState(estate);
}

/*
 * Scan the existing rows in a table to verify they meet a proposed FK
 * constraint.
 *
 * Caller must have opened and locked both relations appropriately.
 */
static void
validateForeignKeyConstraint(char *conname,
							 Relation rel,
							 Relation pkrel,
							 Oid pkindOid,
							 Oid constraintOid)
{
	HeapScanDesc scan;
	HeapTuple	tuple;
	Trigger		trig;
	Snapshot	snapshot;

	ereport(DEBUG1,
			(errmsg("validating foreign key constraint \"%s\"", conname)));

	/* Greenplum Database: Ignore foreign keys for now, with a warning. */
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
		ereport(WARNING,
				(errcode(ERRCODE_GP_FEATURE_NOT_YET),
				 errmsg("referential integrity (FOREIGN KEY) constraints are not supported in Greenplum Database, will not be enforced")));

	/*
	 * Build a trigger call structure; we'll need it either way.
	 */
	MemSet(&trig, 0, sizeof(trig));
	trig.tgoid = InvalidOid;
	trig.tgname = conname;
	trig.tgenabled = TRIGGER_FIRES_ON_ORIGIN;
	trig.tgisinternal = TRUE;
	trig.tgconstrrelid = RelationGetRelid(pkrel);
	trig.tgconstrindid = pkindOid;
	trig.tgconstraint = constraintOid;
	trig.tgdeferrable = FALSE;
	trig.tginitdeferred = FALSE;
	/* we needn't fill in tgargs or tgqual */

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
	snapshot = RegisterSnapshot(GetLatestSnapshot());
	scan = heap_beginscan(rel, snapshot, 0, NULL);

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
	UnregisterSnapshot(snapshot);
}

static void
CreateFKCheckTrigger(Oid myRelOid, Oid refRelOid, Constraint *fkconstraint,
					 Oid constraintOid, Oid indexOid, bool on_insert)
{
	CreateTrigStmt *fk_trigger;
	Oid trigobj;

	/*
	 * Note: for a self-referential FK (referencing and referenced tables are
	 * the same), it is important that the ON UPDATE action fires before the
	 * CHECK action, since both triggers will fire on the same row during an
	 * UPDATE event; otherwise the CHECK trigger will be checking a non-final
	 * state of the row.  Triggers fire in name order, so we ensure this by
	 * using names like "RI_ConstraintTrigger_a_NNNN" for the action triggers
	 * and "RI_ConstraintTrigger_c_NNNN" for the check triggers.
	 */
	fk_trigger = makeNode(CreateTrigStmt);
	fk_trigger->trigname = "RI_ConstraintTrigger_c";
	fk_trigger->relation = NULL;
	fk_trigger->row = true;
	fk_trigger->timing = TRIGGER_TYPE_AFTER;

	/* Either ON INSERT or ON UPDATE */
	if (on_insert)
	{
		fk_trigger->funcname = SystemFuncName("RI_FKey_check_ins");
		fk_trigger->events = TRIGGER_TYPE_INSERT;
		fk_trigger->trigOid = fkconstraint->trig1Oid;
	}
	else
	{
		fk_trigger->funcname = SystemFuncName("RI_FKey_check_upd");
		fk_trigger->events = TRIGGER_TYPE_UPDATE;
		fk_trigger->trigOid = fkconstraint->trig2Oid;
	}

	fk_trigger->columns = NIL;
	fk_trigger->whenClause = NULL;
	fk_trigger->isconstraint = true;
	fk_trigger->deferrable = fkconstraint->deferrable;
	fk_trigger->initdeferred = fkconstraint->initdeferred;
	fk_trigger->constrrel = NULL;
	fk_trigger->args = NIL;

	trigobj = CreateTrigger(fk_trigger, NULL, myRelOid, refRelOid, constraintOid,
							indexOid, true);

	if (on_insert)
		fkconstraint->trig1Oid = trigobj;
	else
		fkconstraint->trig2Oid = trigobj;

	/* Make changes-so-far visible */
	CommandCounterIncrement();
}

/*
 * Create the triggers that implement an FK constraint.
 *
 * NB: if you change any trigger properties here, see also
 * ATExecAlterConstraint.
 */
static void
createForeignKeyTriggers(Relation rel, Oid refRelOid, Constraint *fkconstraint,
						 Oid constraintOid, Oid indexOid)
{
	Oid			myRelOid;
	CreateTrigStmt *fk_trigger;

	/*
	 * Special for Greenplum Database: Ignore foreign keys for now, with warning
	 */
	if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
	{
		ereport(WARNING,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("referential integrity (FOREIGN KEY) constraints are not supported in Greenplum Database, will not be enforced")));
	}

	myRelOid = RelationGetRelid(rel);

	/* Make changes-so-far visible */
	CommandCounterIncrement();

	/*
	 * Build and execute a CREATE CONSTRAINT TRIGGER statement for the ON
	 * DELETE action on the referenced table.
	 */
	fk_trigger = makeNode(CreateTrigStmt);
	fk_trigger->trigname = "RI_ConstraintTrigger_a";
	fk_trigger->relation = NULL;
	fk_trigger->row = true;
	fk_trigger->timing = TRIGGER_TYPE_AFTER;
	fk_trigger->events = TRIGGER_TYPE_DELETE;
	fk_trigger->columns = NIL;
	fk_trigger->whenClause = NULL;
	fk_trigger->isconstraint = true;
	fk_trigger->constrrel = NULL;
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

	fkconstraint->trig3Oid = CreateTrigger(fk_trigger, NULL, refRelOid, myRelOid, constraintOid,
										   indexOid, true);

	/* Make changes-so-far visible */
	CommandCounterIncrement();

	/*
	 * Build and execute a CREATE CONSTRAINT TRIGGER statement for the ON
	 * UPDATE action on the referenced table.
	 */
	fk_trigger = makeNode(CreateTrigStmt);
	fk_trigger->trigname = "RI_ConstraintTrigger_a";
	fk_trigger->relation = NULL;
	fk_trigger->row = true;
	fk_trigger->timing = TRIGGER_TYPE_AFTER;
	fk_trigger->events = TRIGGER_TYPE_UPDATE;
	fk_trigger->columns = NIL;
	fk_trigger->whenClause = NULL;
	fk_trigger->isconstraint = true;
	fk_trigger->constrrel = NULL;
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

	fkconstraint->trig4Oid = CreateTrigger(fk_trigger, NULL, refRelOid, myRelOid, constraintOid,
										   indexOid, true);

	/* Make changes-so-far visible */
	CommandCounterIncrement();

	/*
	 * Build and execute CREATE CONSTRAINT TRIGGER statements for the CHECK
	 * action for both INSERTs and UPDATEs on the referencing table.
	 */
	CreateFKCheckTrigger(myRelOid, refRelOid, fkconstraint, constraintOid,
						 indexOid, true);
	CreateFKCheckTrigger(myRelOid, refRelOid, fkconstraint, constraintOid,
						 indexOid, false);
}

/*
 * ALTER TABLE DROP CONSTRAINT
 *
 * Like DROP COLUMN, we can't use the normal ALTER TABLE recursion mechanism.
 */
static void
ATExecDropConstraint(Relation rel, const char *constrName,
					 DropBehavior behavior,
					 bool recurse, bool recursing,
					 bool missing_ok, LOCKMODE lockmode)
{
	List	   *children;
	ListCell   *child;
	Relation	conrel;
	Form_pg_constraint con;
	SysScanDesc scan;
	ScanKeyData key;
	HeapTuple	tuple;
	bool		found = false;
	bool		is_no_inherit_constraint = false;
	bool		is_check_constraint = false;

	/* At top level, permission check was done in ATPrepCmd, else do it */
	if (recursing)
		ATSimplePermissions(rel, ATT_TABLE);

	conrel = heap_open(ConstraintRelationId, RowExclusiveLock);

	/*
	 * Find and drop the target constraint
	 */
	ScanKeyInit(&key,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	scan = systable_beginscan(conrel, ConstraintRelidIndexId,
							  true, NULL, 1, &key);

	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
	{
		ObjectAddress conobj;

		con = (Form_pg_constraint) GETSTRUCT(tuple);

		if (strcmp(NameStr(con->conname), constrName) != 0)
			continue;

		/* Don't drop inherited constraints */
		if (con->coninhcount > 0 && !recursing)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot drop inherited constraint \"%s\" of relation \"%s\"",
							constrName, RelationGetRelationName(rel))));

		/* Right now only CHECK constraints can be inherited */
		if (con->contype == CONSTRAINT_CHECK)
			is_check_constraint = true;

		is_no_inherit_constraint = con->connoinherit;

		/*
		 * If it's a foreign-key constraint, we'd better lock the referenced
		 * table and check that that's not in use, just as we've already done
		 * for the constrained table (else we might, eg, be dropping a trigger
		 * that has unfired events).  But we can/must skip that in the
		 * self-referential case.
		 */
		if (con->contype == CONSTRAINT_FOREIGN &&
			con->confrelid != RelationGetRelid(rel))
		{
			Relation	frel;

			/* Must match lock taken by RemoveTriggerById: */
			frel = heap_open(con->confrelid, AccessExclusiveLock);
			CheckTableNotInUse(frel, "ALTER TABLE");
			heap_close(frel, NoLock);
		}

		/*
		 * Perform the actual constraint deletion
		 */
		conobj.classId = ConstraintRelationId;
		conobj.objectId = HeapTupleGetOid(tuple);
		conobj.objectSubId = 0;

		performDeletion(&conobj, behavior, 0);

		found = true;

		/* constraint found and dropped -- no need to keep looping */
		break;
	}

	systable_endscan(scan);

	if (!found)
	{
		if (!missing_ok)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
				errmsg("constraint \"%s\" of relation \"%s\" does not exist",
					   constrName, RelationGetRelationName(rel))));
		}
		else
		{
			ereport(NOTICE,
					(errmsg("constraint \"%s\" of relation \"%s\" does not exist, skipping",
							constrName, RelationGetRelationName(rel))));
			heap_close(conrel, RowExclusiveLock);
			return;
		}
	}

	/*
	 * Propagate to children as appropriate.  Unlike most other ALTER
	 * routines, we have to do this one level of recursion at a time; we can't
	 * use find_all_inheritors to do it in one pass.
	 */
	if (!is_no_inherit_constraint && is_check_constraint)
		children = find_inheritance_children(RelationGetRelid(rel), lockmode);
	else
		children = NIL;

	foreach(child, children)
	{
		Oid			childrelid = lfirst_oid(child);
		Relation	childrel;
		HeapTuple	copy_tuple;

		/* find_inheritance_children already got lock */
		childrel = heap_open(childrelid, NoLock);
		CheckTableNotInUse(childrel, "ALTER TABLE");

		ScanKeyInit(&key,
					Anum_pg_constraint_conrelid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(childrelid));
		scan = systable_beginscan(conrel, ConstraintRelidIndexId,
								  true, NULL, 1, &key);

		/* scan for matching tuple - there should only be one */
		while (HeapTupleIsValid(tuple = systable_getnext(scan)))
		{
			con = (Form_pg_constraint) GETSTRUCT(tuple);

			/* Right now only CHECK constraints can be inherited */
			if (con->contype != CONSTRAINT_CHECK)
				continue;

			if (strcmp(NameStr(con->conname), constrName) == 0)
				break;
		}

		if (!HeapTupleIsValid(tuple))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
				errmsg("constraint \"%s\" of relation \"%s\" does not exist",
					   constrName,
					   RelationGetRelationName(childrel))));

		copy_tuple = heap_copytuple(tuple);

		systable_endscan(scan);

		con = (Form_pg_constraint) GETSTRUCT(copy_tuple);

		if (con->coninhcount <= 0)		/* shouldn't happen */
			elog(ERROR, "relation %u has non-inherited constraint \"%s\"",
				 childrelid, constrName);

		if (recurse)
		{
			/*
			 * If the child constraint has other definition sources, just
			 * decrement its inheritance count; if not, recurse to delete it.
			 */
			if (con->coninhcount == 1 && !con->conislocal)
			{
				/* Time to delete this child constraint, too */
				ATExecDropConstraint(childrel, constrName, behavior,
									 true, true,
									 false, lockmode);
			}
			else
			{
				/* Child constraint must survive my deletion */
				con->coninhcount--;
				simple_heap_update(conrel, &copy_tuple->t_self, copy_tuple);
				CatalogUpdateIndexes(conrel, copy_tuple);

				/* Make update visible */
				CommandCounterIncrement();
			}
		}
		else
		{
			/*
			 * If we were told to drop ONLY in this table (no recursion), we
			 * need to mark the inheritors' constraints as locally defined
			 * rather than inherited.
			 */
			con->coninhcount--;
			con->conislocal = true;

			simple_heap_update(conrel, &copy_tuple->t_self, copy_tuple);
			CatalogUpdateIndexes(conrel, copy_tuple);

			/* Make update visible */
			CommandCounterIncrement();
		}

		heap_freetuple(copy_tuple);

		heap_close(childrel, NoLock);
	}

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH) && MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "DROP CONSTRAINT");

	heap_close(conrel, RowExclusiveLock);
}

/*
 * ALTER COLUMN TYPE
 */
static void
ATPrepAlterColumnType(List **wqueue,
					  AlteredTableInfo *tab, Relation rel,
					  bool recurse, bool recursing,
					  AlterTableCmd *cmd, LOCKMODE lockmode)
{
	char	   *colName = cmd->name;
	ColumnDef  *def = (ColumnDef *) cmd->def;
	TypeName   *typeName = def->typeName;
	Node	   *transform = def->raw_default;
	HeapTuple	tuple;
	Form_pg_attribute attTup;
	AttrNumber	attnum;
	Oid			targettype;
	int32		targettypmod;
	Oid			targetcollid;
	NewColumnValue *newval;
	ParseState *pstate = make_parsestate(NULL);
	AclResult	aclresult;
	Oid			new_opclass = InvalidOid;
	Oid			old_opclass = InvalidOid;
	Oid			old_opfamily = InvalidOid;
	Oid			new_opfamily = InvalidOid;

	if (rel->rd_rel->reloftype && !recursing)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot alter column type of typed table")));

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
	typenameTypeIdAndMod(NULL, typeName, &targettype, &targettypmod);

	aclresult = pg_type_aclcheck(targettype, GetUserId(), ACL_USAGE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error_type(aclresult, targettype);

	/* And the collation */
	targetcollid = GetColumnDefCollation(NULL, def, targettype);

	/* make sure datatype is legal for a column */
	CheckAttributeType(colName, targettype, targetcollid,
					   list_make1_oid(rel->rd_rel->reltype),
					   false);

	/*
	 * If the column is part of the distribution key, look up the new operator
	 * class
	 */
	if (rel->rd_cdbpolicy)
	{
		GpPolicy   *policy = rel->rd_cdbpolicy;
		int			i;

		for (i = 0; i < policy->nattrs; i++)
		{
			if (policy->attrs[i] == attnum)
			{
				/* found it! */
				old_opclass = policy->opclasses[i];
				break;
			}
		}

		if (old_opclass)
		{
			old_opfamily = get_opclass_family(old_opclass);

			new_opclass = GetDefaultOpClass(targettype, HASH_AM_OID);
			if (new_opclass)
			{
				new_opfamily = get_opclass_family(new_opclass);

				if (new_opclass != old_opclass)
					tab->new_opclass = new_opclass;

				if (new_opfamily != old_opfamily)
					tab->dist_opfamily_changed = true;
			}
			else
			{
				/*
				 * The new datatype doesn't have a default operator class.
				 * We'll have to turn the table randomly distributed.
				 */
				new_opfamily = InvalidOid;
				tab->new_opclass = InvalidOid;
				tab->dist_opfamily_changed = true;
			}
		}
	}

	if (tab->relkind == RELKIND_RELATION &&
		rel->rd_rel->relstorage != RELSTORAGE_EXTERNAL)
	{
		/*
		 * Set up an expression to transform the old data value to the new
		 * type. If a USING option was given, transform and use that
		 * expression, else just take the old value and try to coerce it.  We
		 * do this first so that type incompatibility can be detected before
		 * we waste effort, and because we need the expression to be parsed
		 * against the original table row type.
		 */
		/*
		 * GPDB: we always need the RTE. The main reason being to support
		 * unknown to text implicit conversion for queries like
		 * CREATE TABLE t AS SELECT j AS a, 'abc' AS i FROM
		 * generate_series(1, 10) j;
		 *
		 * Executes autostats internally which runs query to collect the same
		 * which fails without RTE. More supported examples of unknown to text
		 * can be found in src/test/regress/sql/strings.sql.
		 *
		 * GPDB_10_MERGE_FIXME: Upstream fixes this problem in PG10 hence need
		 * to live with this code till then.
		 */
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

		if (transform)
		{
			transform = transformExpr(pstate, transform,
									  EXPR_KIND_ALTER_COL_TRANSFORM);

			/* It can't return a set */
			if (expression_returns_set(transform))
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
					  errmsg("transform expression must not return a set")));
		}
		else
		{
			transform = (Node *) makeVar(1, attnum,
										 attTup->atttypid, attTup->atttypmod,
										 attTup->attcollation,
										 0);
		}

		transform = coerce_to_target_type(pstate,
										  transform, exprType(transform),
										  targettype, targettypmod,
										  COERCION_ASSIGNMENT,
										  COERCE_IMPLICIT_CAST,
										  -1);
		if (transform == NULL)
		{
			/* error text depends on whether USING was specified or not */
			if (def->raw_default != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("result of USING clause for column \"%s\""
								" cannot be cast automatically to type %s",
								colName, format_type_be(targettype)),
						 errhint("You might need to add an explicit cast.")));
			else
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("column \"%s\" cannot be cast automatically to type %s",
								colName, format_type_be(targettype)),
				/* translator: USING is SQL, don't translate it */
					   errhint("You might need to specify \"USING %s::%s\".",
							   quote_identifier(colName),
							   format_type_with_typemod(targettype,
														targettypmod))));
		}

		/* Fix collations after all else */
		assign_expr_collations(pstate, transform);

		/* Plan the expr now so we can accurately assess the need to rewrite. */
		transform = (Node *) expression_planner((Expr *) transform);

		/*
		 * Add a work queue item to make ATRewriteTable update the column
		 * contents.
		 */
		newval = (NewColumnValue *) palloc0(sizeof(NewColumnValue));
		newval->attnum = attnum;
		newval->expr = (Expr *) transform;

		tab->newvals = lappend(tab->newvals, newval);
		if (ATColumnChangeRequiresRewrite(transform, attnum))
			tab->rewrite = true;

		/*
		 * If the column is part of the distribution key, and the new opclass is not
		 * in the same family as the old one, we'll need to rewrite the table because
		 * the distribution changes. (Unless the new datatype is not hashable, in
		 * which case we're going to drop it from the distribution key, and make
		 * the table randomly distributed.)
		 */
		if (old_opfamily != new_opfamily && new_opfamily != InvalidOid)
			tab->rewrite = true;
	}
	else if (transform &&
			 rel->rd_rel->relstorage == RELSTORAGE_EXTERNAL)
	{
		/* Just to give a better error message than "foo is not a table" */
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot specify a USING expression when altering an external table")));
	}
	else if (transform)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table",
						RelationGetRelationName(rel))));

	if (tab->relkind == RELKIND_COMPOSITE_TYPE ||
		tab->relkind == RELKIND_FOREIGN_TABLE)
	{
		/*
		 * For composite types, do this check now.  Tables will check it later
		 * when the table is being rewritten.
		 */
		find_composite_type_dependencies(rel->rd_rel->reltype, rel, NULL);
	}

	ReleaseSysCache(tuple);

	/*
	 * The recursion case is handled by ATSimpleRecursion.  However, if we are
	 * told not to recurse, there had better not be any child tables; else the
	 * alter would put them out of step.
	 */
	if (recurse)
		ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
	else if (!recursing &&
			 find_inheritance_children(RelationGetRelid(rel), NoLock) != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("type of inherited column \"%s\" must be changed in child tables too",
						colName)));

	if (tab->relkind == RELKIND_COMPOSITE_TYPE)
		ATTypedTableRecursion(wqueue, rel, cmd, lockmode);
}

/*
 * When the data type of a column is changed, a rewrite might not be required
 * if the new type is sufficiently identical to the old one, and the USING
 * clause isn't trying to insert some other value.  It's safe to skip the
 * rewrite if the old type is binary coercible to the new type, or if the
 * new type is an unconstrained domain over the old type.  In the case of a
 * constrained domain, we could get by with scanning the table and checking
 * the constraint rather than actually rewriting it, but we don't currently
 * try to do that.
 */
static bool
ATColumnChangeRequiresRewrite(Node *expr, AttrNumber varattno)
{
	Assert(expr != NULL);

	for (;;)
	{
		/* only one varno, so no need to check that */
		if (IsA(expr, Var) &&((Var *) expr)->varattno == varattno)
			return false;
		else if (IsA(expr, RelabelType))
			expr = (Node *) ((RelabelType *) expr)->arg;
		else if (IsA(expr, CoerceToDomain))
		{
			CoerceToDomain *d = (CoerceToDomain *) expr;

			if (GetDomainConstraints(d->resulttype) != NIL)
				return true;
			expr = (Node *) d->arg;
		}
		else
			return true;
	}
}

static void
ATExecAlterColumnType(AlteredTableInfo *tab, Relation rel,
					  AlterTableCmd *cmd, LOCKMODE lockmode)
{
	char	   *colName = cmd->name;
	ColumnDef  *def = (ColumnDef *) cmd->def;
	TypeName   *typeName = def->typeName;
	HeapTuple	heapTup;
	Form_pg_attribute attTup;
	AttrNumber	attnum;
	HeapTuple	typeTuple;
	Form_pg_type tform;
	Oid			targettype;
	int32		targettypmod;
	Oid			targetcollid;
	Node	   *defaultexpr;
	Relation	attrelation;
	Relation	depRel;
	ScanKeyData key[3];
	SysScanDesc scan;
	HeapTuple	depTup;
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
	typeTuple = typenameType(NULL, typeName, &targettypmod);
	tform = (Form_pg_type) GETSTRUCT(typeTuple);
	targettype = HeapTupleGetOid(typeTuple);
	/* And the collation */
	targetcollid = GetColumnDefCollation(NULL, def, targettype);

	/*
	 * If there is a default expression for the column, get it and ensure we
	 * can coerce it to the new datatype.  (We must do this before changing
	 * the column type, because build_column_default itself will try to
	 * coerce, and will not issue the error message we want if it fails.)
	 *
	 * We remove any implicit coercion steps at the top level of the old
	 * default expression; this has been agreed to satisfy the principle of
	 * least surprise.  (The conversion to the new column type should act like
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
					 errmsg("default for column \"%s\" cannot be cast automatically to type %s",
							colName, format_type_be(targettype))));
	}
	else
		defaultexpr = NULL;

	if (Gp_role == GP_ROLE_DISPATCH &&
		GpPolicyIsPartitioned(rel->rd_cdbpolicy) &&
		tab->dist_opfamily_changed)
	{
		relContainsTuples = cdbRelMaxSegSize(rel) > 0;
	}

	/*
	 * Find everything that depends on the column (constraints, indexes, etc),
	 * and record enough information to let us recreate the objects.
	 *
	 * The actual recreation does not happen here, but only after we have
	 * performed all the individual ALTER TYPE operations.  We have to save
	 * the info before executing ALTER TYPE, though, else the deparser will
	 * get confused.
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
							  NULL, 3, key);

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
									   GpPolicyIsPartitioned(rel->rd_cdbpolicy) &&
									   tab->dist_opfamily_changed);

								for (int ia = 0; ia < rel->rd_cdbpolicy->nattrs; ia++)
								{
									if (attnum == rel->rd_cdbpolicy->attrs[ia])
									{
										ereport(ERROR,
												(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
												 errmsg("changing the type of a column that is part of the distribution policy and used in a unique index is not allowed")));
									}
								}
							}
						}
						RememberIndexForRebuilding(foundObject.objectId, tab);
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
							   tab->dist_opfamily_changed &&
							   rel->rd_cdbpolicy != NULL &&
							   rel->rd_cdbpolicy->ptype != POLICYTYPE_ENTRY);

						for (int ia = 0; ia < rel->rd_cdbpolicy->nattrs; ia++)
						{
							if (attnum == rel->rd_cdbpolicy->attrs[ia])
							{
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("changing the type of a column that is used in a UNIQUE or PRIMARY KEY constraint is not allowed")));
							}
						}
					}
				}
				RememberConstraintForRebuilding(foundObject.objectId, tab,
												foundDep->deptype);
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

			case OCLASS_TRIGGER:

				/*
				 * A trigger can depend on a column because the column is
				 * specified as an update target, or because the column is
				 * used in the trigger's WHEN condition.  The first case would
				 * not require any extra work, but the second case would
				 * require updating the WHEN expression, which will take a
				 * significant amount of new code.  Since we can't easily tell
				 * which case applies, we punt for both.  FIXME someday.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter type of a column used in a trigger definition"),
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
			case OCLASS_COLLATION:
			case OCLASS_CONVERSION:
			case OCLASS_LANGUAGE:
			case OCLASS_LARGEOBJECT:
			case OCLASS_OPERATOR:
			case OCLASS_OPCLASS:
			case OCLASS_OPFAMILY:
			case OCLASS_AMOP:
			case OCLASS_AMPROC:
			case OCLASS_SCHEMA:
			case OCLASS_TSPARSER:
			case OCLASS_TSDICT:
			case OCLASS_TSTEMPLATE:
			case OCLASS_TSCONFIG:
			case OCLASS_ROLE:
			case OCLASS_DATABASE:
			case OCLASS_TBLSPACE:
			case OCLASS_FDW:
			case OCLASS_FOREIGN_SERVER:
			case OCLASS_USER_MAPPING:
			case OCLASS_DEFACL:
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
	 * want to remove, and possibly a collation dependency.
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
							  NULL, 3, key);

	while (HeapTupleIsValid(depTup = systable_getnext(scan)))
	{
		Form_pg_depend foundDep = (Form_pg_depend) GETSTRUCT(depTup);

		if (foundDep->deptype != DEPENDENCY_NORMAL)
			elog(ERROR, "found unexpected dependency type '%c'",
				 foundDep->deptype);
		if (!(foundDep->refclassid == TypeRelationId &&
			  foundDep->refobjid == attTup->atttypid) &&
			!(foundDep->refclassid == CollationRelationId &&
			  foundDep->refobjid == attTup->attcollation))
			elog(ERROR, "found unexpected dependency for column");

		simple_heap_delete(depRel, &depTup->t_self);
	}

	systable_endscan(scan);

	heap_close(depRel, RowExclusiveLock);

	/*
	 * FIXME: we used to allow changing partition key datatype, if the old
	 * and new types were "compatible". The compatibility used to be hard-coded,
	 * so that int2, int4 and int4 were compatible, as were text, varchar, bpchar.
	 * For the check below, on changing the DISTRIBUTED BY columns, I replaced
	 * that hard-coded notion by checking that the old and new hash opclass belongs
	 * to the same operator family. But that's not quite right for the partitioning
	 * key. Firstly, the partitioning is based on the btree operators, not hash
	 * compatibility, so if you used something funny as the hash opclass, even if
	 * the hash opfamily is the same, it doesn't necessarily mean that the btree
	 * operators are compatible. Secondly, even if you change the datatype from
	 * e.g. int4 to int2, their default opclasses belong to the same operator family,
	 * but int2 has a smaller range so the partition boundaries might be out-of-range
	 * with the new datatype (That's actually an existing bug, see issue
	 * https://github.com/greenplum-db/gpdb/issues/6181)
	 *
	 * I think the right thing to do would be to check if the old and new 'partclass'
	 * are in the same opfamily.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		ListCell   *lc;
		List	   *partkeys;

		partkeys = rel_partition_key_attrs(rel->rd_id);
		foreach (lc, partkeys)
		{
			if (attnum == lfirst_int(lc))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot alter type of a column used in "
								"a partitioning key")));
		}
	}

	if (Gp_role == GP_ROLE_DISPATCH &&
		GpPolicyIsPartitioned(rel->rd_cdbpolicy) &&
		tab->dist_opfamily_changed)
	{
		if (relContainsTuples)
		{
			for (int ia = 0; ia < rel->rd_cdbpolicy->nattrs; ia++)
			{
				if (attnum == rel->rd_cdbpolicy->attrs[ia])
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot alter type of a column used in "
									"a distribution policy")));
			}
		}
	}

	/*
	 * Here we go --- change the recorded column type and collation.  (Note
	 * heapTup is a copy of the syscache entry, so okay to scribble on.)
	 */
	attTup->atttypid = targettype;
	attTup->atttypmod = targettypmod;
	attTup->attcollation = targetcollid;
	attTup->attndims = list_length(typeName->arrayBounds);
	attTup->attlen = tform->typlen;
	attTup->attbyval = tform->typbyval;
	attTup->attalign = tform->typalign;
	attTup->attstorage = tform->typstorage;

	ReleaseSysCache(typeTuple);

	simple_heap_update(attrelation, &heapTup->t_self, heapTup);

	/* keep system catalog indexes current */
	CatalogUpdateIndexes(attrelation, heapTup);

	heap_close(attrelation, RowExclusiveLock);

	/* Update gp_distribution_policy */
	if (tab->dist_opfamily_changed || tab->new_opclass)
	{
		GpPolicy   *newpolicy;
		int			i;

		if (tab->dist_opfamily_changed && tab->new_opclass == InvalidOid)
		{
			/*
			 * The column was part of the distribution key, but the new datatype
			 * is not hashable. Make it randomly distributed.
			 *
			 * XXX: Perhaps a NOTICE would be in order? Or an ERROR?
			 */
			newpolicy = createRandomPartitionedPolicy(rel->rd_cdbpolicy->numsegments);
		}
		else
		{
			newpolicy = GpPolicyCopy(rel->rd_cdbpolicy);
			for (i = 0; i < newpolicy->nattrs; i++)
			{
				if (newpolicy->attrs[i] == attnum)
					newpolicy->opclasses[i] = tab->new_opclass;
			}
		}

		GpPolicyReplace(RelationGetRelid(rel), newpolicy);
	}

	/* Install dependencies on new datatype and collation */
	add_column_datatype_dependency(RelationGetRelid(rel), attnum, targettype);
	add_column_collation_dependency(RelationGetRelid(rel), attnum, targetcollid);

	/*
	 * Drop any pg_statistic entry for the column, since it's now wrong type
	 */
	RemoveStatistics(RelationGetRelid(rel), attnum);

	InvokeObjectPostAlterHook(RelationRelationId,
							  RelationGetRelid(rel), attnum);

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
		RemoveAttrDefault(RelationGetRelid(rel), attnum, DROP_RESTRICT, true,
						  true);

		StoreAttrDefault(rel, attnum, defaultexpr, true);
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
 * Subroutine for ATExecAlterColumnType: remember that a constraint needs
 * to be rebuilt (which we might already know).
 */
static void
RememberConstraintForRebuilding(Oid conoid, AlteredTableInfo *tab,
								DependencyType deptype)
{
	/*
	 * This de-duplication check is critical for two independent reasons: we
	 * mustn't try to recreate the same constraint twice, and if a constraint
	 * depends on more than one column whose type is to be altered, we must
	 * capture its definition string before applying any of the column type
	 * changes.  ruleutils.c will get confused if we ask again later.
	 */
	if (!list_member_oid(tab->changedConstraintOids, conoid))
	{
		/* OK, capture the constraint's existing definition string */
		char	   *defstring = pg_get_constraintdef_string(conoid);

		/*
		 * Put NORMAL dependencies at the front of the list and AUTO
		 * dependencies at the back.  This makes sure that foreign-key
		 * constraints depending on this column will be dropped before unique
		 * or primary-key constraints of the column; which we must have
		 * because the FK constraints depend on the indexes belonging to the
		 * unique constraints.
		 */
		if (deptype == DEPENDENCY_NORMAL)
		{
			tab->changedConstraintOids = lcons_oid(conoid,
												   tab->changedConstraintOids);
			tab->changedConstraintDefs = lcons(defstring,
											   tab->changedConstraintDefs);
		}
		else
		{
			tab->changedConstraintOids = lappend_oid(tab->changedConstraintOids,
													 conoid);
			tab->changedConstraintDefs = lappend(tab->changedConstraintDefs,
												 defstring);
		}
	}
}

/*
 * Subroutine for ATExecAlterColumnType: remember that an index needs
 * to be rebuilt (which we might already know).
 */
static void
RememberIndexForRebuilding(Oid indoid, AlteredTableInfo *tab)
{
	/*
	 * This de-duplication check is critical for two independent reasons: we
	 * mustn't try to recreate the same index twice, and if an index depends
	 * on more than one column whose type is to be altered, we must capture
	 * its definition string before applying any of the column type changes.
	 * ruleutils.c will get confused if we ask again later.
	 */
	if (!list_member_oid(tab->changedIndexOids, indoid))
	{
		/*
		 * Before adding it as an index-to-rebuild, we'd better see if it
		 * belongs to a constraint, and if so rebuild the constraint instead.
		 * Typically this check fails, because constraint indexes normally
		 * have only dependencies on their constraint.  But it's possible for
		 * such an index to also have direct dependencies on table columns,
		 * for example with a partial exclusion constraint.
		 */
		Oid			conoid = get_index_constraint(indoid);

		if (OidIsValid(conoid))
		{
			/* index dependencies on columns should generally be AUTO */
			RememberConstraintForRebuilding(conoid, tab, DEPENDENCY_AUTO);
		}
		else
		{
			/* OK, capture the index's existing definition string */
			char	   *defstring = pg_get_indexdef_string(indoid);

			tab->changedIndexOids = lappend_oid(tab->changedIndexOids,
												indoid);
			tab->changedIndexDefs = lappend(tab->changedIndexDefs,
											defstring);
		}
	}
}

static void
ATExecAlterColumnGenericOptions(Relation rel,
								const char *colName,
								List *options,
								LOCKMODE lockmode)
{
	Relation	ftrel;
	Relation	attrel;
	ForeignServer *server;
	ForeignDataWrapper *fdw;
	HeapTuple	tuple;
	HeapTuple	newtuple;
	bool		isnull;
	Datum		repl_val[Natts_pg_attribute];
	bool		repl_null[Natts_pg_attribute];
	bool		repl_repl[Natts_pg_attribute];
	Datum		datum;
	Form_pg_foreign_table fttableform;
	Form_pg_attribute atttableform;

	if (options == NIL)
		return;

	/* First, determine FDW validator associated to the foreign table. */
	ftrel = heap_open(ForeignTableRelationId, AccessShareLock);
	tuple = SearchSysCache1(FOREIGNTABLEREL, rel->rd_id);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("foreign table \"%s\" does not exist",
						RelationGetRelationName(rel))));
	fttableform = (Form_pg_foreign_table) GETSTRUCT(tuple);
	server = GetForeignServer(fttableform->ftserver);
	fdw = GetForeignDataWrapper(server->fdwid);

	heap_close(ftrel, AccessShareLock);
	ReleaseSysCache(tuple);

	attrel = heap_open(AttributeRelationId, RowExclusiveLock);
	tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				 errmsg("column \"%s\" of relation \"%s\" does not exist",
						colName, RelationGetRelationName(rel))));

	/* Prevent them from altering a system attribute */
	atttableform = (Form_pg_attribute) GETSTRUCT(tuple);
	if (atttableform->attnum <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot alter system column \"%s\"", colName)));


	/* Initialize buffers for new tuple values */
	memset(repl_val, 0, sizeof(repl_val));
	memset(repl_null, false, sizeof(repl_null));
	memset(repl_repl, false, sizeof(repl_repl));

	/* Extract the current options */
	datum = SysCacheGetAttr(ATTNAME,
							tuple,
							Anum_pg_attribute_attfdwoptions,
							&isnull);
	if (isnull)
		datum = PointerGetDatum(NULL);

	/* Transform the options */
	datum = transformGenericOptions(AttributeRelationId,
									datum,
									options,
									fdw->fdwvalidator);

	if (PointerIsValid(DatumGetPointer(datum)))
		repl_val[Anum_pg_attribute_attfdwoptions - 1] = datum;
	else
		repl_null[Anum_pg_attribute_attfdwoptions - 1] = true;

	repl_repl[Anum_pg_attribute_attfdwoptions - 1] = true;

	/* Everything looks good - update the tuple */

	newtuple = heap_modify_tuple(tuple, RelationGetDescr(attrel),
								 repl_val, repl_null, repl_repl);

	simple_heap_update(attrel, &newtuple->t_self, newtuple);
	CatalogUpdateIndexes(attrel, newtuple);

	InvokeObjectPostAlterHook(RelationRelationId,
							  RelationGetRelid(rel),
							  atttableform->attnum);

	ReleaseSysCache(tuple);

	heap_close(attrel, RowExclusiveLock);

	heap_freetuple(newtuple);
}

/*
 * Cleanup after we've finished all the ALTER TYPE operations for a
 * particular relation.  We have to drop and recreate all the indexes
 * and constraints that depend on the altered columns.
 */
static void
ATPostAlterTypeCleanup(List **wqueue, AlteredTableInfo *tab, LOCKMODE lockmode)
{
	ObjectAddress obj;
	ListCell   *def_item;
	ListCell   *oid_item;

	/*
	 * Re-parse the index and constraint definitions, and attach them to the
	 * appropriate work queue entries.  We do this before dropping because in
	 * the case of a FOREIGN KEY constraint, we might not yet have exclusive
	 * lock on the table the constraint is attached to, and we need to get
	 * that before reparsing/dropping.
	 *
	 * We can't rely on the output of deparsing to tell us which relation to
	 * operate on, because concurrent activity might have made the name
	 * resolve differently.  Instead, we've got to use the OID of the
	 * constraint or index we're processing to figure out which relation to
	 * operate on.
	 */
	forboth(oid_item, tab->changedConstraintOids,
			def_item, tab->changedConstraintDefs)
	{
		Oid			oldId = lfirst_oid(oid_item);
		HeapTuple	tup;
		Form_pg_constraint con;
		Oid			relid;
		Oid			confrelid;
		char		contype;
		bool		conislocal;

		tup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(oldId));
		if (!HeapTupleIsValid(tup))		/* should not happen */
			elog(ERROR, "cache lookup failed for constraint %u", oldId);
		con = (Form_pg_constraint) GETSTRUCT(tup);
		relid = con->conrelid;
		confrelid = con->confrelid;
		contype = con->contype;
		conislocal = con->conislocal;
		ReleaseSysCache(tup);

		/*
		 * If the constraint is inherited (only), we don't want to inject a
		 * new definition here; it'll get recreated when ATAddCheckConstraint
		 * recurses from adding the parent table's constraint.  But we had to
		 * carry the info this far so that we can drop the constraint below.
		 */
		if (!conislocal)
			continue;

		/*
		 * When rebuilding an FK constraint that references the table we're
		 * modifying, we might not yet have any lock on the FK's table, so get
		 * one now.  We'll need AccessExclusiveLock for the DROP CONSTRAINT
		 * step, so there's no value in asking for anything weaker.
		 */
		if (relid != tab->relid && contype == CONSTRAINT_FOREIGN)
			LockRelationOid(relid, AccessExclusiveLock);

		ATPostAlterTypeParse(oldId, relid, confrelid,
							 (char *) lfirst(def_item),
							 wqueue, lockmode, tab->rewrite);
	}
	forboth(oid_item, tab->changedIndexOids,
			def_item, tab->changedIndexDefs)
	{
		Oid			oldId = lfirst_oid(oid_item);
		Oid			relid;

		ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("cannot alter indexed column"),
			errhint("DROP the index first, and recreate it after the ALTER")));

		relid = IndexGetRelation(oldId, false);
		ATPostAlterTypeParse(oldId, relid, InvalidOid,
							 (char *) lfirst(def_item),
							 wqueue, lockmode, tab->rewrite);
	}

	/*
	 * Now we can drop the existing constraints and indexes --- constraints
	 * first, since some of them might depend on the indexes.  In fact, we
	 * have to delete FOREIGN KEY constraints before UNIQUE constraints, but
	 * we already ordered the constraint list to ensure that would happen. It
	 * should be okay to use DROP_RESTRICT here, since nothing else should be
	 * depending on these objects.
	 */
	foreach(oid_item, tab->changedConstraintOids)
	{
		obj.classId = ConstraintRelationId;
		obj.objectId = lfirst_oid(oid_item);
		obj.objectSubId = 0;
		performDeletion(&obj, DROP_RESTRICT, PERFORM_DELETION_INTERNAL);
	}

	foreach(oid_item, tab->changedIndexOids)
	{
		obj.classId = RelationRelationId;
		obj.objectId = lfirst_oid(oid_item);
		obj.objectSubId = 0;
		performDeletion(&obj, DROP_RESTRICT, PERFORM_DELETION_INTERNAL);
	}

	/*
	 * The objects will get recreated during subsequent passes over the work
	 * queue.
	 */
}

static void
ATPostAlterTypeParse(Oid oldId, Oid oldRelId, Oid refRelId, char *cmd,
					 List **wqueue, LOCKMODE lockmode, bool rewrite)
{
	List	   *raw_parsetree_list;
	List	   *querytree_list;
	ListCell   *list_item;
	Relation	rel;

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
			querytree_list = lappend(querytree_list,
									 transformIndexStmt(oldRelId,
														(IndexStmt *) stmt,
														cmd));
		else if (IsA(stmt, AlterTableStmt))
			querytree_list = list_concat(querytree_list,
										 transformAlterTableStmt(oldRelId,
													 (AlterTableStmt *) stmt,
																 cmd));
		else
			querytree_list = lappend(querytree_list, stmt);
	}

	/* Caller should already have acquired whatever lock we need. */
	rel = relation_open(oldRelId, NoLock);

	/*
	 * Attach each generated command to the proper place in the work queue.
	 * Note this could result in creation of entirely new work-queue entries.
	 *
	 * Also note that we have to tweak the command subtypes, because it turns
	 * out that re-creation of indexes and constraints has to act a bit
	 * differently from initial creation.
	 */
	foreach(list_item, querytree_list)
	{
		Node	   *stm = (Node *) lfirst(list_item);
		AlteredTableInfo *tab;

		switch (nodeTag(stm))
		{
			case T_IndexStmt:
				{
					IndexStmt  *stmt = (IndexStmt *) stm;
					AlterTableCmd *newcmd;

					if (!rewrite)
						TryReuseIndex(oldId, stmt);

					tab = ATGetQueueEntry(wqueue, rel);
					newcmd = makeNode(AlterTableCmd);
					newcmd->subtype = AT_ReAddIndex;
					newcmd->def = (Node *) stmt;
					tab->subcmds[AT_PASS_OLD_INDEX] =
						lappend(tab->subcmds[AT_PASS_OLD_INDEX], newcmd);
					break;
				}
			case T_AlterTableStmt:
				{
					AlterTableStmt *stmt = (AlterTableStmt *) stm;
					ListCell   *lcmd;

					tab = ATGetQueueEntry(wqueue, rel);
					foreach(lcmd, stmt->cmds)
					{
						AlterTableCmd *cmd = (AlterTableCmd *) lfirst(lcmd);
						Constraint *con;

						switch (cmd->subtype)
						{
							case AT_AddIndex:
								Assert(IsA(cmd->def, IndexStmt));
								if (!rewrite)
									TryReuseIndex(get_constraint_index(oldId),
												  (IndexStmt *) cmd->def);
								cmd->subtype = AT_ReAddIndex;
								tab->subcmds[AT_PASS_OLD_INDEX] =
									lappend(tab->subcmds[AT_PASS_OLD_INDEX], cmd);
								break;
							case AT_AddConstraint:
								Assert(IsA(cmd->def, Constraint));
								con = (Constraint *) cmd->def;
								con->old_pktable_oid = refRelId;
								/* rewriting neither side of a FK */
								if (con->contype == CONSTR_FOREIGN &&
									!rewrite && !tab->rewrite)
									TryReuseForeignKey(oldId, con);
								cmd->subtype = AT_ReAddConstraint;
								tab->subcmds[AT_PASS_OLD_CONSTR] =
									lappend(tab->subcmds[AT_PASS_OLD_CONSTR], cmd);
								break;
							default:
								elog(ERROR, "unexpected statement type: %d",
									 (int) cmd->subtype);
						}
					}
					break;
				}
			default:
				elog(ERROR, "unexpected statement type: %d",
					 (int) nodeTag(stm));
		}
	}

	relation_close(rel, NoLock);
}

/*
 * Subroutine for ATPostAlterTypeParse().  Calls out to CheckIndexCompatible()
 * for the real analysis, then mutates the IndexStmt based on that verdict.
 */
static void
TryReuseIndex(Oid oldId, IndexStmt *stmt)
{
	if (CheckIndexCompatible(oldId,
							 stmt->accessMethod,
							 stmt->indexParams,
							 stmt->excludeOpNames))
	{
		Relation	irel = index_open(oldId, NoLock);

		stmt->oldNode = irel->rd_node.relNode;
		index_close(irel, NoLock);
	}
}

/*
 * Subroutine for ATPostAlterTypeParse().
 *
 * Stash the old P-F equality operator into the Constraint node, for possible
 * use by ATAddForeignKeyConstraint() in determining whether revalidation of
 * this constraint can be skipped.
 */
static void
TryReuseForeignKey(Oid oldId, Constraint *con)
{
	HeapTuple	tup;
	Datum		adatum;
	bool		isNull;
	ArrayType  *arr;
	Oid		   *rawarr;
	int			numkeys;
	int			i;

	Assert(con->contype == CONSTR_FOREIGN);
	Assert(con->old_conpfeqop == NIL);	/* already prepared this node */

	tup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(oldId));
	if (!HeapTupleIsValid(tup)) /* should not happen */
		elog(ERROR, "cache lookup failed for constraint %u", oldId);

	adatum = SysCacheGetAttr(CONSTROID, tup,
							 Anum_pg_constraint_conpfeqop, &isNull);
	if (isNull)
		elog(ERROR, "null conpfeqop for constraint %u", oldId);
	arr = DatumGetArrayTypeP(adatum);	/* ensure not toasted */
	numkeys = ARR_DIMS(arr)[0];
	/* test follows the one in ri_FetchConstraintInfo() */
	if (ARR_NDIM(arr) != 1 ||
		ARR_HASNULL(arr) ||
		ARR_ELEMTYPE(arr) != OIDOID)
		elog(ERROR, "conpfeqop is not a 1-D Oid array");
	rawarr = (Oid *) ARR_DATA_PTR(arr);

	/* stash a List of the operator Oids in our Constraint node */
	for (i = 0; i < numkeys; i++)
		con->old_conpfeqop = lappend_oid(con->old_conpfeqop, rawarr[i]);

	ReleaseSysCache(tup);
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
ATExecChangeOwner(Oid relationOid, Oid newOwnerId, bool recursing, LOCKMODE lockmode)
{
	Relation	target_rel;
	Relation	class_rel;
	HeapTuple	tuple;
	Form_pg_class tuple_class;

	/*
	 * Get exclusive lock till end of transaction on the target table. Use
	 * relation_open so that we can work on indexes and sequences.
	 */
	target_rel = relation_open(relationOid, lockmode);

	/* Get its pg_class tuple, too */
	class_rel = heap_open(RelationRelationId, RowExclusiveLock);

	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relationOid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relationOid);
	tuple_class = (Form_pg_class) GETSTRUCT(tuple);

	/* Can we change the ownership of this tuple? */
	switch (tuple_class->relkind)
	{
		case RELKIND_RELATION:
		case RELKIND_VIEW:
		case RELKIND_MATVIEW:
		case RELKIND_FOREIGN_TABLE:
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
			errmsg("\"%s\" is not a table, view, sequence, or foreign table",
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
		 * We must similarly update any per-column ACLs to reflect the new
		 * owner; for neatness reasons that's split out as a subroutine.
		 */
		change_owner_fix_column_acls(relationOid,
									 tuple_class->relowner,
									 newOwnerId);

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
		 * Also change the ownership of the table's row type, if it has one
		 */
		if (tuple_class->relkind != RELKIND_INDEX)
			AlterTypeOwnerInternal(tuple_class->reltype, newOwnerId);

		/*
		 * If we are operating on a table or materialized view, also change
		 * the ownership of any indexes and sequences that belong to the
		 * relation, as well as its toast table (if it has one).
		 */
		if (tuple_class->relkind == RELKIND_RELATION ||
			tuple_class->relkind == RELKIND_MATVIEW ||
			tuple_class->relkind == RELKIND_TOASTVALUE ||
			IsAppendonlyMetadataRelkind(tuple_class->relkind))
		{
			List	   *index_oid_list;
			ListCell   *i;

			/* Find all the indexes belonging to this relation */
			index_oid_list = RelationGetIndexList(target_rel);

			/* For each index, recursively change its ownership */
			foreach(i, index_oid_list)
				ATExecChangeOwner(lfirst_oid(i), newOwnerId, true, lockmode);

			list_free(index_oid_list);
		}

		/* If it has a toast table, recurse to change its ownership */
		if (tuple_class->reltoastrelid != InvalidOid)
			ATExecChangeOwner(tuple_class->reltoastrelid, newOwnerId,
							  true, lockmode);

		if (RelationIsAppendOptimized(target_rel))
		{
			Oid segrelid, blkdirrelid;
			Oid visimap_relid;
			GetAppendOnlyEntryAuxOids(relationOid, NULL,
									  &segrelid,
									  &blkdirrelid, NULL,
									  &visimap_relid, NULL);

			/* If it has an AO segment table, recurse to change its
			 * ownership */
			if (segrelid != InvalidOid)
				ATExecChangeOwner(segrelid, newOwnerId, true, lockmode);

			/* If it has an AO block directory table, recurse to change its
			 * ownership */
			if (blkdirrelid != InvalidOid)
				ATExecChangeOwner(blkdirrelid, newOwnerId, true, lockmode);

			/* If it has an AO visimap table, recurse to change its
			 * ownership */
			if (visimap_relid != InvalidOid)
			{
				ATExecChangeOwner(visimap_relid, newOwnerId, true, lockmode);
			}
		}

		/* If it has dependent sequences, recurse to change them too */
		change_owner_recurse_to_sequences(relationOid, newOwnerId, lockmode);
	}

	InvokeObjectPostAlterHook(RelationRelationId, relationOid, 0);

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
 * change_owner_fix_column_acls
 *
 * Helper function for ATExecChangeOwner.  Scan the columns of the table
 * and fix any non-null column ACLs to reflect the new owner.
 */
static void
change_owner_fix_column_acls(Oid relationOid, Oid oldOwnerId, Oid newOwnerId)
{
	Relation	attRelation;
	SysScanDesc scan;
	ScanKeyData key[1];
	HeapTuple	attributeTuple;

	attRelation = heap_open(AttributeRelationId, RowExclusiveLock);
	ScanKeyInit(&key[0],
				Anum_pg_attribute_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relationOid));
	scan = systable_beginscan(attRelation, AttributeRelidNumIndexId,
							  true, NULL, 1, key);
	while (HeapTupleIsValid(attributeTuple = systable_getnext(scan)))
	{
		Form_pg_attribute att = (Form_pg_attribute) GETSTRUCT(attributeTuple);
		Datum		repl_val[Natts_pg_attribute];
		bool		repl_null[Natts_pg_attribute];
		bool		repl_repl[Natts_pg_attribute];
		Acl		   *newAcl;
		Datum		aclDatum;
		bool		isNull;
		HeapTuple	newtuple;

		/* Ignore dropped columns */
		if (att->attisdropped)
			continue;

		aclDatum = heap_getattr(attributeTuple,
								Anum_pg_attribute_attacl,
								RelationGetDescr(attRelation),
								&isNull);
		/* Null ACLs do not require changes */
		if (isNull)
			continue;

		memset(repl_null, false, sizeof(repl_null));
		memset(repl_repl, false, sizeof(repl_repl));

		newAcl = aclnewowner(DatumGetAclP(aclDatum),
							 oldOwnerId, newOwnerId);
		repl_repl[Anum_pg_attribute_attacl - 1] = true;
		repl_val[Anum_pg_attribute_attacl - 1] = PointerGetDatum(newAcl);

		newtuple = heap_modify_tuple(attributeTuple,
									 RelationGetDescr(attRelation),
									 repl_val, repl_null, repl_repl);

		simple_heap_update(attRelation, &newtuple->t_self, newtuple);
		CatalogUpdateIndexes(attRelation, newtuple);

		heap_freetuple(newtuple);
	}
	systable_endscan(scan);
	heap_close(attRelation, RowExclusiveLock);
}

/*
 * change_owner_recurse_to_sequences
 *
 * Helper function for ATExecChangeOwner.  Examines pg_depend searching
 * for sequences that are dependent on serial columns, and changes their
 * ownership.
 */
static void
change_owner_recurse_to_sequences(Oid relationOid, Oid newOwnerId, LOCKMODE lockmode)
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
							  NULL, 2, key);

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

		/* We don't need to close the sequence while we alter it. */
		ATExecChangeOwner(depForm->objid, newOwnerId, true, lockmode);

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
ATExecClusterOn(Relation rel, const char *indexName, LOCKMODE lockmode)
{
	Oid			indexOid;

	if (RelationIsAppendOptimized(rel))
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
	check_index_is_clusterable(rel, indexOid, false, lockmode);

	/* And do the work */
	mark_index_clustered(rel, indexOid, false);

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
ATExecDropCluster(Relation rel, LOCKMODE lockmode)
{
	mark_index_clustered(rel, InvalidOid, false);

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

	/* Check that the tablespace exists */
	tablespaceId = get_tablespace_oid(tablespacename, false);

	/* Check permissions except when moving to database's default */
	if (OidIsValid(tablespaceId) && tablespaceId != MyDatabaseTableSpace)
	{
		AclResult	aclresult;

		aclresult = pg_tablespace_aclcheck(tablespaceId, GetUserId(), ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_TABLESPACE, tablespacename);
	}

	return tablespaceId;
}

static void
ATPrepSetTableSpace(AlteredTableInfo *tab, Relation rel, char *tablespacename, LOCKMODE lockmode)
{
	Oid			tablespaceId;

	tablespaceId = get_settable_tablespace_oid(tablespacename);

	/* Check permissions except when moving to database's default */
	if (OidIsValid(tablespaceId) && tablespaceId != MyDatabaseTableSpace)
	{
		AclResult	aclresult;

		aclresult = pg_tablespace_aclcheck(tablespaceId, GetUserId(), ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_TABLESPACE, tablespacename);
	}

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
				(errmsg("will SET TABLESPACE on \"%s\"",
						RelationGetRelationName(partrel))));
		
		relation_close(partrel, NoLock);
	}
}

/*
 * Set, reset, or replace reloptions.
 */
static void
ATExecSetRelOptions(Relation rel, List *defList, AlterTableType operation,
					LOCKMODE lockmode)
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
	static char *validnsps[] = HEAP_RELOPT_NAMESPACES;

	if (defList == NIL && operation != AT_ReplaceRelOptions)
		return;					/* nothing to do */

	pgclass = heap_open(RelationRelationId, RowExclusiveLock);

	/* Fetch heap tuple */
	relid = RelationGetRelid(rel);
	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relid);

	if (operation == AT_ReplaceRelOptions)
	{
		/*
		 * If we're supposed to replace the reloptions list, we just pretend
		 * there were none before.
		 */
		datum = (Datum) 0;
		isnull = true;
	}
	else
	{
		/* Get the old reloptions */
		datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions,
								&isnull);
	}

	/*
	 * Disallow all AO options.  ALTER TABLE SET ... command does not rewrite a
	 * table.  Setting AO options (e.g. setting appendonly=true when it's
	 * currently unset) involves rewrite of the table and is handled using the
	 * SET WITH variant of ALTER TABLE.
	 *
	 * GPDP_91_MERGE_FIXME: Is it possible to use the new reloptions format to
	 * avoid replicating each AO reloption here?  How about introducing new
	 * RELOPT_KIND_AO and RELOPT_KIND_AOCO to relopt_kind?
	 *
	 * Future work: could convert from SET to SET WITH codepath which
	 * can support additional reloption types
	 */
	if (defList != NIL)
	{
		ListCell   *cell;

		foreach(cell, defList)
		{
			DefElem    *def = lfirst(cell);
			int			kw_len = strlen(def->defname);

			if (pg_strncasecmp(SOPT_APPENDONLY, def->defname, kw_len) == 0 ||
				pg_strncasecmp(SOPT_BLOCKSIZE, def->defname, kw_len) == 0 ||
				pg_strncasecmp(SOPT_COMPTYPE, def->defname, kw_len) == 0 ||
				pg_strncasecmp(SOPT_COMPLEVEL, def->defname, kw_len) == 0 ||
				pg_strncasecmp(SOPT_CHECKSUM, def->defname, kw_len) == 0 ||
				pg_strncasecmp(SOPT_ORIENTATION, def->defname, kw_len) == 0)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot SET reloption \"%s\"",
								def->defname)));
			/*
			 * Autovacuum on user tables is not enabled in Greenplum.  Move on
			 * with a warning.  The decision to not error out is in favor of
			 * DDL compatibility with external BI tools.
			 */
			if ((Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY) &&
				pg_strncasecmp(def->defname, "autovacuum",
							   strlen("autovaccum")) == 0)
				ereport(WARNING,
						(errcode(ERRCODE_GP_FEATURE_NOT_YET),
						 errmsg("autovacuum is not supported in Greenplum")));
		}
	}

	/* Generate new proposed reloptions (text array) */
	newOptions = transformRelOptions(isnull ? (Datum) 0 : datum,
									 defList, NULL, validnsps, false,
									 operation == AT_ResetRelOptions);

	/* Validate */
	switch (rel->rd_rel->relkind)
	{
		case RELKIND_RELATION:
		case RELKIND_TOASTVALUE:
		case RELKIND_MATVIEW:
		case RELKIND_AOSEGMENTS:
		case RELKIND_AOBLOCKDIR:
		case RELKIND_AOVISIMAP:
			if (RelationIsAppendOptimized(rel))
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("altering reloptions for append only tables"
								" is not permitted")));

			(void) heap_reloptions(rel->rd_rel->relkind, newOptions, true);
			break;
		case RELKIND_VIEW:
			(void) view_reloptions(newOptions, true);
			break;
		case RELKIND_INDEX:
			(void) index_reloptions(rel->rd_am->amoptions, newOptions, true);
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a table, view, materialized view, index, or TOAST table",
							RelationGetRelationName(rel))));
			break;
	}

	/* Special-case validation of view options */
	if (rel->rd_rel->relkind == RELKIND_VIEW)
	{
		Query	   *view_query = get_view_query(rel);
		List	   *view_options = untransformRelOptions(newOptions);
		ListCell   *cell;
		bool		check_option = false;

		foreach(cell, view_options)
		{
			DefElem    *defel = (DefElem *) lfirst(cell);

			if (pg_strcasecmp(defel->defname, "check_option") == 0)
				check_option = true;
		}

		/*
		 * If the check option is specified, look to see if the view is
		 * actually auto-updatable or not.
		 */
		if (check_option)
		{
			const char *view_updatable_error =
			view_query_is_auto_updatable(view_query, true);

			if (view_updatable_error)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("WITH CHECK OPTION is supported only on automatically updatable views"),
						 errhint("%s", _(view_updatable_error))));
		}
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

	InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), 0);

	heap_freetuple(newtuple);

	ReleaseSysCache(tuple);

	/* repeat the whole exercise for the toast table, if there's one */
	if (OidIsValid(rel->rd_rel->reltoastrelid))
	{
		Relation	toastrel;
		Oid			toastid = rel->rd_rel->reltoastrelid;

		toastrel = heap_open(toastid, lockmode);

		/* Fetch heap tuple */
		tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(toastid));
		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for relation %u", toastid);

		if (operation == AT_ReplaceRelOptions)
		{
			/*
			 * If we're supposed to replace the reloptions list, we just
			 * pretend there were none before.
			 */
			datum = (Datum) 0;
			isnull = true;
		}
		else
		{
			/* Get the old reloptions */
			datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions,
									&isnull);
		}

		newOptions = transformRelOptions(isnull ? (Datum) 0 : datum,
										 defList, "toast", validnsps, false,
										 operation == AT_ResetRelOptions);

		(void) heap_reloptions(RELKIND_TOASTVALUE, newOptions, true);

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

		InvokeObjectPostAlterHookArg(RelationRelationId,
									 RelationGetRelid(toastrel), 0,
									 InvalidOid, true);

		heap_freetuple(newtuple);

		ReleaseSysCache(tuple);

		heap_close(toastrel, NoLock);
	}

	heap_close(pgclass, RowExclusiveLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER",
						   operation == AT_ResetRelOptions ? "RESET" : "SET"
				);
}

/*
 * Execute ALTER TABLE SET TABLESPACE for cases where there is no tuple
 * rewriting to be done, so we just want to copy the data as fast as possible.
 */
static void
ATExecSetTableSpace(Oid tableOid, Oid newTableSpace, LOCKMODE lockmode)
{
	Relation    rel;
	Oid			oldTableSpace;
	Oid			reltoastrelid;
	Oid			relaosegrelid = InvalidOid;
	Oid			relaoblkdirrelid = InvalidOid;
	Oid			relaoblkdiridxid = InvalidOid;
	Oid         relaovisimaprelid = InvalidOid;
	Oid         relaovisimapidxid = InvalidOid;
	Oid			relbmrelid = InvalidOid;
	Oid			relbmidxid = InvalidOid;
	Oid			newrelfilenode;
	RelFileNode newrnode;
	SMgrRelation dstrel;
	Relation	pg_class;
	HeapTuple	tuple;
	Form_pg_class rd_rel;
	ForkNumber	forkNum;
	List	   *reltoastidxids = NIL;
	ListCell   *lc;

	/*
	 * Need lock here in case we are recursing to toast table or index
	 */
	rel = relation_open(tableOid, lockmode);

	/*
	 * No work if no change in tablespace.
	 */
	oldTableSpace = rel->rd_rel->reltablespace;
	if (newTableSpace == oldTableSpace ||
		(newTableSpace == MyDatabaseTableSpace && oldTableSpace == 0))
	{
		InvokeObjectPostAlterHook(RelationRelationId,
								  RelationGetRelid(rel), 0);

		relation_close(rel, NoLock);
		return;
	}

	/*
	 * We cannot support moving mapped relations into different tablespaces.
	 * (In particular this eliminates all shared catalogs.)
	 */
	if (RelationIsMapped(rel))
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
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot move temporary tables of other sessions")));

	reltoastrelid = rel->rd_rel->reltoastrelid;
	/* Fetch the list of indexes on toast relation if necessary */
	if (OidIsValid(reltoastrelid))
	{
		Relation	toastRel = relation_open(reltoastrelid, lockmode);

		reltoastidxids = RelationGetIndexList(toastRel);
		relation_close(toastRel, lockmode);
	}

	/* Get the ao sub objects */
	if (RelationIsAppendOptimized(rel))
		GetAppendOnlyEntryAuxOids(tableOid, NULL,
								  &relaosegrelid,
								  &relaoblkdirrelid, &relaoblkdiridxid,
								  &relaovisimaprelid, &relaovisimapidxid);

	/* Get the bitmap sub objects */
	if (RelationIsBitmapIndex(rel))
		GetBitmapIndexAuxOids(rel, &relbmrelid, &relbmidxid);

	/* Get a modifiable copy of the relation's pg_class row */
	pg_class = heap_open(RelationRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(tableOid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", tableOid);
	rd_rel = (Form_pg_class) GETSTRUCT(tuple);

	/*
	 * Since we copy the file directly without looking at the shared buffers,
	 * we'd better first flush out any pages of the source relation that are
	 * in shared buffers.  We assume no new changes will be made while we are
	 * holding exclusive lock on the rel.
	 */
	FlushRelationBuffers(rel);

	/*
	 * Relfilenodes are not unique in databases across tablespaces, so we need
	 * to allocate a new one in the new tablespace.
	 */
	newrelfilenode = GetNewRelFileNode(newTableSpace, NULL,
									   rel->rd_rel->relpersistence);

	/* Open old and new relation */
	newrnode = rel->rd_node;
	newrnode.relNode = newrelfilenode;
	newrnode.spcNode = newTableSpace;
	dstrel = smgropen(newrnode, rel->rd_backend);

	RelationOpenSmgr(rel);

	/*
	 * Create and copy all forks of the relation, and schedule unlinking of
	 * old physical files.
	 *
	 * NOTE: any conflict in relfilenode value will be caught in
	 * RelationCreateStorage().
	 */
	RelationCreateStorage(newrnode, rel->rd_rel->relpersistence,
						  rel->rd_rel->relstorage);

	if (RelationIsAppendOptimized(rel))
	{
		copy_append_only_data(rel->rd_node, newrnode, rel->rd_backend, rel->rd_rel->relpersistence);
	}
	else
	{
		/* copy main fork */
		copy_relation_data(rel->rd_smgr, dstrel, MAIN_FORKNUM,
						   rel->rd_rel->relpersistence);
	}

	/*
	 * Append-only tables now include init forks for unlogged tables, so we copy
	 * over all forks. AO tables, so far, do not have visimap or fsm forks.
	 */
	
	/* copy those extra forks that exist */
	for (forkNum = MAIN_FORKNUM + 1; forkNum <= MAX_FORKNUM; forkNum++)
	{
		if (smgrexists(rel->rd_smgr, forkNum))
		{
			smgrcreate(dstrel, forkNum, false);

			/*
			 * WAL log creation if the relation is persistent, or this is the
			 * init fork of an unlogged relation.
			 */
			if (rel->rd_rel->relpersistence == RELPERSISTENCE_PERMANENT ||
				(rel->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED &&
				 forkNum == INIT_FORKNUM))
				log_smgrcreate(&newrnode, forkNum);
			copy_relation_data(rel->rd_smgr, dstrel, forkNum,
							   rel->rd_rel->relpersistence);
		}
	}

	/* drop old relation, and close new one */
	RelationDropStorage(rel);
	smgrclose(dstrel);

	/* update the pg_class row */
	rd_rel->reltablespace = (newTableSpace == MyDatabaseTableSpace) ? InvalidOid : newTableSpace;
	rd_rel->relfilenode = newrelfilenode;
	simple_heap_update(pg_class, &tuple->t_self, tuple);
	CatalogUpdateIndexes(pg_class, tuple);

	InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), 0);

	heap_freetuple(tuple);

	heap_close(pg_class, RowExclusiveLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH) && MetaTrackValidKindNsp(rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(rel),
						   GetUserId(),
						   "ALTER", "SET TABLESPACE");

	relation_close(rel, NoLock);

	/* Make sure the reltablespace change is visible */
	CommandCounterIncrement();

	/* Move associated toast relation and/or indexes, too */
	if (OidIsValid(reltoastrelid))
		ATExecSetTableSpace(reltoastrelid, newTableSpace, lockmode);
	foreach(lc, reltoastidxids)
		ATExecSetTableSpace(lfirst_oid(lc), newTableSpace, lockmode);

	/* Move associated ao subobjects */
	if (OidIsValid(relaosegrelid))
		ATExecSetTableSpace(relaosegrelid, newTableSpace, lockmode);
	if (OidIsValid(relaoblkdirrelid))
		ATExecSetTableSpace(relaoblkdirrelid, newTableSpace, lockmode);
	if (OidIsValid(relaoblkdiridxid))
		ATExecSetTableSpace(relaoblkdiridxid, newTableSpace, lockmode);
	if (OidIsValid(relaovisimaprelid))
		ATExecSetTableSpace(relaovisimaprelid, newTableSpace, lockmode);
	if (OidIsValid(relaovisimapidxid))
		ATExecSetTableSpace(relaovisimapidxid, newTableSpace, lockmode);

	/* 
	 * MPP-7996 - bitmap index subobjects w/Alter Table Set tablespace
	 */
	if (OidIsValid(relbmrelid))
	{
		Assert(!relaosegrelid);
		ATExecSetTableSpace(relbmrelid, newTableSpace, lockmode);
	}
	if (OidIsValid(relbmidxid))
		ATExecSetTableSpace(relbmidxid, newTableSpace, lockmode);

	/* Clean up */
	list_free(reltoastidxids);
}

/*
 * Alter Table ALL ... SET TABLESPACE
 *
 * Allows a user to move all objects of some type in a given tablespace in the
 * current database to another tablespace.  Objects can be chosen based on the
 * owner of the object also, to allow users to move only their objects.
 * The user must have CREATE rights on the new tablespace, as usual.   The main
 * permissions handling is done by the lower-level table move function.
 *
 * All to-be-moved objects are locked first. If NOWAIT is specified and the
 * lock can't be acquired then we ereport(ERROR).
 */
Oid
AlterTableMoveAll(AlterTableMoveAllStmt *stmt)
{
	List	   *relations = NIL;
	ListCell   *l;
	ScanKeyData key[1];
	Relation	rel;
	HeapScanDesc scan;
	HeapTuple	tuple;
	Oid			orig_tablespaceoid;
	Oid			new_tablespaceoid;
	List	   *role_oids = roleNamesToIds(stmt->roles);

	/* Ensure we were not asked to move something we can't */
	if (stmt->objtype != OBJECT_TABLE && stmt->objtype != OBJECT_INDEX &&
		stmt->objtype != OBJECT_MATVIEW)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("only tables, indexes, and materialized views exist in tablespaces")));

	/* Get the orig and new tablespace OIDs */
	orig_tablespaceoid = get_tablespace_oid(stmt->orig_tablespacename, false);
	new_tablespaceoid = get_tablespace_oid(stmt->new_tablespacename, false);

	/* Can't move shared relations in to or out of pg_global */
	/* This is also checked by ATExecSetTableSpace, but nice to stop earlier */
	if (orig_tablespaceoid == GLOBALTABLESPACE_OID ||
		new_tablespaceoid == GLOBALTABLESPACE_OID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("cannot move relations in to or out of pg_global tablespace")));

	/*
	 * Must have CREATE rights on the new tablespace, unless it is the
	 * database default tablespace (which all users implicitly have CREATE
	 * rights on).
	 */
	if (OidIsValid(new_tablespaceoid) && new_tablespaceoid != MyDatabaseTableSpace)
	{
		AclResult	aclresult;

		aclresult = pg_tablespace_aclcheck(new_tablespaceoid, GetUserId(),
										   ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_TABLESPACE,
						   get_tablespace_name(new_tablespaceoid));
	}

	/*
	 * Now that the checks are done, check if we should set either to
	 * InvalidOid because it is our database's default tablespace.
	 */
	if (orig_tablespaceoid == MyDatabaseTableSpace)
		orig_tablespaceoid = InvalidOid;

	if (new_tablespaceoid == MyDatabaseTableSpace)
		new_tablespaceoid = InvalidOid;

	/* no-op */
	if (orig_tablespaceoid == new_tablespaceoid)
		return new_tablespaceoid;

	/*
	 * Walk the list of objects in the tablespace and move them. This will
	 * only find objects in our database, of course.
	 */
	ScanKeyInit(&key[0],
				Anum_pg_class_reltablespace,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(orig_tablespaceoid));

	rel = heap_open(RelationRelationId, AccessShareLock);
	scan = heap_beginscan_catalog(rel, 1, key);
	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		Oid			relOid = HeapTupleGetOid(tuple);
		Form_pg_class relForm;

		relForm = (Form_pg_class) GETSTRUCT(tuple);

		/*
		 * Do not move objects in pg_catalog as part of this, if an admin
		 * really wishes to do so, they can issue the individual ALTER
		 * commands directly.
		 *
		 * Also, explicitly avoid any shared tables, temp tables, or TOAST
		 * (TOAST will be moved with the main table).
		 */
		if (IsSystemNamespace(relForm->relnamespace) || relForm->relisshared ||
			isAnyTempNamespace(relForm->relnamespace) ||
			relForm->relnamespace == PG_TOAST_NAMESPACE)
			continue;

		/* Only move the object type requested */
		if ((stmt->objtype == OBJECT_TABLE &&
			 relForm->relkind != RELKIND_RELATION) ||
			(stmt->objtype == OBJECT_INDEX &&
			 relForm->relkind != RELKIND_INDEX) ||
			(stmt->objtype == OBJECT_MATVIEW &&
			 relForm->relkind != RELKIND_MATVIEW))
			continue;

		/* Check if we are only moving objects owned by certain roles */
		if (role_oids != NIL && !list_member_oid(role_oids, relForm->relowner))
			continue;

		/*
		 * Handle permissions-checking here since we are locking the tables
		 * and also to avoid doing a bunch of work only to fail part-way. Note
		 * that permissions will also be checked by AlterTableInternal().
		 *
		 * Caller must be considered an owner on the table to move it.
		 */
		if (!pg_class_ownercheck(relOid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
						   NameStr(relForm->relname));

		if (stmt->nowait &&
			!ConditionalLockRelationOid(relOid, AccessExclusiveLock))
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_IN_USE),
			   errmsg("aborting because lock on relation \"%s\".\"%s\" is not available",
					  get_namespace_name(relForm->relnamespace),
					  NameStr(relForm->relname))));
		else
			LockRelationOid(relOid, AccessExclusiveLock);

		/* Add to our list of objects to move */
		relations = lappend_oid(relations, relOid);
	}

	heap_endscan(scan);
	heap_close(rel, AccessShareLock);

	if (relations == NIL)
		ereport(NOTICE,
				(errcode(ERRCODE_NO_DATA_FOUND),
				 errmsg("no matching relations in tablespace \"%s\" found",
					orig_tablespaceoid == InvalidOid ? "(database default)" :
						get_tablespace_name(orig_tablespaceoid))));

	/* Everything is locked, loop through and move all of the relations. */
	foreach(l, relations)
	{
		List	   *cmds = NIL;
		AlterTableCmd *cmd = makeNode(AlterTableCmd);

		cmd->subtype = AT_SetTableSpace;
		cmd->name = stmt->new_tablespacename;

		cmds = lappend(cmds, cmd);

		AlterTableInternal(lfirst_oid(l), cmds, false);
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
	}

	return new_tablespaceoid;
}

/*
 * Copy data, block by block
 */
static void
copy_relation_data(SMgrRelation src, SMgrRelation dst,
				   ForkNumber forkNum, char relpersistence)
{
	PGAlignedBlock buf;
	Page		page;
	bool		use_wal;
	bool		copying_initfork;
	BlockNumber nblocks;
	BlockNumber blkno;

	page = (Page) buf.data;

	/*
	 * The init fork for an unlogged relation in many respects has to be
	 * treated the same as normal relation, changes need to be WAL logged and
	 * it needs to be synced to disk.
	 */
	copying_initfork = relpersistence == RELPERSISTENCE_UNLOGGED &&
		forkNum == INIT_FORKNUM;

	/*
	 * We need to log the copied data in WAL iff WAL archiving/streaming is
	 * enabled AND it's a permanent relation.
	 */
	use_wal = XLogIsNeeded() &&
		(relpersistence == RELPERSISTENCE_PERMANENT || copying_initfork);

	nblocks = smgrnblocks(src, forkNum);

	for (blkno = 0; blkno < nblocks; blkno++)
	{
		/* If we got a cancel signal during the copy of the data, quit */
		CHECK_FOR_INTERRUPTS();

		smgrread(src, forkNum, blkno, buf.data);

		if (!PageIsVerified(page, blkno))
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("invalid page in block %u of relation %s",
							blkno,
							relpathbackend(src->smgr_rnode.node,
										   src->smgr_rnode.backend,
										   forkNum))));

		/*
		 * WAL-log the copied page. Unfortunately we don't know what kind of a
		 * page this is, so we have to log the full page including any unused
		 * space.
		 */
		if (use_wal)
			log_newpage(&dst->smgr_rnode.node, forkNum, blkno, page, false);

		PageSetChecksumInplace(page, blkno);

		PageSetChecksumInplace(page, blkno);

		/*
		 * Now write the page.  We say isTemp = true even if it's not a temp
		 * rel, because there's no need for smgr to schedule an fsync for this
		 * write; we'll do it ourselves below.
		 */
		smgrextend(dst, forkNum, blkno, buf.data, true);
	}

	/*
	 * If the rel is WAL-logged, must fsync before commit.  We use heap_sync
	 * to ensure that the toast table gets fsync'd too.  (For a temp or
	 * unlogged rel we don't care since the data will be gone after a crash
	 * anyway.)
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
	if (relpersistence == RELPERSISTENCE_PERMANENT || copying_initfork)
		smgrimmedsync(dst, forkNum);
}

/*
 * ALTER TABLE ENABLE/DISABLE TRIGGER
 *
 * We just pass this off to trigger.c.
 */
static void
ATExecEnableDisableTrigger(Relation rel, char *trigname,
						char fires_when, bool skip_system, LOCKMODE lockmode)
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
						char fires_when, LOCKMODE lockmode)
{
	EnableDisableRule(rel, trigname, fires_when);
}

/*
 * ALTER TABLE INHERIT
 *
 * Add a parent to the child's parents. This verifies that all the columns and
 * check constraints of the parent appear in the child and that they have the
 * same data types and expressions.
 */
static void
ATPrepAddInherit(Relation child_rel)
{
	if (child_rel->rd_rel->reloftype)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot change inheritance of typed table")));
}

static void
ATExecAddInherit(Relation child_rel, Node *node, LOCKMODE lockmode)
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
	 * A self-exclusive lock is needed here.  See the similar case in
	 * MergeAttributes() for a full explanation.
	 */
	parent_rel = heap_openrv(parent, ShareUpdateExclusiveLock);

	/*
	 * Must be owner of both parent and child -- child was checked by
	 * ATSimplePermissions call in ATPrepCmd
	 */
	ATSimplePermissions(parent_rel, ATT_TABLE);

	/* Permanent rels cannot inherit from temporary ones */
	if (parent_rel->rd_rel->relpersistence == RELPERSISTENCE_TEMP &&
		child_rel->rd_rel->relpersistence != RELPERSISTENCE_TEMP)
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
	 * Also copy the ACL from the parent, if we're attaching a new partition
	 * to a partitioned table.
	 */
	if (is_partition)
	{
		/*
		 * make any previous changes to the pg_class and pg_attribute entries
		 * visible to us, first.
		 */
		CommandCounterIncrement();
		CopyRelationAcls(RelationGetRelid(parent_rel),
						 RelationGetRelid(child_rel));
	}

	/*
	 * Keep our lock on the parent relation until commit, unless we're
	 * doing partitioning, in which case the parent is sufficiently locked.
	 * We want to unlock here in case we're doing deep sub partitioning. We do
	 * not want to acquire too many locks since we're overflow the lock buffer.
	 * An exclusive lock on the parent table is sufficient to guard against
	 * concurrency issues.
	 */
	if (is_partition)
		heap_close(parent_rel, ShareUpdateExclusiveLock);
	else
		heap_close(parent_rel, NoLock);

	/* MPP-6929: metadata tracking */
	if ((Gp_role == GP_ROLE_DISPATCH)
		&& MetaTrackValidKindNsp(child_rel->rd_rel))
		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(child_rel),
						   GetUserId(),
						   "ALTER", "INHERIT");
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
							  true, NULL, 1, &key);

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
	 *
	 * We use weakest lock we can on child's children, namely AccessShareLock.
	 */
	children = find_all_inheritors(RelationGetRelid(child_rel),
								   AccessShareLock, NULL);

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

	/* Match up the constraints and bump coninhcount as needed */
	MergeConstraintsIntoExisting(child_rel, parent_rel);

	/*
	 * OK, it looks valid.  Make the catalog entries that show inheritance.
	 */
	StoreCatalogInheritance1(RelationGetRelid(child_rel),
							 RelationGetRelid(parent_rel),
							 inhseqno + 1,
							 catalogRelation, is_partition);

	/* Now we're done with pg_inherits */
	heap_close(catalogRelation, RowExclusiveLock);
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
 * Determine whether two check constraints are functionally equivalent
 *
 * The test we apply is to see whether they reverse-compile to the same
 * source string.  This insulates us from issues like whether attributes
 * have the same physical column numbers in parent and child relations.
 */
static bool
constraints_equivalent(HeapTuple a, HeapTuple b, TupleDesc tupleDesc)
{
	Form_pg_constraint acon = (Form_pg_constraint) GETSTRUCT(a);
	Form_pg_constraint bcon = (Form_pg_constraint) GETSTRUCT(b);

	if (acon->condeferrable != bcon->condeferrable ||
		acon->condeferred != bcon->condeferred ||
		strcmp(decompile_conbin(a, tupleDesc),
			   decompile_conbin(b, tupleDesc)) != 0)
		return false;
	else
		return true;
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
	HeapTuple	tuple;
	ListCell	*attNameCell;

	attrrel = heap_open(AttributeRelationId, RowExclusiveLock);

	tupleDesc = RelationGetDescr(parent_rel);
	parent_natts = tupleDesc->natts;

	/*
	 * If we have an inherited column list, ensure all named columns exist in
	 * parent and that the list excludes system columns.
	 */
	foreach( attNameCell, inhAttrNameList )
	{
		bool	columnDefined = false;
		char   *inhAttrName = strVal((Value *) lfirst(attNameCell));

		for (parent_attno = 1; parent_attno <= parent_natts && !columnDefined; parent_attno++)
		{
			char	*attributeName;
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
			/* Check they are same type, typmod, and collation */
			Form_pg_attribute childatt = (Form_pg_attribute) GETSTRUCT(tuple);

			if (attribute->atttypid != childatt->atttypid ||
				attribute->atttypmod != childatt->atttypmod)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("child table \"%s\" has different type for column \"%s\"",
								RelationGetRelationName(child_rel),
								attributeName)));

			if (attribute->attcollation != childatt->attcollation)
				ereport(ERROR,
						(errcode(ERRCODE_COLLATION_MISMATCH),
						 errmsg("child table \"%s\" has different collation for column \"%s\"",
								RelationGetRelationName(child_rel),
								attributeName)));

			/*
			 * Check child doesn't discard NOT NULL property.  (Other
			 * constraints are checked elsewhere.)
			 */
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
			 * For locally-defined attributes, check to see if the attribute is
			 * named in the "fully-inherited" list.  If so, mark the child
			 * attribute as not locally defined.  (Default/standard behaviour
			 * is to leave the attribute locally defined.)
			 */
			if (childatt->attislocal)
			{
				/* never local when we're doing partitioning */
				if (is_partition)
					childatt->attislocal = false;
				else
				{
					foreach(attNameCell, inhAttrNameList)
					{
						char	*inhAttrName;
						
						inhAttrName = strVal((Value *) lfirst(attNameCell));
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

	/*
	 * If the parent has an OID column, so must the child, and we'd better
	 * update the child's attinhcount and attislocal the same as for normal
	 * columns.  We needn't check data type or not-nullness though.
	 */
	if (tupleDesc->tdhasoid)
	{
		/*
		 * Here we match by column number not name; the match *must* be the
		 * system column, not some random column named "oid".
		 */
		tuple = SearchSysCacheCopy2(ATTNUM,
							   ObjectIdGetDatum(RelationGetRelid(child_rel)),
									Int16GetDatum(ObjectIdAttributeNumber));
		if (HeapTupleIsValid(tuple))
		{
			Form_pg_attribute childatt = (Form_pg_attribute) GETSTRUCT(tuple);

			/* See comments above; these changes should be the same */
			childatt->attinhcount++;
			simple_heap_update(attrrel, &tuple->t_self, tuple);
			CatalogUpdateIndexes(attrrel, tuple);
			heap_freetuple(tuple);
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("child table is missing column \"%s\"",
							"oid")));
		}
	}

	heap_close(attrrel, RowExclusiveLock);
}

/*
 * Check constraints in child table match up with constraints in parent,
 * and increment their coninhcount.
 *
 * Constraints that are marked ONLY in the parent are ignored.
 *
 * Called by ATExecAddInherit
 *
 * Currently all constraints in parent must be present in the child. One day we
 * may consider adding new constraints like CREATE TABLE does.
 *
 * XXX This is O(N^2) which may be an issue with tables with hundreds of
 * constraints. As long as tables have more like 10 constraints it shouldn't be
 * a problem though. Even 100 constraints ought not be the end of the world.
 *
 * XXX See MergeWithExistingConstraint too if you change this code.
 */
static void
MergeConstraintsIntoExisting(Relation child_rel, Relation parent_rel)
{
	Relation	catalog_relation;
	TupleDesc	tuple_desc;
	SysScanDesc parent_scan;
	ScanKeyData parent_key;
	HeapTuple	parent_tuple;

	catalog_relation = heap_open(ConstraintRelationId, RowExclusiveLock);
	tuple_desc = RelationGetDescr(catalog_relation);

	/* Outer loop scans through the parent's constraint definitions */
	ScanKeyInit(&parent_key,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(parent_rel)));
	parent_scan = systable_beginscan(catalog_relation, ConstraintRelidIndexId,
									 true, NULL, 1, &parent_key);

	while (HeapTupleIsValid(parent_tuple = systable_getnext(parent_scan)))
	{
		Form_pg_constraint parent_con = (Form_pg_constraint) GETSTRUCT(parent_tuple);
		SysScanDesc child_scan;
		ScanKeyData child_key;
		HeapTuple	child_tuple;
		bool		found = false;

		if (parent_con->contype != CONSTRAINT_CHECK)
			continue;

		/* if the parent's constraint is marked NO INHERIT, it's not inherited */
		if (parent_con->connoinherit)
			continue;

		/* Search for a child constraint matching this one */
		ScanKeyInit(&child_key,
					Anum_pg_constraint_conrelid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(RelationGetRelid(child_rel)));
		child_scan = systable_beginscan(catalog_relation, ConstraintRelidIndexId,
										true, NULL, 1, &child_key);

		while (HeapTupleIsValid(child_tuple = systable_getnext(child_scan)))
		{
			Form_pg_constraint child_con = (Form_pg_constraint) GETSTRUCT(child_tuple);
			HeapTuple	child_copy;

			if (child_con->contype != CONSTRAINT_CHECK)
				continue;

			if (strcmp(NameStr(parent_con->conname),
					   NameStr(child_con->conname)) != 0)
				continue;

			if (!constraints_equivalent(parent_tuple, child_tuple, tuple_desc))
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("child table \"%s\" has different definition for check constraint \"%s\"",
								RelationGetRelationName(child_rel),
								NameStr(parent_con->conname))));

			/* If the child constraint is "no inherit" then cannot merge */
			if (child_con->connoinherit)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("constraint \"%s\" conflicts with non-inherited constraint on child table \"%s\"",
								NameStr(child_con->conname),
								RelationGetRelationName(child_rel))));

			/*
			 * If the child constraint is "not valid" then cannot merge with a
			 * valid parent constraint
			 */
			if (parent_con->convalidated && !child_con->convalidated)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("constraint \"%s\" conflicts with NOT VALID constraint on child table \"%s\"",
								NameStr(child_con->conname),
								RelationGetRelationName(child_rel))));

			/*
			 * OK, bump the child constraint's inheritance count.  (If we fail
			 * later on, this change will just roll back.)
			 */
			child_copy = heap_copytuple(child_tuple);
			child_con = (Form_pg_constraint) GETSTRUCT(child_copy);
			child_con->coninhcount++;
			simple_heap_update(catalog_relation, &child_copy->t_self, child_copy);
			CatalogUpdateIndexes(catalog_relation, child_copy);
			heap_freetuple(child_copy);

			found = true;
			break;
		}

		systable_endscan(child_scan);

		if (!found)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("child table is missing constraint \"%s\"",
							NameStr(parent_con->conname))));
	}

	systable_endscan(parent_scan);
	heap_close(catalog_relation, RowExclusiveLock);
}

/*
 * ALTER TABLE NO INHERIT
 *
 * Return value is the address of the relation that is no longer parent.
 */
static void
ATExecDropInherit(Relation rel, RangeVar *parent, LOCKMODE lockmode)
{
	Relation	parent_rel;

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

	/* Off to RemoveInheritance() where most of the work happens */
	RemoveInheritance(rel, parent_rel, false);

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
 * RemoveInheritance
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
 *
 * coninhcount and conislocal for inherited constraints are adjusted in
 * exactly the same way.
 *
 * Common to ATExecDropInherit() and ATExecDetachPartition().
 */
static void
RemoveInheritance(Relation child_rel, Relation parent_rel, bool is_partition)
{
	Relation	catalogRelation;
	SysScanDesc scan;
	ScanKeyData key[3];
	HeapTuple	attributeTuple,
				constraintTuple;
	List	   *connames;
	bool		found;


	found = DeleteInheritsTuple(RelationGetRelid(child_rel),
								RelationGetRelid(parent_rel));
	if (!found)
	{
		if (is_partition)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_TABLE),
					 errmsg("relation \"%s\" is not a partition of relation \"%s\"",
							RelationGetRelationName(child_rel),
							RelationGetRelationName(parent_rel))));
		else
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_TABLE),
					 errmsg("relation \"%s\" is not a parent of relation \"%s\"",
							RelationGetRelationName(parent_rel),
							RelationGetRelationName(child_rel))));
	}

	/*
	 * Search through child columns looking for ones matching parent rel
	 */
	catalogRelation = heap_open(AttributeRelationId, RowExclusiveLock);
	ScanKeyInit(&key[0],
				Anum_pg_attribute_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(child_rel)));
	scan = systable_beginscan(catalogRelation, AttributeRelidNumIndexId,
							  true, NULL, 1, key);
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
	 * Likewise, find inherited check constraints and disinherit them. To do
	 * this, we first need a list of the names of the parent's check
	 * constraints.  (We cheat a bit by only checking for name matches,
	 * assuming that the expressions will match.)
	 */
	catalogRelation = heap_open(ConstraintRelationId, RowExclusiveLock);
	ScanKeyInit(&key[0],
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(parent_rel)));
	scan = systable_beginscan(catalogRelation, ConstraintRelidIndexId,
							  true, NULL, 1, key);

	connames = NIL;

	while (HeapTupleIsValid(constraintTuple = systable_getnext(scan)))
	{
		Form_pg_constraint con = (Form_pg_constraint) GETSTRUCT(constraintTuple);

		if (con->contype == CONSTRAINT_CHECK)
			connames = lappend(connames, pstrdup(NameStr(con->conname)));
	}

	systable_endscan(scan);

	/* Now scan the child's constraints */
	ScanKeyInit(&key[0],
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(child_rel)));
	scan = systable_beginscan(catalogRelation, ConstraintRelidIndexId,
							  true, NULL, 1, key);

	while (HeapTupleIsValid(constraintTuple = systable_getnext(scan)))
	{
		Form_pg_constraint con = (Form_pg_constraint) GETSTRUCT(constraintTuple);
		bool		match;
		ListCell   *lc;

		if (con->contype != CONSTRAINT_CHECK)
			continue;

		match = false;
		foreach(lc, connames)
		{
			if (strcmp(NameStr(con->conname), (char *) lfirst(lc)) == 0)
			{
				match = true;
				break;
			}
		}

		if (match)
		{
			/* Decrement inhcount and possibly set islocal to true */
			HeapTuple	copyTuple = heap_copytuple(constraintTuple);
			Form_pg_constraint copy_con = (Form_pg_constraint) GETSTRUCT(copyTuple);

			if (copy_con->coninhcount <= 0)		/* shouldn't happen */
				elog(ERROR, "relation %u has non-inherited constraint \"%s\"",
					 RelationGetRelid(child_rel), NameStr(copy_con->conname));

			copy_con->coninhcount--;
			if (copy_con->coninhcount == 0)
				copy_con->conislocal = true;

			simple_heap_update(catalogRelation, &copyTuple->t_self, copyTuple);
			CatalogUpdateIndexes(catalogRelation, copyTuple);
			heap_freetuple(copyTuple);
		}
	}

	systable_endscan(scan);
	heap_close(catalogRelation, RowExclusiveLock);

	drop_parent_dependency(RelationGetRelid(child_rel),
						   RelationRelationId,
						   RelationGetRelid(parent_rel),
						   is_partition);

	/*
	 * Post alter hook of this inherits. Since object_access_hook doesn't take
	 * multiple object identifiers, we relay oid of parent relation using
	 * auxiliary_id argument.
	 */
	InvokeObjectPostAlterHookArg(InheritsRelationId,
								 RelationGetRelid(child_rel), 0,
								 RelationGetRelid(parent_rel), false);

}

/*
 * Drop the dependency created by StoreCatalogInheritance1 (CREATE TABLE
 * INHERITS/ALTER TABLE INHERIT -- refclassid will be RelationRelationId) or
 * heap_create_with_catalog (CREATE TABLE OF/ALTER TABLE OF -- refclassid will
 * be TypeRelationId).  There's no convenient way to do this, so go trawling
 * through pg_depend.
 */
static void
drop_parent_dependency(Oid relid, Oid refclassid, Oid refobjid, bool is_partition)
{
	Relation	catalogRelation;
	SysScanDesc scan;
	ScanKeyData key[3];
	HeapTuple	depTuple;

	catalogRelation = heap_open(DependRelationId, RowExclusiveLock);

	ScanKeyInit(&key[0],
				Anum_pg_depend_classid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationRelationId));
	ScanKeyInit(&key[1],
				Anum_pg_depend_objid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	ScanKeyInit(&key[2],
				Anum_pg_depend_objsubid,
				BTEqualStrategyNumber, F_INT4EQ,
				Int32GetDatum(0));

	scan = systable_beginscan(catalogRelation, DependDependerIndexId, true,
							  NULL, 3, key);

	while (HeapTupleIsValid(depTuple = systable_getnext(scan)))
	{
		Form_pg_depend dep = (Form_pg_depend) GETSTRUCT(depTuple);

		if (dep->refclassid == refclassid &&
			dep->refobjid == refobjid &&
			dep->refobjsubid == 0 &&
			((dep->deptype == DEPENDENCY_NORMAL && !is_partition) ||
			 (dep->deptype == DEPENDENCY_AUTO && is_partition)))
		{
			simple_heap_delete(catalogRelation, &depTuple->t_self);
		}
	}

	systable_endscan(scan);
	heap_close(catalogRelation, RowExclusiveLock);
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
							  true, NULL, 1, &key);
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
build_ctas_with_dist(Relation rel, DistributedBy *dist_clause,
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
	IntoClause	*into = NULL;

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
			t->val = (Node *) makeConst(INT4OID, -1, InvalidOid, sizeof(int32), (Datum) 0, true, true);
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
									RelationIsAppendOptimized(rel),
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
		List *q_list, *p_list;

		into = makeNode(IntoClause);
		into->rel = tmprel;
		into->options = storage_opts;
		into->tableSpaceName = get_tablespace_name(tblspc);
		into->distributedBy = (Node *)dist_clause;
		s->intoClause = into;

		q_list = pg_analyze_and_rewrite((Node *) s, synthetic_sql, NULL, 0);
		p_list = pg_plan_queries(q_list, 0, NULL);
		q = (Query *) linitial(p_list);
		Assert(IsA(q, CreateTableAsStmt));

		n = ((CreateTableAsStmt *)q)->query;
	}
	*tmprv = tmprel;

	if (pre_built)
		q = parse_analyze((Node *) n, synthetic_sql, NULL, 0);
	else
		q = (Query *) n;

	AcquireRewriteLocks(q, true, false);

	/* Rewrite through rule system */
	rewritten = QueryRewrite(q);

	/* We don't expect more or less than one result query */
	Assert(list_length(rewritten) == 1);

	q = (Query *) linitial(rewritten);
	Assert(q->commandType == CMD_SELECT || q->commandType == CMD_INSERT);

	/* plan the query */
	stmt = planner(q, 0, NULL);
	stmt->intoClause = into;

	/*
	 * Update snapshot command ID to ensure this query sees results of any
	 * previously executed queries.
	 */
	PushCopiedSnapshot(GetActiveSnapshot());
	UpdateActiveSnapshotCommandId();

	/* Create dest receiver for COPY OUT */
	dest = CreateDestReceiver(DestIntoRel);

	/* Create a QueryDesc requesting no output */
	queryDesc = CreateQueryDesc(stmt, debug_query_string,
								GetActiveSnapshot(), InvalidSnapshot,
								dest, NULL, GP_INSTRUMENT_OPTS);
	PopActiveSnapshot();

	return queryDesc;
}

static Datum
new_rel_opts(Relation rel)
{
	Datum newOptions = PointerGetDatum(NULL);

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

	return newOptions;
}

static RangeVar *
make_temp_table_name(Relation rel, BackendId id)
{
	char	   *nspname;
	char		tmpname[NAMEDATALEN];
	RangeVar   *tmprel;

	/* temporary enough */
	snprintf(tmpname, NAMEDATALEN, "pg_temp_%u_%i", RelationGetRelid(rel), id);
	nspname = get_namespace_name(RelationGetNamespace(rel));

	tmprel = makeRangeVar(nspname, pstrdup(tmpname), -1);

	/*
	 * Ensure the temp relation has the same persistence setting with the
	 * original relation.
	 */
	tmprel->relpersistence = rel->rd_rel->relpersistence;

	return tmprel;
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
prebuild_temp_table(Relation rel, RangeVar *tmpname, DistributedBy *distro, List *opts,
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
				int32 ndims = att->attndims;

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
			}

			/* Per column encoding settings */
			if (col_encs)
				cd->encoding = col_encs[attno];

			tname->location = -1;
			cd->typeName = tname;
			cs->tableElts = lappend(cs->tableElts, cd);
		}
		q = parse_analyze((Node *) cs, synthetic_sql, NULL, 0);
		ProcessUtility((Node *)q->utilityStmt,
					   synthetic_sql,
					   PROCESS_UTILITY_SUBCOMMAND,
					   NULL,
					   dest,
					   NULL);
		CommandCounterIncrement();
	}
	return need_rebuild;
}

/* Build a human readable tag for what we're doing */
static char *
make_distro_str(List *lwith, DistributedBy *ldistro)
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
						 errhint("The DISTRIBUTED BY columns must be a subset of the UNIQUE INDEX columns.")));
			}

			bms_free(indbm);
		}

		ReleaseSysCache(indexTuple);
	}

	list_free(indexoidlist);
	bms_free(polbm);
}

/*
 * ALTER TABLE EXPAND TABLE
 *
 * Update a table's "numsegments" value to current cluster size, and move
 * data as needed to the new segments.
 *
 * There are currently only one way we can perform EXPAND TABLE:
 *
 * 1. Create a whole new relation file, with the new 'numsegments', copy all
 *    the data to the new reltion file, and swap it in place of the old one.
 *    This is called the "CTAS method", because it uses a CREATE TABLE AS
 *    command internally to create the new physical relation.
 */
static void
ATExecExpandTable(List **wqueue, Relation rel, AlterTableCmd *cmd)
{
	AlteredTableInfo	*tab;
	AlterTableCmd		*rootCmd;
	MemoryContext		oldContext;
	Oid					relid = RelationGetRelid(rel);
	GpPolicy			*newPolicy;
	GpPolicy			*policy = rel->rd_cdbpolicy;

	if (Gp_role == GP_ROLE_UTILITY)
		ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("EXPAND not supported in utility mode")));

	/* Permissions checks */
	if (!pg_class_ownercheck(relid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   RelationGetRelationName(rel));

	/* Can't ALTER TABLE SET system catalogs */
	if (IsSystemRelation(rel))
		ereport(ERROR,
			(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
			errmsg("permission denied: \"%s\" is a system catalog", RelationGetRelationName(rel))));

	oldContext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));
	newPolicy = GpPolicyCopy(policy);
	MemoryContextSwitchTo(oldContext);

	tab = linitial(*wqueue);
	rootCmd = (AlterTableCmd *)linitial(tab->subcmds[AT_PASS_MISC]);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* only rootCmd is dispatched to QE, we can store */
		if (rootCmd == cmd)
			rootCmd->def = (Node*)makeNode(ExpandStmtSpec);
	}

	if (RelationIsExternal(rel))
	{
		ExtTableEntry* ext = GetExtTableEntry(relid);
		bool           iswritable = ext->iswritable;

		relation_close(rel, NoLock);
		if (!iswritable)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("unsupported ALTER command for external table")));
	}
	else
	{
		ATExecExpandTableCTAS(rootCmd, rel, cmd);
	}

	/* Update numsegments to cluster size */
	newPolicy->numsegments = getgpsegmentCount();
	GpPolicyReplace(relid, newPolicy);
}

static void
ATExecExpandTableCTAS(AlterTableCmd *rootCmd, Relation rel, AlterTableCmd *cmd)
{
	RangeVar			*tmprv;
	Datum				newOptions;
	Oid					tmprelid;
	Oid					relid = RelationGetRelid(rel);
	char				relstorage = rel->rd_rel->relstorage;
	ExpandStmtSpec		*spec = (ExpandStmtSpec *)rootCmd->def;

	/*--
	 * a) Ensure that the proposed policy is sensible
	 * b) Create a temporary table and reorganise data according to our desired
	 *    distribution policy. To do this, we build a Query node which express
	 *    the query:
	 *    CREATE TABLE tmp_tab_nam AS SELECT * FROM cur_table DISTRIBUTED BY (policy)
	 * c) Execute the query across all nodes
	 * d) Update our parse tree to include the details of the newly created
	 *    table
	 * e) Update the ownership of the temporary table
	 * f) Swap the relfilenodes of the existing table and the temporary table
	 * g) Update the policy on the QD to reflect the underlying data
	 * h) Drop the temporary table -- and with it, the old copy of the data
	 *--
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		AutoStatsCmdType	cmdType = AUTOSTATS_CMDTYPE_SENTINEL;
		DistributedBy		*distby;
		QueryDesc			*queryDesc;
		Oid					relationOid = InvalidOid;
		bool 				saveOptimizerGucValue;;

		/* Step (a) */
		/*
		 * Force the use of Postgres query optimizer, since Pivotal Optimizer (GPORCA) will not
		 * redistribute the tuples if the current and required distributions
		 * are both RANDOM even when reorganize is set to "true"
		 */
		saveOptimizerGucValue = optimizer;
		optimizer = false;

		/* Step (b) - build CTAS */
		distby = make_distributedby_for_rel(rel);
		distby->numsegments = getgpsegmentCount();

		newOptions = new_rel_opts(rel);

		queryDesc = build_ctas_with_dist(rel, distby,
						untransformRelOptions(newOptions),
						&tmprv,
						true);

		/*
		 * bypass gpmon info collecting in following ExecutorStart
		 * to be consistent with other alter table commands,
		 * ALTER TABLE SET DISTRIBUTED BY should not be logged in gpperfmon.
		 */
		queryDesc->gpmon_pkt = NULL;

		/*
		 * We need to update our snapshot here to make sure we see all
		 * committed work. We have an exclusive lock on the table so no one
		 * will be able to access the table now.
		 */
		PushActiveSnapshot(GetLatestSnapshot());

		/* Step (c) - run on all nodes */
		queryDesc->ddesc = makeNode(QueryDispatchDesc);
		queryDesc->ddesc->useChangedAOOpts = false;

		queryDesc->plannedstmt->query_mem =
				ResourceManagerGetQueryMemoryLimit(queryDesc->plannedstmt);

		ExecutorStart(queryDesc, 0);
		ExecutorRun(queryDesc, ForwardScanDirection, 0L);

		autostats_get_cmdtype(queryDesc, &cmdType, &relationOid);

		queryDesc->dest->rDestroy(queryDesc->dest);
		ExecutorFinish(queryDesc);
		ExecutorEnd(queryDesc);

		auto_stats(cmdType, relationOid, queryDesc->es_processed, false);

		FreeQueryDesc(queryDesc);

		/* Restore the old snapshot */
		PopActiveSnapshot();
		optimizer = saveOptimizerGucValue;

		CommandCounterIncrement(); /* see the effects of the command */

		/*
		 * Step (d) - tell the seg nodes about the temporary relation. This
		 * involves stomping on the node we've been given
		 */
		if (rootCmd == cmd)
			spec->backendId = MyBackendId;
	}
	else
	{
		tmprv = make_temp_table_name(rel, spec->backendId);
	}

	/*
	 * Step (e) - Correct ownership on temporary table:
	 *   necessary so that the toast tables/indices have the correct
	 *   owner after we swap them.
	 *
	 * Note: ATExecChangeOwner does NOT dispatch, so this does not
	 * belong in the dispatch block above (MPP-9663).
	 */
	ATExecChangeOwner(RangeVarGetRelid(tmprv, NoLock, false),
					  rel->rd_rel->relowner, true, AccessExclusiveLock);
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
	heap_close(rel, NoLock);
	rel = NULL;
	tmprelid = RangeVarGetRelid(tmprv, NoLock, false);
	swap_relation_files(relid, tmprelid,
						false, /* target_is_pg_class */
						false, /* swap_toast_by_content */
						false, /* swap_stats */
						true,
						RecentXmin,
						ReadNextMultiXactId(),
						NULL);

	/*
	 * Make changes from swapping relation files visible before updating
	 * options below or else we get an already updated tuple error.
	 */
	CommandCounterIncrement();

	/* now, reindex */
	reindex_relation(relid, 0);

	/* Step (h) Drop the table */
	{
		ObjectAddress object;
		object.classId = RelationRelationId;
		object.objectId = tmprelid;
		object.objectSubId = 0;

		performDeletion(&object, DROP_RESTRICT, 0);
	}

	if (relstorage_is_ao(relstorage) && IS_QUERY_DISPATCHER())
	{
		/*
		 * Drop the shared memory hash table entry for this table if it
		 * exists. We must do so since before the rewrite we probably have few
		 * non-zero segfile entries for this table while after the rewrite
		 * only segno zero will be full and the others will be empty. By
		 * dropping the hash entry we force refreshing the entry from the
		 * catalog the next time a write into this AO table comes along.
		 *
		 * Note that ALTER already took an exclusive lock on the old relation
		 * so we are guaranteed to not drop the hash entry from under any
		 * concurrent operation.
		 */
		LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);
		AORelRemoveHashEntry(relid);
		LWLockRelease(AOSegFileLock);
	}
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
	List 	   *lwith;
	DistributedBy *ldistro;
	List	   *cols = NIL;
	ListCell   *lc;
	GpPolicy   *policy = NULL;
	QueryDesc  *queryDesc;
	RangeVar   *tmprv;
	Oid			tmprelid;
	Oid			tarrelid = RelationGetRelid(rel);
	char		tarrelstorage = rel->rd_rel->relstorage;
	bool        rand_pol = false;
	bool        rep_pol = false;
	bool        force_reorg = false;
	Datum		newOptions = PointerGetDatum(NULL);
	bool		change_policy = false;
	int			numsegments;
	SetDistributionCmd *qe_data = NULL; 
	bool 				save_optimizer_replicated_table_insert;
	Oid					relationOid = InvalidOid;
	AutoStatsCmdType 	cmdType = AUTOSTATS_CMDTYPE_SENTINEL;

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
	ldistro = (DistributedBy *)lsecond(lprime);

	if (Gp_role == GP_ROLE_UTILITY)
		ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("SET DISTRIBUTED BY not supported in utility mode")));
	/*
	 * SET DISTRIBUTED BY only change the distribution policy, but should not
	 * change numsegments, keep the old value.
	 */
	numsegments = rel->rd_cdbpolicy->numsegments;

	if (Gp_role == GP_ROLE_DISPATCH && ldistro)
		ldistro->numsegments = numsegments;

	/* we only support partitioned/replicated tables */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (GpPolicyIsEntry(rel->rd_cdbpolicy))
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("%s not supported on non-distributed tables",
						ldistro ? "SET DISTRIBUTED BY" : "SET WITH")));
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (lwith)
		{
			char		*reorg_str = "reorganize";
			List		*nlist = NIL;

			if (list_length(lwith) > 1)
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot specify more than one option in WITH clause")));
			}

			DefElem *def = linitial(lwith);
			if (pg_strcasecmp(reorg_str, def->defname) == 0)
			{
				if (!def->arg)
					force_reorg = true;
				else if (IsA(def->arg, String) && pg_strcasecmp("TRUE", strVal(def->arg)) == 0)
					force_reorg = true;
				else if (IsA(def->arg, String) && pg_strcasecmp("FALSE", strVal(def->arg)) == 0)
					force_reorg = false;
				else
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("invalid REORGANIZE option"),
							 errhint("Valid REORGANIZE options are \"true\" or \"false\".")));
				}
			}
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("option \"%s\" not supported",
								def->defname)));
			}

			lwith = nlist;
		}

		newOptions = new_rel_opts(rel);

		if (ldistro)
			change_policy = true;

		if (ldistro && ldistro->ptype == POLICYTYPE_PARTITIONED && ldistro->keyCols == NIL)
		{
			bool hasPrimaryKey = relationHasPrimaryKey(rel);
			bool hasUniqueIndex = relationHasUniqueIndex(rel);
			rand_pol = true;

			if (hasPrimaryKey || hasUniqueIndex)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("cannot set to DISTRIBUTED RANDOMLY because relation has %s", hasPrimaryKey ? "primary Key" : "unique index"),
						 errhint("Drop the %s first.", hasPrimaryKey ? "primary key" : "unique index")));
			}

			if (!force_reorg)
			{
				if (GpPolicyIsRandomPartitioned(rel->rd_cdbpolicy))
					ereport(WARNING,
							(errcode(ERRCODE_DUPLICATE_OBJECT),
							 errmsg("distribution policy of relation \"%s\" already set to DISTRIBUTED RANDOMLY",
									RelationGetRelationName(rel)),
							 errhint("Use ALTER TABLE \"%s\" SET WITH (REORGANIZE=TRUE) DISTRIBUTED RANDOMLY to force a random redistribution.",
									 RelationGetRelationName(rel))));
			}

			policy = createRandomPartitionedPolicy(ldistro->numsegments);

			/* always need to rebuild if changed from replicated policy */
			if (!GpPolicyIsReplicated(rel->rd_cdbpolicy))
			{
				MemoryContext oldcontext;

				GpPolicyReplace(RelationGetRelid(rel), policy);
				oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));
				rel->rd_cdbpolicy = GpPolicyCopy(policy);
				MemoryContextSwitchTo(oldcontext);

				/* only need to rebuild if have new storage options */
				if (!(DatumGetPointer(newOptions) || force_reorg))
				{
					/*
					 * caller expects ATExecSetDistributedBy() to close rel
					 * (see the non-random distribution case below for why.
					 */
					heap_close(rel, NoLock);
					lsecond(lprime) = makeNode(SetDistributionCmd);
					lprime = lappend(lprime, policy);
					goto l_distro_fini;
				}
			}
		}

		if (ldistro && ldistro->ptype == POLICYTYPE_REPLICATED)
		{
			rep_pol = true;

			if (GpPolicyIsReplicated(rel->rd_cdbpolicy))
				ereport(WARNING,
						(errcode(ERRCODE_DUPLICATE_OBJECT),
						 errmsg("distribution policy of relation \"%s\" already set to DISTRIBUTED REPLICATED",
								RelationGetRelationName(rel)),
						 errhint("Use ALTER TABLE \"%s\" SET WITH (REORGANIZE=TRUE) DISTRIBUTED REPLICATED to force a replicated redistribution.",
								 RelationGetRelationName(rel))));

			policy = createReplicatedGpPolicy(ldistro->numsegments);

			/* rebuild if have new storage options or policy changed */
			if (!DatumGetPointer(newOptions) &&
				GpPolicyIsReplicated(rel->rd_cdbpolicy))
			{
				/*
				 * caller expects ATExecSetDistributedBy() to close rel
				 * (see the non-random distribution case below for why.
				 */
				heap_close(rel, NoLock);
				lsecond(lprime) = makeNode(SetDistributionCmd);
				lprime = lappend(lprime, policy);
				goto l_distro_fini;
			}

			/*
			 * system columns is not visiable to users for replicated table,
			 * so if table is convertint to replicated table, check if there
			 * are dependencies on the system columns.
			 */
			AttrNumber	attr;
			ObjectAddresses *checkObjects = new_object_addresses();

			for (attr = FirstLowInvalidHeapAttributeNumber + 1;
				 attr != InvalidAttrNumber; attr++)
			{
				ObjectAddress obj;
				obj.classId = RelationRelationId;
				obj.objectId = RelationGetRelid(rel);
				obj.objectSubId = attr;

				add_exact_object_address(&obj, checkObjects);
			}

			checkDependencies(checkObjects,
							  "cannot set distributed replicated because "
							  "other object depend on its system columns",
							  "system columns of replicated table will be exposed "
							  "to users after altering, resolve dependencies first");

			free_object_addresses(checkObjects);
		}
	}

	/*--
	 * Changing a table from random distribution to a specific distribution
	 * policy is the hard bit. For that, we must do the following:
	 *
	 * a) Ensure that the proposed policy is sensible
	 * b) Create a temporary table and reorganise data according to our desired
	 *    distribution policy. To do this, we build a Query node which express
	 *    the query:
	 *    CREATE TABLE tmp_tab_nam AS SELECT * FROM cur_table DISTRIBUTED BY (policy)
	 * c) Execute the query across all nodes
	 * d) Update our parse tree to include the details of the newly created
	 *    table
	 * e) Update the ownership of the temporary table
	 * f) Swap the relfilenodes of the existing table and the temporary table
	 * g) Update the policy on the QD to reflect the underlying data
	 * h) Drop the temporary table -- and with it, the old copy of the data
	 *--
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (change_policy)
		{
			List	*policykeys = NIL;
			List	*policyopclasses = NIL;

			/* Step (a) */
			if (!(rand_pol || rep_pol))
			{
				foreach(lc, ldistro->keyCols)
				{
					IndexElem  *ielem = (IndexElem *) lfirst(lc);
					char	   *colName = ielem->name;
					HeapTuple	tuple;
					AttrNumber	attnum;
					Form_pg_attribute attform;
					Oid			opclass;

					tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);

					if (!HeapTupleIsValid(tuple))
						ereport(ERROR,
								(errcode(ERRCODE_UNDEFINED_COLUMN),
								 errmsg("column \"%s\" of relation \"%s\" does not exist",
										colName,
										RelationGetRelationName(rel))));
					attform = (Form_pg_attribute) GETSTRUCT(tuple);
					attnum = attform->attnum;

					/* Prevent them from altering a system attribute */
					if (attnum <= 0)
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("cannot distribute by system column \"%s\"",
										colName)));

					/*
					 * Look up the opclass, like we do in for CREATE TABLE.
					 */
					opclass = cdb_get_opclass_for_column_def(ielem->opclass, attform->atttypid);
					policykeys = lappend_int(policykeys, attnum);
					policyopclasses = lappend_oid(policyopclasses, opclass);

					ReleaseSysCache(tuple);
					cols = lappend(cols, lfirst(lc));
				} /* end foreach */

				Assert(policykeys != NIL);
				policy = createHashPartitionedPolicy(policykeys,
													 policyopclasses,
													 ldistro->numsegments);

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
						if (policy->opclasses[i] != rel->rd_cdbpolicy->opclasses[i])
						{
							diff = true;
							break;
						}
					}
					if (!diff)
					{
						StringInfoData buf;

						initStringInfo(&buf);
						foreach(lc, ldistro->keyCols)
						{
							IndexElem *ielem = (IndexElem *) lfirst(lc);

							if (buf.len > 0)
								appendStringInfo(&buf, ", ");
							appendStringInfoString(&buf, ielem->name);
						}
						ereport(WARNING,
							(errcode(ERRCODE_DUPLICATE_OBJECT),
							 errmsg("distribution policy of relation \"%s\" "
								"already set to (%s)",
								RelationGetRelationName(rel),
								buf.data),
							 errhint("Use ALTER TABLE \"%s\" "
								"SET WITH (REORGANIZE=TRUE) "
								"DISTRIBUTED BY (%s) "
								"to force redistribution",
								RelationGetRelationName(rel),
								buf.data)));
						heap_close(rel, NoLock);
						/* Tell QEs to do nothing */
						linitial(lprime) = NULL;
						lsecond(lprime) = makeNode(SetDistributionCmd);
						lprime = lappend(lprime, NULL);

						return;
						/* don't goto l_distro_fini -- didn't do anything! */
					}
				}
			}

			checkUniqueIndexCompatible(rel, policy);
		}

		if (!ldistro)
			ldistro = make_distributedby_for_rel(rel);

		/*
		 * Force the use of Postgres query optimizer, since Pivotal Optimizer (GPORCA) will not
		 * redistribute the tuples if the current and required distributions
		 * are both RANDOM even when reorganize is set to "true"
		 */
		bool saveOptimizerGucValue = optimizer;
		optimizer = false;

		if (saveOptimizerGucValue)
			ereport(LOG,
					(errmsg("ALTER SET DISTRIBUTED BY: falling back to Postgres query optimizer to ensure re-distribution of tuples.")));

		GpPolicy *original_policy = NULL;

		/*
		 * Disable optimizer_replicated_table_insert so planner 
		 * can force a broadcast motion even both source and target
		 * are replicated table. This is important when altering
		 * distribution policy is called by gpexpand.
		 */
		save_optimizer_replicated_table_insert = optimizer_replicated_table_insert;
		optimizer_replicated_table_insert = false;

		if (force_reorg && !rand_pol && !GpPolicyIsReplicated(rel->rd_cdbpolicy))
		{
			/*
			 * since we force the reorg, we don't care about the original
			 * distribution policy of the source table hence, we can set the
			 * policy to random, which will force it to redistribute if the new
			 * distribution policy is partitioned, even the new partition policy
			 * is same as the original one, the query optimizer will generate
			 * redistribute plan.
			 */
			MemoryContext oldcontext;
			GpPolicy *random_policy = createRandomPartitionedPolicy(ldistro->numsegments);

			original_policy = rel->rd_cdbpolicy;
			/*
			 * break the link to avoid original_policy from getting deleted if
			 * relcache invalidation happens.
			 */
			rel->rd_cdbpolicy = NULL;
			/* update the catalog first and then assign the policy to rd_cdbpolicy */
			GpPolicyReplace(RelationGetRelid(rel), random_policy);

			oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));
			rel->rd_cdbpolicy = GpPolicyCopy(random_policy);
			MemoryContextSwitchTo(oldcontext);
		}

		/* Step (b) - build CTAS */
		queryDesc = build_ctas_with_dist(rel, ldistro,
						untransformRelOptions(newOptions),
						&tmprv,
						true);

		/*
		 * bypass gpmon info collecting in following ExecutorStart
		 * to be consistent with other alter table commands,
		 * ALTER TABLE SET DISTRIBUTED BY should not be logged in gpperfmon.
		 */
		queryDesc->gpmon_pkt = NULL;

		/* 
		 * We need to update our snapshot here to make sure we see all
		 * committed work. We have an exclusive lock on the table so no one
		 * will be able to access the table now.
		 */
		PushActiveSnapshot(GetLatestSnapshot());

		/* Step (c) - run on all nodes */
		queryDesc->ddesc = makeNode(QueryDispatchDesc);
		queryDesc->ddesc->useChangedAOOpts = false;
		
		/* GPDB hook for collecting query info */
		if (query_info_collect_hook)
			(*query_info_collect_hook)(METRICS_QUERY_SUBMIT, queryDesc);

		queryDesc->plannedstmt->query_mem =
				ResourceManagerGetQueryMemoryLimit(queryDesc->plannedstmt);

		ExecutorStart(queryDesc, 0);
		ExecutorRun(queryDesc, ForwardScanDirection, 0L);

		if (Gp_role == GP_ROLE_DISPATCH)
			autostats_get_cmdtype(queryDesc, &cmdType, &relationOid);

		queryDesc->dest->rDestroy(queryDesc->dest);
		ExecutorFinish(queryDesc);
		ExecutorEnd(queryDesc);

		if (Gp_role == GP_ROLE_DISPATCH)
			auto_stats(cmdType,
					   relationOid,
					   queryDesc->es_processed,
					   false);

		FreeQueryDesc(queryDesc);

		/* Restore the old snapshot */
		PopActiveSnapshot();
		optimizer = saveOptimizerGucValue;
		optimizer_replicated_table_insert = save_optimizer_replicated_table_insert;

		CommandCounterIncrement(); /* see the effects of the command */

		if (original_policy)
		{
			/*
			 * update catalog first and then update the rd_cdbpolicy. This order
			 * avoids original_policy from getting freed before we use it for
			 * GpPolicyReplace() if relcache invalidation happens. Also, helps
			 * to have the rd_cdbpolicy current instead of reverse order which
			 * can invalidate our assignment to rd_cdbpolicy.
			 */
			GpPolicyReplace(RelationGetRelid(rel), original_policy);
			rel->rd_cdbpolicy = original_policy;
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
		int			backend_id;
		bool		reorg = false;

		Assert(list_length(lprime) >= 2);

		lwith = linitial(lprime);
		qe_data = lsecond(lprime);
		policy = lthird(lprime);

		if (policy)
			GpPolicyReplace(RelationGetRelid(rel), policy);

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

		newOptions = new_rel_opts(rel);
	}

	/*
	 * Step (e) - Correct ownership on temporary table:
	 *   necessary so that the toast tables/indices have the correct
	 *   owner after we swap them.
	 *
	 * Note: ATExecChangeOwner does NOT dispatch, so this does not
	 * belong in the dispatch block above (MPP-9663).
	 */
	ATExecChangeOwner(RangeVarGetRelid(tmprv, NoLock, false),
					  rel->rd_rel->relowner, true, AccessExclusiveLock);
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
	heap_close(rel, NoLock);
	rel = NULL;
	tmprelid = RangeVarGetRelid(tmprv, NoLock, false);
	swap_relation_files(tarrelid, tmprelid,
						false, /* target_is_pg_class */
						false, /* swap_toast_by_content */
						false, /* swap_stats */
						true,
						RecentXmin,
						ReadNextMultiXactId(),
						NULL);

	/*
	 * Make changes from swapping relation files visible before updating
	 * options below or else we get an already updated tuple error.
	 */
	CommandCounterIncrement();

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

	/* now, reindex */
	reindex_relation(tarrelid, 0);

	/* Step (g) */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (change_policy)
			GpPolicyReplace(tarrelid, policy);

		linitial(lprime) = lwith;
		lsecond(lprime) = qe_data;
		lprime = lappend(lprime, policy);
	}

	/* Step (h) Drop the table */
	{
		ObjectAddress object;
		object.classId = RelationRelationId;
		object.objectId = tmprelid;
		object.objectSubId = 0;

		performDeletion(&object, DROP_RESTRICT, 0);
	}

	if (relstorage_is_ao(tarrelstorage) && IS_QUERY_DISPATCHER())
	{
		/*
		 * Drop the shared memory hash table entry for this table if it
		 * exists. We must do so since before the rewrite we probably have few
		 * non-zero segfile entries for this table while after the rewrite
		 * only segno zero will be full and the others will be empty. By
		 * dropping the hash entry we force refreshing the entry from the
		 * catalog the next time a write into this AO table comes along.
		 *
		 * Note that ALTER already took an exclusive lock on the old relation
		 * so we are guaranteed to not drop the hash entry from under any
		 * concurrent operation.
		 */
		LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);
		AORelRemoveHashEntry(tarrelid);
		LWLockRelease(AOSegFileLock);
	}

l_distro_fini:

	/* MPP-6929: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		char *distro_str = make_distro_str(lwith, ldistro);
		/* don't check relkind - must be a table */
		MetaTrackUpdObject(RelationRelationId, tarrelid, GetUserId(), "ALTER",
						   distro_str);
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
	else if (IsAppendonlyMetadataRelkind(rel->rd_rel->relkind) ||
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

		sscan = systable_beginscan(deprel, DependDependerIndexId, true,
								   NULL, 2, scankey);

		while (HeapTupleIsValid(tup = systable_getnext(sscan)))
		{
			Form_pg_depend foundDep = (Form_pg_depend) GETSTRUCT(tup);

			if (foundDep->deptype == DEPENDENCY_INTERNAL)
			{
				toid = foundDep->refobjid;
				break;
			}
		}
		systable_endscan(sscan);
		heap_close(deprel, AccessShareLock);
	}
	return toid;
}

/*
 * Check if a relation is a parent in partition hierarchy by probing
 * pg_inherits catalog table.
 */
bool
rel_is_parent(Oid relid)
{
	Relation inhrel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	bool is_parent = false;

	ScanKeyInit(&scankey,
				Anum_pg_inherits_inhparent,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	inhrel = heap_open(InheritsRelationId, AccessShareLock);

	sscan = systable_beginscan(inhrel, InheritsParentIndexId,
							   true, NULL, 1, &scankey);

	if (systable_getnext(sscan))
		is_parent = true;

	systable_endscan(sscan);
	heap_close(inhrel, AccessShareLock);
	return is_parent;
}

/*
 * partition children, toast tables and indexes, and indexes on partition
 * children do not need long lived locks because the lock on the partition master
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
								   true, NULL, 2, scankey);

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
	char			 	 namBuf[NAMEDATALEN];
	AlterPartitionId 	*locPid 	= NULL;	/* local pid if IDRule */
	PgPartRule* 		 par_prule 	= NULL;	/* prule for parent if IDRule */
	char 		 		 lRelNameBuf[(NAMEDATALEN*2)];
	char 				*lrelname   = NULL;
	bool				 is_split = false;
	bool				 bSetTemplate = (att == AT_PartSetTemplate);
	PartitionElem *pelem;
	List	   *colencs = NIL;

	if (!(Gp_role == GP_ROLE_DISPATCH || IsBinaryUpgrade))
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

	if (locPid->idtype == AT_AP_IDName)
		snprintf(namBuf, sizeof(namBuf), " \"%s\"", strVal(locPid->partiddef));
	else
		namBuf[0] = '\0';

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

	Assert(!prule);

	/* DEFAULT checks */
	if (!pelem->isDefault && (pNode->default_part) &&
		!is_split && !bSetTemplate) /* MPP-6093: ok to reset template */
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
			 errmsg("cannot add %s partition%s to %s with DEFAULT partition \"%s\"",
					pNode->part->parkind == 'r' ? "RANGE" : "LIST",
					namBuf,
					lrelname,
					pNode->default_part->parname),
			 errhint("need to SPLIT partition \"%s\"",
					 pNode->default_part->parname)));

	if (pelem->isDefault && !is_split)
	{
		/* MPP-6093: ok to reset template */
		if (pNode->default_part && !bSetTemplate)
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("DEFAULT partition \"%s\" for %s already exists",
							pNode->default_part->parname,
							lrelname)));

		/* XXX XXX: move this check to gram.y ? */
		if (pelem->boundSpec)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("invalid use of boundary specification for DEFAULT partition%s of %s",
							namBuf,
							lrelname)));
	}

	/* Do the real work for add ... */

	if ('r' == pNode->part->parkind)
	{
		if (!pelem->isDefault && pelem->boundSpec && !IsA(pelem->boundSpec, PartitionBoundSpec))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("cannot add list partition to range partitioned table")));

		(void) atpxPartAddList(rel, is_split, colencs, pNode,
						(locPid->idtype == AT_AP_IDName) ?
						strVal(locPid->partiddef) : NULL, /* partition name */
						pelem->isDefault, pelem,
						PARTTYP_RANGE,
						par_prule,
						lrelname,
						bSetTemplate,
						rel->rd_rel->relowner);
	}
	else if ('l' == pNode->part->parkind)
	{
		if (!pelem->isDefault && pelem->boundSpec && !IsA(pelem->boundSpec, PartitionValuesSpec))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("cannot add range partition to list partitioned table")));

		(void) atpxPartAddList(rel, is_split, colencs, pNode,
						(locPid->idtype == AT_AP_IDName) ?
						strVal(locPid->partiddef) : NULL, /* partition name */
						pelem->isDefault, pelem,
						PARTTYP_LIST,
						par_prule,
						lrelname,
						bSetTemplate,
						rel->rd_rel->relowner);
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
ATPExecPartAlter(List **wqueue, AlteredTableInfo *tab, Relation *rel,
                 AlterPartitionCmd *pc)
{
	AlterPartitionId 	*pid		   = (AlterPartitionId *)pc->partid;
	AlterTableCmd 		*atc		   = (AlterTableCmd *)pc->arg1;
	PgPartRule   		*prule		   = NULL;
	List 				*pidlst		   = NIL;
	AlterPartitionId 	*pid2		   = makeNode(AlterPartitionId);
	AlterPartitionCmd 	*pc2		   = NULL;
	bool				 bPartitionCmd = true;	/* true if a "partition" cmd */
	Relation			 rel2		   = NULL;
	bool				 prepSplit	   = false;	/* true if the sub command of ALTER PARTITION is a SPLIT PARTITION */
	bool				 prepExchange  = false;	/* true if the sub command of ALTER PARTITION is a SPLIT PARTITION */

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
		  Gp_role != GP_ROLE_DISPATCH &&
		  !IsBinaryUpgrade)
		return;

	switch (atc->subtype)
	{
		case AT_PartSplit:				/* Split */
		{
			prepSplit = true; /* if sub-command is split partition then it will require some preprocessing */
		}
		/* FALL THRU */
		case AT_PartAdd:				/* Add */
		case AT_PartAddForSplit:		/* Add, as part of a split */
		case AT_PartDrop:				/* Drop */
		case AT_PartSetTemplate:		/* Set Subpartition Template */
				if (!gp_allow_non_uniform_partitioning_ddl)
				{
					ereport(ERROR,
						   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("cannot modify multi-level partitioned table to have non-uniform partitioning hierarchy")));
				}
				break;
				/* XXX XXX: treat set subpartition template special:

				need to pass the pNode to ATPExecPartSetTemplate and bypass
				ATExecCmd ...

				*/
		case AT_PartRename:	 			/* Rename */
				break;
		case AT_PartExchange:			/* Exchange */
		{
			prepExchange = true; /* if sub-command is exchange partition then it will require some preprocessing */
		}
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
							RelationGetRelationName(*rel))));
	}

	if (Gp_role == GP_ROLE_DISPATCH || IsBinaryUpgrade)
	{
		pid2->idtype = AT_AP_IDList;
		pid2->partiddef = (Node *)pidlst;
		pid2->location  = -1;

		prule = get_part_rule(*rel, pid2, true, true, NULL, false);

		if (!prule)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot ALTER PARTITION for relation \"%s\"",
							RelationGetRelationName(*rel))));
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

			if (prepSplit)
			{
				PgPartRule			*prule1	= NULL;
				bool is_at = true;
				prule1 = get_part_rule(*rel, pid2, true, true, NULL, false);

				if (linitial((List *)pc2->arg1)) /* Check if the SPLIT PARTITION command has an AT clause */
					is_at = false;

				prepSplitCmd(*rel, prule1, is_at);
			}
			else if (prepExchange)
			{
				ATPartitionCheck(atc->subtype, *rel, false, false);
				if (Gp_role == GP_ROLE_UTILITY)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("EXCHANGE is not supported in utility mode")));
				ATPrepExchange(*rel, pc2);
			}
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
				DistributedBy *dist = NULL;
				Assert(IsA(atc->def, List));

				dist = lsecond((List*)atc->def);

				/*	might be null if no policy set, e.g. just a change
				 *	of storage options...
				 */
				if (dist)
				{
					Assert(IsA(dist, DistributedBy));

					if (! can_implement_dist_on_part(*rel, dist) )
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
	ATExecCmd(wqueue, tab, (rel2 ? &rel2 : rel), atc, AccessExclusiveLock);

	if (!bPartitionCmd)
	{
		/* NOTE: for the case of Set Distro,
		 * ATExecSetDistributedBy rebuilds the relation, so rel2
		 * is already gone!
		 */
		if (atc->subtype != AT_SetDistributedBy && rel2)
			heap_close(rel2, NoLock);
	}

	/* MPP-6929: metadata tracking - don't track this! */

} /* end ATPExecPartAlter */


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
					 errmsg("FOR expression matches DEFAULT partition%s of %s",
							prule->partIdStr, prule->relname),
					 errhint("FOR expression may only specify a non-default partition in this context.")));

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
						 errmsg("partition \"%s\" of %s does not exist, skipping",
								strVal(locPid->partiddef), lrelname)));

				break;
			case AT_AP_IDValue:				/* IDentifier FOR Value */
				ereport(NOTICE,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("partition for specified value of %s does not exist, skipping",
								lrelname)));

				break;
			case AT_AP_IDRank:				/* IDentifier FOR Rank */
				ereport(NOTICE,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("partition for specified rank of %s does not exist, skipping",
								lrelname)));

				break;
			case AT_AP_ID_oid:				/* IDentifier by oid */
				ereport(NOTICE,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("partition for specified oid of %s does not exist, skipping",
								lrelname)));
				break;
			case AT_AP_IDDefault:			/* IDentify DEFAULT partition */
				ereport(NOTICE,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("DEFAULT partition for %s does not exist, skipping",
								lrelname)));
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
					   PROCESS_UTILITY_SUBCOMMAND,
					   NULL,
					   dest,
					   NULL);

		/* Notify of name if did not use name for partition id spec */
		if (prule->topRule && prule->topRule->children
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
							  true, NULL, 1, &scankey);

	/* should be one and only one parent when it comes to inheritance */
	tuple = systable_getnext(scan);
	if (!tuple)
		elog(ERROR, "could not find pg_inherits row for rel %u", oldrelid);

	parentid = ((Form_pg_inherits) GETSTRUCT(tuple))->inhparent;

	Assert(systable_getnext(scan) == NULL);

	systable_endscan(scan);
	heap_close(catalogRelation, AccessShareLock);

	parent = heap_open(parentid, AccessShareLock); /* should be enough */
	ATExecDetachPartitionInheritance(parent,
			makeRangeVar(get_namespace_name(oldrel->rd_rel->relnamespace),
					     RelationGetRelationName(oldrel), -1));

	inherit_parent(parent, newrel, true /* it's a partition */, NIL);

	AttachPartitionEnsureIndexes(parent, newrel);
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

		newrelid = RangeVarGetRelid(newrelrv, NoLock, false);
		Assert(OidIsValid(newrelid));

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
		oldrelid = RangeVarGetRelid(oldrelrv, NoLock, false);
		newrelid = RangeVarGetRelid(newrelrv, NoLock, false);
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
		Relation		 parentrel = NULL;
		AttrMap			*newmap; /* used for compatability check below only */
		AttrMap			*oldmap; /* used for compatability check below only */
		List			*newcons;
		bool			 ok;
		bool			 validate	= intVal(pc2->arg1) ? true : false;
		Oid				 parentrelid = InvalidOid;
		Oid				 oldnspid;
		Oid				 newnspid;
		Oid				 newrel_owner;
		Oid				 newrel_type;

		newrel = heap_open(newrelid, AccessExclusiveLock);
		if (RelationIsExternal(newrel) && validate)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("validation of external tables not supported"),
					 errhint("Use WITHOUT VALIDATION.")));

		oldrel = heap_open(oldrelid, AccessExclusiveLock);

		newrel_owner = oldrel->rd_rel->relowner;
		newrel_type = oldrel->rd_rel->reltype;

		oldnspid = RelationGetNamespace(oldrel);
		newnspid = RelationGetNamespace(newrel);

		newname = pstrdup(RelationGetRelationName(newrel));
		oldname = pstrdup(RelationGetRelationName(oldrel));
		
		ok = map_part_attrs(rel, newrel, &newmap, TRUE);
		Assert(ok);
		ok = map_part_attrs(rel, oldrel, &oldmap, TRUE);
		Assert(ok);

		parentrelid = rel_partition_get_root(oldrelid);
		if (parentrelid != RelationGetRelid(rel))
			parentrel = heap_open(parentrelid, AccessExclusiveLock);

		newcons = cdb_exchange_part_constraints(
			(parentrel == NULL) ? rel : parentrel,
			oldrel, newrel, validate, is_split, pc);

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
		if (parentrel != NULL)
			heap_close(parentrel, NoLock);

		/* RenameRelation renames the type too */
		RenameRelationInternal(oldrelid, tmpname1, true);
		CommandCounterIncrement();
		RelationForgetRelation(oldrelid);

		/* MPP-6979: if the namespaces are different, switch them */
		if (oldnspid != newnspid)
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
			RenameRelationInternal(newrelid, tmpname2, true);
			CommandCounterIncrement();
			RelationForgetRelation(newrelid);

			newrel = heap_open(newrelid, AccessExclusiveLock);
			AlterTableNamespaceInternal(newrel, newnspid, oldnspid, objsMoved);
			heap_close(newrel, NoLock);
			CommandCounterIncrement();
			RelationForgetRelation(newrelid);

			free_object_addresses(objsMoved);
		}

		RenameRelationInternal(newrelid, oldname, true);
		CommandCounterIncrement();
		RelationForgetRelation(newrelid);

		RenameRelationInternal(oldrelid, newname, true);
		CommandCounterIncrement();
		RelationForgetRelation(oldrelid);

		CommandCounterIncrement();

		/*
		 * Add array type for newname if it does not exist. Need to create
		 * the array type and modify the corresponding composite type to
		 * associate with the array type oid.
		 */
		Oid	         array_oid;
		char        *relarrayname;
		Relation     typrel;
		HeapTuple    typtup;
		Form_pg_type typform;
		bool         create_array;

		/* Has array type already existed? */
		typrel = heap_open(TypeRelationId, AccessShareLock);

		typtup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(newrel_type));
		if (!HeapTupleIsValid(typtup))
			elog(ERROR, "cache lookup failed for type %u", newrel_type);
		typform = (Form_pg_type) GETSTRUCT(typtup);

		create_array = (typform->typarray == 0);
		ReleaseSysCache(typtup);

		if (!create_array)
			heap_close(typrel, AccessShareLock);
		else
		{
			/* Create the array type for newname */
			relarrayname = makeArrayTypeName(newname, newnspid);

			if (Gp_role == GP_ROLE_EXECUTE)
				array_oid = GetPreassignedOidForType(newnspid, relarrayname, true);
			else
				array_oid = GetNewOid(typrel);

			heap_close(typrel, AccessShareLock);

			TypeCreate(array_oid,		/* force the type's OID to this */
					   relarrayname,	/* Array type name */
					   newnspid,	/* namespace */
					   InvalidOid,	/* Not composite, no relationOid */
					   0,			/* relkind, also N/A here */
					   newrel_owner,		/* owner's ID */
					   -1,			/* Internal size (varlena) */
					   TYPTYPE_BASE,	/* Not composite - typelem is */
					   TYPCATEGORY_ARRAY,	/* type-category (array) */
					   false,		/* array types are never preferred */
					   DEFAULT_TYPDELIM,	/* default array delimiter */
					   F_ARRAY_IN,	/* array input proc */
					   F_ARRAY_OUT, /* array output proc */
					   F_ARRAY_RECV,	/* array recv (bin) proc */
					   F_ARRAY_SEND,	/* array send (bin) proc */
					   InvalidOid,	/* typmodin procedure - none */
					   InvalidOid,	/* typmodout procedure - none */
					   F_ARRAY_TYPANALYZE,	/* array analyze procedure */
					   newrel_type,	/* array element type - the rowtype */
					   true,		/* yes, this is an array type */
					   InvalidOid,	/* this has no array type */
					   InvalidOid,	/* domain base type - irrelevant */
					   NULL,		/* default value - none */
					   NULL,		/* default binary representation */
					   false,		/* passed by reference */
					   'd',			/* alignment - must be the largest! */
					   'x',			/* fully TOASTable */
					   -1,			/* typmod */
					   0,			/* array dimensions for typBaseType */
					   false,		/* Type NOT NULL */
					   InvalidOid); /* rowtypes never have a collation */
			pfree(relarrayname);

			CommandCounterIncrement();

			/* Modify composite type to associate with the new array oid. */
			AlterTypeArray(newrel_type, array_oid);
			CommandCounterIncrement();
		}

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
					   PROCESS_UTILITY_SUBCOMMAND,
					   NULL,
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
							   PROCESS_UTILITY_SUBCOMMAND,
							   NULL,
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

	if (!(Gp_role == GP_ROLE_DISPATCH || IsBinaryUpgrade))
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
	map_part_attrs(temprel, intoa, &rria->ri_partInsertMap, true);
	map_part_attrs(temprel, intob, &rrib->ri_partInsertMap, true);
	Assert(NULL != rria->ri_RelationDesc);
	rria->ri_resultSlot = MakeSingleTupleTableSlot(rria->ri_RelationDesc->rd_att);
	Assert(NULL != rrib->ri_RelationDesc);
	rrib->ri_resultSlot = MakeSingleTupleTableSlot(rrib->ri_RelationDesc->rd_att);

	/* constr might not be defined if this is a default partition */
	if (intoa->rd_att->constr && intoa->rd_att->constr->num_check)
	{
		uint16 idx;
		List *bins = NIL;
		
		for (idx = 0; idx < intoa->rd_att->constr->num_check; idx++)
		{
			bins = list_concat(bins,
					make_ands_implicit(
						(Expr *)stringToNode(intoa->rd_att->constr->check[idx].ccbin)));
		}

		achk = ExecPrepareExpr((Expr *)bins, estate);
	}

	if (intob->rd_att->constr && intob->rd_att->constr->num_check)
	{
		uint16 idx;
		List *bins = NIL;
		
		for (idx = 0; idx < intob->rd_att->constr->num_check; idx++)
		{
			bins = list_concat(bins,
					make_ands_implicit(
						(Expr *)stringToNode(intob->rd_att->constr->check[idx].ccbin)));
		}

		bchk = ExecPrepareExpr((Expr *)bins, estate);
	}

	/* be careful about AO vs. normal heap tables */
	Snapshot snapshot = RegisterSnapshot(GetLatestSnapshot());
	if (RelationIsHeap(temprel))
		heapscan = heap_beginscan(temprel, snapshot, 0, NULL);
	else if (RelationIsAoRows(temprel))
		aoscan = appendonly_beginscan(temprel, snapshot, snapshot, 0, NULL);
	else if (RelationIsAoCols(temprel))
	{
		int nvp = temprel->rd_att->natts;
		int i;

		aocsproj = (bool *) palloc(sizeof(bool) * nvp);
		for(i=0; i<nvp; ++i)
			aocsproj[i] = true;

		aocsscan = aocs_beginscan(temprel, snapshot, snapshot, NULL /* relationTupleDesc */, aocsproj);
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
			appendonly_getnext(aoscan, ForwardScanDirection, slotT);
			if (TupIsNull(slotT))
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

			if (!(*targetAODescPtr))
			{
				MemoryContextSwitchTo(oldCxt);
				*targetAODescPtr = appendonly_insert_init(targetRelation,
														  RESERVED_SEGNO, false);
				MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
			}

			mtuple = ExecFetchSlotMemTuple(targetSlot);
			appendonly_insert(*targetAODescPtr, mtuple, InvalidOid, &aoTupleId);

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
			ExecInsertIndexTuples(targetSlot, tid, estate);
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

	UnregisterSnapshot(snapshot);

	destroy_split_resultrel(rria);
	destroy_split_resultrel(rrib);
}

/* ALTER TABLE ... SPLIT PARTITION */

/* Given a Relation, make a DISTRIBUTED BY (...) clause for parser consumption. */
DistributedBy *
make_distributedby_for_rel(Relation rel)
{
	GpPolicy *policy = rel->rd_cdbpolicy;
	int			i;
	DistributedBy *dist;

	dist = makeNode(DistributedBy);

	Assert(policy->ptype != POLICYTYPE_ENTRY);

	if (GpPolicyIsReplicated(policy))
	{
		/* must be random distribution */
		dist->ptype = POLICYTYPE_REPLICATED;
		dist->numsegments = policy->numsegments;
		dist->keyCols = NIL;
	}
	else
	{
		TupleDesc tupdesc = RelationGetDescr(rel);
		List 		*keys = NIL;

		for (i = 0; i < policy->nattrs; i++)
		{
			int			attno = policy->attrs[i];
			Oid			opclassoid = policy->opclasses[i];
			char	   *attname = pstrdup(NameStr(tupdesc->attrs[attno - 1]->attname));
			IndexElem  *ielem;
			HeapTuple	ht_opc;
			Form_pg_opclass opcrec;
			char	   *opcname;
			char	   *nspname;

			ht_opc = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclassoid));
			if (!HeapTupleIsValid(ht_opc))
				elog(ERROR, "cache lookup failed for opclass %u", opclassoid);
			opcrec = (Form_pg_opclass) GETSTRUCT(ht_opc);
			nspname = get_namespace_name(opcrec->opcnamespace);
			opcname = pstrdup(NameStr(opcrec->opcname));

			ielem = makeNode(IndexElem);
			ielem->name = attname;
			ielem->opclass = list_make2(makeString(nspname), makeString(opcname));

			keys = lappend(keys, ielem);

			ReleaseSysCache(ht_opc);
		}

		dist->ptype = POLICYTYPE_PARTITIONED;
		dist->numsegments = policy->numsegments;
		dist->keyCols = keys;
	}

	return dist;
}

/*
 * Given a relation, get all column encodings for that relation as a list of
 * ColumnReferenceStorageDirective structures.
 */
List *
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

	if (RelationIsAppendOptimized(rel))
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
		TableLikeClause *inh = makeNode(TableLikeClause);
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
		DistributedBy *distro = NULL;
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
		distro = make_distributedby_for_rel(existrel);
		colencs = rel_get_column_encodings(existrel);
		orient = make_orientation_options(existrel);

		heap_close(existrel, NoLock);

		/* 1) Create temp table */
		rv = makeRangeVar(nspname, relname, -1);
		inh->relation = copyObject(rv);
		inh->options = CREATE_TABLE_LIKE_DEFAULTS
			| CREATE_TABLE_LIKE_CONSTRAINTS
			| CREATE_TABLE_LIKE_INDEXES;
		ct->tableElts = list_make1(inh);
		ct->distributedBy = copyObject(distro); /* must preserve the list for later */

		/* should be unique enough */
		snprintf(tmpname, NAMEDATALEN, "pg_temp_%u", relid);
		tmprv = makeRangeVar(nspname, tmpname, -1);
		tmprv->relpersistence = (*rel)->rd_rel->relpersistence;
		ct->relation = tmprv;
		ct->relKind = RELKIND_RELATION;
		ct->ownerid = (*rel)->rd_rel->relowner;
		/* Don't treat this table differently than a normal CREATE TABLE LIKE */
		ct->is_split_part = false;
		/* No transformation happens for this stmt in parse_analyze() */
		q = parse_analyze((Node *) ct, synthetic_sql, NULL, 0);
		ProcessUtility((Node *)q->utilityStmt,
					   synthetic_sql,
					   PROCESS_UTILITY_SUBCOMMAND,
					   NULL,
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
		q = parse_analyze((Node *) ats, synthetic_sql, NULL, 0);

		heap_close(*rel, NoLock);
		ProcessUtility((Node *)q->utilityStmt,
					   synthetic_sql,
					   PROCESS_UTILITY_SUBCOMMAND,
					   NULL,
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
		q = parse_analyze((Node *) ats, synthetic_sql, NULL, 0);
		heap_close(*rel, NoLock);

		ProcessUtility((Node *)q->utilityStmt,
					   synthetic_sql,
					   PROCESS_UTILITY_SUBCOMMAND,
					   NULL,
					   dest,
					   NULL);

		*rel = heap_open(relid, AccessExclusiveLock);
		CommandCounterIncrement();

		elog(DEBUG5, "Split partition: adding two new partitions");

		/* 4) add two new partitions, via a loop to reduce code duplication */
		for (i = 1; i <= 2; i++)
		{
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
				/*
				 * MPP-10223: pid contains a "stale" pgPartRule with a
				 * partition rule for the partition we just dropped.
				 * Refresh partition rule from the catalog.
				 */
				AlterPartitionId *tmppid = copyObject(pid);

				/*
				 * Save the pointer value before we modify the partition rule
				 * tree.
				 */
				aapid = tmppid;


				while (tmppid->idtype == AT_AP_IDRule)
				{
					PgPartRule	*tmprule = NULL;
					PgPartRule	*newRule = NULL;

					List *l = (List *) tmppid->partiddef;

					/*
					 * We need to update the PgPartNode under
					 * our AlterPartitionId because it still
					 * contains the partition that we just
					 * dropped. If we do not remove it here
					 * PartAdd will find it and refuse to
					 * add the new partition.
					 */
					tmprule = (PgPartRule *) linitial(l);
					Assert(nodeTag(tmprule) == T_PgPartRule);

					AlterPartitionId *apidByName = makeNode(AlterPartitionId);

					apidByName->partiddef = (Node *)makeString(tmprule->topRule->parname);
					apidByName->idtype = AT_AP_IDName;

					newRule = get_part_rule(*rel, apidByName, true, true, NULL, false);
					list_nth_replace(l, 0, newRule);
					pfree(tmprule);

					tmppid = lsecond(l);
				}
			}

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
											(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
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

									d = OidFunctionCall2Coll(funcid,
															 v->constcollid,
															 c->constvalue,
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
			q = parse_analyze((Node *) ats, synthetic_sql, NULL, 0);

			heap_close(*rel, NoLock);
			ProcessUtility((Node *)q->utilityStmt,
						   synthetic_sql,
						   PROCESS_UTILITY_QUERY,
						   NULL,
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

		performDeletion(&addr, DROP_RESTRICT, 0);
	}

	heap_close(intoa, NoLock);
	heap_close(intob, NoLock);
}

/*
 * ALTER TABLE OF
 *
 * Attach a table to a composite type, as though it had been created with CREATE
 * TABLE OF.  All attname, atttypid, atttypmod and attcollation must match.  The
 * subject table must not have inheritance parents.  These restrictions ensure
 * that you cannot create a configuration impossible with CREATE TABLE OF alone.
 */
static void
ATExecAddOf(Relation rel, const TypeName *ofTypename, LOCKMODE lockmode)
{
	Oid			relid = RelationGetRelid(rel);
	Type		typetuple;
	Oid			typeid;
	Relation	inheritsRelation,
				relationRelation;
	SysScanDesc scan;
	ScanKeyData key;
	AttrNumber	table_attno,
				type_attno;
	TupleDesc	typeTupleDesc,
				tableTupleDesc;
	ObjectAddress tableobj,
				typeobj;
	HeapTuple	classtuple;

	/* Validate the type. */
	typetuple = typenameType(NULL, ofTypename, NULL);
	check_of_type(typetuple);
	typeid = HeapTupleGetOid(typetuple);

	/* Fail if the table has any inheritance parents. */
	inheritsRelation = heap_open(InheritsRelationId, AccessShareLock);
	ScanKeyInit(&key,
				Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	scan = systable_beginscan(inheritsRelation, InheritsRelidSeqnoIndexId,
							  true, NULL, 1, &key);
	if (HeapTupleIsValid(systable_getnext(scan)))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("typed tables cannot inherit")));
	systable_endscan(scan);
	heap_close(inheritsRelation, AccessShareLock);

	/*
	 * Check the tuple descriptors for compatibility.  Unlike inheritance, we
	 * require that the order also match.  However, attnotnull need not match.
	 * Also unlike inheritance, we do not require matching relhasoids.
	 */
	typeTupleDesc = lookup_rowtype_tupdesc(typeid, -1);
	tableTupleDesc = RelationGetDescr(rel);
	table_attno = 1;
	for (type_attno = 1; type_attno <= typeTupleDesc->natts; type_attno++)
	{
		Form_pg_attribute type_attr,
					table_attr;
		const char *type_attname,
				   *table_attname;

		/* Get the next non-dropped type attribute. */
		type_attr = typeTupleDesc->attrs[type_attno - 1];
		if (type_attr->attisdropped)
			continue;
		type_attname = NameStr(type_attr->attname);

		/* Get the next non-dropped table attribute. */
		do
		{
			if (table_attno > tableTupleDesc->natts)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("table is missing column \"%s\"",
								type_attname)));
			table_attr = tableTupleDesc->attrs[table_attno++ - 1];
		} while (table_attr->attisdropped);
		table_attname = NameStr(table_attr->attname);

		/* Compare name. */
		if (strncmp(table_attname, type_attname, NAMEDATALEN) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("table has column \"%s\" where type requires \"%s\"",
						table_attname, type_attname)));

		/* Compare type. */
		if (table_attr->atttypid != type_attr->atttypid ||
			table_attr->atttypmod != type_attr->atttypmod ||
			table_attr->attcollation != type_attr->attcollation)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
				  errmsg("table \"%s\" has different type for column \"%s\"",
						 RelationGetRelationName(rel), type_attname)));
	}
	DecrTupleDescRefCount(typeTupleDesc);

	/* Any remaining columns at the end of the table had better be dropped. */
	for (; table_attno <= tableTupleDesc->natts; table_attno++)
	{
		Form_pg_attribute table_attr = tableTupleDesc->attrs[table_attno - 1];

		if (!table_attr->attisdropped)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("table has extra column \"%s\"",
							NameStr(table_attr->attname))));
	}

	/* If the table was already typed, drop the existing dependency. */
	if (rel->rd_rel->reloftype)
		drop_parent_dependency(relid, TypeRelationId, rel->rd_rel->reloftype, false);

	/* Record a dependency on the new type. */
	tableobj.classId = RelationRelationId;
	tableobj.objectId = relid;
	tableobj.objectSubId = 0;
	typeobj.classId = TypeRelationId;
	typeobj.objectId = typeid;
	typeobj.objectSubId = 0;
	recordDependencyOn(&tableobj, &typeobj, DEPENDENCY_NORMAL);

	/* Update pg_class.reloftype */
	relationRelation = heap_open(RelationRelationId, RowExclusiveLock);
	classtuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(classtuple))
		elog(ERROR, "cache lookup failed for relation %u", relid);
	((Form_pg_class) GETSTRUCT(classtuple))->reloftype = typeid;
	simple_heap_update(relationRelation, &classtuple->t_self, classtuple);
	CatalogUpdateIndexes(relationRelation, classtuple);

	InvokeObjectPostAlterHook(RelationRelationId, relid, 0);

	heap_freetuple(classtuple);
	heap_close(relationRelation, RowExclusiveLock);

	ReleaseSysCache(typetuple);
}

/*
 * ALTER TABLE NOT OF
 *
 * Detach a typed table from its originating type.  Just clear reloftype and
 * remove the dependency.
 */
static void
ATExecDropOf(Relation rel, LOCKMODE lockmode)
{
	Oid			relid = RelationGetRelid(rel);
	Relation	relationRelation;
	HeapTuple	tuple;

	if (!OidIsValid(rel->rd_rel->reloftype))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a typed table",
						RelationGetRelationName(rel))));

	/*
	 * We don't bother to check ownership of the type --- ownership of the
	 * table is presumed enough rights.  No lock required on the type, either.
	 */

	drop_parent_dependency(relid, TypeRelationId, rel->rd_rel->reloftype, false);

	/* Clear pg_class.reloftype */
	relationRelation = heap_open(RelationRelationId, RowExclusiveLock);
	tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relid);
	((Form_pg_class) GETSTRUCT(tuple))->reloftype = InvalidOid;
	simple_heap_update(relationRelation, &tuple->t_self, tuple);
	CatalogUpdateIndexes(relationRelation, tuple);

	InvokeObjectPostAlterHook(RelationRelationId, relid, 0);

	heap_freetuple(tuple);
	heap_close(relationRelation, RowExclusiveLock);
}

/*
 * relation_mark_replica_identity: Update a table's replica identity
 *
 * Iff ri_type = REPLICA_IDENTITY_INDEX, indexOid must be the Oid of a suitable
 * index. Otherwise, it should be InvalidOid.
 */
static void
relation_mark_replica_identity(Relation rel, char ri_type, Oid indexOid,
							   bool is_internal)
{
	Relation	pg_index;
	Relation	pg_class;
	HeapTuple	pg_class_tuple;
	HeapTuple	pg_index_tuple;
	Form_pg_class pg_class_form;
	Form_pg_index pg_index_form;

	ListCell   *index;

	/*
	 * Check whether relreplident has changed, and update it if so.
	 */
	pg_class = heap_open(RelationRelationId, RowExclusiveLock);
	pg_class_tuple = SearchSysCacheCopy1(RELOID,
									ObjectIdGetDatum(RelationGetRelid(rel)));
	if (!HeapTupleIsValid(pg_class_tuple))
		elog(ERROR, "cache lookup failed for relation \"%s\"",
			 RelationGetRelationName(rel));
	pg_class_form = (Form_pg_class) GETSTRUCT(pg_class_tuple);
	if (pg_class_form->relreplident != ri_type)
	{
		pg_class_form->relreplident = ri_type;
		simple_heap_update(pg_class, &pg_class_tuple->t_self, pg_class_tuple);
		CatalogUpdateIndexes(pg_class, pg_class_tuple);
	}
	heap_close(pg_class, RowExclusiveLock);
	heap_freetuple(pg_class_tuple);

	/*
	 * Check whether the correct index is marked indisreplident; if so, we're
	 * done.
	 */
	if (OidIsValid(indexOid))
	{
		Assert(ri_type == REPLICA_IDENTITY_INDEX);

		pg_index_tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexOid));
		if (!HeapTupleIsValid(pg_index_tuple))
			elog(ERROR, "cache lookup failed for index %u", indexOid);
		pg_index_form = (Form_pg_index) GETSTRUCT(pg_index_tuple);

		if (pg_index_form->indisreplident)
		{
			ReleaseSysCache(pg_index_tuple);
			return;
		}
		ReleaseSysCache(pg_index_tuple);
	}

	/*
	 * Clear the indisreplident flag from any index that had it previously,
	 * and set it for any index that should have it now.
	 */
	pg_index = heap_open(IndexRelationId, RowExclusiveLock);
	foreach(index, RelationGetIndexList(rel))
	{
		Oid			thisIndexOid = lfirst_oid(index);
		bool		dirty = false;

		pg_index_tuple = SearchSysCacheCopy1(INDEXRELID,
											 ObjectIdGetDatum(thisIndexOid));
		if (!HeapTupleIsValid(pg_index_tuple))
			elog(ERROR, "cache lookup failed for index %u", thisIndexOid);
		pg_index_form = (Form_pg_index) GETSTRUCT(pg_index_tuple);

		/*
		 * Unset the bit if set.  We know it's wrong because we checked this
		 * earlier.
		 */
		if (pg_index_form->indisreplident)
		{
			dirty = true;
			pg_index_form->indisreplident = false;
		}
		else if (thisIndexOid == indexOid)
		{
			dirty = true;
			pg_index_form->indisreplident = true;
		}

		if (dirty)
		{
			simple_heap_update(pg_index, &pg_index_tuple->t_self, pg_index_tuple);
			CatalogUpdateIndexes(pg_index, pg_index_tuple);
			InvokeObjectPostAlterHookArg(IndexRelationId, thisIndexOid, 0,
										 InvalidOid, is_internal);
		}
		heap_freetuple(pg_index_tuple);
	}

	heap_close(pg_index, RowExclusiveLock);
}

/*
 * ALTER TABLE <name> REPLICA IDENTITY ...
 */
static void
ATExecReplicaIdentity(Relation rel, ReplicaIdentityStmt *stmt, LOCKMODE lockmode)
{
	Oid			indexOid;
	Relation	indexRel;
	int			key;

	if (stmt->identity_type == REPLICA_IDENTITY_DEFAULT)
	{
		relation_mark_replica_identity(rel, stmt->identity_type, InvalidOid, true);
		return;
	}
	else if (stmt->identity_type == REPLICA_IDENTITY_FULL)
	{
		relation_mark_replica_identity(rel, stmt->identity_type, InvalidOid, true);
		return;
	}
	else if (stmt->identity_type == REPLICA_IDENTITY_NOTHING)
	{
		relation_mark_replica_identity(rel, stmt->identity_type, InvalidOid, true);
		return;
	}
	else if (stmt->identity_type == REPLICA_IDENTITY_INDEX)
	{
		 /* fallthrough */ ;
	}
	else
		elog(ERROR, "unexpected identity type %u", stmt->identity_type);


	/* Check that the index exists */
	indexOid = get_relname_relid(stmt->name, rel->rd_rel->relnamespace);
	if (!OidIsValid(indexOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("index \"%s\" for table \"%s\" does not exist",
						stmt->name, RelationGetRelationName(rel))));

	indexRel = index_open(indexOid, ShareLock);

	/* Check that the index is on the relation we're altering. */
	if (indexRel->rd_index == NULL ||
		indexRel->rd_index->indrelid != RelationGetRelid(rel))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not an index for table \"%s\"",
						RelationGetRelationName(indexRel),
						RelationGetRelationName(rel))));
	/* The AM must support uniqueness, and the index must in fact be unique. */
	if (!indexRel->rd_am->amcanunique || !indexRel->rd_index->indisunique)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
			 errmsg("cannot use non-unique index \"%s\" as replica identity",
					RelationGetRelationName(indexRel))));
	/* Deferred indexes are not guaranteed to be always unique. */
	if (!indexRel->rd_index->indimmediate)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		  errmsg("cannot use non-immediate index \"%s\" as replica identity",
				 RelationGetRelationName(indexRel))));
	/* Expression indexes aren't supported. */
	if (RelationGetIndexExpressions(indexRel) != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("cannot use expression index \"%s\" as replica identity",
					RelationGetRelationName(indexRel))));
	/* Predicate indexes aren't supported. */
	if (RelationGetIndexPredicate(indexRel) != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot use partial index \"%s\" as replica identity",
						RelationGetRelationName(indexRel))));
	/* And neither are invalid indexes. */
	if (!IndexIsValid(indexRel->rd_index))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot use invalid index \"%s\" as replica identity",
						RelationGetRelationName(indexRel))));

	/* Check index for nullable columns. */
	for (key = 0; key < indexRel->rd_index->indnatts; key++)
	{
		int16		attno = indexRel->rd_index->indkey.values[key];
		Form_pg_attribute attr;

		/* Allow OID column to be indexed; it's certainly not nullable */
		if (attno == ObjectIdAttributeNumber)
			continue;

		/*
		 * Reject any other system columns.  (Going forward, we'll disallow
		 * indexes containing such columns in the first place, but they might
		 * exist in older branches.)
		 */
		if (attno <= 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
					 errmsg("index \"%s\" cannot be used as replica identity because column %d is a system column",
							RelationGetRelationName(indexRel), attno)));

		attr = rel->rd_att->attrs[attno - 1];
		if (!attr->attnotnull)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("index \"%s\" cannot be used as replica identity because column \"%s\" is nullable",
							RelationGetRelationName(indexRel),
							NameStr(attr->attname))));
	}

	/* This index is suitable for use as a replica identity. Mark it. */
	relation_mark_replica_identity(rel, stmt->identity_type, indexOid, true);

	index_close(indexRel, NoLock);
}

/*
 * ALTER FOREIGN TABLE <name> OPTIONS (...)
 */
static void
ATExecGenericOptions(Relation rel, List *options)
{
	Relation	ftrel;
	ForeignServer *server;
	ForeignDataWrapper *fdw;
	HeapTuple	tuple;
	bool		isnull;
	Datum		repl_val[Natts_pg_foreign_table];
	bool		repl_null[Natts_pg_foreign_table];
	bool		repl_repl[Natts_pg_foreign_table];
	Datum		datum;
	Form_pg_foreign_table tableform;

	if (options == NIL)
		return;

	ftrel = heap_open(ForeignTableRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopy1(FOREIGNTABLEREL, rel->rd_id);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("foreign table \"%s\" does not exist",
						RelationGetRelationName(rel))));
	tableform = (Form_pg_foreign_table) GETSTRUCT(tuple);
	server = GetForeignServer(tableform->ftserver);
	fdw = GetForeignDataWrapper(server->fdwid);

	memset(repl_val, 0, sizeof(repl_val));
	memset(repl_null, false, sizeof(repl_null));
	memset(repl_repl, false, sizeof(repl_repl));

	/* Extract the current options */
	datum = SysCacheGetAttr(FOREIGNTABLEREL,
							tuple,
							Anum_pg_foreign_table_ftoptions,
							&isnull);
	if (isnull)
		datum = PointerGetDatum(NULL);

	/* Transform the options */
	datum = transformGenericOptions(ForeignTableRelationId,
									datum,
									options,
									fdw->fdwvalidator);

	if (PointerIsValid(DatumGetPointer(datum)))
		repl_val[Anum_pg_foreign_table_ftoptions - 1] = datum;
	else
		repl_null[Anum_pg_foreign_table_ftoptions - 1] = true;

	repl_repl[Anum_pg_foreign_table_ftoptions - 1] = true;

	/* Everything looks good - update the tuple */

	tuple = heap_modify_tuple(tuple, RelationGetDescr(ftrel),
							  repl_val, repl_null, repl_repl);

	simple_heap_update(ftrel, &tuple->t_self, tuple);
	CatalogUpdateIndexes(ftrel, tuple);

	/*
	 * Invalidate relcache so that all sessions will refresh any cached plans
	 * that might depend on the old options.
	 */
	CacheInvalidateRelcache(rel);

	InvokeObjectPostAlterHook(ForeignTableRelationId,
							  RelationGetRelid(rel), 0);

	heap_close(ftrel, RowExclusiveLock);

	heap_freetuple(tuple);
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
					   PROCESS_UTILITY_QUERY,
					   NULL,
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
 */
Oid
AlterTableNamespace(AlterObjectSchemaStmt *stmt)
{
	Relation	rel;
	Oid			relid;
	Oid			oldNspOid;
	Oid			nspOid;
	RangeVar   *newrv;
	ObjectAddresses *objsMoved;

	relid = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock,
									 stmt->missing_ok, false,
									 RangeVarCallbackForAlterRelation,
									 (void *) stmt);

	if (!OidIsValid(relid))
	{
		ereport(NOTICE,
				(errmsg("relation \"%s\" does not exist, skipping",
						stmt->relation->relname)));
		return InvalidOid;
	}

	rel = relation_open(relid, NoLock);

	oldNspOid = RelationGetNamespace(rel);

	/* If it's an owned sequence, disallow moving it by itself. */
	if (rel->rd_rel->relkind == RELKIND_SEQUENCE)
	{
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

	/* Get and lock schema OID and check its permissions. */
	newrv = makeRangeVar(stmt->newschema, RelationGetRelationName(rel), -1);
	nspOid = RangeVarGetAndCheckCreationNamespace(newrv, NoLock, NULL);

	/* common checks on switching namespaces */
	CheckSetNamespace(oldNspOid, nspOid, RelationRelationId, relid);

	objsMoved = new_object_addresses();
	AlterTableNamespaceInternal(rel, oldNspOid, nspOid, objsMoved);
	free_object_addresses(objsMoved);

	/* close rel, but keep lock until commit */
	relation_close(rel, NoLock);

	return relid;
}

/*
 * The guts of relocating a table to another namespace: besides moving
 * the table itself, its dependent objects are relocated to the new schema.
 */
void
AlterTableNamespaceInternal(Relation rel, Oid oldNspOid, Oid nspOid,
							ObjectAddresses *objsMoved)
{
	Relation	classRel;

	Assert(objsMoved != NULL);

	/* OK, modify the pg_class row and pg_depend entry */
	classRel = heap_open(RelationRelationId, RowExclusiveLock);

	AlterRelationNamespaceInternal(classRel, RelationGetRelid(rel), oldNspOid,
								   nspOid, true, objsMoved);

	/* Fix the table's row type too */
	AlterTypeNamespaceInternal(rel->rd_rel->reltype,
							   nspOid, false, false, objsMoved);

	/* Fix other dependent stuff */
	if (rel->rd_rel->relkind == RELKIND_RELATION ||
		rel->rd_rel->relkind == RELKIND_MATVIEW)
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
							   bool hasDependEntry,
							   ObjectAddresses *objsMoved)
{
	HeapTuple	classTup;
	Form_pg_class classForm;
	ObjectAddress thisobj;

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
			changeDependencyFor(RelationRelationId,
								relOid,
								NamespaceRelationId,
								oldNspOid,
								newNspOid) != 1)
			elog(ERROR, "failed to change schema dependency for relation \"%s\"",
				 NameStr(classForm->relname));

		add_exact_object_address(&thisobj, objsMoved);

		InvokeObjectPostAlterHook(RelationRelationId, relOid, 0);
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
		 * row type in pg_type, either.
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
							  NULL, 2, key);

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
				#if 0

				/*
				 * If this transaction hasn't accessed any temporary
				 * relations, we can skip truncating ON COMMIT DELETE ROWS
				 * tables, as they must still be empty.
				 */
				if (MyXactAccessedTempRel)
				#endif
				oids_to_truncate = lappend_oid(oids_to_truncate, oc->relid);
				break;
			case ONCOMMIT_DROP:
				{
					ObjectAddress object;

					object.classId = RelationRelationId;
					object.objectId = oc->relid;
					object.objectSubId = 0;

					/*
					 * Since this is an automatic drop, rather than one
					 * directly initiated by the user, we pass the
					 * PERFORM_DELETION_INTERNAL flag.
					 */
					performDeletion(&object,
									DROP_CASCADE, PERFORM_DELETION_INTERNAL);

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
 * subtransaction.  During subcommit, just relabel entries marked during
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

/* ALTER TABLE DROP CONSTRAINT */
static void
ATPrepDropConstraint(List **wqueue, Relation rel, AlterTableCmd *cmd, bool recurse, bool recursing)
{
	HeapTuple       tuple;
	ScanKeyData scankey;
	SysScanDesc sscan;
	Relation        pgcon;

	/*
	 * This command never recurses, but the offered relation may be partitioned,
	 * in which case, we need to act as if the command specified the top-level
	 * list of parts.
	 */
	if (cmd->subtype == AT_DropConstraintRecurse)
		recursing = true;

	/* (!recurse &&  !recursing) is supposed to detect the ONLY clause.
	 * We allow operations on the root of a partitioning hierarchy, but
	 * not ONLY the root.
	 */
	ATExternalPartitionCheck(cmd->subtype, rel, recursing);

	/*
	 * Don't allow check constraints on partitions to be dropped. Scan pg_constraint
	 * to find the constraint that we are dropping.
	 */
	pgcon = heap_open(ConstraintRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	sscan = systable_beginscan(pgcon, ConstraintRelidIndexId, true,
							   NULL, 1, &scankey);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{

		Form_pg_constraint constraint = (Form_pg_constraint) GETSTRUCT(tuple);
		if (namestrcmp(&constraint->conname, cmd->name) == 0)
		{
			if (constraint->contype == CONSTRAINT_CHECK)
				ATPartitionCheck(cmd->subtype, rel, (!recurse && !recursing), recursing);

			break;
		}
	}

	systable_endscan(sscan);
	heap_close(pgcon, AccessShareLock);
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

		oldrel = heap_open(prule->topRule->parchildrelid, AccessExclusiveLock);
		newrel = heap_openrv((RangeVar *)pc->arg1, AccessExclusiveLock);
	}
	else
	{
		oldrel = heap_openrv((RangeVar *)pc->partid,
							 AccessExclusiveLock);
		newrel = heap_openrv((RangeVar *)pc->arg1,
							 AccessExclusiveLock);
		
	}
	ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
	ATSimplePermissions(oldrel, ATT_TABLE | ATT_FOREIGN_TABLE);
	ATSimplePermissions(newrel, ATT_TABLE | ATT_FOREIGN_TABLE);
	
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
		case AT_AddColumnRecurse: /* internal to command/tablecmds.c */
		case AT_AddColumnToView: /* implicitly via CREATE OR REPLACE VIEW */
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
			
		case AT_SetStatistics: /* alter column set statistics */
		case AT_SetOptions: /* alter column set (<reloptions>) */
		case AT_ResetOptions: /* alter column reset (<reloptions>) */
		case AT_SetStorage: /* alter column set storage */
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
		case AT_ReAddConstraint: /* add constraint */
		case AT_AddConstraintRecurse: /* internal to commands/tablecmds.c */
		case AT_AddIndexConstraint:
			cmdstring = pstrdup("add a constraint to");
			break;

		case AT_AlterConstraint:
			cmdstring = pstrdup("alter constraint of");
			break;

		case AT_ValidateConstraint:
			cmdstring = pstrdup("validate constraint of");
			break;

		case AT_ValidateConstraintRecurse:
			cmdstring = pstrdup("validate constraint recurse of");
			break;

		case AT_ProcessedConstraint: /* pre-processed add constraint (local in parser/analyze.c) */
			break;

		case AT_DropConstraintRecurse:
			break;

		case AT_DropConstraint: /* drop constraint */
			cmdstring = pstrdup("drop a constraint from");
			break;
			
		case AT_AlterColumnType: /* alter column type */
			cmdstring = pstrdup("alter a column datatype of");
			break;
		case AT_AlterColumnGenericOptions: /* alter column options */
			cmdstring = pstrdup("alter a column option of");
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
		case AT_ReplaceRelOptions: /* REPLACE (...) -- AM specific parameters */
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

		case AT_AddOf:
		case AT_DropOf:
			cmdstring = pstrdup("alter OF type on");
			break;

		case AT_ReplicaIdentity:
			cmdstring = pstrdup("alter replica identity on");
			break;

		case AT_GenericOptions:
			cmdstring = pstrdup("alter options on");
			break;

		case AT_SetDistributedBy: /* SET DISTRIBUTED BY */
			cmdstring = pstrdup("set distributed on");
			break;

		case AT_ExpandTable:
			cmdstring = pstrdup("expand table data on");
			break;

		case AT_PartAdd: /* Add */
		case AT_PartAddForSplit: /* Add */
		case AT_PartAlter: /* Alter */
		case AT_PartDrop: /* Drop */
			break;
			
		case AT_PartExchange: /* Exchange */
			cmdstring = pstrdup("exchange a part into");
			break;
			
		case AT_PartRename: /* Rename */
		case AT_PartSetTemplate: /* Set Subpartition Template */
			break;
			
		case AT_PartSplit: /* Split */
			cmdstring = pstrdup("split parts of");
			break;
			
		case AT_PartTruncate: /* Truncate */
		case AT_PartAddInternal: /* CREATE TABLE time partition addition */
			break;

		case AT_AddOids: /* ALTER TABLE SET WITH OIDS */
		case AT_AddOidsRecurse: /* ALTER TABLE SET WITH OIDS */
			break;

		case AT_PartAttachIndex:
			break;
	}
	
	if ( cmdstring == NULL )
	{
		cmdstring = pstrdup("alter");
	}
	
	return cmdstring;
}

/*
 * CheckDropRelStorage
 *
 * Catch a mismatch between the DROP object type requested and the
 * actual object in the catalog. For example, if DROP EXTERNAL TABLE t
 * was issued, verify that t is indeed an external table, error if not.
 */
static void
CheckDropRelStorage(RangeVar *rel, ObjectType removeType)
{
	Oid relOid;
	HeapTuple tuple;
	char relstorage;

	relOid = RangeVarGetRelid(rel, NoLock, true);

	if (!OidIsValid(relOid))
		elog(ERROR, "Oid %u is invalid", relOid);

	/* Find out the relstorage */
	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relOid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relOid);
	relstorage = ((Form_pg_class) GETSTRUCT(tuple))->relstorage;
	ReleaseSysCache(tuple);

	/*
	 * Skip the check if it's external partition. Also, rel_is_child_partition
	 * is only performed on QD and since we do the check there, no need to do
	 * it again on QE.
	 */
	if (relstorage == RELSTORAGE_EXTERNAL &&
		(!IS_QUERY_DISPATCHER() || rel_is_child_partition(relOid)))
		return;

	if ((removeType == OBJECT_EXTTABLE && relstorage != RELSTORAGE_EXTERNAL) ||
		(removeType == OBJECT_TABLE && (relstorage == RELSTORAGE_EXTERNAL ||
										relstorage == RELSTORAGE_FOREIGN)))
	{
		/* we have a mismatch. format an error string and shoot */

		char *want_type;
		char *hint;

		if (removeType == OBJECT_EXTTABLE)
			want_type = pstrdup("an external");
		else
			want_type = pstrdup("a base");

		if (relstorage == RELSTORAGE_EXTERNAL)
			hint = pstrdup("Use DROP EXTERNAL TABLE to remove an external table.");
		else if (relstorage == RELSTORAGE_FOREIGN)
			hint = pstrdup("Use DROP FOREIGN TABLE to remove a foreign table.");
		else
			hint = pstrdup("Use DROP TABLE to remove a base table.");

		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						errmsg("\"%s\" is not %s table", rel->relname, want_type),
						errhint("%s", hint)));
	}
}

/*
 * This is intended as a callback for RangeVarGetRelidExtended().  It allows
 * the relation to be locked only if (1) it's a plain table, materialized
 * view, or TOAST table and (2) the current user is the owner (or the
 * superuser).  This meets the permission-checking needs of CLUSTER, REINDEX
 * TABLE, and REFRESH MATERIALIZED VIEW; we expose it here so that it can be
 * used by all.
 */
void
RangeVarCallbackOwnsTable(const RangeVar *relation,
						  Oid relId, Oid oldRelId, void *arg)
{
	char		relkind;

	/* Nothing to do if the relation was not found. */
	if (!OidIsValid(relId))
		return;

	/*
	 * If the relation does exist, check whether it's an index.  But note that
	 * the relation might have been dropped between the time we did the name
	 * lookup and now.  In that case, there's nothing to do.
	 */
	relkind = get_rel_relkind(relId);
	if (!relkind)
		return;
	if (relkind != RELKIND_RELATION && relkind != RELKIND_TOASTVALUE &&
		relkind != RELKIND_MATVIEW)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table or materialized view", relation->relname)));

	/* Check permissions */
	if (!pg_class_ownercheck(relId, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS, relation->relname);
}

/*
 * Callback to RangeVarGetRelidExtended(), similar to
 * RangeVarCallbackOwnsTable() but without checks on the type of the relation.
 */
void
RangeVarCallbackOwnsRelation(const RangeVar *relation,
							 Oid relId, Oid oldRelId, void *arg)
{
	HeapTuple	tuple;

	/* Nothing to do if the relation was not found. */
	if (!OidIsValid(relId))
		return;

	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relId));
	if (!HeapTupleIsValid(tuple))		/* should not happen */
		elog(ERROR, "cache lookup failed for relation %u", relId);

	if (!pg_class_ownercheck(relId, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   relation->relname);

	if (!allowSystemTableMods &&
		IsSystemClass(relId, (Form_pg_class) GETSTRUCT(tuple)))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						relation->relname)));

	ReleaseSysCache(tuple);
}

/*
 * Common RangeVarGetRelid callback for rename, set schema, and alter table
 * processing.
 */
static void
RangeVarCallbackForAlterRelation(const RangeVar *rv, Oid relid, Oid oldrelid,
								 void *arg)
{
	Node	   *stmt = (Node *) arg;
	ObjectType	reltype;
	HeapTuple	tuple;
	Form_pg_class classform;
	AclResult	aclresult;
	char		relkind;

	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tuple))
		return;					/* concurrently dropped */
	classform = (Form_pg_class) GETSTRUCT(tuple);
	relkind = classform->relkind;

	/* Must own relation. */
	if (!pg_class_ownercheck(relid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS, rv->relname);

	/* No system table modifications unless explicitly allowed. */
	if (!allowSystemTableMods && IsSystemClass(relid, classform))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						rv->relname)));

	/*
	 * Extract the specified relation type from the statement parse tree.
	 *
	 * Also, for ALTER .. RENAME, check permissions: the user must (still)
	 * have CREATE rights on the containing namespace.
	 */
	if (IsA(stmt, RenameStmt))
	{
		aclresult = pg_namespace_aclcheck(classform->relnamespace,
										  GetUserId(), ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
						   get_namespace_name(classform->relnamespace));
		reltype = ((RenameStmt *) stmt)->renameType;
	}
	else if (IsA(stmt, AlterObjectSchemaStmt))
		reltype = ((AlterObjectSchemaStmt *) stmt)->objectType;

	else if (IsA(stmt, AlterTableStmt))
		reltype = ((AlterTableStmt *) stmt)->relkind;
	else
	{
		reltype = OBJECT_TABLE; /* placate compiler */
		elog(ERROR, "unrecognized node type: %d", (int) nodeTag(stmt));
	}

	/*
	 * For compatibility with prior releases, we allow ALTER TABLE to be used
	 * with most other types of relations (but not composite types). We allow
	 * similar flexibility for ALTER INDEX in the case of RENAME, but not
	 * otherwise.  Otherwise, the user must select the correct form of the
	 * command for the relation at issue.
	 */
	if (reltype == OBJECT_SEQUENCE && relkind != RELKIND_SEQUENCE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a sequence", rv->relname)));

	if (reltype == OBJECT_VIEW && relkind != RELKIND_VIEW)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a view", rv->relname)));

	if (reltype == OBJECT_MATVIEW && relkind != RELKIND_MATVIEW)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a materialized view", rv->relname)));

	if (reltype == OBJECT_FOREIGN_TABLE && relkind != RELKIND_FOREIGN_TABLE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a foreign table", rv->relname)));

	if (reltype == OBJECT_TYPE && relkind != RELKIND_COMPOSITE_TYPE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a composite type", rv->relname)));

	if (reltype == OBJECT_INDEX && relkind != RELKIND_INDEX
		&& !IsA(stmt, RenameStmt))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not an index", rv->relname)));

	/*
	 * Don't allow ALTER TABLE on composite types. We want people to use ALTER
	 * TYPE for that.
	 */
	if (reltype != OBJECT_TYPE && relkind == RELKIND_COMPOSITE_TYPE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is a composite type", rv->relname),
				 errhint("Use ALTER TYPE instead.")));

	/*
	 * Don't allow ALTER TABLE .. SET SCHEMA on relations that can't be moved
	 * to a different schema, such as indexes and TOAST tables.
	 */
	if (IsA(stmt, AlterObjectSchemaStmt) &&
		relkind != RELKIND_RELATION &&
		relkind != RELKIND_VIEW &&
		relkind != RELKIND_MATVIEW &&
		relkind != RELKIND_SEQUENCE &&
		relkind != RELKIND_FOREIGN_TABLE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not a table, view, materialized view, sequence, or foreign table",
						rv->relname)));

	ReleaseSysCache(tuple);
}
/*
 * AttachPartitionEnsureIndexes
 *		subroutine for ATExecAttachPartition to create/match indexes
 *
 * Enforce the indexing rule for partitioned tables during ALTER TABLE / ATTACH
 * PARTITION: every partition must have an index attached to each index on the
 * partitioned table.
 */
static void
AttachPartitionEnsureIndexes(Relation rel, Relation attachrel)
{
	List	   *idxes;
	List	   *attachRelIdxs;
	Relation   *attachrelIdxRels;
	IndexInfo **attachInfos;
	int			i;
	ListCell   *cell;
	MemoryContext cxt;
	MemoryContext oldcxt;

	cxt = AllocSetContextCreate(CurrentMemoryContext,
								"AttachPartitionEnsureIndexes",
								ALLOCSET_DEFAULT_SIZES);
	oldcxt = MemoryContextSwitchTo(cxt);

	idxes = RelationGetIndexList(rel);
	attachRelIdxs = RelationGetIndexList(attachrel);
	attachrelIdxRels = palloc(sizeof(Relation) * list_length(attachRelIdxs));
	attachInfos = palloc(sizeof(IndexInfo *) * list_length(attachRelIdxs));

	/* Build arrays of all existing indexes and their IndexInfos */
	i = 0;
	foreach(cell, attachRelIdxs)
	{
		Oid			cldIdxId = lfirst_oid(cell);

		attachrelIdxRels[i] = index_open(cldIdxId, AccessShareLock);
		attachInfos[i] = BuildIndexInfo(attachrelIdxRels[i]);
		i++;
	}

	/*
	 * For each index on the partitioned table, find a matching one in the
	 * partition-to-be; if one is not found, create one.
	 */
	foreach(cell, idxes)
	{
		Oid			idx = lfirst_oid(cell);
		Relation	idxRel;
		IndexInfo  *info;
		AttrNumber *attmap;
		bool		found = false;
		Oid			constraintOid;

		/*
		 * Ignore indexes in the partitioned table other than partitioned
		 * indexes.
		 */
		if (!has_subclass(idx))
		{
			continue;
		}

		idxRel = index_open(idx, AccessShareLock);

		/* construct an indexinfo to compare existing indexes against */
		info = BuildIndexInfo(idxRel);
		attmap = convert_tuples_by_name_map(RelationGetDescr(attachrel),
											RelationGetDescr(rel),
											gettext_noop("could not convert row type"));
		constraintOid = get_relation_idx_constraint_oid(RelationGetRelid(rel), idx);

		/*
		 * Scan the list of existing indexes in the partition-to-be, and mark
		 * the first matching, unattached one we find, if any, as partition of
		 * the parent index.  If we find one, we're done.
		 */
		for (i = 0; i < list_length(attachRelIdxs); i++)
		{
			Oid			cldIdxId = RelationGetRelid(attachrelIdxRels[i]);
			Oid			cldConstrOid = InvalidOid;

			/* does this index have a parent?  if so, can't use it */
			if (has_superclass(cldIdxId))
				continue;

			if (CompareIndexInfo(attachInfos[i], info,
								 attachrelIdxRels[i]->rd_indcollation,
								 idxRel->rd_indcollation,
								 attachrelIdxRels[i]->rd_opfamily,
								 idxRel->rd_opfamily,
								 attmap,
								 RelationGetDescr(rel)->natts))
			{
				/*
				 * If this index is being created in the parent because of a
				 * constraint, then the child needs to have a constraint also,
				 * so look for one.  If there is no such constraint, this
				 * index is no good, so keep looking.
				 */
				if (OidIsValid(constraintOid))
				{
					cldConstrOid =
						get_relation_idx_constraint_oid(RelationGetRelid(attachrel),
														cldIdxId);
					/* no dice */
					if (!OidIsValid(cldConstrOid))
						continue;
				}

				/* bingo. */
				IndexSetParentIndex(attachrelIdxRels[i], idx);
				if (OidIsValid(constraintOid))
					ConstraintSetParentConstraint(cldConstrOid, constraintOid);
				found = true;
				break;
			}
		}

		/*
		 * If no suitable index was found in the partition-to-be, throw an error.
		 */
		if (!found)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("index like \"%s\" does not exist on \"%s\"",
							RelationGetRelationName(idxRel),
							RelationGetRelationName(attachrel))));
		}

		index_close(idxRel, AccessShareLock);
	}

	/* Clean up. */
	for (i = 0; i < list_length(attachRelIdxs); i++)
		index_close(attachrelIdxRels[i], AccessShareLock);
	MemoryContextSwitchTo(oldcxt);
	MemoryContextDelete(cxt);
}

/*
 * ALTER TABLE DETACH PARTITION
 *
 * Return the address of the relation that is no longer a partition of rel.
 *
 * GPDB: This is only a partial backport of the upstream ALTER TABLE DETACH
 * PARTITON. This is only used internally for EXCHANGE PARTITION.  We do not
 * expose the DDL to the end user.
 *
 * Unlike upstream's ATExecDetachPartition, this version only drops the
 * inheritance for a partition, and leaves the partition definition in place.
 */
static void
ATExecDetachPartitionInheritance(Relation rel, RangeVar *name)
{
	Relation	partRel;
	List	   *indexes;
	ListCell   *cell;

	partRel = heap_openrv(name, AccessShareLock);

	/* All inheritance related checks are performed within the function */
	RemoveInheritance(partRel, rel, true);

	/* detach indexes too */
	indexes = RelationGetIndexList(partRel);
	foreach(cell, indexes)
	{
		Oid			idxid = lfirst_oid(cell);
		Relation	idx;
		Oid			constrOid;

		if (!has_superclass(idxid))
			continue;

		Assert((IndexGetRelation(rel_partition_get_root(idxid), false) ==
				RelationGetRelid(rel)));

		idx = index_open(idxid, AccessExclusiveLock);
		IndexSetParentIndex(idx, InvalidOid);

		/* If there's a constraint associated with the index, detach it too */
		constrOid = get_relation_idx_constraint_oid(RelationGetRelid(partRel),
													idxid);
		if (OidIsValid(constrOid))
			ConstraintSetParentConstraint(constrOid, InvalidOid);

		index_close(idx, NoLock);
	}

	/*
	 * Invalidate the parent's relcache so that the partition is no longer
	 * included in its partition descriptor.
	 */
	CacheInvalidateRelcache(rel);

	/* keep our lock until commit */
	heap_close(partRel, NoLock);
}

/*
 * Before acquiring lock on an index, acquire the same lock on the owning
 * table.
 */
struct AttachIndexCallbackState
{
	Oid			partitionOid;
	Oid			parentTblOid;
	bool		lockedParentTbl;
};

static void
RangeVarCallbackForAttachIndex(const RangeVar *rv, Oid relOid, Oid oldRelOid,
							   void *arg)
{
	struct AttachIndexCallbackState *state;
	Form_pg_class classform;
	HeapTuple	tuple;

	state = (struct AttachIndexCallbackState *) arg;

	if (!state->lockedParentTbl)
	{
		LockRelationOid(state->parentTblOid, AccessShareLock);
		state->lockedParentTbl = true;
	}

	/*
	 * If we previously locked some other heap, and the name we're looking up
	 * no longer refers to an index on that relation, release the now-useless
	 * lock.  XXX maybe we should do *after* we verify whether the index does
	 * not actually belong to the same relation ...
	 */
	if (relOid != oldRelOid && OidIsValid(state->partitionOid))
	{
		UnlockRelationOid(state->partitionOid, AccessShareLock);
		state->partitionOid = InvalidOid;
	}

	/* Didn't find a relation, so no need for locking or permission checks. */
	if (!OidIsValid(relOid))
		return;

	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relOid));
	if (!HeapTupleIsValid(tuple))
		return;					/* concurrently dropped, so nothing to do */
	classform = (Form_pg_class) GETSTRUCT(tuple);
	if (classform->relkind != RELKIND_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("\"%s\" is not an index", rv->relname)));
	ReleaseSysCache(tuple);

	/*
	 * Since we need only examine the heap's tupledesc, an access share lock
	 * on it (preventing any DDL) is sufficient.
	 */
	state->partitionOid = IndexGetRelation(relOid, false);
	LockRelationOid(state->partitionOid, AccessShareLock);
}

/*
 * ALTER INDEX i1 ATTACH PARTITION i2
 *
 * GPDB: This is only being used internally in DefineIndex. The DDL has not
 * been exposed to the end user.  This is the workhorse of ALTER INDEX ...
 * ATTACH PARTITION in upstream, which attaches parent indexes to their
 * children.
 */
static void
ATExecAttachPartitionIdx(List **wqueue, Relation parentIdx, AlterPartitionId *alterpartId)
{
	Relation	partIdx;
	Relation	partTbl;
	Relation	parentTbl;
	RangeVar	*name;
	Oid			partIdxId;
	Oid			currParent;
	struct AttachIndexCallbackState state;

	Assert(alterpartId->idtype == AT_AP_IDRangeVar);
	Assert(IsA(alterpartId->partiddef, RangeVar));

	name = (RangeVar *) alterpartId->partiddef;
	
	/*
	 * We need to obtain lock on the index 'name' to modify it, but we also
	 * need to read its owning table's tuple descriptor -- so we need to lock
	 * both.  To avoid deadlocks, obtain lock on the table before doing so on
	 * the index.  Furthermore, we need to examine the parent table of the
	 * partition, so lock that one too.
	 */
	state.partitionOid = InvalidOid;
	state.parentTblOid = parentIdx->rd_index->indrelid;
	state.lockedParentTbl = false;
	partIdxId =
		RangeVarGetRelidExtended(name, AccessExclusiveLock, false, false,
								 RangeVarCallbackForAttachIndex,
								 (void *) &state);
	/* Not there? */
	if (!OidIsValid(partIdxId))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("index \"%s\" does not exist", name->relname)));

	/* no deadlock risk: RangeVarGetRelidExtended already acquired the lock */
	partIdx = relation_open(partIdxId, AccessExclusiveLock);

	/* we already hold locks on both tables, so this is safe: */
	parentTbl = relation_open(parentIdx->rd_index->indrelid, AccessShareLock);
	partTbl = relation_open(partIdx->rd_index->indrelid, NoLock);

	/* Silently do nothing if already in the right state */
	currParent = rel_partition_get_root(partIdxId);
	if (currParent != RelationGetRelid(parentIdx))
	{
		IndexInfo  *childInfo;
		IndexInfo  *parentInfo;
		AttrNumber *attmap;
		bool		found;
		List *childPartitions;
		Oid			constraintOid,
					cldConstrId = InvalidOid;

		/*
		 * If this partition already has an index attached, refuse the
		 * operation.
		 */
		refuseDupeIndexAttach(parentIdx, partIdx, partTbl);

		if (OidIsValid(currParent))
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("cannot attach index \"%s\" as a partition of index \"%s\"",
							RelationGetRelationName(partIdx),
							RelationGetRelationName(parentIdx)),
					 errdetail("Index \"%s\" is already attached to another index.",
							   RelationGetRelationName(partIdx))));

		/* Make sure it indexes a partition of the other index's table */
		childPartitions = find_inheritance_children(RelationGetRelid(parentTbl), NoLock);
		found = false;

		ListCell *lc;
		foreach(lc, childPartitions)
		{
			Oid			childRelid = lfirst_oid(lc);
			if (childRelid == state.partitionOid)
			{
				found = true;
				break;
			}
		}
		if (!found)
			ereport(ERROR,
					(errmsg("cannot attach index \"%s\" as a partition of index \"%s\"",
							RelationGetRelationName(partIdx),
							RelationGetRelationName(parentIdx)),
					 errdetail("Index \"%s\" is not an index on any partition of table \"%s\".",
							   RelationGetRelationName(partIdx),
							   RelationGetRelationName(parentTbl))));

		/* Ensure the indexes are compatible */
		childInfo = BuildIndexInfo(partIdx);
		parentInfo = BuildIndexInfo(parentIdx);
		attmap = convert_tuples_by_name_map(RelationGetDescr(partTbl),
											RelationGetDescr(parentTbl),
											gettext_noop("could not convert row type"));
		if (!CompareIndexInfo(childInfo, parentInfo,
							  partIdx->rd_indcollation,
							  parentIdx->rd_indcollation,
							  partIdx->rd_opfamily,
							  parentIdx->rd_opfamily,
							  attmap,
							  RelationGetDescr(partTbl)->natts))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
					 errmsg("cannot attach index \"%s\" as a partition of index \"%s\"",
							RelationGetRelationName(partIdx),
							RelationGetRelationName(parentIdx)),
					 errdetail("The index definitions do not match.")));

		/*
		 * If there is a constraint in the parent, make sure there is one in
		 * the child too.
		 */
		constraintOid = get_relation_idx_constraint_oid(RelationGetRelid(parentTbl),
														RelationGetRelid(parentIdx));

		if (OidIsValid(constraintOid))
		{
			cldConstrId = get_relation_idx_constraint_oid(RelationGetRelid(partTbl),
														  partIdxId);
			if (!OidIsValid(cldConstrId))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("cannot attach index \"%s\" as a partition of index \"%s\"",
								RelationGetRelationName(partIdx),
								RelationGetRelationName(parentIdx)),
						 errdetail("The index \"%s\" belongs to a constraint in table \"%s\" but no constraint exists for index \"%s\".",
								   RelationGetRelationName(parentIdx),
								   RelationGetRelationName(parentTbl),
								   RelationGetRelationName(partIdx))));
		}

		/* All good -- do it */
		IndexSetParentIndex(partIdx, RelationGetRelid(parentIdx));
		if (OidIsValid(constraintOid))
			ConstraintSetParentConstraint(cldConstrId, constraintOid);

		pfree(attmap);
	}

	relation_close(parentTbl, AccessShareLock);
	/* keep these locks till commit */
	relation_close(partTbl, NoLock);
	relation_close(partIdx, NoLock);

}

/*
 * Verify whether the given partition already contains an index attached
 * to the given partitioned index.  If so, raise an error.
 */
static void
refuseDupeIndexAttach(Relation parentIdx, Relation partIdx, Relation partitionTbl)
{
	Relation	pg_inherits;
	ScanKeyData key;
	HeapTuple	tuple;
	SysScanDesc scan;

	pg_inherits = heap_open(InheritsRelationId, AccessShareLock);
	ScanKeyInit(&key, Anum_pg_inherits_inhparent,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(parentIdx)));
	scan = systable_beginscan(pg_inherits, InheritsParentIndexId, true,
							  NULL, 1, &key);
	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
	{
		Form_pg_inherits inhForm;
		Oid			tab;

		inhForm = (Form_pg_inherits) GETSTRUCT(tuple);
		tab = IndexGetRelation(inhForm->inhrelid, false);
		if (tab == RelationGetRelid(partitionTbl))
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("cannot attach index \"%s\" as a partition of index \"%s\"",
							RelationGetRelationName(partIdx),
							RelationGetRelationName(parentIdx)),
					 errdetail("Another index is already attached for partition \"%s\".",
							   RelationGetRelationName(partitionTbl))));
	}

	systable_endscan(scan);
	heap_close(pg_inherits, AccessShareLock);
}

static inline void SetSchema(TupleDesc tuple_desc, List **schema, AttrNumber **attnos)
{
	AttrNumber	source_attno;
	AttrNumber target_attno = 0;
	TupleConstr *constr = tuple_desc->constr;

	for (source_attno = 1; source_attno <= tuple_desc->natts;
		 source_attno++)
	{
		Form_pg_attribute attribute = tuple_desc->attrs[source_attno - 1];
		ColumnDef  *def;

		/* Ignore dropped columns in the source. */
		if (attribute->attisdropped)
			continue;		/* leave attnos entry as zero */

		def = makeNode(ColumnDef);
		def->colname = pstrdup(NameStr(attribute->attname));
		def->typeName = makeTypeNameFromOid(attribute->atttypid,
			attribute->atttypmod);
		def->inhcount = 1;
		def->is_local = false;
		def->is_not_null = attribute->attnotnull;
		def->is_from_type = false;
		def->storage = attribute->attstorage;
		def->raw_default = NULL;
		def->cooked_default = NULL;
		def->collClause = NULL;
		def->collOid = attribute->attcollation;
		def->constraints = NIL;
		def->location = -1;
		(*attnos)[source_attno - 1] = ++target_attno;

		/* Copy default if any */
		if (attribute->atthasdef)
		{
			Node	   *this_default = NULL;
			AttrDefault *attrdef;
			int			i;

			/* Find default in constraint structure */
			Assert(constr != NULL);
			attrdef = constr->defval;
			for (i = 0; i < constr->num_defval; i++)
			{
				if (attrdef[i].adnum == source_attno)
				{
					this_default = stringToNode(attrdef[i].adbin);
					break;
				}
			}
			Assert(def->raw_default == NULL);
			Assert(def->cooked_default == NULL);
			/* we assume this is only called with cooked constraints */
			def->cooked_default = this_default;
		}
		*schema = lappend(*schema, def);
	}
}

static inline void SetConstraints(TupleDesc tupleDesc, char *relName, List **constraints, AttrNumber *attnos)
{
	Assert(tupleDesc->constr);
	TupleConstr *tupleConstr = tupleDesc->constr;

	ConstrCheck *check = tupleConstr->check;
	int			i;

	for (i = 0; i < tupleConstr->num_check; i++)
	{
		char	   *name = check[i].ccname;
		Node	   *expr;
		bool		found_whole_row;

		/* ignore if the constraint is non-inheritable */
		if (check[i].ccnoinherit)
			continue;

		/* Adjust Vars to match new table's column numbering */
		expr = map_variable_attnos(stringToNode(check[i].ccbin), 1, 0, attnos,
				tupleDesc->natts, &found_whole_row);

		/*
		* For the moment we have to reject whole-row variables. We
		* could convert them, if we knew the new table's rowtype OID,
		* but that hasn't been assigned yet.
		* This is copied from the function, MergeAttributes, so add it here too
		*/
		if (found_whole_row)
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot convert whole-row table reference"),
				 errdetail("Constraint \"%s\" contains a whole-row reference to table \"%s\".",
					name, relName)));

		CookedConstraint *cooked;

		cooked = (CookedConstraint *) palloc(sizeof(CookedConstraint));
		cooked->contype = CONSTR_CHECK;
		cooked->name = pstrdup(name);
		cooked->attnum = 0; /* not used for constraints */
		cooked->expr = expr;
		cooked->skip_validation = false;
		cooked->is_local = false;
		cooked->inhcount = 1;
		cooked->is_no_inherit = false;
		*constraints = lappend(*constraints, cooked);
	}
}

/*
 * SetSchemaAndConstraints
 *		Find the attribute names and constraints and add them to the provided lists
 *
 * This is only used for partition tables because it sets cooked constraints
 * In the list it makes, it skips dropped columns
 */
void SetSchemaAndConstraints(RangeVar *rangeVar, List **schema, List **constraints)
{
	Relation relation;
	TupleDesc	tupleDesc;
	AttrNumber *attrNumbers;

	relation = heap_openrv(rangeVar, ShareUpdateExclusiveLock);
	/* TODO: check if the relation is part of an inheritance hierarchy
	 * if it is not, we cannot make assumptions about the Constraint being cooked
	 */
	ATSimplePermissions(relation, ATT_TABLE);

	char *relname = RelationGetRelationName(relation);

	tupleDesc = RelationGetDescr(relation);

	/*
	* attrNumbers[] will contain the child-table attribute numbers for the
	* attributes of this parent table.  (They are not the same for
	* parent after the first one, nor if we have dropped columns.)
	*/
	attrNumbers = (AttrNumber *)
		palloc0(tupleDesc->natts * sizeof(AttrNumber));

	SetSchema(tupleDesc, schema, &attrNumbers);
	if (tupleDesc->constr && tupleDesc->constr->num_check > 0)
	{
		SetConstraints(tupleDesc, relname, constraints, attrNumbers);
	}

	pfree(attrNumbers);
	heap_close(relation, NoLock);
}
