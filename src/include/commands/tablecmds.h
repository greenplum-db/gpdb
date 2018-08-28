/*-------------------------------------------------------------------------
 *
 * tablecmds.h
 *	  prototypes for tablecmds.c.
 *
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/commands/tablecmds.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef TABLECMDS_H
#define TABLECMDS_H

#include "access/attnum.h"
#include "catalog/dependency.h"
#include "catalog/gp_policy.h"
#include "executor/executor.h"
#include "executor/tuptable.h"
#include "nodes/execnodes.h"
#include "access/htup.h"
#include "nodes/parsenodes.h"
#include "nodes/relation.h"
#include "parser/parse_node.h"
#include "storage/lock.h"
#include "utils/relcache.h"

/* Struct describing one new constraint to check in ALTER Phase 3 scan.
 *
 * Note: new NOT NULL constraints are handled differently.
 * Also note: This structure is shared only to allow collaboration with
 * partitioning-related functions in cdbpartition.c.  Most items like this
 * are local to tablecmds.c.
 */
typedef struct NewConstraint
{
	char	   *name;			/* Constraint name, or NULL if none */
	ConstrType	contype;		/* CHECK or FOREIGN */
	Oid			refrelid;		/* PK rel, if FOREIGN */
	Oid			refindid;		/* OID of PK's index, if FOREIGN */
	Oid			conid;			/* OID of pg_constraint entry, if FOREIGN */
	Node	   *qual;			/* Check expr or FkConstraint struct */
	List	   *qualstate;		/* Execution state for CHECK */
} NewConstraint;

extern const char *synthetic_sql;

extern Oid	DefineRelation(CreateStmt *stmt, char relkind, Oid ownerId, char relstorage, bool dispatch, bool useChangedOpts, GpPolicy *intoPolicy);

extern void	DefineExternalRelation(CreateExternalStmt *stmt);

extern void	DefinePartitionedRelation(CreateStmt *stmt, Oid reloid);

extern void EvaluateDeferredStatements(List *deferredStmts);

extern void RemoveRelations(DropStmt *drop);

extern bool RelationToRemoveIsTemp(const RangeVar *relation, DropBehavior behavior);

extern Oid	AlterTableLookupRelation(AlterTableStmt *stmt, LOCKMODE lockmode);

extern void AlterTable(Oid relid, LOCKMODE lockmode, AlterTableStmt *stmt);

extern LOCKMODE AlterTableGetLockLevel(List *cmds);

extern void ATExecChangeOwner(Oid relationOid, Oid newOwnerId, bool recursing, LOCKMODE lockmode);

extern void AlterTableInternal(Oid relid, List *cmds, bool recurse);

extern void AlterTableNamespace(AlterObjectSchemaStmt *stmt);

extern void AlterTableNamespaceInternal(Relation rel, Oid oldNspOid,
							Oid nspOid, ObjectAddresses *objsMoved);

extern void AlterRelationNamespaceInternal(Relation classRel, Oid relOid,
							   Oid oldNspOid, Oid newNspOid,
							   bool hasDependEntry,
							   ObjectAddresses *objsMoved);

extern void CheckTableNotInUse(Relation rel, const char *stmt);

extern void ExecuteTruncate(TruncateStmt *stmt);

extern void SetRelationHasSubclass(Oid relationId, bool relhassubclass);

extern void renameatt(RenameStmt *stmt);

extern void RenameConstraint(RenameStmt *stmt);

extern void RenameRelation(RenameStmt *stmt);

extern void RenameRelationInternal(Oid myrelid,
					   const char *newrelname);

extern void find_composite_type_dependencies(Oid typeOid,
								 Relation origRelation,
								 const char *origTypeName);

extern void check_of_type(HeapTuple typetuple);

extern void register_on_commit_action(Oid relid, OnCommitAction action);
extern void remove_on_commit_action(Oid relid);

extern void PreCommit_on_commit_actions(void);
extern void AtEOXact_on_commit_actions(bool isCommit);
extern void AtEOSubXact_on_commit_actions(bool isCommit,
							  SubTransactionId mySubid,
							  SubTransactionId parentSubid);

extern bool rel_is_parent(Oid relid);
extern bool rel_needs_long_lock(Oid relid);
extern Oid  rel_partition_get_master(Oid relid);

extern Oid get_settable_tablespace_oid(char *tablespacename);

extern List * MergeAttributes(List *schema, List *supers, bool istemp, bool isPartitioned,
			List **supOids, List **supconstr, int *supOidCount);

extern DistributedBy *make_dist_clause(Relation rel);

extern Oid transformFkeyCheckAttrs(Relation pkrel,
								   int numattrs, int16 *attnums,
								   Oid *opclasses);

extern void RangeVarCallbackOwnsTable(const RangeVar *relation,
						  Oid relId, Oid oldRelId, void *arg);

#endif   /* TABLECMDS_H */
