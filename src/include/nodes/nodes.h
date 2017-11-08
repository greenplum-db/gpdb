/*-------------------------------------------------------------------------
 *
 * nodes.h
 *	  Definitions for tagged nodes.
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/nodes/nodes.h,v 1.215 2008/11/22 22:47:06 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODES_H
#define NODES_H

/*
 * The first field of every node is NodeTag. Each node created (with makeNode)
 * will have one of the following tags as the value of its first field.
 *
 * Note that the numbers of the node tags are not contiguous. We left holes
 * here so that we can add more tags without changing the existing enum's.
 * (Since node tag numbers never exist outside backend memory, there's no
 * real harm in renumbering, it just costs a full rebuild ...)
 */
typedef enum NodeTag
{
	T_Invalid = 0,

	/*
	 * TAGS FOR EXECUTOR NODES (execnodes.h)
	 */
	T_IndexInfo = 10,
	T_ExprContext,
	T_ProjectionInfo,
	T_JunkFilter,
	T_ResultRelInfo,
	T_EState,
	T_TupleTableSlot,
	T_CdbProcess,
	T_Slice,
	T_SliceTable,
	T_CursorPosInfo,
	T_ShareNodeEntry,
	T_PartitionState,
	T_QueryDispatchDesc,
	T_OidAssignment,

	/*
	 * TAGS FOR PLAN NODES (plannodes.h)
	 */
	T_Plan = 100,
	T_Scan,
	T_Join,

	/* Real plan node starts below.  Scan and Join are "Virtal nodes",
	 * It will take the form of IndexScan, SeqScan, etc.
	 * CteScan will take the form of SubqueryScan.
	 */
	T_Result,
	T_Plan_Start = T_Result,
	T_Append,
	T_RecursiveUnion,
	T_Sequence,
	T_BitmapAnd,
	T_BitmapOr,
	T_SeqScan,
	T_ExternalScan,
	T_AppendOnlyScan,
	T_AOCSScan,
	T_TableScan,
	T_DynamicTableScan,
	T_IndexScan,
	T_DynamicIndexScan,
	T_BitmapIndexScan,
	T_BitmapHeapScan,
	T_BitmapAppendOnlyScan,
	T_BitmapTableScan,
	T_TidScan,
	T_SubqueryScan,
	T_FunctionScan,
	T_TableFunctionScan,
	T_ValuesScan,
	T_CteScan,
	T_WorkTableScan,
	T_NestLoop,
	T_MergeJoin,
	T_HashJoin,
	T_Material,
	T_Sort,
	T_Agg,
	T_WindowAgg,
	T_Unique,
	T_Hash,
	T_SetOp,
	T_Limit,
	T_Motion,
	T_ShareInputScan,
	T_Repeat,
	T_DML,
	T_SplitUpdate,
	T_RowTrigger,
	T_AssertOp,
	T_PartitionSelector,
	T_Plan_End,
	/* this one isn't a subclass of Plan: */
	T_PlanInvalItem,

	/*
	 * TAGS FOR PLAN STATE NODES (execnodes.h)
	 *
	 * These should correspond one-to-one with Plan node types.
	 */
	T_PlanState = 200,
	T_ScanState,
	T_JoinState,

	/* Real plan node starts below.  Scan and Join are "Virtal nodes",
	 * It will take the form of IndexScan, SeqScan, etc.
	 */
	T_ResultState,
	T_AppendState,
	T_RecursiveUnionState,
	T_SequenceState,
	T_BitmapAndState,
	T_BitmapOrState,
	T_SeqScanState,
	T_AppendOnlyScanState,
	T_AOCSScanState,
	T_TableScanState,
	T_DynamicTableScanState,
	T_ExternalScanState,
	T_IndexScanState,
	T_DynamicIndexScanState,
	T_BitmapIndexScanState,
	T_BitmapHeapScanState,
	T_BitmapAppendOnlyScanState,
	T_BitmapTableScanState,
	T_TidScanState,
	T_SubqueryScanState,
	T_FunctionScanState,
	T_TableFunctionState,
	T_ValuesScanState,
	T_CteScanState,
	T_WorkTableScanState,
	T_NestLoopState,
	T_MergeJoinState,
	T_HashJoinState,
	T_MaterialState,
	T_SortState,
	T_AggState,
	T_UniqueState,
	T_HashState,
	T_SetOpState,
	T_LimitState,
	T_MotionState,
	T_ShareInputScanState,
	T_WindowState,
	T_RepeatState,
	T_DMLState,
	T_SplitUpdateState,
	T_RowTriggerState,
	T_AssertOpState,
	T_PartitionSelectorState,
	T_TupleDescNode,

	/*
	 * TAGS FOR PRIMITIVE NODES (primnodes.h)
	 */
	T_Alias = 300,
	T_RangeVar,
	T_Expr,
	T_Var,
	T_Const,
	T_Param,
	T_Aggref,
	T_WindowFunc,
	T_ArrayRef,
	T_FuncExpr,
	T_OpExpr,
	T_DistinctExpr,
	T_ScalarArrayOpExpr,
	T_BoolExpr,
	T_SubLink,
	T_SubPlan,
	T_AlternativeSubPlan,
	T_FieldSelect,
	T_FieldStore,
	T_RelabelType,
	T_CoerceViaIO,
	T_ArrayCoerceExpr,
	T_ConvertRowtypeExpr,
	T_CaseExpr,
	T_CaseWhen,
	T_CaseTestExpr,
	T_ArrayExpr,
	T_RowExpr,
	T_RowCompareExpr,
	T_CoalesceExpr,
	T_MinMaxExpr,
	T_XmlExpr,
	T_NullIfExpr,
	T_NullTest,
	T_BooleanTest,
	T_CoerceToDomain,
	T_CoerceToDomainValue,
	T_SetToDefault,
	T_CurrentOfExpr,
	T_TargetEntry,
	T_RangeTblRef,
	T_JoinExpr,
	T_FromExpr,
	T_IntoClause,
	T_Flow,
	T_WindowFrame,
	T_WindowFrameEdge,
	T_WindowKey,
	T_Grouping,
	T_GroupId,
    T_AggOrder,
	T_PercentileExpr,
	T_DMLActionExpr,
	T_PartSelectedExpr,
	T_PartDefaultExpr,
	T_PartBoundExpr,
	T_PartBoundInclusionExpr,
	T_PartBoundOpenExpr,
	T_PartListRuleExpr,
	T_PartListNullTestExpr,
	T_TableOidInfo,

	/*
	 * TAGS FOR EXPRESSION STATE NODES (execnodes.h)
	 *
	 * These correspond (not always one-for-one) to primitive nodes derivedO
	 * from Expr.
	 */
	T_ExprState = 400,
	T_GenericExprState,
	T_AggrefExprState,
	T_WindowFuncExprState,
	T_ArrayRefExprState,
	T_FuncExprState,
	T_ScalarArrayOpExprState,
	T_BoolExprState,
	T_SubPlanState,
	T_AlternativeSubPlanState,
	T_FieldSelectState,
	T_FieldStoreState,
	T_CoerceViaIOState,
	T_ArrayCoerceExprState,
	T_ConvertRowtypeExprState,
	T_CaseExprState,
	T_CaseWhenState,
	T_ArrayExprState,
	T_RowExprState,
	T_RowCompareExprState,
	T_CoalesceExprState,
	T_MinMaxExprState,
	T_XmlExprState,
	T_NullTestState,
	T_CoerceToDomainState,
	T_DomainConstraintState,
	T_WholeRowVarExprState,		/* will be in a more natural position in 9.3 */
	T_GroupingFuncExprState,
	T_PercentileExprState,
	T_PartSelectedExprState,
	T_PartDefaultExprState,
	T_PartBoundExprState,
	T_PartBoundInclusionExprState,
	T_PartBoundOpenExprState,
	T_PartListRuleExprState,
	T_PartListNullTestExprState,

	/*
	 * TAGS FOR PLANNER NODES (relation.h)
	 */
	T_PlannerInfo = 500,
	T_PlannerGlobal,
	T_RelOptInfo,
	T_IndexOptInfo,
	T_Path,
	T_AppendOnlyPath,
	T_AOCSPath,
	T_ExternalPath,
	T_IndexPath,
	T_BitmapHeapPath,
	T_BitmapAppendOnlyPath,
	T_BitmapTableScanPath,
	T_BitmapAndPath,
	T_BitmapOrPath,
	T_NestPath,
	T_MergePath,
	T_HashPath,
	T_TidPath,
	T_AppendPath,
	T_ResultPath,
	T_MaterialPath,
	T_UniquePath,
	T_CtePath,
	T_EquivalenceClass,
	T_EquivalenceMember,
	T_PathKey,
	T_RestrictInfo,
	T_InnerIndexscanInfo,
	T_PlaceHolderVar,
	T_SpecialJoinInfo,
	T_AppendRelInfo,
	T_PlaceHolderInfo,
	T_Partition,
	T_PartitionRule,
	T_PartitionNode,
	T_PgPartRule,
	T_SegfileMapNode,
	T_PlannerParamItem,

    /* Tags for MPP planner nodes (relation.h) */
    T_CdbMotionPath = 580,
    T_CdbRelDedupInfo,
    T_CdbRelColumnInfo,

	/*
	 * TAGS FOR MEMORY NODES (memnodes.h)
	 */
	T_MemoryContext = 600,
	T_AllocSetContext,
	T_MemoryAccount,

	/*
	 * TAGS FOR VALUE NODES (value.h)
	 */
	T_Value = 650,
	T_Integer,
	T_Float,
	T_String,
	T_BitString,
	T_Null,

	/*
	 * TAGS FOR LIST NODES (pg_list.h)
	 */
	T_List,
	T_IntList,
	T_OidList,

	/*
	 * TAGS FOR STATEMENT NODES (mostly in parsenodes.h)
	 */
	T_Query = 700,
	T_PlannedStmt,
	T_InsertStmt,
	T_DeleteStmt,
	T_UpdateStmt,
	T_SelectStmt,
	T_AlterTableStmt,
	T_AlterTableCmd,
	T_AlterDomainStmt,
	T_SetOperationStmt,
	T_GrantStmt,
	T_GrantRoleStmt,
	T_ClosePortalStmt,
	T_ClusterStmt,
	T_CopyStmt,
	T_CreateStmt,
	T_SingleRowErrorDesc,
	T_ExtTableTypeDesc,
	T_CreateExternalStmt,
	T_DefineStmt,
	T_DropStmt,
	T_TruncateStmt,
	T_CommentStmt,
	T_FetchStmt,
	T_IndexStmt,
	T_CreateFunctionStmt,
	T_AlterFunctionStmt,
	T_RemoveFuncStmt,
	T_DoStmt,
	T_RenameStmt,
	T_RuleStmt,
	T_NotifyStmt,
	T_ListenStmt,
	T_UnlistenStmt,
	T_TransactionStmt,
	T_ViewStmt,
	T_LoadStmt,
	T_CreateDomainStmt,
	T_CreatedbStmt,
	T_DropdbStmt,
	T_VacuumStmt,
	T_ExplainStmt,
	T_CreateSeqStmt,
	T_AlterSeqStmt,
	T_VariableSetStmt,
	T_VariableShowStmt,
	T_DiscardStmt,
	T_CreateTrigStmt,
	T_DropPropertyStmt,
	T_CreatePLangStmt,
	T_DropPLangStmt,
	T_CreateRoleStmt,
	T_AlterRoleStmt,
	T_DropRoleStmt,
	T_CreateQueueStmt,
	T_AlterQueueStmt,
	T_DropQueueStmt,
	T_CreateResourceGroupStmt,
	T_DropResourceGroupStmt,
	T_AlterResourceGroupStmt,
	T_LockStmt,
	T_ConstraintsSetStmt,
	T_ReindexStmt,
	T_CheckPointStmt,
	T_CreateSchemaStmt,
	T_AlterDatabaseStmt,
	T_AlterDatabaseSetStmt,
	T_AlterRoleSetStmt,
	T_CreateConversionStmt,
	T_CreateCastStmt,
	T_DropCastStmt,
	T_CreateOpClassStmt,
	T_CreateOpFamilyStmt,
	T_AlterOpFamilyStmt,
	T_RemoveOpClassStmt,
	T_RemoveOpFamilyStmt,
	T_PrepareStmt,
	T_ExecuteStmt,
	T_DeallocateStmt,
	T_DeclareCursorStmt,
	T_CreateTableSpaceStmt,
	T_DropTableSpaceStmt,
	T_AlterObjectSchemaStmt,
	T_AlterOwnerStmt,
	T_DropOwnedStmt,
	T_ReassignOwnedStmt,
	T_CompositeTypeStmt,
	T_CreateEnumStmt,
	T_AlterTSDictionaryStmt,
	T_AlterTSConfigurationStmt,
	T_PartitionBy,
	T_PartitionElem,
	T_PartitionRangeItem,
	T_PartitionBoundSpec,
	T_PartitionSpec,
	T_PartitionValuesSpec,
	T_AlterPartitionId,
	T_AlterPartitionCmd,
	T_InheritPartitionCmd,
	T_CreateFileSpaceStmt,
	T_FileSpaceEntry,
	T_DropFileSpaceStmt,
	T_TableValueExpr,
	T_DenyLoginInterval,
	T_DenyLoginPoint,
	T_AlterTypeStmt,
	T_CreateExtensionStmt,
	T_AlterExtensionStmt,
	T_AlterExtensionContentsStmt,
	T_SetDistributionCmd,

	/*
	 * TAGS FOR PARSE TREE NODES (parsenodes.h)
	 */
	T_A_Expr = 900,
	T_ColumnRef,
	T_ParamRef,
	T_A_Const,
	T_FuncCall,
	T_A_Star,
	T_A_Indices,
	T_A_Indirection,
	T_A_ArrayExpr,
	T_ResTarget,
	T_TypeCast,
	T_SortBy,
	T_WindowDef,
	T_RangeSubselect,
	T_RangeFunction,
	T_TypeName,
	T_ColumnDef,
	T_IndexElem,
	T_Constraint,
	T_DefElem,
	T_RangeTblEntry,
	T_GroupingClause,
	T_GroupingFunc,
	T_WindowClause,
	T_SortGroupClause,
	T_FkConstraint,
	T_PrivGrantee,
	T_FuncWithArgs,
	T_PrivTarget,
	T_CreateOpClassItem,
	T_InhRelation,
	T_FunctionParameter,
	T_LockingClause,
	T_RowMarkClause,
	T_XmlSerialize,
	T_WithClause,
	T_CommonTableExpr,
	T_ColumnReferenceStorageDirective,

	/*
	 * TAGS FOR REPLICATION GRAMMAR PARSE NODES (replnodes.h)
	 */
	T_IdentifySystemCmd,
	T_BaseBackupCmd,
	T_StartReplicationCmd,

	/*
	 * TAGS FOR RANDOM OTHER STUFF
	 *
	 * These are objects that aren't part of parse/plan/execute node tree
	 * structures, but we give them NodeTags anyway for identification
	 * purposes (usually because they are involved in APIs where we want to
	 * pass multiple object types through the same pointer).
	 */
	T_TriggerData = 950,		/* in commands/trigger.h */
	T_ReturnSetInfo,			/* in nodes/execnodes.h */
	T_InlineCodeBlock,			/* in nodes/parsenodes.h */
    T_HashBitmap,               /* in nodes/tidbitmap.h */
    T_StreamBitmap,             /* in nodes/tidbitmap.h */
	T_FormatterData,            /* in access/formatter.h */
	T_ExtProtocolData,          /* in access/extprotocol.h */
	T_ExtProtocolValidatorData, /* in access/extprotocol.h */
	T_SelectedParts,            /* in executor/nodePartitionSelector.h */
	T_CookedConstraint,			/* in catalog/heap.h */

    /* CDB: tags for random other stuff */
    T_CdbExplain_StatHdr = 1000,             /* in cdb/cdbexplain.c */

} NodeTag;

/*
 * The first field of a node of any type is guaranteed to be the NodeTag.
 * Hence the type of any node can be gotten by casting it to Node. Declaring
 * a variable to be of Node * (instead of void *) can also facilitate
 * debugging.
 */
typedef struct Node
{
	NodeTag		type;
} Node;

#define nodeTag(nodeptr)		(((Node*)(nodeptr))->type)

/*
 * newNode -
 *	  create a new node of the specified size and tag the node with the
 *	  specified tag.
 *
 * !WARNING!: Avoid using newNode directly. You should be using the
 *	  macro makeNode.  eg. to create a Query node, use makeNode(Query)
 *
 * Note: the size argument should always be a compile-time constant, so the
 * apparent risk of multiple evaluation doesn't matter in practice.
 */
#ifdef __GNUC__

/* With GCC, we can use a compound statement within an expression */
#define newNode(size, tag) \
({	Node   *_result; \
	AssertMacro((size) >= sizeof(Node));		/* need the tag, at least */ \
	_result = (Node *) palloc0fast(size); \
	_result->type = (tag); \
	_result; \
})
#else

/*
 *	There is no way to dereference the palloc'ed pointer to assign the
 *	tag, and also return the pointer itself, so we need a holder variable.
 *	Fortunately, this macro isn't recursive so we just define
 *	a global variable for this purpose.
 */
extern PGDLLIMPORT Node *newNodeMacroHolder;

#define newNode(size, tag) \
( \
	AssertMacro((size) >= sizeof(Node)),		/* need the tag, at least */ \
	newNodeMacroHolder = (Node *) palloc0fast(size), \
	newNodeMacroHolder->type = (tag), \
	newNodeMacroHolder \
)
#endif   /* __GNUC__ */


#define makeNode(_type_)		((_type_ *) newNode(sizeof(_type_),T_##_type_))
#define NodeSetTag(nodeptr,t)	(((Node*)(nodeptr))->type = (t))

#define IsA(nodeptr,_type_)		(nodeTag(nodeptr) == T_##_type_)

/* ----------------------------------------------------------------
 *					  extern declarations follow
 * ----------------------------------------------------------------
 */

/*
 * nodes/{outfuncs.c,print.c}
 */
extern char *nodeToString(void *obj);

/*
 * nodes/outfast.c. This special version of nodeToString is only used by serializeNode.
 * It's a quick hack that allocates 8K buffer for StringInfo struct through initStringIinfoSizeOf
 */
extern char *nodeToBinaryStringFast(void *obj, int * size);

extern Node *readNodeFromBinaryString(const char *str, int len);
/*
 * nodes/{readfuncs.c,read.c}
 */
extern void *stringToNode(char *str);

/*
 * nodes/copyfuncs.c
 */
extern void *copyObject(void *obj);

/*
 * nodes/equalfuncs.c
 */
extern bool equal(void *a, void *b);


/*
 * Typedefs for identifying qualifier selectivities and plan costs as such.
 * These are just plain "double"s, but declaring a variable as Selectivity
 * or Cost makes the intent more obvious.
 *
 * These could have gone into plannodes.h or some such, but many files
 * depend on them...
 */
typedef double Selectivity;		/* fraction of tuples a qualifier will pass */
typedef double Cost;			/* execution cost (in page-access units) */


/*
 * CmdType -
 *	  enums for type of operation represented by a Query or PlannedStmt
 *
 * This is needed in both parsenodes.h and plannodes.h, so put it here...
 */
typedef enum CmdType
{
	CMD_UNKNOWN,
	CMD_SELECT,					/* select stmt */
	CMD_UPDATE,					/* update stmt */
	CMD_INSERT,					/* insert stmt */
	CMD_DELETE,
	CMD_UTILITY,				/* cmds like create, destroy, copy, vacuum,
								 * etc. */
	CMD_NOTHING					/* dummy command for instead nothing rules
								 * with qual */
} CmdType;


/*
 * JoinType -
 *	  enums for types of relation joins
 *
 * JoinType determines the exact semantics of joining two relations using
 * a matching qualification.  For example, it tells what to do with a tuple
 * that has no match in the other relation.
 *
 * This is needed in both parsenodes.h and plannodes.h, so put it here...
 */
typedef enum JoinType
{
	/*
	 * The canonical kinds of joins according to the SQL JOIN syntax.
	 * Only these codes can appear in parser output (e.g., JoinExpr nodes).
	 */
	JOIN_INNER,					/* matching tuple pairs only */
	JOIN_LEFT,					/* pairs + unmatched LHS tuples */
	JOIN_FULL,					/* pairs + unmatched LHS + unmatched RHS */
	JOIN_RIGHT,					/* pairs + unmatched RHS tuples */

	/*
	 * Semijoins and anti-semijoins (as defined in relational theory) do
	 * not appear in the SQL JOIN syntax, but there are standard idioms for
	 * representing them (e.g., using EXISTS).  The planner recognizes these
	 * cases and converts them to joins.  So the planner and executor must
	 * support these codes.  NOTE: in JOIN_SEMI output, it is unspecified
	 * which matching RHS row is joined to.  In JOIN_ANTI output, the row
	 * is guaranteed to be null-extended.
     *
     * CDB: We no longer use JOIN_REVERSE_IN, JOIN_UNIQUE_OUTER or
     * JOIN_UNIQUE_INNER.  The definitions are retained in case they
     * might be referenced in the source code of user-defined
     * selectivity functions brought over from PostgreSQL.
	 */
	JOIN_SEMI,					/* 1 copy of each LHS row that has match(es) */
	JOIN_ANTI,					/* 1 copy of each LHS row that has no match */
	JOIN_LASJ_NOTIN,			/* Left Anti Semi Join with Not-In semantics:
									If any NULL values are produced by inner side,
									return no join results. Otherwise, same as LASJ */
	JOIN_REVERSE_IN,			/* at most one result per inner row */
	/*
	 * These codes are used internally in the planner, but are not supported
	 * by the executor (nor, indeed, by most of the planner).
	 */
	JOIN_UNIQUE_OUTER,			/* LHS path must be made unique */
	JOIN_UNIQUE_INNER			/* RHS path must be made unique */
	/*
	 * We might need additional join types someday.
	 */
} JoinType;

/*
 * OUTER joins are those for which pushed-down quals must behave differently
 * from the join's own quals.  This is in fact everything except INNER and
 * SEMI joins.  However, this macro must also exclude the JOIN_UNIQUE symbols
 * since those are temporary proxies for what will eventually be an INNER
 * join.
 *
 * Note: semijoins are a hybrid case, but we choose to treat them as not
 * being outer joins.  This is okay principally because the SQL syntax makes
 * it impossible to have a pushed-down qual that refers to the inner relation
 * of a semijoin; so there is no strong need to distinguish join quals from
 * pushed-down quals.  This is convenient because for almost all purposes,
 * quals attached to a semijoin can be treated the same as innerjoin quals.
 */
#define IS_OUTER_JOIN(jointype) \
	(((1 << (jointype)) & \
	  ((1 << JOIN_LEFT) | \
	   (1 << JOIN_FULL) | \
	   (1 << JOIN_RIGHT) | \
	   (1 << JOIN_ANTI) | \
	   (1 << JOIN_LASJ_NOTIN))) != 0)

/*
 * FlowType - kinds of tuple flows in parallelized plans.
 *
 * This enum is a MPP extension.
 */
typedef enum FlowType
{
	FLOW_UNDEFINED,		/* used prior to calculation of type of derived flow */
	FLOW_SINGLETON,		/* flow has single stream */
	FLOW_REPLICATED,	/* flow is replicated across IOPs */
	FLOW_PARTITIONED,	/* flow is partitioned across IOPs */
} FlowType;

/*
 * DispatchMethod - MPP dispatch method.
 *
 * There are currently three possibilties, an initial value of undetermined,
 * and a value for each of the ways the dispatch code implements.
 */
typedef enum DispatchMethod
{
	DISPATCH_UNDETERMINED = 0,	/* Used prior to determination. */
	DISPATCH_SEQUENTIAL,		/* Dispatch on entry postgres process only. */
	DISPATCH_PARALLEL			/* Dispatch on query executor and entry processes. */

} DispatchMethod;

/*
 * Inside the executor, if a caller to some data type manipulation functions
 * (e.g., int8inc()) is doing aggregate or window function work, we want to
 * avoid copying the input datum and just write directly over the input. This
 * isn't legal if the function is being used outside this context.
 */
#define IS_AGG_EXECUTION_NODE(node) \
	((IsA((Node *)(node), AggState) || IsA((Node *)(node), WindowState)) ? \
	 true : false)

/*
 * If the partIndex in Scan set to 0 then we don't have
 * any dynamic partition scanning
 */
#define INVALID_PART_INDEX 0

#endif   /* NODES_H */
