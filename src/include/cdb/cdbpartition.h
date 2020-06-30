/*--------------------------------------------------------------------------
 *
 * cdbpartition.h
 *	 Definitions and API functions for cdbpartition.c
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpartition.h
 *
 *--------------------------------------------------------------------------
 */
#ifndef CDBPARTITION_H
#define CDBPARTITION_H

#include "catalog/gp_policy.h"
#include "catalog/pg_partition.h"
#include "catalog/pg_partition_rule.h"
#include "nodes/execnodes.h"
#include "nodes/relation.h"
#include "optimizer/clauses.h"
#include "optimizer/plancat.h"
#include "utils/hsearch.h"
#include "utils/relcache.h"

typedef enum
{
	PART_STATUS_NONE,	/* a regular table */
	PART_STATUS_ROOT,	/* a partitioned table */
	PART_STATUS_INTERIOR,	/* a non-leaf part of a partitioned table */
	PART_STATUS_LEAF	/* a leaf part of a partitioned table */
} PartStatus;

typedef struct LogicalIndexes
{
	int			numLogicalIndexes;
	LogicalIndexInfo	**logicalIndexInfo;
} LogicalIndexes;

extern bool rel_is_default_partition(Oid relid);

extern bool rel_is_partitioned(Oid relid);

extern List *rel_partition_key_attrs(Oid relid);

extern List *rel_partition_keys_ordered(Oid relid);

extern void rel_partition_keys_kinds_ordered(Oid relid, List **pkeys, List **pkinds);

extern bool rel_has_external_partition(Oid relid);

extern bool rel_has_appendonly_partition(Oid relid);

extern bool rel_is_child_partition(Oid relid);

extern bool rel_is_interior_partition(Oid relid);

extern bool rel_is_leaf_partition(Oid relid);

extern bool rel_partitioning_is_uniform(Oid rootOid);

extern PartStatus rel_part_status(Oid relid);

extern List *
cdb_exchange_part_constraints(Relation table, Relation part, Relation cand, 
							  bool validate, bool is_split, AlterPartitionCmd *pc);

extern PartitionByType
char_to_parttype(char c);

extern int
del_part_template(Oid rootrelid, int16 parlevel, Oid parent);

extern void
add_part_to_catalog(Oid relid, PartitionBy *pby, bool bTemplate_Only);

extern AttrNumber 
max_partition_attr(PartitionNode *pn);

extern List *
get_partition_attrs(PartitionNode *pn);

extern int 
num_partition_levels(PartitionNode *pn);

extern PartitionRule *ruleMakePartitionRule(HeapTuple tuple);

extern Partition *partMakePartition(HeapTuple tuple);

extern List *
all_partition_relids(PartitionNode *pn);

extern Node *
get_relation_part_constraints(Oid rootOid, List **defaultLevels);

extern List *
all_prule_relids(PartitionRule *prule);

extern PgPartRule *get_part_rule1(Relation rel, AlterPartitionId *pid,
			   bool bExistError, bool bMustExist,
			   int *pSearch,
			   PartitionNode *pNode,
			   char *relname, PartitionNode **ppNode);

extern PgPartRule *get_part_rule(Relation rel, AlterPartitionId *pid,
			  bool bExistError, bool bMustExist,
			  int *pSearch,
			  bool inctemplate);


extern char *
rel_get_part_path_pretty(Oid relid, char *separator, char *lastsep);

extern bool 
partition_policies_equal(GpPolicy *p, PartitionNode *pn);

/* RelationBuildPartitionDesc is built from get_parts */
extern PartitionNode *get_parts(Oid relid, int16 level, Oid parent, bool inctemplate,
		  bool includesubparts);

extern PartitionNode *RelationBuildPartitionDesc(Relation rel,
												 bool inctemplate);
extern PartitionNode *RelationBuildPartitionDescByOid(Oid relid,
												 bool inctemplate);

extern Oid 
rel_partition_get_root(Oid relid);

extern Oid  
rel_partition_get_master(Oid relid);

extern char *
ChoosePartitionName(const char *tablename, int partDepth, const char *partname, Oid nspace);

extern Oid selectPartition(PartitionNode *partnode, Datum *values,
						   bool *isnull, TupleDesc tupdesc,
						   PartitionAccessMethods *accessMethods);

extern Node *atpxPartAddList(Relation rel, 
							 bool is_split,
							 List *colencs,
							 PartitionNode *pNode, 
							 char *partName,
							 bool isDefault,
							 PartitionElem *pelem,
							 PartitionByType part_type,
							 PgPartRule* par_prule,
							 char *lrelname,
							 bool bSetTemplate,
							 Oid ownerid);
extern List *
atpxDropList(Relation rel, PartitionNode *pNode);

extern void 
exchange_part_rule(Oid oldrelid, Oid newrelid);

extern void
exchange_permissions(Oid oldrelid, Oid newrelid);

extern List *
atpxRenameList(PartitionNode *pNode,
			   char *old_parentname, const char *new_parentname, int *skipped);

extern List *
basic_AT_oids(Relation rel, AlterTableCmd *cmd);

extern AlterTableCmd *basic_AT_cmd(AlterTableCmd *cmd);
extern bool can_implement_dist_on_part(Relation rel, DistributedBy *dist);
extern bool is_exchangeable(Relation rel, Relation oldrel, Relation newrel, bool fthrow);

extern void
fixCreateStmtForPartitionedTable(CreateStmt *stmt);

extern void index_check_partitioning_compatible(Relation rel, AttrNumber *indattr, Oid *exclops, int nidxatts, bool primary);

extern List *
selectPartitionMulti(PartitionNode *partnode, Datum *values, bool *isnull,
				 TupleDesc tupdesc, PartitionAccessMethods *accessMethods);

extern void AddPartitionEncoding(Oid relid, Oid paroid, AttrNumber attnum,
								 bool deflt, List *encoding);
extern void RemovePartitionEncodingByRelid(Oid relid);
extern void RemovePartitionEncodingByRelidAttribute(Oid relid, AttrNumber attnum);
extern Datum *get_partition_encoding_attoptions(Relation rel, Oid paroid);

extern LogicalIndexes * BuildLogicalIndexInfo(Oid relid);
extern Oid getPhysicalIndexRelid(Relation partRel, LogicalIndexInfo *iInfo);

extern LogicalIndexInfo *logicalIndexInfoForIndexOid(Oid rootOid, Oid indexOid);

extern void InsertPidIntoDynamicTableScanInfo(EState *estate, int32 index, Oid partOid, int32 selectorId);

extern bool isPartitionSelected(EState *estate, int index, Oid partOid);

extern char *
DebugPartitionOid(Datum *elements, int n);

extern List *
all_leaf_partition_relids(PartitionNode *pn);

extern List *
all_interior_partition_relids(PartitionNode *pn);

extern List *rel_get_part_path1(Oid relid);

extern int
countLeafPartTables(Oid rootOid);

extern void
findPartitionMetadataEntry(List *partsMetadata, Oid partOid, PartitionNode **partsAndRules,
							PartitionAccessMethods **accessMethods);

extern PartitionRule*
get_next_level_matched_partition(PartitionNode *partnode, Datum *values, bool *isnull,
								TupleDesc tupdesc, PartitionAccessMethods *accessMethods, Oid exprTypid);

#endif   /* CDBPARTITION_H */
