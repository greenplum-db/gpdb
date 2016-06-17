/*-------------------------------------------------------------------------
 *
 * tablecmds.h
 *	  prototypes for tablecmds.c.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/commands/tablecmds.h,v 1.37 2008/01/30 19:46:48 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TABLECMDS_H
#define TABLECMDS_H

#include "access/attnum.h"
#include "catalog/gp_policy.h"
#include "executor/executor.h"
#include "executor/tuptable.h"
#include "nodes/execnodes.h"
#include "nodes/parsenodes.h"
#include "nodes/relation.h"
#include "parser/parse_node.h"

/* Struct describing one new constraint to check in ALTER Phase 3 scan.
 *
 * Note: new NOT NULL constraints are handled differently.
 * Also note: This structure is shared only to allow colloboration with
 * partitioning-related functions in cdbpartition.c.  Most items like this
 * are local to tablecmds.c.
 */
typedef struct NewConstraint
{
	char	   *name;			/* Constraint name, or NULL if none */
	ConstrType	contype;		/* CHECK or FOREIGN */
	Oid			refrelid;		/* PK rel, if FOREIGN */
	Oid			conid;			/* OID of pg_constraint entry, if FOREIGN */
	Node	   *qual;			/* Check expr or FkConstraint struct */
	List	   *qualstate;		/* Execution state for CHECK */
} NewConstraint;

extern const char *synthetic_sql;

extern Oid	DefineRelation(CreateStmt *stmt, char relkind, char relstorage);

extern void	DefineExternalRelation(CreateExternalStmt *stmt);

extern void DefineForeignRelation(CreateForeignStmt *createForeignStmt);

extern void	DefinePartitionedRelation(CreateStmt *stmt, Oid reloid);

extern void EvaluateDeferredStatements(List *deferredStmts);

extern bool RemoveRelation(const RangeVar *relation, DropBehavior behavior,
						   DropStmt *stmt /* MPP */, char relkind);

extern bool RelationToRemoveIsTemp(const RangeVar *relation, DropBehavior behavior);

extern void AlterTable(AlterTableStmt *stmt);

extern void ATExecChangeOwner(Oid relationOid, Oid newOwnerId, bool recursing);

extern void AlterTableInternal(Oid relid, List *cmds, bool recurse);

extern void AlterTableNamespace(RangeVar *relation, const char *newschema);

extern void AlterRelationNamespaceInternal(Relation classRel, Oid relOid,
							   Oid oldNspOid, Oid newNspOid,
							   bool hasDependEntry);

extern void CheckTableNotInUse(Relation rel, const char *stmt);

extern void ExecuteTruncate(TruncateStmt *stmt);

extern void renameatt(Oid myrelid,
		  const char *oldattname,
		  const char *newattname,
		  bool recurse,
		  bool recursing);

extern void renamerel(Oid myrelid,
		  const char *newrelname,
		  ObjectType reltype,
		  RenameStmt *stmt /* MPP */);

extern void find_composite_type_dependencies(Oid typeOid,
											 const char *origTblName,
											 const char *origTypeName);

extern void find_composite_type_dependencies(Oid typeOid,
								 const char *origTblName,
								 const char *origTypeName);

extern void change_varattnos_of_a_varno(Node *node, const AttrNumber *newattno, Index varno);


extern void register_on_commit_action(Oid relid, OnCommitAction action);
extern void remove_on_commit_action(Oid relid);

extern void PreCommit_on_commit_actions(void);
extern void AtEOXact_on_commit_actions(bool isCommit);
extern void AtEOSubXact_on_commit_actions(bool isCommit,
							  SubTransactionId mySubid,
							  SubTransactionId parentSubid);
extern PartitionNode *RelationBuildPartitionDesc(Relation rel,
												 bool inctemplate);
extern PartitionNode *RelationBuildPartitionDescByOid(Oid relid,
												 bool inctemplate);

extern bool rel_needs_long_lock(Oid relid);
extern Oid  rel_partition_get_master(Oid relid);

extern Oid get_settable_tablespace_oid(char *tablespacename);

extern List *
MergeAttributes(List *schema, List *supers, bool istemp, bool isPartitioned,
				List **supOids, List **supconstr, int *supOidCount, GpPolicy *policy);
extern List *make_dist_clause(Relation rel);

extern Oid transformFkeyCheckAttrs(Relation pkrel,
								   int numattrs, int16 *attnums,
								   Oid *opclasses);
#endif   /* TABLECMDS_H */
