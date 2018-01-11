//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		gpdbwrappers.h
//
//	@doc:
//		Definition of GPDB function wrappers
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef GPDB_gpdbwrappers_H
#define GPDB_gpdbwrappers_H

#include "postgres.h"
#include "access/attnum.h"
#include "utils/faultinjector.h"
#include "parser/parse_coerce.h"

// fwd declarations
typedef struct SysScanDescData *SysScanDesc;
typedef int LOCKMODE;
struct TypeCacheEntry;
typedef struct NumericData *Numeric;
typedef struct HeapTupleData *HeapTuple;
struct PartitionNode;
typedef struct RelationData *Relation;
struct Value;
typedef struct tupleDesc *TupleDesc;
struct Query;
typedef struct ScanKeyData *ScanKey;
struct Bitmapset;
struct Plan;
struct ListCell;
struct TargetEntry;
struct Expr;
struct ExtTableEntry;
struct Uri;
struct CdbComponentDatabases;
struct StringInfoData;
typedef StringInfoData *StringInfo;
struct LogicalIndexes;
struct LogicalIndexInfo;
struct ParseState;
struct DefElem;
struct GpPolicy;
struct PartitionSelector;
struct SelectedParts;
struct Motion;
struct Var;
struct Const;
struct ArrayExpr;

namespace gpdb {

	// convert datum to bool
	bool FBoolFromDatum(Datum d);

	// convert bool to datum
	Datum DDatumFromBool(bool b);

	// convert datum to char
	char CCharFromDatum(Datum d);

	// convert char to datum
	Datum DDatumFromChar(char c);

	// convert datum to int8
	int8 CInt8FromDatum(Datum d);

	// convert int8 to datum
	Datum DDatumFromInt8(int8 i8);

	// convert datum to uint8
	uint8 UcUint8FromDatum(Datum d);

	// convert uint8 to datum
	Datum DDatumFromUint8(uint8 ui8);

	// convert datum to int16
	int16 SInt16FromDatum(Datum d);

	// convert int16 to datum
	Datum DDatumFromInt16(int16 i16);

	// convert datum to uint16
	uint16 UsUint16FromDatum(Datum d);

	// convert uint16 to datum
	Datum DDatumFromUint16(uint16 ui16);

	// convert datum to int32
	int32 IInt32FromDatum(Datum d);

	// convert int32 to datum
	Datum DDatumFromInt32(int32 i32);

	// convert datum to uint32
	uint32 UlUint32FromDatum(Datum d);

	// convert uint32 to datum
	Datum DDatumFromUint32(uint32 ui32);

	// convert datum to int64
	int64 LlInt64FromDatum(Datum d);

	// convert int64 to datum
	Datum DDatumFromInt64(int64 i64);

	// convert datum to uint64
	uint64 UllUint64FromDatum(Datum d);

	// convert uint64 to datum
	Datum DDatumFromUint64(uint64 ui64);

	// convert datum to oid
	Oid OidFromDatum(Datum d);

	// convert datum to generic object with pointer handle
	void *PvPointerFromDatum(Datum d);

	// convert datum to float4
	float4 FpFloat4FromDatum(Datum d);

	// convert datum to float8
	float8 DFloat8FromDatum(Datum d);

	// convert pointer to datum
	Datum DDatumFromPointer(const void *p);

	// does an aggregate exist with the given oid
	bool FAggregateExists(Oid oid);

	// add member to Bitmapset
	Bitmapset *PbmsAddMember(Bitmapset *a, int x);

	// create a copy of an object
	void *PvCopyObject(void *from);

	// datum size
	Size SDatumSize(Datum value, bool typByVal, int typLen);

	// expression type
	Oid OidExprType(Node *expr);

	// expression type modifier
	int32 IExprTypeMod(Node *expr);

	// extract nodes with specific tag from a plan tree
	List *PlExtractNodesPlan(Plan *pl, int nodeTag, bool descendIntoSubqueries);

	// extract nodes with specific tag from an expression tree
	List *PlExtractNodesExpression(Node *node, int nodeTag, bool descendIntoSubqueries);
	
	// intermediate result type of given aggregate
	Oid OidAggIntermediateResultType(Oid aggid);

	// replace Vars that reference JOIN outputs with references to the original
	// relation variables instead
	Query *PqueryFlattenJoinAliasVar(Query *pquery, gpos::ULONG ulQueryLevel);

	// is aggregate ordered
	bool FOrderedAgg(Oid aggid);
	
	// does aggregate have a preliminary function
	bool FAggHasPrelimFunc(Oid aggid);

	// does aggregate have a prelim or inverse prelim function
	bool FAggHasPrelimOrInvPrelimFunc(Oid aggid);

	// intermediate result type of given aggregate
	Oid OidAggregate(const char*szArg, Oid oidType);

	// array type oid
	Oid OidArrayType(Oid typid);

	// deconstruct array
	void DeconstructArray(struct ArrayType *array, Oid elmtype, int elmlen, bool elmbyval,
			char elmalign, Datum **elemsp, bool **nullsp, int *nelemsp);

	// attribute stats slot
	bool FGetAttrStatsSlot(HeapTuple statstuple, Oid atttype, int32 atttypmod, int reqkind,
			Oid reqop, Datum **values, int *nvalues, float4 **numbers, int *nnumbers);

	// free attribute stats slot
	void FreeAttrStatsSlot(Oid atttype, Datum *values, int nvalues, float4 *numbers, int nnumbers);

	// attribute statistics
	HeapTuple HtAttrStats(Oid relid, AttrNumber attnum);

	// function oids
	List *PlFunctionOids(void);

	// does a function exist with the given oid
	bool FFunctionExists(Oid oid);

	// is the given function strict
	bool FFuncStrict(Oid funcid);

	// stability property of given function
	char CFuncStability(Oid funcid);

	// data access property of given function
	char CFuncDataAccess(Oid funcid);

	// exec location property of given function
	char CFuncExecLocation(Oid funcid);

	// trigger name
	char *SzTriggerName(Oid triggerid);

	// trigger relid
	Oid OidTriggerRelid(Oid triggerid);

	// trigger funcid
	Oid OidTriggerFuncid(Oid triggerid);

	// trigger type
	int32 ITriggerType(Oid triggerid);

	// is trigger enabled
	bool FTriggerEnabled(Oid triggerid);

	// does trigger exist
	bool FTriggerExists(Oid oid);

	// does check constraint exist
	bool FCheckConstraintExists(Oid oidCheckConstraint);

	// check constraint name
	char *SzCheckConstraintName(Oid oidCheckConstraint);

	// check constraint relid
	Oid OidCheckConstraintRelid(Oid oidCheckConstraint);

	// check constraint expression tree
	Node *PnodeCheckConstraint(Oid oidCheckConstraint);

	// get the list of check constraints for a given relation
	List *PlCheckConstraint(Oid oidRel);

	// part constraint expression tree
	Node *PnodePartConstraintRel(Oid oidRel, List **pplDefaultLevels);

	// get the cast function for the specified source and destination types
	bool FCastFunc(Oid oidSrc, Oid oidDest, bool *is_binary_coercible, Oid *oidCastFunc, CoercionPathType *pathtype);
	
	// get type of operator
	unsigned int UlCmpt(Oid oidOp, Oid oidLeft, Oid oidRight);
	
	// get scalar comparison between given types
	Oid OidScCmp(Oid oidLeft, Oid oidRight, unsigned int ulCmpt);

	// get equality operator for given type
	Oid OidEqualityOp(Oid oidType);

	// get equality operator for given ordering op (i.e. < or >)
	Oid OidEqualityOpForOrderingOp(Oid opno, bool *reverse);
	
	// get ordering operator for given equality op (i.e. =)
	Oid OidOrderingOpForEqualityOp(Oid opno, bool *reverse);

	// function name
	char *SzFuncName(Oid funcid);

	// output argument types of the given function
	List *PlFuncOutputArgTypes(Oid funcid);

	// argument types of the given function
	List *PlFuncArgTypes(Oid funcid);

	// does a function return a set of rows
	bool FFuncRetset(Oid funcid);

	// return type of the given function
	Oid OidFuncRetType(Oid funcid);

	// commutator operator of the given operator
	Oid OidCommutatorOp(Oid opno);

	// inverse operator of the given operator
	Oid OidInverseOp(Oid opno);

	// function oid corresponding to the given operator oid
	RegProcedure OidOpFunc(Oid opno);

	// operator name
	char *SzOpName(Oid opno);

	// parts of a partitioned table
	bool FLeafPartition(Oid oid);

	// partition table has an external partition
	bool FHasExternalPartition(Oid oid);

	// find the oid of the root partition given partition oid belongs to
	Oid OidRootPartition(Oid oid);
	
	// partition attributes
	List *PlPartitionAttrs(Oid oid);

	// get partition keys and kinds ordered by partition level
	void GetOrderedPartKeysAndKinds(Oid oid, List **pkeys, List **pkinds);

	// parts of a partitioned table
	PartitionNode *PpnParts(Oid relid, int2 level, Oid parent, bool inctemplate, bool includesubparts);

	// keys of the relation with the given oid
	List *PlRelationKeys(Oid relid);

	// relid of a composite type
	Oid OidTypeRelid(Oid typid);

	// name of the type with the given oid
	char *SzTypeName(Oid typid);

	// number of GP segments
	int UlSegmentCountGP(void);

	// heap attribute is null
	bool FHeapAttIsNull(HeapTuple tup, int attnum);

	// free heap tuple
	void FreeHeapTuple(HeapTuple htup);

	// does an index exist with the given oid
	bool FIndexExists(Oid oid);

	// check if given oid is hashable internally in Greenplum Database
	bool FGreenplumDbHashable(Oid typid);

	// append an element to a list
	List *PlAppendElement(List *list, void *datum);

	// append an integer to a list
	List *PlAppendInt(List *list, int datum);

	// append an oid to a list
	List *PlAppendOid(List *list, Oid datum);

	// prepend a new element to the list
	List *PlPrependElement(void *datum, List *list);

	// prepend an integer to the list
	List *PlPrependInt(int datum, List *list);

	// prepend an oid to a list
	List *PlPrependOid(Oid datum, List *list);

	// concatenate lists
	List *PlConcat(List *list1, List *list2);

	// copy list
	List *PlCopy(List *list);

	// first cell in a list
	ListCell *PlcListHead(List *l);

	// last cell in a list
	ListCell *PlcListTail(List *l);

	// number of items in a list
	uint32 UlListLength(List *l);

	// return the nth element in a list of pointers
	void *PvListNth(List *list, int n);

	// return the nth element in a list of ints
	int IListNth(List *list, int n);

	// return the nth element in a list of oids
	Oid OidListNth(List *list, int n);

	// check whether the given oid is a member of the given list
	bool FMemberOid(List *list, Oid oid);

	// free list
	void FreeList(List *plist);
	
	// deep free of a list
	void FreeListDeep(List *plist);

	// if pplist is non-NULL, and *pplist is non-NULL then free the list and set
	// *pplist to NULL
	void FreeListAndNull(List **pplist);

	// is this a Gather motion
	bool FMotionGather(const Motion *pmotion);

	// does a partition table have an appendonly child
	bool FAppendOnlyPartitionTable(Oid rootOid);

	// does a multi-level partitioned table have uniform partitioning hierarchy
	bool FMultilevelPartitionUniform(Oid rootOid);

	// lookup type cache
	TypeCacheEntry *PtceLookup(Oid type_id, int flags);

	// create a value node for a string
	Value *PvalMakeString(char *str);

	// create a value node for an integer
	Value *PvalMakeInteger(long i);

	// create a bool constant
	Node *PnodeMakeBoolConst(bool value, bool isnull);

	// make a NULL constant of the given type
	Node *PnodeMakeNULLConst(Oid oidType);
	
	// create a new target entry
	TargetEntry *PteMakeTargetEntry(Expr *expr, AttrNumber resno, char *resname, bool resjunk);

	// create a new var node
	Var *PvarMakeVar(Index varno, AttrNumber varattno, Oid vartype, int32 vartypmod, Index varlevelsup);

	// memory allocation functions
	void *PvMemoryContextAllocImpl(MemoryContext context, Size size, const char* file, const char * func, int line);
	void *PvMemoryContextAllocZeroAlignedImpl(MemoryContext context, Size size, const char* file, const char * func, int line);
	void *PvMemoryContextAllocZeroImpl(MemoryContext context, Size size, const char* file, const char * func, int line);
	void *PvMemoryContextReallocImpl(void *pointer, Size size, const char* file, const char * func, int line);
	void *GPDBAlloc(Size size);
	void GPDBFree(void *ptr);

	// create a duplicate of the given string in the given memory context
	char *SzMemoryContextStrdup(MemoryContext context, const char *string);

	// similar to ereport for logging messages
	void GpdbEreportImpl(int xerrcode, int severitylevel, const char *xerrmsg, const char *xerrhint, const char *filename, int lineno, const char *funcname);
#define GpdbEreport(xerrcode, severitylevel, xerrmsg, xerrhint) \
	gpdb::GpdbEreportImpl(xerrcode, severitylevel, xerrmsg, xerrhint , __FILE__, __LINE__, PG_FUNCNAME_MACRO)

	// string representation of a node
	char *SzNodeToString(void *obj);

	// node representation from a string
	Node *Pnode(char *string);

	// return the default value of the type
	Node *PnodeTypeDefault(Oid typid);

	// convert numeric to double; if out of range, return +/- HUGE_VAL
	double DNumericToDoubleNoOverflow(Numeric num);

	// convert time-related datums to double for stats purpose
	double DConvertTimeValueToScalar(Datum datum, Oid typid);

	// convert network-related datums to double for stats purpose
	double DConvertNetworkToScalar(Datum datum, Oid typid);

	// is the given operator hash-joinable
	bool FOpHashJoinable(Oid opno);

	// is the given operator strict
	bool FOpStrict(Oid opno);

	// get input types for a given operator
	void GetOpInputTypes(Oid opno, Oid *lefttype, Oid *righttype);

	// does an operator exist with the given oid
	bool FOperatorExists(Oid oid);

	// fetch detoasted copies of toastable datatypes
	struct varlena *PvlenDetoastDatum(struct varlena * datum);

	// expression tree walker
	bool FWalkExpressionTree(Node *node, bool(*walker)(), void *context);

	// query or expression tree walker
	bool FWalkQueryOrExpressionTree(Node *node, bool(*walker)(), void *context, int flags);

	// modify a query tree
	Query *PqueryMutateQueryTree(Query *query, Node *(*mutator)(), void *context, int flags);

	// modify an expression tree
	Node *PnodeMutateExpressionTree(Node *node, Node *(*mutator)(), void *context);

	// modify a query or an expression tree
	Node *PnodeMutateQueryOrExpressionTree(Node *node, Node *(*mutator)(), void *context, int flags);

	// the part of PqueryMutateQueryTree that processes a query's rangetable
	List *PlMutateRangeTable(List *rtable, Node *(*mutator)(), void *context, int flags);

	// check whether the part with the given oid is the root of a partition table
	bool FRelPartIsRoot(Oid relid);
	
	// check whether the part with the given oid is an interior subpartition
	bool FRelPartIsInterior(Oid relid);
	
	// check whether table with the given oid is a regular table and not part of a partitioned table
	bool FRelPartIsNone(Oid relid);

	// check whether a relation is inherited
	bool FHasSubclassSlow(Oid oidRel);

    // check whether a relation has parquet children
    bool FHasParquetChildren(Oid oidRel);
    
    // return the distribution policy of a relation; if the table is partitioned
    // and the parts are distributed differently, return Random distribution
    GpPolicy *Pdistrpolicy(Relation rel);
    
    // return true if the table is partitioned and hash-distributed, and one of  
    // the child partitions is randomly distributed
    gpos::BOOL FChildPartDistributionMismatch(Relation rel);

    // return true if the table is partitioned and any of the child partitions
    // have a trigger of the given type
    gpos::BOOL FChildTriggers(Oid oid, int triggerType);

	// does a relation exist with the given oid
	bool FRelationExists(Oid oid);

	// extract all relation oids from the catalog
	List *PlRelationOids(void);

	// estimate the relation size using the real number of blocks and tuple density
	void EstimateRelationSize(Relation rel,	int32 *attr_widths,	BlockNumber *pages,	double *tuples);

	// close the given relation
	void CloseRelation(Relation rel);

	// return the logical indexes for a partitioned table
	LogicalIndexes *Plgidx(Oid oid);
	
	// return the logical info structure for a given logical index oid
	LogicalIndexInfo *Plgidxinfo(Oid rootOid, Oid indexOid);
	
	// return a list of index oids for a given relation
	List *PlRelationIndexes(Relation relation);

	// build an array of triggers for this relation
	void BuildRelationTriggers(Relation rel);

	// get relation with given oid
	Relation RelGetRelation(Oid relationId);

	// get external table entry with given oid
	ExtTableEntry *Pexttable(Oid relationId);

	// get external table entry with given oid
	List *PlExternalScanUriList(ExtTableEntry *ext, bool *isMasterOnlyP);

	// return the first member of the given targetlist whose expression is
	// equal to the given expression, or NULL if no such member exists
	TargetEntry *PteMember(Node *node, List *targetlist);

	// return a list of members of the given targetlist whose expression is
	// equal to the given expression, or NULL if no such member exists
	List *PteMembers(Node *node, List *targetlist);

	// check if two gpdb objects are equal
	bool FEqual(void *p1, void *p2);

	// does a type exist with the given oid
	bool FTypeExists(Oid oid);

	// check whether a type is composite
	bool FCompositeType(Oid typid);

	// get integer value from an Integer value node
	int IValue(Node *node);

	// parse external table URI
	Uri *PuriParseExternalTable(const char *szUri);
	
	// returns ComponentDatabases
	CdbComponentDatabases *PcdbComponentDatabases(void);

	// compare two strings ignoring case
	int IStrCmpIgnoreCase(const char *sz1, const char *sz2);

	// construct random segment map
	bool *RgfRandomSegMap(int total_primaries, int total_to_skip);

	// create an empty 'StringInfoData' & return a pointer to it
	StringInfo SiMakeStringInfo(void);

	// append the two given strings to the StringInfo object
	void AppendStringInfo(StringInfo str, const char *str1, const char *str2);

	// look for the given node tags in the given tree and return the index of
	// the first one found, or -1 if there are none
	int IFindNodes(Node *node, List *nodeTags);

	Node *PnodeCoerceToCommonType(ParseState *pstate, Node *pnode, Oid oidTargetType, const char *context);

	// replace any polymorphic type with correct data type deduced from input arguments
	bool FResolvePolymorphicType(int numargs, Oid *argtypes, char *argmodes, FuncExpr *call_expr);
	
	// hash a const value with GPDB's hash function
	int32 ICdbHash(Const *pconst, int iSegments);
	
	// hash a list of const values with GPDB's hash function
	int32 ICdbHashList(List *plConsts, int iSegments);
	
	// check permissions on range table 
	void CheckRTPermissions(List *plRangeTable);
	
	// get index operator family properties
	void IndexOpProperties(Oid opno, Oid opfamily, int *strategy, Oid *subtype);
	
	// get oids of families this operator belongs to
	List *PlScOpOpFamilies(Oid opno);
	
	// get oids of op classes for the index keys
	List *PlIndexOpFamilies(Oid oidIndex);

	// returns the result of evaluating 'pexpr' as an Expr. Caller keeps ownership of 'pexpr'
	// and takes ownership of the result 
	Expr *PexprEvaluate(Expr *pexpr, Oid oidResultType, int32 iTypeMod);
	
	// interpret the value of "With oids" option from a list of defelems
	bool FInterpretOidsOption(List *plOptions);
	
	// extract string value from defelem's value
	char *SzDefGetString(DefElem *pdefelem);

	// transform array Const to an ArrayExpr
	Expr *PexprTransformArrayConstToArrayExpr(Const *pConst);

	// transform array Const to an ArrayExpr
	Node *PnodeEvalConstExpressions(Node *node);

	// static partition selection given a PartitionSelector node
	SelectedParts *SpStaticPartitionSelection(PartitionSelector *ps);

	// simple fault injector used by COptTasks.cpp to inject GPDB fault
	FaultInjectorType_e OptTasksFaultInjector(FaultInjectorIdentifier_e identifier);

	// return the number of leaf partition for a given table oid
	gpos::ULONG UlLeafPartitions(Oid oidRelation);

	// Does the metadata cache need to be reset (because of a catalog
	// table has been changed?)
	bool FMDCacheNeedsReset(void);

	// functions for tracking ORCA memory consumption
	void *OptimizerAlloc(size_t size);

	void OptimizerFree(void *ptr);

	// returns true if a query cancel is requested in GPDB
	bool FAbortRequested(void);

} //namespace gpdb

#define ForEach(cell, l)	\
	for ((cell) = gpdb::PlcListHead(l); (cell) != NULL; (cell) = lnext(cell))

#define ForBoth(cell1, list1, cell2, list2)							\
	for ((cell1) = gpdb::PlcListHead(list1), (cell2) = gpdb::PlcListHead(list2);	\
		 (cell1) != NULL && (cell2) != NULL;						\
		 (cell1) = lnext(cell1), (cell2) = lnext(cell2))

#define ForEachWithCount(cell, list, counter) \
	for ((cell) = gpdb::PlcListHead(list), (counter)=0; \
	     (cell) != NULL; \
	     (cell) = lnext(cell), ++(counter))

#define ListMake1(x1) gpdb::PlPrependElement(x1, NIL)

#define ListMake2(x1,x2) gpdb::PlPrependElement(x1, ListMake1(x2))

#define ListMake1Int(x1) gpdb::PlPrependInt(x1, NIL)

#define ListMake1Oid(x1) gpdb::PlPrependOid(x1, NIL)
#define ListMake2Oid(x1,x2) gpdb::PlPrependOid(x1, ListMake1Oid(x2))

#define LInitial(l) lfirst(gpdb::PlcListHead(l))

#define LInitialOID(l) lfirst_oid(gpdb::PlcListHead(l))

#define Palloc0Fast(sz) \
	( MemSetTest(0, (sz)) ? \
		gpdb::PvMemoryContextAllocZeroAlignedImpl(CurrentMemoryContext, (sz), __FILE__, PG_FUNCNAME_MACRO, __LINE__) : \
		gpdb::PvMemoryContextAllocZeroImpl(CurrentMemoryContext, (sz), __FILE__, PG_FUNCNAME_MACRO, __LINE__))

#ifdef __GNUC__

/* With GCC, we can use a compound statement within an expression */
#define NewNode(size, tag) \
({	Node   *_result; \
	AssertMacro((size) >= sizeof(Node));		/* need the tag, at least */ \
	_result = (Node *) Palloc0Fast(size); \
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

#define NewNode(size, tag) \
( \
	AssertMacro((size) >= sizeof(Node)),		/* need the tag, at least */ \
	newNodeMacroHolder = (Node *) Palloc0Fast(size), \
	newNodeMacroHolder->type = (tag), \
	newNodeMacroHolder \
)
#endif   // __GNUC__

#define MakeNode(_type_) 		((_type_ *) NewNode(sizeof(_type_),T_##_type_))

#define PStrDup(str) gpdb::SzMemoryContextStrdup(CurrentMemoryContext, (str))

#endif // !GPDB_gpdbwrappers_H

// EOF
