//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorExprToDXL.h
//
//	@doc:
//		Class providing methods for translating optimizer Expression trees
//		into DXL Tree.
//---------------------------------------------------------------------------

#ifndef GPOPT_CTranslatorExprToDXL_H
#define GPOPT_CTranslatorExprToDXL_H


#include "gpos/base.h"
#include "gpos/memory/CMemoryPool.h"

#include "gpopt/base/CDrvdPropPlan.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalDML.h"
#include "gpopt/operators/CPhysicalHashJoin.h"
#include "gpopt/operators/CPhysicalScan.h"
#include "gpopt/operators/CScalarArrayRefIndexList.h"
#include "gpopt/operators/CScalarBoolOp.h"
#include "gpopt/operators/CScalarWindowFunc.h"
#include "naucrates/dxl/operators/CDXLColRef.h"
#include "naucrates/dxl/operators/CDXLIndexDescr.h"
#include "naucrates/dxl/operators/CDXLPhysicalAgg.h"
#include "naucrates/dxl/operators/CDXLPhysicalDML.h"
#include "naucrates/dxl/operators/CDXLPhysicalMotion.h"
#include "naucrates/dxl/operators/CDXLScalarArrayRefIndexList.h"
#include "naucrates/dxl/operators/CDXLScalarBoolExpr.h"
#include "naucrates/dxl/operators/CDXLScalarSubPlan.h"
#include "naucrates/dxl/operators/CDXLScalarWindowRef.h"

// fwd declaration
namespace gpnaucrates
{
class IStatistics;
}

// forward declarations
namespace gpdxl
{
class CDXLTableDescr;
class CDXLPhysicalProperties;
class CDXLWindowFrame;
}  // namespace gpdxl

namespace gpopt
{
using namespace gpmd;
using namespace gpdxl;
using namespace gpnaucrates;

// hash map mapping CColRef -> CDXLNode
using ColRefToDXLNodeMap =
	CHashMap<CColRef, CDXLNode, CColRef::HashValue, CColRef::Equals,
			 CleanupNULL<CColRef>, CleanupRelease<CDXLNode>>;

//---------------------------------------------------------------------------
//	@class:
//		CTranslatorExprToDXL
//
//	@doc:
//		Class providing methods for translating optimizer Expression trees
//		into DXL Trees.
//
//---------------------------------------------------------------------------
class CTranslatorExprToDXL
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// metadata accessor
	CMDAccessor *m_pmda;

	// mappings CColRef -> CDXLNode used to lookup subplans
	ColRefToDXLNodeMap *m_phmcrdxln;

	// mappings CColRef -> CDXLNode used to for index predicates with outer references
	ColRefToDXLNodeMap *m_phmcrdxlnIndexLookup;

	// mappings CColRef -> ColId for translating filters for child partitions
	// (see PdxlnFilterForChildPart())
	ColRefToUlongMap *m_phmcrulPartColId = nullptr;

	// derived plan properties of the translated expression
	CDrvdPropPlan *m_pdpplan;

	// a copy of the pointer to column factory, obtained at construction time
	CColumnFactory *m_pcf;

	// segment ids on target system
	IntPtrArray *m_pdrgpiSegments;

	// id of master node
	INT m_iMasterId;

	// private copy ctor
	CTranslatorExprToDXL(const CTranslatorExprToDXL &);

	static EdxlBoolExprType Edxlbooltype(
		const CScalarBoolOp::EBoolOperator eboolop);

	// return the EdxlDmlType for a given DML op type
	static EdxlDmlType Edxldmloptype(const CLogicalDML::EDMLOperator edmlop);

	// return outer refs in correlated join inner child
	static CColRefSet *PcrsOuterRefsForCorrelatedNLJoin(CExpression *pexpr);

	// functions translating different optimizer expressions into their
	// DXL counterparts

	CDXLNode *PdxlnTblScan(CExpression *pexprTblScan, CColRefSet *pcrsOutput,
						   CColRefArray *colref_array,
						   CDistributionSpecArray *pdrgpdsBaseTables,
						   CExpression *pexprScalarCond,
						   CDXLPhysicalProperties *dxl_properties);

	// create a (dynamic) index scan node after inlining the given scalar condition, if needed
	CDXLNode *PdxlnIndexScanWithInlinedCondition(
		CExpression *pexprIndexScan, CExpression *pexprScalarCond,
		CDXLPhysicalProperties *dxl_properties, CColRefArray *colref_array,
		CDistributionSpecArray *pdrgpdsBaseTables);

	// translate index scan based on passed properties
	CDXLNode *PdxlnIndexScan(CExpression *pexprIndexScan,
							 CColRefArray *colref_array,
							 CDXLPhysicalProperties *dxl_properties,
							 CReqdPropPlan *prpp);

	// translate index scan
	CDXLNode *PdxlnIndexScan(CExpression *pexprIndexScan,
							 CColRefArray *colref_array,
							 CDistributionSpecArray *pdrgpdsBaseTables,
							 ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnIndexOnlyScan(CExpression *pexprIndexScan,
								 CColRefArray *colref_array,
								 CDistributionSpecArray *pdrgpdsBaseTables,
								 ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnIndexOnlyScan(CExpression *pexprIndexScan,
								 CColRefArray *colref_array,
								 CDXLPhysicalProperties *dxl_properties,
								 CReqdPropPlan *prpp);

	// translate a bitmap index probe expression to DXL
	CDXLNode *PdxlnBitmapIndexProbe(CExpression *pexprBitmapIndexProbe);

	// translate a bitmap bool op expression to DXL
	CDXLNode *PdxlnBitmapBoolOp(CExpression *pexprBitmapBoolOp);

	// translate a bitmap table scan expression to DXL
	CDXLNode *PdxlnBitmapTableScan(CExpression *pexprBitmapTableScan,
								   CColRefArray *colref_array,
								   CDistributionSpecArray *pdrgpdsBaseTables,
								   ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a bitmap table scan expression to DXL
	CDXLNode *PdxlnBitmapTableScan(CExpression *pexprBitmapTableScan,
								   CColRefSet *pcrsOutput,
								   CColRefArray *colref_array,
								   CDistributionSpecArray *pdrgpdsBaseTables,
								   CExpression *pexprScalar,
								   CDXLPhysicalProperties *dxl_properties);

	// create a DXL result node from an optimizer filter node
	CDXLNode *PdxlnResultFromFilter(CExpression *pexprFilter,
									CColRefArray *colref_array,
									CDistributionSpecArray *pdrgpdsBaseTables,
									ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnResult(CExpression *pexprFilter, CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML);

	// create a DXL result node for the given project list
	CDXLNode *PdxlnResult(CDXLNode *proj_list_dxlnode, CExpression *pexprFilter,
						  CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML);

	// given a DXL plan tree child_dxlnode which represents the physical plan pexprRelational, construct a DXL
	// Result node that filters on top of it using the scalar condition pdxlnScalar
	CDXLNode *PdxlnAddScalarFilterOnRelationalChild(
		CDXLNode *pdxlnRelationalChild, CDXLNode *pdxlnScalarChild,
		CDXLPhysicalProperties *dxl_properties, CColRefSet *pcrsOutput,
		CColRefArray *pdrgpcrOrder);

	CDXLNode *PdxlnFromFilter(CExpression *pexprFilter,
							  CColRefArray *colref_array,
							  CDistributionSpecArray *pdrgpdsBaseTables,
							  ULONG *pulNonGatherMotions, BOOL *pfDML,
							  CDXLPhysicalProperties *dxl_properties);

	CDXLNode *PdxlnResult(CExpression *pexprRelational,
						  CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML,
						  CDXLNode *pdxlnScalar,
						  CDXLPhysicalProperties *dxl_properties);

	CDXLNode *PdxlnResult(CExpression *pexprRelational,
						  CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML,
						  CDXLNode *pdxlnScalar);

	CDXLNode *PdxlnComputeScalar(CExpression *pexprComputeScalar,
								 CColRefArray *colref_array,
								 CDistributionSpecArray *pdrgpdsBaseTables,
								 ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnAggregate(CExpression *pexprHashAgg,
							 CColRefArray *colref_array,
							 CDistributionSpecArray *pdrgpdsBaseTables,
							 ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnAggregateDedup(CExpression *pexprAgg,
								  CColRefArray *colref_array,
								  CDistributionSpecArray *pdrgpdsBaseTables,
								  ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnAggregate(CExpression *pexprAgg, CColRefArray *colref_array,
							 CDistributionSpecArray *pdrgpdsBaseTables,
							 ULONG *pulNonGatherMotions, BOOL *pfDML,
							 EdxlAggStrategy dxl_agg_strategy,
							 const CColRefArray *pdrgpcrGroupingCols,
							 CColRefSet *pcrsKeys);

	CDXLNode *PdxlnSort(CExpression *pexprSort, CColRefArray *colref_array,
						CDistributionSpecArray *pdrgpdsBaseTables,
						ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnLimit(CExpression *pexprLimit, CColRefArray *colref_array,
						 CDistributionSpecArray *pdrgpdsBaseTables,
						 ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnWindow(CExpression *pexprSeqPrj, CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnNLJoin(CExpression *pexprNLJ, CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnMergeJoin(CExpression *pexprMJ, CColRefArray *colref_array,
							 CDistributionSpecArray *pdrgpdsBaseTables,
							 ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnHashJoin(CExpression *pexprHJ, CColRefArray *colref_array,
							CDistributionSpecArray *pdrgpdsBaseTables,
							ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnCorrelatedNLJoin(CExpression *pexprNLJ,
									CColRefArray *colref_array,
									CDistributionSpecArray *pdrgpdsBaseTables,
									ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnCTEProducer(CExpression *pexprCTEProducer,
							   CColRefArray *colref_array,
							   CDistributionSpecArray *pdrgpdsBaseTables,
							   ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnCTEConsumer(CExpression *pexprCTEConsumer,
							   CColRefArray *colref_array,
							   CDistributionSpecArray *pdrgpdsBaseTables,
							   ULONG *pulNonGatherMotions, BOOL *pfDML);

	// store outer references in index NLJ inner child into global map
	void StoreIndexNLJOuterRefs(CPhysical *pop);

	// build a scalar DXL subplan node
	void BuildDxlnSubPlan(CDXLNode *pdxlnRelChild, const CColRef *colref,
						  CDXLColRefArray *dxl_colref_array);

	// build a boolean scalar dxl node with a subplan as its child
	CDXLNode *PdxlnBooleanScalarWithSubPlan(CDXLNode *pdxlnRelChild,
											CDXLColRefArray *dxl_colref_array);

	CDXLNode *PdxlnScBoolExpr(EdxlBoolExprType boolexptype,
							  CDXLNode *dxlnode_left, CDXLNode *dxlnode_right);

	CDXLNode *PdxlnTblScanFromNLJoinOuter(
		CExpression *pexprRelational, CDXLNode *pdxlnScalar,
		CColRefArray *colref_array, CDistributionSpecArray *pdrgpdsBaseTables,
		ULONG *pulNonGatherMotions, CDXLPhysicalProperties *dxl_properties);

	CDXLNode *PdxlnResultFromNLJoinOuter(
		CExpression *pexprRelational, CDXLNode *pdxlnScalar,
		CColRefArray *colref_array, CDistributionSpecArray *pdrgpdsBaseTables,
		ULONG *pulNonGatherMotions, BOOL *pfDML,
		CDXLPhysicalProperties *dxl_properties);

	CDXLNode *PdxlnMotion(CExpression *pexprMotion, CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML);

	CDXLNode *PdxlnMaterialize(CExpression *pexprSpool,
							   CColRefArray *colref_array,
							   CDistributionSpecArray *pdrgpdsBaseTables,
							   ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a sequence expression
	CDXLNode *PdxlnSequence(CExpression *pexprSequence,
							CColRefArray *colref_array,
							CDistributionSpecArray *pdrgpdsBaseTables,
							ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a dynamic table scan
	CDXLNode *PdxlnDynamicTableScan(CExpression *pexprDTS,
									CColRefArray *colref_array,
									CDistributionSpecArray *pdrgpdsBaseTables,
									ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a dynamic table scan with a scalar condition
	CDXLNode *PdxlnDynamicTableScan(CExpression *pexprDTS,
									CColRefArray *colref_array,
									CDistributionSpecArray *pdrgpdsBaseTables,
									CExpression *pexprScalarCond,
									CDXLPhysicalProperties *dxl_properties);

	// translate a dynamic bitmap table scan
	CDXLNode *PdxlnDynamicBitmapTableScan(
		CExpression *pexprDynamicBitmapTableScan, CColRefArray *colref_array,
		CDistributionSpecArray *pdrgpdsBaseTables, ULONG *pulNonGatherMotions,
		BOOL *pfDML);

	// translate a dynamic foreign scan
	CDXLNode *PdxlnDynamicForeignScan(CExpression *pexprDFS,
									  CColRefArray *colref_array,
									  CDistributionSpecArray *pdrgpdsBaseTables,
									  ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a dynamic foreign scan with a scalar condition
	CDXLNode *PdxlnDynamicForeignScan(CExpression *pexprDFS,
									  CColRefArray *colref_array,
									  CDistributionSpecArray *pdrgpdsBaseTables,
									  CExpression *pexprScalarCond,
									  CDXLPhysicalProperties *dxl_properties);

	// Construct a table descr for a child partition
	CTableDescriptor *MakeTableDescForPart(const IMDRelation *part,
										   CTableDescriptor *root_table_desc);

	// Construct a dxl index descriptor for a child partition
	static CDXLIndexDescr *PdxlnIndexDescForPart(
		CMemoryPool *m_mp, MdidHashSet *child_index_mdids_set,
		const IMDRelation *part, const CWStringConst *index_name);

	// translate a dynamic bitmap table scan
	CDXLNode *PdxlnDynamicBitmapTableScan(
		CExpression *pexprDynamicBitmapTableScan, CColRefArray *colref_array,
		CDistributionSpecArray *pdrgpdsBaseTables, CExpression *pexprScalar,
		CDXLPhysicalProperties *dxl_properties);

	// translate a dynamic index scan based on passed properties
	CDXLNode *PdxlnDynamicIndexScan(CExpression *pexprDIS,
									CColRefArray *colref_array,
									CDXLPhysicalProperties *dxl_properties,
									CReqdPropPlan *prpp);

	// translate a dynamic index scan
	CDXLNode *PdxlnDynamicIndexScan(CExpression *pexprDIS,
									CColRefArray *colref_array,
									CDistributionSpecArray *pdrgpdsBaseTables,
									ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a const table get into a result node
	CDXLNode *PdxlnResultFromConstTableGet(
		CExpression *pexprCTG, CColRefArray *colref_array,
		CDistributionSpecArray *pdrgpdsBaseTables, ULONG *pulNonGatherMotions,
		BOOL *pfDML);

	// translate a const table get into a result node
	CDXLNode *PdxlnResultFromConstTableGet(CExpression *pexprCTG,
										   CColRefArray *colref_array,
										   CExpression *pexprScalarCond);

	// translate a table-valued function
	CDXLNode *PdxlnTVF(CExpression *pexprTVF, CColRefArray *colref_array,
					   CDistributionSpecArray *pdrgpdsBaseTables,
					   ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate an union all op
	CDXLNode *PdxlnAppend(CExpression *pexprUnionAll,
						  CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a partition selector
	CDXLNode *PdxlnPartitionSelector(CExpression *pexpr,
									 CColRefArray *colref_array,
									 CDistributionSpecArray *pdrgpdsBaseTables,
									 ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a DML operator
	CDXLNode *PdxlnDML(CExpression *pexpr, CColRefArray *colref_array,
					   CDistributionSpecArray *pdrgpdsBaseTables,
					   ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a CTAS operator
	CDXLNode *PdxlnCTAS(CExpression *pexpr,
						CDistributionSpecArray *pdrgpdsBaseTables,
						ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a split operator
	CDXLNode *PdxlnSplit(CExpression *pexpr, CColRefArray *colref_array,
						 CDistributionSpecArray *pdrgpdsBaseTables,
						 ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate an assert operator
	CDXLNode *PdxlnAssert(CExpression *pexprAssert, CColRefArray *colref_array,
						  CDistributionSpecArray *pdrgpdsBaseTables,
						  ULONG *pulNonGatherMotions, BOOL *pfDML);

	// translate a scalar If statement
	CDXLNode *PdxlnScIfStmt(CExpression *pexprScIf);

	// translate a scalar switch
	CDXLNode *PdxlnScSwitch(CExpression *pexprScSwitch);

	// translate a scalar switch case
	CDXLNode *PdxlnScSwitchCase(CExpression *pexprScSwitchCase);

	// translate a scalar case test
	CDXLNode *PdxlnScCaseTest(CExpression *pexprScCaseTest);

	// translate a scalar comparison
	CDXLNode *PdxlnScCmp(CExpression *pexprScCmp);

	// translate a scalar distinct comparison
	CDXLNode *PdxlnScDistinctCmp(CExpression *pexprScIsDistFrom);

	// translate a scalar op node
	CDXLNode *PdxlnScOp(CExpression *pexprScOp);

	// translate a scalar constant
	CDXLNode *PdxlnScConst(CExpression *pexprScConst);

	CDXLNode *PdxlnScSortGroupClause(CExpression *pexprScSortGroupClause);

	// translate a scalar coalesce
	CDXLNode *PdxlnScCoalesce(CExpression *pexprScCoalesce);

	// translate a scalar MinMax
	CDXLNode *PdxlnScMinMax(CExpression *pexprScMinMax);

	// translate a scalar boolean expression
	CDXLNode *PdxlnScBoolExpr(CExpression *pexprScBoolExpr);

	// translate a scalar identifier
	CDXLNode *PdxlnScId(CExpression *pexprScId);

	// translate a scalar function expression
	CDXLNode *PdxlnScFuncExpr(CExpression *pexprScFunc);

	// translate a scalar window function expression
	CDXLNode *PdxlnScWindowFuncExpr(CExpression *pexprScFunc);

	// get the DXL representation of the window stage
	static EdxlWinStage Ews(CScalarWindowFunc::EWinStage ews);

	// translate a scalar aggref
	CDXLNode *PdxlnScAggref(CExpression *pexprScAggFunc);

	// translate a scalar nullif
	CDXLNode *PdxlnScNullIf(CExpression *pexprScNullIf);

	// translate a scalar null test
	CDXLNode *PdxlnScNullTest(CExpression *pexprScNullTest);

	// translate a scalar boolean test
	CDXLNode *PdxlnScBooleanTest(CExpression *pexprScBooleanTest);

	// translate a scalar cast
	CDXLNode *PdxlnScCast(CExpression *pexprScCast);

	// translate a scalar coerce
	CDXLNode *PdxlnScCoerceToDomain(CExpression *pexprScCoerce);

	// translate a scalar coerce using I/O functions
	CDXLNode *PdxlnScCoerceViaIO(CExpression *pexprScCoerce);

	// translate a scalar array coerce expr with element coerce function
	CDXLNode *PdxlnScArrayCoerceExpr(CExpression *pexprScArrayCoerceExpr);

	CDXLNode *PdxlnValuesList(CExpression *pexpr);

	// translate an array
	CDXLNode *PdxlnArray(CExpression *pexpr);

	// translate an arrayref
	CDXLNode *PdxlnArrayRef(CExpression *pexpr);

	// translate an arrayref index list
	CDXLNode *PdxlnArrayRefIndexList(CExpression *pexpr);

	// translate the arrayref index list bound
	static CDXLScalarArrayRefIndexList::EIndexListBound Eilb(
		const CScalarArrayRefIndexList::EIndexListType eilt);

	// translate an array compare
	CDXLNode *PdxlnArrayCmp(CExpression *pexpr);

	// translate an assert predicate
	CDXLNode *PdxlnAssertPredicate(CExpression *pexpr);

	// translate an assert constraint
	CDXLNode *PdxlnAssertConstraint(CExpression *pexpr);

	// translate a DML action expression
	CDXLNode *PdxlnDMLAction(CExpression *pexpr);

	// translate a window frame
	CDXLWindowFrame *GetWindowFrame(CWindowFrame *pwf);

	CDXLTableDescr *MakeDXLTableDescr(const CTableDescriptor *ptabdesc,
									  const CColRefArray *pdrgpcrOutput,
									  const CReqdPropPlan *requiredProperties);

	// compute physical properties like operator cost from the expression
	CDXLPhysicalProperties *GetProperties(const CExpression *pexpr);

	// translate a colref set of output col into a dxl proj list
	CDXLNode *PdxlnProjList(const CColRefSet *pcrsOutput,
							CColRefArray *colref_array);

	// Construct a project list for a child partition
	CDXLNode *PdxlnProjListForChildPart(
		const ColRefToUlongMap *root_col_mapping,
		const CColRefArray *part_colrefs, const CColRefSet *reqd_colrefs,
		const CColRefArray *colref_array);

	// translate a filter expr on the root for a child partition
	CDXLNode *PdxlnCondForChildPart(const ColRefToUlongMap *root_col_mapping,
									const CColRefArray *part_colrefs,
									const CColRefArray *root_colrefs,
									CExpression *pred);

	// translate a project list expression into a DXL proj list node
	// according to the order specified in the dynamic array
	CDXLNode *PdxlnProjList(const CExpression *pexprProjList,
							const CColRefSet *pcrsOutput,
							CColRefArray *colref_array);

	// translate a project list expression into a DXL proj list node
	CDXLNode *PdxlnProjList(const CExpression *pexprProjList,
							const CColRefSet *pcrsOutput);

	// create a project list for a result node from a tuple of a
	// const table get operator
	CDXLNode *PdxlnProjListFromConstTableGet(CColRefArray *pdrgpcrReqOutput,
											 CColRefArray *pdrgpcrCTGOutput,
											 IDatumArray *pdrgpdatumValues);

	// create a DXL project elem node from a proj element expression
	CDXLNode *PdxlnProjElem(const CExpression *pexprProjElem);

	// create a project element for a computed column from a column reference
	// and value expresison
	CDXLNode *PdxlnProjElem(const CColRef *colref, CDXLNode *pdxlnValue);

	// create a DXL sort col list node from an order spec
	CDXLNode *GetSortColListDXL(const COrderSpec *pos);

	// create a DXL sort col list node for a Motion expression
	CDXLNode *GetSortColListDXL(CExpression *pexprMotion);

	// create a DXL hash expr list from an array of hash columns
	CDXLNode *PdxlnHashExprList(const CExpressionArray *pdrgpexpr,
								const IMdIdArray *opfamilies);

	// create a DXL filter node with the given scalar expression
	CDXLNode *PdxlnFilter(CDXLNode *pdxlnCond);

	// construct an array with input segment ids for the given motion expression
	IntPtrArray *GetInputSegIdsArray(CExpression *pexprMotion);

	// construct an array with output segment ids for the given motion expression
	IntPtrArray *GetOutputSegIdsArray(CExpression *pexprMotion);

	// find the position of the given colref in the array
	static ULONG UlPosInArray(const CColRef *colref,
							  const CColRefArray *colref_array);

	// return hash join type
	static EdxlJoinType EdxljtHashJoin(CPhysicalHashJoin *popHJ);

	// main translation routine for Expr tree -> DXL tree
	CDXLNode *CreateDXLNode(CExpression *pexpr, CColRefArray *colref_array,
							CDistributionSpecArray *pdrgpdsBaseTables,
							ULONG *pulNonGatherMotions, BOOL *pfDML,
							BOOL fRemap, BOOL fRoot);

	// translate expression children and add them as children of the DXL node
	void TranslateScalarChildren(CExpression *pexpr, CDXLNode *dxlnode);

	// add a result node, if required a materialize node is added below result node to avoid deadlock hazard
	CDXLNode *PdxlnResult(CDXLPhysicalProperties *dxl_properties,
						  CDXLNode *pdxlnPrL, CDXLNode *child_dxlnode);

	// add a materialize node
	CDXLNode *PdxlnMaterialize(CDXLNode *dxlnode);

	// add result node if necessary
	CDXLNode *PdxlnRemapOutputColumns(CExpression *pexpr, CDXLNode *dxlnode,
									  CColRefArray *pdrgpcrRequired,
									  CColRefArray *pdrgpcrOrder);

	// combines the ordered columns and required columns into a single list
	static CColRefArray *PdrgpcrMerge(CMemoryPool *mp,
									  CColRefArray *pdrgpcrOrder,
									  CColRefArray *pdrgpcrRequired);

	// helper to add a project of bool constant
	CDXLNode *PdxlnProjectBoolConst(CDXLNode *dxlnode, BOOL value);

	// helper to build a Result expression with project list restricted to required column
	CDXLNode *PdxlnRestrictResult(CDXLNode *dxlnode, const CColRef *colref);

	// helper to build a Result expression with project list restricted to required columns
	CDXLNode *PdxlnRestrictResult(CDXLNode *dxlnode, const CColRefSet *colrefs);

	//	helper to build subplans from correlated LOJ
	void BuildSubplansForCorrelatedLOJ(
		CExpression *pexprCorrelatedLOJ, CDXLColRefArray *dxl_colref_array,
		CDXLNode **
			ppdxlnScalar,  // output: scalar condition after replacing inner child reference with subplan
		CDistributionSpecArray *pdrgpdsBaseTables, ULONG *pulNonGatherMotions,
		BOOL *pfDML);

	// helper to build subplans of different types
	void BuildSubplans(
		CExpression *pexprCorrelatedNLJoin, CDXLColRefArray *dxl_colref_array,
		CDXLNode **
			ppdxlnScalar,  // output: scalar condition after replacing inner child reference with subplan
		CDistributionSpecArray *pdrgpdsBaseTables, ULONG *pulNonGatherMotions,
		BOOL *pfDML);

	// helper to build scalar subplans from inner column references and store them
	// in subplan map
	void BuildScalarSubplans(CColRefArray *pdrgpcrInner,
							 CExpression *pexprInner,
							 CDXLColRefArray *dxl_colref_array,
							 CDistributionSpecArray *pdrgpdsBaseTables,
							 ULONG *pulNonGatherMotions, BOOL *pfDML);

	// helper to build subplans for quantified (ANY/ALL) subqueries
	CDXLNode *PdxlnQuantifiedSubplan(CColRefArray *pdrgpcrInner,
									 CExpression *pexprCorrelatedNLJoin,
									 CDXLColRefArray *dxl_colref_array,
									 CDistributionSpecArray *pdrgpdsBaseTables,
									 ULONG *pulNonGatherMotions, BOOL *pfDML);

	// helper to build subplans for existential subqueries
	CDXLNode *PdxlnExistentialSubplan(CColRefArray *pdrgpcrInner,
									  CExpression *pexprCorrelatedNLJoin,
									  CDXLColRefArray *dxl_colref_array,
									  CDistributionSpecArray *pdrgpdsBaseTables,
									  ULONG *pulNonGatherMotions, BOOL *pfDML);

	// compute the direct dispatch info for the given DML expression
	CDXLDirectDispatchInfo *GetDXLDirectDispatchInfo(CExpression *pexprDML);

	// check if result node imposes a motion hazard
	BOOL FNeedsMaterializeUnderResult(CDXLNode *proj_list_dxlnode,
									  CDXLNode *child_dxlnode);

	void AddPartForScanId(ULONG scanid, ULONG index);

	// helper to find subplan type from a correlated left outer join expression
	static EdxlSubPlanType EdxlsubplantypeCorrelatedLOJ(
		CExpression *pexprCorrelatedLOJ);

	// helper to find subplan type from a correlated join expression
	static EdxlSubPlanType Edxlsubplantype(CExpression *pexprCorrelatedNLJoin);

	// add used columns in the bitmap re-check and the remaining scalar filter condition to the
	// required output column
	static void AddBitmapFilterColumns(
		CMemoryPool *mp, CPhysicalScan *pop, CExpression *pexprRecheckCond,
		CExpression *pexprScalar,
		CColRefSet *pcrsReqdOutput	// append the required column reference
	);

public:
	// ctor
	CTranslatorExprToDXL(CMemoryPool *mp, CMDAccessor *md_accessor,
						 IntPtrArray *pdrgpiSegments,
						 BOOL fInitColumnFactory = true);

	// dtor
	~CTranslatorExprToDXL();

	// main driver
	CDXLNode *PdxlnTranslate(CExpression *pexpr, CColRefArray *colref_array,
							 CMDNameArray *pdrgpmdname);

	// translate a scalar expression into a DXL scalar node
	// if the expression is not a scalar, an UnsupportedOp exception is raised
	CDXLNode *PdxlnScalar(CExpression *pexpr);
};
}  // namespace gpopt

#endif	// !GPOPT_CTranslatorExprToDXL_H

// EOF
