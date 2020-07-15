# Copyright (c) 2015, Pivotal Software, Inc. All Rights Reserved.

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include"
                    "${PROJECT_SOURCE_DIR}/libgpdbcost/include"
                    "${PROJECT_SOURCE_DIR}/libgpopt/include"
                    "${PROJECT_SOURCE_DIR}/libnaucrates/include")
# for the generated config.h file.
include_directories(${PROJECT_BINARY_DIR}/libgpos/include/)
# for the auto generated minidump test classes
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/)

# Some tests use C99 long long integer constants, but the C++ dialect may not
# explicitly include C99. We supress such errors here.
check_cxx_compiler_flag("-Wno-long-long"
                        COMPILER_HAS_WNO_LONG_LONG)
if (COMPILER_HAS_WNO_LONG_LONG)
    set_source_files_properties(src/unittest/dxl/statistics/CStatisticsTest.cpp
                                src/unittest/dxl/statistics/CBucketTest.cpp
                                src/unittest/dxl/statistics/CPointTest.cpp
                                src/unittest/dxl/statistics/CHistogramTest.cpp
                                src/unittest/dxl/statistics/CMCVTest.cpp
                                src/unittest/dxl/statistics/CJoinCardinalityTest.cpp
                                src/unittest/dxl/statistics/CFilterCardinalityTest.cpp
                                src/unittest/gpopt/base/CConstraintTest.cpp
                                src/unittest/gpopt/metadata/CPartConstraintTest.cpp
                                PROPERTIES COMPILE_FLAGS "-Wno-long-long")
endif()

option(ENABLE_EXTENDED_TESTS
       "Enable extended tests for fault-injection and timing that may take a long time to run."
       OFF)

# Convenience function to add the test specified by 'TEST_NAME' to the set of
# tests to be run by CTest.
function(add_orca_test TEST_NAME)
  # Unit tests have many hard-coded paths to test data in optimizer/data. We
  # explicitly set the WORKING_DIRECTORY so that the files can be found
  # properly.
  add_test(NAME gporca_test_${TEST_NAME}
           COMMAND gporca_test -U ${TEST_NAME}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

# minidump tests must follow rule:
# "CxxxTest: file1 file2; CxxxTest: file3 file4"
# each test case should end with ";", EXCEPT LAST ONE.
# comments with format /*...*/ are allowed at any place.
set(MDP_GROUPS "

CSemiJoinTest:
SemiJoin2InnerJoin SemiJoin2Select-1 SemiJoin2Select-2 SemiJoin2Select-EnforceSubplan
InSubqWithPrjListOuterRefNoInnerRefColumnIn InSubqWithPrjListOuterRefNoInnerRefConstIn
NestedInSubqWithPrjListOuterRefNoInnerRef  InEqualityJoin Correlated-SemiJoin
CorrelatedSemiJoin-True CorrelatedIN-LeftSemiJoin-True CorrelatedIN-LeftSemiNotIn-True
InSubqWithPrjListOuterRefNoInnerRefColumnPlusConstIn CorrelatedIN-LeftSemiJoin-Limit
CorrelatedLeftSemiNLJoinWithLimit PushFilterToSemiJoinLeftChild SubqOuterReferenceInClause;

CAntiSemiJoinTest:
AntiSemiJoin2Select-1 AntiSemiJoin2Select-2 NOT-IN-NotNullBoth NOT-IN-NullInner NOT-IN-NullOuter
Correlated-AntiSemiJoin CorrelatedAntiSemiJoin-True Correlated-LASJ-With-Outer-Col
Correlated-LASJ-With-Outer-Const Correlated-LASJ-With-Outer-Expr LeftOuter2InnerUnionAllAntiSemiJoin-Tpcds;

CPredicateTest:
ConvertBoolConstNullToConstTableFalseFilter 
TranslateOneTimeFilterConjunctQuals TranslateFilterDisjunctQuals TranslateFilterWithCTEAndTableScanIntoFilterAndOneTimeFilter
InferPredicates InferPredicatesForLimit InferPredicatesBCC-vc-vc InferPredicatesBCC-vc-txt
InferPredicatesBCC-txt-txt InferPredicatesBCC-oid-oid InferPredicatesBCC-vcpart-txt
OR-WithIsNullPred Int2Predicate IN-Numeric DeduplicatePredicates
AddEqualityPredicates AddPredsInSubqueries ExtractPredicateFromDisjWithComputedColumns
Join-With-Subq-Preds-1 Non-Hashjoinable-Pred Non-Hashjoinable-Pred-2 Factorized-Preds IN OR AvoidConstraintDerivationForLike
NLJ-DistCol-No-Broadcast NLJ-EqAllCol-No-Broadcast NLJ-EqDistCol-InEqNonDistCol-No-Broadcast
NLJ-InEqDistCol-EqNonDistCol-Redistribute CorrelatedNLJWithTrueCondition InferPredicatesInnerOfLOJ
InferredPredicatesConstraintSimplification NoPushdownPredicateWithCTEAnchor;

CLikeIDFTest:
LIKE-Pattern-green LIKE-Pattern-green-2 LIKE-Pattern-Empty Nested-Or-Predicates Join-IDF
LikePredStatsNotComparable PredStatsNotComparable ExtractPredicateFromDisj CollapseNot
IDF-NotNullConstant IDF-NullConstant INDF-NotNullConstant INDF-NullConstant NullConstant-INDF-Col
Join-INDF Join-INDF-NoBroadcast TimeTypeStatsNotComparable Join-INDF-Nulls-Not-Collocated;

CLeftOuterIndexApplyTest:
IndexApply-LeftOuter-NLJoin LOJ-IndexApply-MultiIndexes LOJ-IndexApply-Negative-NonEqual-Predicate
LOJ-IndexApply-DistKey-Multiple-Predicates LOJ-IndexApply-MasterOnly-Table LOJ-IndexApply-NonDistKey
LOJ-IndexApply-CompsiteKey-Equiv LOJ-IndexApply-CompsiteKey-NoMotion LOJ-IndexApply-MultiDistKeys-IndexKeys
LOJ-IndexApply-WithComplexPredicates LOJ-IndexApply-MultiDistKeys-WithComplexPreds
LOJ-IndexApply-MultiDistKey-MultiIndexKey-NoExtraFilter LOJ-IndexApply-MultiDistKey-MultiIndexKey
LOJ-IndexApply-MultiDistKeys-Bitmap LOJ-IndexApply-MultiDistKeys-Bitmap-WithComplexPreds
LOJ-DynBitmapIndex LOJ-DynBitmapIndexWithSelect LOJ-DynBtreeIndex LOJ-DynBtreeIndexWithSelect;

CTypeModifierTest:
TypeModifierColumn TypeModifierCast TypeModifierConst TypeModifierDoubleMappableConst TypeModifierArrayRef;

CIndexScanTest:
BTreeIndex-Against-InList BTreeIndex-Against-ScalarSubquery
IndexScan-AOTable IndexScan-DroppedColumns IndexScan-BoolTrue IndexScan-BoolFalse
IndexScan-Relabel IndexGet-OuterRefs LogicalIndexGetDroppedCols NewBtreeIndexScanCost
IndexScan-ORPredsNonPart IndexScan-ORPredsAOPart IndexScan-AndedIn;

CBitmapScanTest:
IndexedNLJBitmap BitmapIndex-ChooseHashJoin BitmapTableScan-AO-Btree-PickOnlyHighNDV
BitmapIndex-Against-InList BitmapTableScan-AO-Btree-PickIndexWithNoGap
BitmapTableScan-ComplexConjDisj BitmapTableScan-ConjDisjWithOuterRefs;

CIndexApplyTest:
IndexNLJ-IndexGet-OuterRef IndexApply_NestLoopWithNestParamTrue IndexApply1
IndexApply1-CalibratedCostModel IndexApply2 IndexApply3 IndexApply4
IndexApply-IndexCondDisjointWithHashedDistr IndexApply-IndexCondIntersectWithHashedDistr
IndexApply-IndexCondMatchHashedDistr IndexApply-IndexCondSubsetOfHashedDistr
IndexApply-IndexCondSupersetOfHashedDistr IndexApply-IndexOnMasterOnlyTable
IndexApply-PartTable IndexApply-Heterogeneous-NoDTS IndexApply-Heterogeneous-DTS
IndexApply-No-Motion-Below-Join IndexApply-Redistribute-Const-Table IndexApply-PartResolverExpand
IndexApply-InnerSelect-Basic IndexApply-InnerSelect-PartTable IndexApply-PartKey-Is-IndexKey
IndexApply-InnerSelect-Heterogeneous-DTS IndexApply-Heterogeneous-BothSidesPartitioned
IndexApply-InnerSelect-PartTable2 IndexApply-ForPartialIndex CastedScalarIf-On-Index-Key
IndexApply-MultiDistKey-WithComplexPreds IndexApply-MultiDistKeys-Bitmap
IndexApply-MultiDistKeys-Bitmap-WithComplexPreds;

CDynamicIndexScanTest:
DynamicIndexScan-Homogenous DynamicIndexScan-BoolTrue DynamicIndexScan-BoolFalse
DynamicIndexScan-Heterogenous-NoDTS DynamicIndexScan-Heterogenous-Union
DynamicIndexScan-Heterogenous-Overlapping DynamicIndexScan-Heterogenous
DynamicIndexScan-DefaultPartition DynamicIndexScan-DefaultPartition-2 DynamicIndexGetDroppedCols
DynamicIndexScan-Homogenous-UnsupportedConstraint DynamicIndexScan-Heterogenous-UnsupportedConstraint
DynamicIndexScan-Heterogenous-UnsupportedPredicate DynamicIndexScan-Heterogenous-PartSelectEquality
DynamicIndexScan-Heterogenous-PartSelectRange DynamicIndexScan-OpenEndedPartitions
DynamicIndexScan-Relabel DynamicIndexScan-DroppedColumns DynamicIndexGet-OuterRefs;

CIndexScanOnLeafTest:
IndexOnLeaf-AddNewPartitionToRootTableContainingHeterogenousIndex
IndexOnLeaf-AddPartitionToRootWithHomogenousIndex IndexOnLeaf-AddPartitionToRootWithHomogenousIndex
IndexOnLeaf-IndexOnPartitionsWithDifferentStorageTypes IndexOnLeaf-NonOverlappingHeterogenousIndex-ANDPredicate-AO
IndexOnLeaf-NonOverlappingHeterogenousIndex-ANDPredicate-HEAP IndexOnLeaf-NonOverlappingHomogenousIndexesOnRoot-ANDPredicate-AO
IndexOnLeaf-NonOverlappingHomogenousIndexesOnRoot-ORPredicate-AO IndexOnLeaf-OverlappingHeterogenousIndex-ANDPredicate-AO
IndexOnLeaf-OverlappingHeterogenousIndex-ANDPredicate-HEAP IndexOnLeaf-OverlappingHeterogenousIndex-ORPredicate-AO
IndexOnLeaf-OverlappingHomogenousIndexesOnRoot-ANDPredicate-AO IndexOnLeaf-OverlappingHomogenousIndexesOnRoot-AO
IndexOnLeaf-OverlappingHomogenousIndexesOnRoot-ORPredicate-AO IndexOnLeaf-SingleColumnHeterogenousIndexOnRoot-1-AO
IndexOnLeaf-SingleColumnHeterogenousIndexOnRoot-1-HEAP IndexOnLeaf-SingleColumnHeterogenousIndexOnRoot-2-AO
IndexOnLeaf-SingleColumnHeterogenousIndexOnRoot-2-HEAP NonOverlappingHomogenousIndexesOnRoot-ANDPredicate-HEAP
OverlappingHomogenousIndexesOnRoot-ANDPredicate-HEAP OverlappingHomogenousIndexesOnRoot-HEAP
SingleColumnHomogenousIndexOnRoot-AO SingleColumnHomogenousIndexOnRoot-HEAP;

CStatsTest:
Stat-Derivation-Leaf-Pattern MissingBoolColStats JoinColWithOnlyNDV UnsupportedStatsPredicate
StatsFilter-AnyWithNewColStats EquiJoinOnExpr-Supported EquiJoinOnExpr-Unsupported;

CICGMiscTest:
BroadcastSkewedHashjoin OrderByNullsFirst ConvertHashToRandomSelect ConvertHashToRandomInsert HJN-DeeperOuter CTAS CTAS-Random CheckAsUser
ProjectRepeatedColumn1 ProjectRepeatedColumn2 NLJ-BC-Outer-Spool-Inner Self-Comparison Self-Comparison-Nullable
SelectCheckConstraint ExpandJoinOrder SelectOnBpchar EqualityJoin EffectsOfJoinFilter InnerJoin-With-OuterRefs
UDA-AnyElement-1 UDA-AnyElement-2 Project-With-NonScalar-Func SixWayDPv2 Join-Varchar-Equality;

CJoinPredTest:
MultipleDampedPredJoinCardinality MultipleIndependentPredJoinCardinality MultiDistKeyJoinCardinality
MultiDistKeyWithOtherPredsJoinCardinality NoDistKeyMultiPredJoinCardinality OneDistKeyMultiPredJoinCardinality;

CArrayCmpTest:
ArrayConcat ArrayRef FoldedArrayCmp IN-ArrayCmp NOT-IN-ArrayCmp ArrayCmpAll UDA-AnyArray InClauseWithMCV
CastedInClauseWithMCV FilterScalarCast IN-Nulls-ArrayCmpAny ArrayCmp-IN-ManyElements;

CProjectTest:
ProjectWithConstant ProjectWithTextConstant ProjectSetFunction Equivalence-class-project-over-LOJ;

CPartTbl1Test:
NoPartConstraint-WhenNoDefaultPartsAndIndices
PartConstraint-WhenIndicesAndNoDefaultParts PartConstraint-WithOnlyDefaultPartInfo
PartConstraint-WhenDefaultPartsAndIndices CorrelatedNLJ-PartSelector-Subplan
DonotPushPartConstThruLimit Select-Over-PartTbl
PartSelectorOnJoinSide PartSelectorOnJoinSide2;

CPartTbl2Test:
Part-Selection-IN Part-Selection-NOT-IN
Part-Selection-ConstArray-1 Part-Selection-ConstArray-2 PartTbl-WindowFunction
PartTbl-MultiWayJoin PartTbl-AsymmetricRangePredicate PartTbl-NEqPredicate PartTbl-SQExists;

CPartTbl3Test:
PartTbl-SQNotExists PartTbl-SQAny PartTbl-SQAll PartTbl-SQScalar PartTbl-HJ3
PartTbl-HJ4 PartTbl-HJ5 PartTbl-JoinOverGbAgg PartTbl-JoinOverGbAgg-2
PartTbl-NonConstSelect PartTbl-VolatileFunc PartTbl-DateTime
PartTbl-ComplexRangePredicate-DefaultPart
PartTbl-ComplexRangePredicate-NoDefaultPart PartTbl-LASJ;

CPartTbl4Test:
PartTbl-Relabel-Equality PartTbl-Relabel-Range PartTbl-DisablePartSelection
PartTbl-DisablePartSelectionJoin PartTbl-JoinOverUnion-1
PartTbl-JoinOverUnion-2 PartTbl-JoinOverIntersect PartTbl-JoinOverExcept
PartTbl-RangeJoinPred PartTbl-ArrayIn PartTbl-ArrayCoerce PartTbl-Disjunction
PartTbl-ComplexPredicate1 PartTbl-ComplexPredicate2 PartTbl-ComplexPredicate3
PartTbl-ComplexPredicate4 PartTbl-ComplexPredicate5 LargeMultiColumnInList;

CPartTbl5Test:
PartTbl-IsNullPredicate PartTbl-IsNotNullPredicate PartTbl-IndexOnDefPartOnly
PartTbl-SubqueryOuterRef PartTbl-CSQ-PartKey PartTbl-CSQ-NonPartKey
PartTbl-AggWithExistentialSubquery;

CPartTbl6Test:
PartTbl-PredicateWithCast PartTbl-IDFList PartTbl-IDFNull
PartTbl-PredicateWithCast PartTbl-EqPredicateWithCastRange
PartTbl-PredicateWithCastList PartTbl-IDFWithCast
PartTbl-PredicateWithCastMultiLevelList PartTbl-WindowFuncPredPushDown
PartTbl-WindowFuncPartialPredPushDown PartTbl-WindowFuncNoPredPushDown
PartTbl-WindowFuncNoDisjunctPredPushDown PartTbl-WindowFuncSinglePredPushDown;

CPartTableLossyCastTest:
RangePartLossyCastLTEqOnEndPartitionRange RangePartLossyCastLTOnEndPartitionRange
RangePartLossyCastEqOnEndPartitionRange RangePartLossyCastLTEqInPartitionRange
RangePartLossyCastLTInPartitionRange RangePartLossyCastEqInPartitionRange
ListPartLossyCastLT ListPartLossyCastEq;

CPartTblDPETest:
PartTbl-AvoidRangePred-DPE PartTbl-DPE-Correlated-NLOuter
PartTbl-DPE PartTbl-DPE-WindowFunction PartTbl-DPE-GroupBy PartTbl-DPE-Limit
DPE-with-unsupported-pred DPE-SemiJoin DPE-IN DPE-NOT-IN HJN-DPE-Bitmap-Outer-Child
NLJ-Broadcast-DPE-Outer-Child PartTbl-MultiWayJoinWithDPE PartTbl-MultiWayJoinWithDPE-2
PartTbl-LeftOuterHashJoin-DPE-IsNull PartTbl-LeftOuterNLJoin-DPE-IsNull
PartTbl-List-DPE-Varchar-Predicates PartTbl-List-DPE-Int-Predicates;

CSetop1Test:
ValueScanWithDuplicateAndSelfComparison PushGbBelowNaryUnionAll
PushGbBelowNaryUnion-1 PushGbBelowNaryUnion-2 MS-UnionAll-1
MS-UnionAll-5 MS-UnionAll-6 MS-UnionAll-7 ManyTextUnionsInSubquery;

CSetop2Test:
Cascaded-UnionAll-Same-Cols-Order Cascaded-UnionAll-Differing-Cols
Cascaded-UnionAll-Differing-Cols-Order Union-Over-UnionAll Nested-Setops
Nested-Setops-2 Except IsNullUnionAllIsNotNull UnionAllWithTruncatedOutput
Union-OuterRefs-Output Union-OuterRefs-Casting-Output
Union-OuterRefs-InnerChild MS-UnionAll-2;

CSetop3Test:
UnionWithOuterRefs UnionAll
Union-Distributed-Table-With-Const-Table ExceptAllCompatibleDataType
UnionAllCompatibleDataType UnionOfDQAQueries Union-Volatile-Func
Intersect-Volatile-Func Except-Volatile-Func UnionWithCTE;

CSetop4Test:
PushSelectDownUnionAllOfCTG Push-Subplan-Below-Union Intersect-OuterRefs
PushSelectWithOuterRefBelowUnion PushGbBelowUnion UnionGbSubquery MS-UnionAll-4
AnyPredicate-Over-UnionOfConsts EquivClassesUnion EquivClassesIntersect
IndexScanWithNestedCTEAndSetOp Blocking-Spool-Parallel-Union-All;

CEquivClassTest:
Equiv-HashedDistr-1 Equiv-HashedDistr-2 EquivClassesAndOr EquivClassesLimit IndexNLJoin_Cast_NoMotion;

COuterJoin1Test:
ExpandNAryJoinGreedyWithLOJOnly NaryWithLojAndNonLojChilds LOJ_bb_mpph LOJ-Condition-False
LeftJoin-With-Pred-On-Inner LeftJoin-With-Pred-On-Inner2
LeftJoin-With-Col-Const-Pred LeftJoin-With-Coalesce LOJWithFalsePred LeftJoin-DPv2-With-Select
DPv2GreedyOnly DPv2MinCardOnly DPv2QueryOnly;

COuterJoin2Test:
LOJ-IsNullPred Select-Proj-OuterJoin OuterJoin-With-OuterRefs Join-Disj-Subqs
EffectOfLocalPredOnJoin EffectOfLocalPredOnJoin2 EffectOfLocalPredOnJoin3
LeftJoin-UnsupportedFilter-Cardinality LeftOuter2InnerUnionAllAntiSemiJoin;

COuterJoin3Test:
LOJ_IDF_no_convert_outer_ref_predicate_with_NULL
LOJ_IDF_no_convert_outer_ref_predicate_with_inner_ref LOJ_convert_to_inner_with_inner_predicate_IDF
LOJ_dont_convert_to_inner_with_inner_predicate_IDF_const LOJ_dont_convert_to_inner_with_outer_predicate_INDF
LOJ_dont_convert_to_inner_with_outer_predicate_INDF_const LOJ-With-Agg
LOJReorderWithComplexPredicate LOJReorderComplexNestedLOJs LOJReorderWithIDF
LOJReorderWithNestedLOJAndFilter LOJReorderWithSimplePredicate LOJReorderWithOneSidedFilter
LOJ-HashJoin-MultiDistKeys-WithComplexPreds LOJ_convert_to_inner_with_and_predicate
LOJ_convert_to_inner_with_or_predicate LOJNonNullRejectingPredicates LOJNullRejectingPredicates;

CFullJoinTest:
FOJ-Condition-False ExpandFullOuterJoin ExpandFullOuterJoin2 FullOuterJoin-NullRejectingLHS1 FullOuterJoin-NullRejectingLHS2
FullJoin-InnerNotOnDistributionColumn FullJoin-NotOnDistributionColumn FullJoin-2PredicateOnDistColumns
FullJoin-OnDistributionColumn FullJoin-SwappedPredicates FullJoin-Replicated 
FullJoin-SubqueryWithRedistribute FullJoin-SubquerySingleton FullJoin-Subquery-CastedPredicates
FullJoin-Caps;

CJoinOrderGreedyTest:
GreedyNAryJoin GreedyNAryDelayCrossJoin GreedyNAryOnlyCrossJoins GreedyNAryJoinWithDisconnectedEdges;

CMotionHazardTest:
MotionHazard-NoMaterializeSortUnderResult  MotionHazard-NoMaterializeGatherUnderResult MotionHazard-MaterializeUnderResult
MotionHazard-NoMaterializeHashAggUnderResult CorrelatedNLJWithStreamingSpool NestedNLJWithBlockingSpool;

CConstraintNegationTest:
AggregatesConstraintDeriveNegation CTEConstraintDeriveNegation Union-NOT-Plus-OR-Constraint;

CNonRedistributableColTest:
JOIN-NonRedistributableCol DQA-NonRedistributableCol
MotionHazard-NoMaterializeHashAggUnderResult;

CInvalidCostContextTest:
SpoolShouldInvalidateUnresolvedDynamicScans;

CRandomDataInsertionTest:
AddRedistributeBeforeInsert-1 AddRedistributeBeforeInsert-2 AddRedistributeBeforeInsert-3
AddRedistributeBeforeInsert-4 AddRedistributeBeforeInsert-5 DontAddRedistributeBeforeInsert-1
DontAddRedistributeBeforeInsert-2;

CGistIndexTest:
Gist-AOCOTable-NonLossy-BitmapIndexPlan Gist-NonPart-Lossy-BitmapIndexPlan Gist-NonPart-Lossy-IndexPlan
Gist-PartTable-Lossy-IndexPlan Gist-PartTable-NonLossy-BitmapIndexPlan Gist-PartialIndex-TableScan
Gist-NestedLoopJoin-Lossy-IndexPlan Gist-NestedLoopJoin-Postgis-IndexPlan Gist-OrderBy-BitmapPlan Gist-OrderBy-IndexPlan;

CGinIndexTest:
GinIndex GinIndexPathOpfamily GinIndexSearchModeAll;

CReplicatedTableTest:
ReplicatedTableSelect ReplicatedTableInsert ReplicatedTable-CTAS
ReplicatedHashJoinReplicated ReplicatedJoinHashDistributedTable
ReplicatedJoinRandomDistributedTable ReplicatedLOJHashDistributedTable
ReplicatedLOJRandomDistributedTable ReplicatedLOJReplicated
ReplicatedNLJReplicated ReplicatedTableAggregate ReplicatedTableCTE
ReplicatedTableGroupBy ReplicatedJoinPartitionedTable
ReplicatedTableInClause;

CDqaTest:
NonSplittableAgg DqaHavingMax DqaMax DqaMin DqaSubqueryMax DqaNoRedistribute;

CMCVCardinalityTest:
BpCharMCVCardinalityEquals BpCharMCVCardinalityGreaterThan
TextMCVCardinalityEquals TextMCVCardinalityGreaterThan
VarcharMCVCardinalityEquals VarcharMCVCardinalityGreaterThan Citext-Cardinality
Name-Cardinality Char-Cardinality;

COpfamiliesTest:
JoinCitextVarchar JoinDefaultOpfamiliesUsingNonDefaultOpfamilyOp
MultiColumnAggWithDefaultOpfamilies JoinTinterval FullJoin-NonDefaultOpfamily
JoinAbsEqWithoutOpfamilies 3WayJoinUsingOperatorsOfNonDefaultOpfamily

")

set(mdp_dir "../data/dxl/minidump/")
set(mdp_incl_dir "unittest/gpopt/minidump")
set(mdp_test_hdr_tpl "${CMAKE_CURRENT_SOURCE_DIR}/include/${mdp_incl_dir}/MinidumpTest.h.in")
set(mdp_test_src_tpl "${CMAKE_CURRENT_SOURCE_DIR}/src/${mdp_incl_dir}/MinidumpTest.cpp.in")
set(mdp_test_hdr_dir "${CMAKE_CURRENT_BINARY_DIR}/include/${mdp_incl_dir}")
set(mdp_test_src_dir "${CMAKE_CURRENT_BINARY_DIR}/src/${mdp_incl_dir}")
set(mdp_headers "${mdp_test_hdr_dir}/MinidumpTestHeaders")
set(mdp_tests "${mdp_test_hdr_dir}/MinidumpTestArray")
set(mdp_headers_h "${mdp_headers}.h")
set(mdp_tests_inl "${mdp_tests}.inl")

# if the old file exists and the new file is the same with old file,
# do nothing. otherwise, write the old file with new file's content.
function(update_file old_file new_file)
  if(NOT EXISTS ${old_file})
    file(RENAME ${new_file} ${old_file})
  else()
    file(READ ${old_file} old_hex HEX)
    file(READ ${new_file} new_hex HEX)
    if("${old_hex}" STREQUAL "${new_hex}")
      file(REMOVE ${new_file})
    else()
      file(RENAME ${new_file} ${old_file})
    endif()
  endif()
endfunction()

file(WRITE ${mdp_headers} "")
file(WRITE ${mdp_tests} "")

foreach(mdp_group IN LISTS MDP_GROUPS)

  # remove comments /*...*/ from minidump test group string
  string(REGEX REPLACE "\\/\\*.*\\*\\/" "" mdp_group ${mdp_group})

  string(STRIP ${mdp_group} mdp_group)

  # get minidump test name
  string(REGEX MATCH "C.+Test" test_name ${mdp_group})

  # get the length of "CxxxTest:"
  string(LENGTH "${test_name}:" name_len)

  # get minidump file names (without .mdp) concatenated by spaces
  string(SUBSTRING ${mdp_group} ${name_len} -1 mdp_names)

  string(STRIP ${mdp_names} mdp_names)

  # generate full minidump paths array
  string(REGEX REPLACE "([A-Za-z0-9_\-]+)" "\"${mdp_dir}\\1.mdp\"," mdp_names ${mdp_names})
  string(REGEX REPLACE "[ \t\n]+" "\n" mdp_files ${mdp_names})

  # generate header/src file name for current test
  set(header_file ${mdp_test_hdr_dir}/${test_name})
  set(source_file ${mdp_test_src_dir}/${test_name})

  # auto generate test class from template
  configure_file(${mdp_test_hdr_tpl} ${header_file})
  configure_file(${mdp_test_src_tpl} ${source_file})

  update_file(${header_file}.h ${header_file})
  update_file(${source_file}.cpp ${source_file})

  # add test header file into include file used in main.cpp
  file(APPEND ${mdp_headers} "#include \"${mdp_incl_dir}/${test_name}.h\"\n")

  # add unittest into test array in main.cpp
  file(APPEND ${mdp_tests} "GPOS_UNITTEST_STD(${test_name}),\n")

  add_orca_test(${test_name})
endforeach()

update_file(${mdp_headers_h} ${mdp_headers})
update_file(${mdp_tests_inl} ${mdp_tests})

# The ordering of tests and the conditions by which tests are enabled or
# disabled matches the static array of tests in "src/startup/main.cpp".

# Naucrates tests.
add_orca_test(CCostTest)
add_orca_test(CExternalTableTest)
add_orca_test(CDatumTest)
add_orca_test(CDXLMemoryManagerTest)
add_orca_test(CDXLUtilsTest)
add_orca_test(CMDAccessorTest)
add_orca_test(CMDProviderTest)
add_orca_test(CArrayExpansionTest)
add_orca_test(CJoinOrderDPTest)
add_orca_test(CMiniDumperDXLTest)
add_orca_test(CExpressionPreprocessorTest)
add_orca_test(CWindowTest)
add_orca_test(CICGTest)
add_orca_test(CMultilevelPartitionTest)
add_orca_test(CDMLTest)
add_orca_test(CDirectDispatchTest)
add_orca_test(CTVFTest)
add_orca_test(CPullUpProjectElementTest)
add_orca_test(CAggTest)
add_orca_test(CSubqueryTest)
add_orca_test(CCollapseProjectTest)
add_orca_test(CPruneColumnsTest)
add_orca_test(CMissingStatsTest)
add_orca_test(CBitmapTest)
add_orca_test(CCTETest)
add_orca_test(CEscapeMechanismTest)
add_orca_test(CPhysicalParallelUnionAllTest)
add_orca_test(CMinidumpWithConstExprEvaluatorTest)
add_orca_test(CParseHandlerManagerTest)
add_orca_test(CParseHandlerTest)
add_orca_test(CParseHandlerCostModelTest)
add_orca_test(CParseHandlerOptimizerConfigTest)
add_orca_test(CStatisticsTest)
add_orca_test(CPointTest)
add_orca_test(CBucketTest)
add_orca_test(CHistogramTest)
add_orca_test(CMCVTest)
add_orca_test(CJoinCardinalityTest)
add_orca_test(CJoinCardinalityNDVBasedEqPredTest)
add_orca_test(CFilterCardinalityTest)
add_orca_test(CTranslatorDXLToExprTest)
add_orca_test(CTranslatorExprToDXLTest)
add_orca_test(CXMLSerializerTest)

# Opt tests.
add_orca_test(CColumnDescriptorTest)
add_orca_test(CColumnFactoryTest)
add_orca_test(CColRefSetIterTest)
add_orca_test(CColRefSetTest)
add_orca_test(CConstraintTest)
add_orca_test(CContradictionTest)
add_orca_test(CCorrelatedExecutionTest)
add_orca_test(CDecorrelatorTest)
add_orca_test(CDistributionSpecTest)
add_orca_test(CCastTest)
add_orca_test(CConstTblGetTest)
add_orca_test(CScalarIsDistinctFromTest)

add_orca_test(CSubqueryHandlerTest)

add_orca_test(CBindingTest)
add_orca_test(CEngineTest)
add_orca_test(CEquivalenceClassesTest)
add_orca_test(CExpressionTest)
add_orca_test(CJoinOrderTest)
add_orca_test(CKeyCollectionTest)
add_orca_test(CMaxCardTest)
add_orca_test(CFunctionalDependencyTest)
add_orca_test(CNameTest)
add_orca_test(COrderSpecTest)
add_orca_test(CRangeTest)
add_orca_test(CPredicateUtilsTest)
add_orca_test(CPartConstraintTest)

add_orca_test(CSearchStrategyTest)

add_orca_test(COptimizationJobsTest)
add_orca_test(CStateMachineTest)
add_orca_test(CTableDescriptorTest)
add_orca_test(CIndexDescriptorTest)
add_orca_test(CTreeMapTest)
add_orca_test(CXformFactoryTest)
add_orca_test(CXformTest)

add_orca_test(CConstExprEvaluatorDefaultTest)
add_orca_test(CConstExprEvaluatorDXLTest)

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
  if (ENABLE_EXTENDED_TESTS)
    # fault-simulation while running other tests
    add_orca_test(CFSimulatorTestExt)

    # time-slicing
    add_orca_test(CTimeSliceTest)
  endif()
endif()

file(GLOB_RECURSE hdrs ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/include/*.inl
                       ${mdp_test_hdr_dir}/*.h
                       ${mdp_test_hdr_dir}/*.inl)
file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
                       ${mdp_test_src_dir}/*.cpp)

# Add headers to make them visible in some IDEs (Clion, VS, Xcode)
list(APPEND srcs ${hdrs})

add_executable(gporca_test ${srcs})

target_link_libraries(gporca_test
                      gpdbcost
                      gpopt
                      naucrates
                      gpos)
