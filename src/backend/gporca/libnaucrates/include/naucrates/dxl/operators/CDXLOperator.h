//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLOperator.h
//
//	@doc:
//		Base class for the DXL tree operators.
//---------------------------------------------------------------------------


#ifndef GPDXL_CDXLOperator_H
#define GPDXL_CDXLOperator_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

#include "naucrates/md/CDXLStatsDerivedRelation.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

// fwd decl
class CDXLNode;
class CXMLSerializer;

enum Edxlopid
{
	EdxlopLogicalGet,
	EdxlopLogicalForeignGet,
	EdxlopLogicalProject,
	EdxlopLogicalSelect,
	EdxlopLogicalJoin,
	EdxlopLogicalCTEProducer,
	EdxlopLogicalCTEConsumer,
	EdxlopLogicalCTEAnchor,
	EdxlopLogicalLimit,
	EdxlopLogicalGrpBy,
	EdxlopLogicalGrpCl,
	EdxlopLogicalGrpCols,
	EdxlopLogicalConstTable,
	EdxlopLogicalTVF,
	EdxlopLogicalSetOp,
	EdxlopLogicalWindow,

	EdxlopLogicalInsert,
	EdxlopLogicalDelete,
	EdxlopLogicalUpdate,

	EdxlopLogicalCTAS,
	EdxlopPhysicalCTAS,

	EdxlopScalarCmp,
	EdxlopScalarDistinct,
	EdxlopScalarIdent,
	EdxlopScalarProjectList,
	EdxlopScalarProjectElem,
	EdxlopScalarConstTuple,
	EdxlopScalarFilter,
	EdxlopScalarOneTimeFilter,
	EdxlopScalarJoinFilter,
	EdxlopScalarRecheckCondFilter,
	EdxlopScalarHashExprList,
	EdxlopScalarHashExpr,
	EdxlopScalarHashCondList,
	EdxlopScalarArray,
	EdxlopScalarArrayRef,
	EdxlopScalarArrayRefIndexList,

	EdxlopScalarAssertConstraintList,
	EdxlopScalarAssertConstraint,

	EdxlopScalarMergeCondList,

	EdxlopScalarIndexCondList,

	EdxlopScalarSortColList,
	EdxlopScalarSortCol,

	EdxlopScalarQueryOutput,

	EdxlopScalarOpExpr,
	EdxlopScalarBoolExpr,
	EdxlopScalarCaseTest,
	EdxlopScalarCoalesce,
	EdxlopScalarConstValue,
	EdxlopScalarIfStmt,
	EdxlopScalarSwitch,
	EdxlopScalarSwitchCase,
	EdxlopScalarLimitCount,
	EdxlopScalarLimitOffset,
	EdxlopScalarFuncExpr,
	EdxlopScalarMinMax,
	EdxlopScalarWindowRef,
	EdxlopScalarNullTest,
	EdxlopScalarNullIf,
	EdxlopScalarCast,
	EdxlopScalarCoerceToDomain,
	EdxlopScalarCoerceViaIO,
	EdxlopScalarArrayCoerceExpr,
	EdxlopScalarAggref,
	EdxlopScalarArrayComp,
	EdxlopScalarBooleanTest,
	EdxlopScalarSubPlan,
	EdxlopScalarWindowFrameEdge,

	EdxlopScalarSubquery,
	EdxlopScalarSubqueryAny,
	EdxlopScalarSubqueryAll,
	EdxlopScalarSubqueryExists,
	EdxlopScalarSubqueryNotExists,
	EdxlopScalarBitmapBoolOp,

	EdxlopScalarDMLAction,
	EdxlopScalarOpList,
	EdxlopScalarValuesList,
	EdxlopScalarSortGroupClause,

	EdxlopPhysicalResult,
	EdxlopPhysicalValuesScan,
	EdxlopPhysicalProjection,
	EdxlopPhysicalTableScan,
	EdxlopPhysicalBitmapTableScan,
	EdxlopPhysicalDynamicBitmapTableScan,
	EdxlopPhysicalForeignScan,
	EdxlopPhysicalIndexScan,
	EdxlopPhysicalIndexOnlyScan,
	EdxlopScalarBitmapIndexProbe,
	EdxlopPhysicalSubqueryScan,
	EdxlopPhysicalConstTable,
	EdxlopPhysicalNLJoin,
	EdxlopPhysicalHashJoin,
	EdxlopPhysicalMergeJoin,
	EdxlopPhysicalMotionGather,
	EdxlopPhysicalMotionBroadcast,
	EdxlopPhysicalMotionRedistribute,
	EdxlopPhysicalMotionRoutedDistribute,
	EdxlopPhysicalMotionRandom,
	EdxlopPhysicalLimit,
	EdxlopPhysicalAgg,
	EdxlopPhysicalSort,
	EdxlopPhysicalAppend,
	EdxlopPhysicalMaterialize,
	EdxlopPhysicalDynamicForeignScan,
	EdxlopPhysicalSequence,
	EdxlopPhysicalDynamicTableScan,
	EdxlopPhysicalDynamicIndexScan,
	EdxlopPhysicalPartitionSelector,
	EdxlopPhysicalTVF,
	EdxlopPhysicalWindow,

	EdxlopPhysicalCTEProducer,
	EdxlopPhysicalCTEConsumer,

	EdxlopPhysicalDML,
	EdxlopPhysicalSplit,

	EdxlopPhysicalAssert,

	EdxlopSentinel
};

enum Edxloptype
{
	EdxloptypeLogical,
	EdxloptypePhysical,
	EdxloptypeScalar
};

// Join types
enum EdxlJoinType
{
	EdxljtInner,
	EdxljtLeft,
	EdxljtFull,
	EdxljtRight,
	EdxljtIn,
	EdxljtLeftAntiSemijoin,
	EdxljtLeftAntiSemijoinNotIn,
	EdxljtSentinel
};

// Scan direction of the index
enum EdxlIndexScanDirection
{
	EdxlisdBackward = 0,
	EdxlisdForward,
	EdxlisdNoMovement,

	EdxlisdSentinel
};

// coercion form
enum EdxlCoercionForm
{
	EdxlcfExplicitCall,	 // display as a function call
	EdxlcfExplicitCast,	 // display as an explicit cast
	EdxlcfImplicitCast,	 // implicit cast, so hide it
	EdxlcfDontCare		 // don't care about display
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLOperator
//
//	@doc:
//		Base class for operators in a DXL tree
//
//---------------------------------------------------------------------------
class CDXLOperator : public CRefCount
{
private:
protected:
	// memory pool
	CMemoryPool *m_mp;

public:
	CDXLOperator(const CDXLOperator &) = delete;

	// ctor/dtor
	explicit CDXLOperator(CMemoryPool *);

	~CDXLOperator() override;

	// ident accessors
	virtual Edxlopid GetDXLOperator() const = 0;

	// name of the operator
	virtual const CWStringConst *GetOpNameStr() const = 0;

	virtual Edxloptype GetDXLOperatorType() const = 0;

	// serialize operator in DXL format given a serializer object and the
	// host DXL node, providing access to the operator's children
	virtual void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const = 0;

	// check if given column is defined by operator
	virtual BOOL IsColDefined(ULONG	 // colid
	) const
	{
		// by default, operator does not define columns
		return false;
	}

	static const CWStringConst *GetJoinTypeNameStr(EdxlJoinType);

	// Return the index scan direction
	static const CWStringConst *GetIdxScanDirectionStr(EdxlIndexScanDirection);

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	virtual void AssertValid(const CDXLNode *,
							 BOOL validate_children) const = 0;
#endif	// GPOS_DEBUG
};

}  // namespace gpdxl

#endif	// !GPDXL_CDXLOperator_H

// EOF
