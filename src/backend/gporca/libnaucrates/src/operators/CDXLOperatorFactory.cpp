//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLOperatorFactory.cpp
//
//	@doc:
//		Implementation of the factory methods for creation of DXL elements.
//---------------------------------------------------------------------------

#include "gpos/string/CWStringConst.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/common/clibwrapper.h"

#include "naucrates/md/CMDIdColStats.h"
#include "naucrates/md/CMDIdRelStats.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/CMDIdGPDBCtas.h"
#include "naucrates/md/CMDIdCast.h"
#include "naucrates/md/CMDIdScCmp.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLDatumGeneric.h"
#include "naucrates/dxl/operators/CDXLDatumStatsLintMappable.h"
#include "naucrates/dxl/operators/CDXLDatumStatsDoubleMappable.h"
#include "naucrates/dxl/operators/CDXLDatumInt2.h"
#include "naucrates/dxl/operators/CDXLDatumInt4.h"
#include "naucrates/dxl/operators/CDXLDatumInt8.h"
#include "naucrates/dxl/operators/CDXLDatumBool.h"
#include "naucrates/dxl/operators/CDXLDatumOid.h"

#include "naucrates/dxl/operators/dxlops.h"

#include <xercesc/util/NumberFormatException.hpp>

using namespace gpos;
using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

#define GPDXL_GPDB_MDID_COMPONENTS 3
#define GPDXL_DEFAULT_USERID 0

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLTblScan
//
//	@doc:
//		Construct a table scan operator
//
//---------------------------------------------------------------------------
CDXLPhysical*
CDXLOperatorFactory::MakeDXLTblScan
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes & // attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	return GPOS_NEW(mp) CDXLPhysicalTableScan(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLSubqScan
//
//	@doc:
//		Construct a subquery scan operator
//
//---------------------------------------------------------------------------
CDXLPhysical*
CDXLOperatorFactory::MakeDXLSubqScan
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// parse subquery name from attributes
	const XMLCh *subquery_name_xml = ExtractAttrValue
								(
								attrs,
								EdxltokenAlias,
								EdxltokenPhysicalSubqueryScan
								);

	CWStringDynamic *subquery_name_str = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, subquery_name_xml);
	

	// create a copy of the string in the CMDName constructor
	CMDName *subquery_name = GPOS_NEW(mp) CMDName(mp, subquery_name_str);
	
	GPOS_DELETE(subquery_name_str);

	return GPOS_NEW(mp) CDXLPhysicalSubqueryScan(mp, subquery_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLResult
//
//	@doc:
//		Construct a result operator
//
//---------------------------------------------------------------------------
CDXLPhysical*
CDXLOperatorFactory::MakeDXLResult
	(
	CDXLMemoryManager *dxl_memory_manager
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	return GPOS_NEW(mp) CDXLPhysicalResult(mp);
}

//		Construct a hashjoin operator
CDXLPhysical*
CDXLOperatorFactory::MakeDXLHashJoin
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	const XMLCh *join_type_xml = ExtractAttrValue
									(
									attrs,
									EdxltokenJoinType,
									EdxltokenPhysicalHashJoin
									);
	
	EdxlJoinType join_type = ParseJoinType(join_type_xml, CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalHashJoin));
	
	return GPOS_NEW(mp) CDXLPhysicalHashJoin(mp, join_type);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLNLJoin
//
//	@doc:
//		Construct a nested loop join operator
//
//---------------------------------------------------------------------------
CDXLPhysical*
CDXLOperatorFactory::MakeDXLNLJoin
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	const XMLCh *join_type_xml = ExtractAttrValue
									(
									attrs,
									EdxltokenJoinType,
									EdxltokenPhysicalNLJoin
									);
	
	BOOL is_index_nlj = false;
	const XMLCh *index_nlj_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenPhysicalNLJoinIndex));
	if (NULL != index_nlj_xml)
	{
		is_index_nlj = ConvertAttrValueToBool
						(
						dxl_memory_manager,
						index_nlj_xml,
						EdxltokenPhysicalNLJoinIndex,
						EdxltokenPhysicalNLJoin
						);
	}

	// identify if nest params are expected in dxl
	BOOL nest_params_exists = false;
	const XMLCh *nest_param_exists_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenNLJIndexOuterRefAsParam));
	if (NULL != nest_param_exists_xml)
	{
		nest_params_exists = ConvertAttrValueToBool
						(
						dxl_memory_manager,
						nest_param_exists_xml,
						EdxltokenNLJIndexOuterRefAsParam,
						EdxltokenPhysicalNLJoin
						);
	}

	EdxlJoinType join_type = ParseJoinType(join_type_xml, CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalNLJoin));
	
	return GPOS_NEW(mp) CDXLPhysicalNLJoin(mp, join_type, is_index_nlj, nest_params_exists);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLMergeJoin
//
//	@doc:
//		Construct a merge join operator
//
//---------------------------------------------------------------------------
CDXLPhysical*
CDXLOperatorFactory::MakeDXLMergeJoin
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	const XMLCh *join_type_xml = ExtractAttrValue
									(
									attrs,
									EdxltokenJoinType,
									EdxltokenPhysicalMergeJoin
									);
	
	EdxlJoinType join_type = ParseJoinType(join_type_xml, CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalMergeJoin));
	
	BOOL is_unique_outer = ExtractConvertAttrValueToBool
								(
								dxl_memory_manager,
								attrs,
								EdxltokenMergeJoinUniqueOuter,
								EdxltokenPhysicalMergeJoin
								);
	
	return GPOS_NEW(mp) CDXLPhysicalMergeJoin(mp, join_type, is_unique_outer);
}

//		Construct a gather motion operator
CDXLPhysical *
CDXLOperatorFactory::MakeDXLGatherMotion
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{	
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	CDXLPhysicalGatherMotion *dxl_op = GPOS_NEW(mp) CDXLPhysicalGatherMotion(mp);
	SetSegmentInfo(dxl_memory_manager, dxl_op, attrs, EdxltokenPhysicalGatherMotion);

	return dxl_op;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLBroadcastMotion
//
//	@doc:
//		Construct a broadcast motion operator
//
//---------------------------------------------------------------------------
CDXLPhysical *
CDXLOperatorFactory::MakeDXLBroadcastMotion
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	CDXLPhysicalBroadcastMotion *dxl_op = GPOS_NEW(mp) CDXLPhysicalBroadcastMotion(mp);
	SetSegmentInfo(dxl_memory_manager, dxl_op, attrs, EdxltokenPhysicalBroadcastMotion);
	
	return dxl_op;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLRedistributeMotion
//
//	@doc:
//		Construct a redistribute motion operator
//
//---------------------------------------------------------------------------
CDXLPhysical *
CDXLOperatorFactory::MakeDXLRedistributeMotion
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	BOOL is_duplicate_sensitive = false;
	
	const XMLCh *duplicate_sensitive_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenDuplicateSensitive));
	if (NULL != duplicate_sensitive_xml)
	{
		is_duplicate_sensitive = ConvertAttrValueToBool(dxl_memory_manager, duplicate_sensitive_xml, EdxltokenDuplicateSensitive, EdxltokenPhysicalRedistributeMotion);
	}

	CDXLPhysicalRedistributeMotion *dxl_op = GPOS_NEW(mp) CDXLPhysicalRedistributeMotion(mp, is_duplicate_sensitive);
	SetSegmentInfo(dxl_memory_manager, dxl_op, attrs, EdxltokenPhysicalRedistributeMotion);

	return dxl_op;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLRoutedMotion
//
//	@doc:
//		Construct a routed motion operator
//
//---------------------------------------------------------------------------
CDXLPhysical *
CDXLOperatorFactory::MakeDXLRoutedMotion
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	ULONG segment_colid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(dxl_memory_manager, attrs, EdxltokenSegmentIdCol, EdxltokenPhysicalRoutedDistributeMotion);

	CDXLPhysicalRoutedDistributeMotion *dxl_op = GPOS_NEW(mp) CDXLPhysicalRoutedDistributeMotion(mp, segment_colid);
	SetSegmentInfo(dxl_memory_manager, dxl_op, attrs, EdxltokenPhysicalRoutedDistributeMotion);
	
	return dxl_op;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLRandomMotion
//
//	@doc:
//		Construct a random motion operator
//
//---------------------------------------------------------------------------
CDXLPhysical *
CDXLOperatorFactory::MakeDXLRandomMotion
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	BOOL is_duplicate_sensitive = false;

	const XMLCh *duplicate_sensitive_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenDuplicateSensitive));
	if (NULL != duplicate_sensitive_xml)
	{
		is_duplicate_sensitive = ConvertAttrValueToBool(dxl_memory_manager, duplicate_sensitive_xml, EdxltokenDuplicateSensitive, EdxltokenPhysicalRandomMotion);
	}

	CDXLPhysicalRandomMotion *dxl_op = GPOS_NEW(mp) CDXLPhysicalRandomMotion(mp, is_duplicate_sensitive);
	SetSegmentInfo(dxl_memory_manager, dxl_op, attrs, EdxltokenPhysicalRandomMotion);
	
	return dxl_op;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLAppend
//	@doc:
//		Construct an Append operator
//
//---------------------------------------------------------------------------
CDXLPhysical*
CDXLOperatorFactory::MakeDXLAppend
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	BOOL is_target = ExtractConvertAttrValueToBool
						(
						dxl_memory_manager,
						attrs,
						EdxltokenAppendIsTarget,
						EdxltokenPhysicalAppend
						);
	
	BOOL is_zapped = ExtractConvertAttrValueToBool
						(
						dxl_memory_manager,
						attrs,
						EdxltokenAppendIsZapped,
						EdxltokenPhysicalAppend
						);

	return GPOS_NEW(mp) CDXLPhysicalAppend(mp, is_target, is_zapped);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLLimit
//	@doc:
//		Construct a Limit operator
//
//---------------------------------------------------------------------------
CDXLPhysical*
CDXLOperatorFactory::MakeDXLLimit
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes & // attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	return GPOS_NEW(mp) CDXLPhysicalLimit(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLLimitCount
//
//	@doc:
//		Construct a Limit Count operator
//
//---------------------------------------------------------------------------
CDXLScalar*
CDXLOperatorFactory::MakeDXLLimitCount
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes & // attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	return GPOS_NEW(mp) CDXLScalarLimitCount(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLLimitOffset
//
//	@doc:
//		Construct a Limit Offset operator
//
//---------------------------------------------------------------------------
CDXLScalar*
CDXLOperatorFactory::MakeDXLLimitOffset
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes & // attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	return GPOS_NEW(mp) CDXLScalarLimitOffset(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLAgg
//
//	@doc:
//		Construct an aggregate operator
//
//---------------------------------------------------------------------------
CDXLPhysical *
CDXLOperatorFactory::MakeDXLAgg
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes & attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	const XMLCh *agg_strategy_xml = ExtractAttrValue
										(
										attrs,
										EdxltokenAggStrategy,
										EdxltokenPhysicalAggregate
										);
	
	EdxlAggStrategy dxl_agg_strategy = EdxlaggstrategySentinel;
	
	if (0 == XMLString::compareString
							(
							CDXLTokens::XmlstrToken(EdxltokenAggStrategyPlain),
							agg_strategy_xml
							))
	{
		dxl_agg_strategy = EdxlaggstrategyPlain;
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenAggStrategySorted),
									agg_strategy_xml))
	{
		dxl_agg_strategy = EdxlaggstrategySorted;
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenAggStrategyHashed),
									agg_strategy_xml))
	{
		dxl_agg_strategy = EdxlaggstrategyHashed;
	}
	else
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenAggStrategy)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalAggregate)->GetBuffer()
			);		
	}
	
	BOOL stream_safe = false;

	const XMLCh *stream_safe_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenAggStreamSafe));
	if (NULL != stream_safe_xml)
	{
		stream_safe = ConvertAttrValueToBool
						(
						dxl_memory_manager,
						stream_safe_xml,
						EdxltokenAggStreamSafe,
						EdxltokenPhysicalAggregate
						);
	}

	return GPOS_NEW(mp) CDXLPhysicalAgg(mp, dxl_agg_strategy, stream_safe);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLSort
//
//	@doc:
//		Construct a sort operator
//
//---------------------------------------------------------------------------
CDXLPhysical *
CDXLOperatorFactory::MakeDXLSort
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// parse discard duplicates and nulls first properties from the attributes
	BOOL discard_duplicates = ExtractConvertAttrValueToBool
								(
								dxl_memory_manager,
								attrs,
								EdxltokenSortDiscardDuplicates,
								EdxltokenPhysicalSort
								);
	
	return GPOS_NEW(mp) CDXLPhysicalSort(mp, discard_duplicates);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLMaterialize
//
//	@doc:
//		Construct a materialize operator
//
//---------------------------------------------------------------------------
CDXLPhysical *
CDXLOperatorFactory::MakeDXLMaterialize
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// parse spooling info from the attributes
	
	CDXLPhysicalMaterialize *materialize_dxlnode = NULL;
	
	// is this a multi-slice spool
	BOOL eager_materialize = ExtractConvertAttrValueToBool
								(
								dxl_memory_manager,
								attrs,
								EdxltokenMaterializeEager,
								EdxltokenPhysicalMaterialize
								);
	
	if (1 == attrs.getLength())
	{
		// no spooling info specified -> create a non-spooling materialize operator
		materialize_dxlnode = GPOS_NEW(mp) CDXLPhysicalMaterialize(mp, eager_materialize);
	}
	else
	{
		// parse spool id
		ULONG spool_id = ExtractConvertAttrValueToUlong
							(
							dxl_memory_manager,
							attrs,
							EdxltokenSpoolId,
							EdxltokenPhysicalMaterialize
							);
	
		// parse id of executor slice
		INT executor_slice = ExtractConvertAttrValueToInt
									(
									dxl_memory_manager,
									attrs,
									EdxltokenExecutorSliceId,
									EdxltokenPhysicalMaterialize
									);
		
		ULONG num_consumer_slices = ExtractConvertAttrValueToUlong
									(
									dxl_memory_manager,
									attrs,
									EdxltokenConsumerSliceCount,
									EdxltokenPhysicalMaterialize
									);
	
		materialize_dxlnode = GPOS_NEW(mp) CDXLPhysicalMaterialize(mp, eager_materialize, spool_id, executor_slice, num_consumer_slices);
	}
	
	return materialize_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLScalarCmp
//
//	@doc:
//		Construct a scalar comparison operator
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLScalarCmp
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	 // get comparison operator from attributes
	const XMLCh *scalar_cmp_xml = ExtractAttrValue(attrs, EdxltokenComparisonOp, EdxltokenScalarComp);
	
	// parse op no and function id
	IMDId *op_id = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenOpNo, EdxltokenScalarComp);

	// parse comparison operator from string
	CWStringDynamic *comp_op_name = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, scalar_cmp_xml);
	
	// copy dynamic string into const string
	CWStringConst *comp_op_name_copy = GPOS_NEW(mp) CWStringConst(mp, comp_op_name->GetBuffer());

	// cleanup
	GPOS_DELETE(comp_op_name);
	
	return GPOS_NEW(mp) CDXLScalarComp(mp, op_id, comp_op_name_copy);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLDistinctCmp
//
//	@doc:
//		Construct a scalar distinct comparison operator
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLDistinctCmp
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// parse operator and function id
	IMDId *op_id = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenOpNo, EdxltokenScalarDistinctComp);

	return GPOS_NEW(mp) CDXLScalarDistinctComp(mp, op_id);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLOpExpr
//
//	@doc:
//		Construct a scalar OpExpr
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLOpExpr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	// get scalar OpExpr from attributes
	const XMLCh *op_expr_xml = ExtractAttrValue
									(
									attrs,
									EdxltokenOpName,
									EdxltokenScalarOpExpr
									);

	IMDId *op_id = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenOpNo,
							EdxltokenScalarOpExpr
							);

	IMDId *return_type_mdid = NULL;
	const XMLCh *return_type_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenOpType));

	if (NULL != return_type_xml)
	{
		return_type_mdid = ExtractConvertAttrValueToMdId(
			dxl_memory_manager, attrs, EdxltokenOpType, EdxltokenScalarOpExpr);
	}

	CWStringDynamic *value = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, op_expr_xml);
	CWStringConst *value_copy = GPOS_NEW(mp) CWStringConst(mp, value->GetBuffer());
	GPOS_DELETE(value);

	return GPOS_NEW(mp) CDXLScalarOpExpr(mp, op_id, return_type_mdid, value_copy);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLArrayComp
//
//	@doc:
//		Construct a scalar array comparison
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLArrayComp
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	 // get attributes
	const XMLCh *op_expr_xml = ExtractAttrValue
								(
								attrs,
								EdxltokenOpName,
								EdxltokenScalarArrayComp
								);
	
	const XMLCh *op_type_xml = ExtractAttrValue
									(
									attrs,
									EdxltokenOpType,
									EdxltokenScalarArrayComp
									);

	// parse operator no and function id
	IMDId *op_id = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenOpNo,
							EdxltokenScalarArrayComp
							);

	EdxlArrayCompType array_comp_type = Edxlarraycomptypeany;

	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenOpTypeAll), op_type_xml))
	{
		array_comp_type = Edxlarraycomptypeall;
	}
	else if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenOpTypeAny), op_type_xml))
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenOpType)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenScalarArrayComp)->GetBuffer()
			);
	}

	CWStringDynamic *opname = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, op_expr_xml);
	CWStringConst *opname_copy = GPOS_NEW(mp) CWStringConst(mp, opname->GetBuffer());
	GPOS_DELETE(opname);

	return GPOS_NEW(mp) CDXLScalarArrayComp(mp, op_id, opname_copy, array_comp_type);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLBoolExpr
//
//	@doc:
//		Construct a scalar BoolExpr
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLBoolExpr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const EdxlBoolExprType edxlboolexprType
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	return GPOS_NEW(mp) CDXLScalarBoolExpr(mp, edxlboolexprType);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLBooleanTest
//
//	@doc:
//		Construct a scalar BooleanTest
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLBooleanTest
	(
	CDXLMemoryManager *dxl_memory_manager,
	const EdxlBooleanTestType edxlbooleantesttype
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	return GPOS_NEW(mp) CDXLScalarBooleanTest(mp,	edxlbooleantesttype);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLSubPlan
//
//	@doc:
//		Construct a SubPlan node
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLSubPlan
	(
	CDXLMemoryManager *dxl_memory_manager,
	IMDId *mdid,
									CDXLColRefArray *dxl_colref_array,
	EdxlSubPlanType dxl_subplan_type,
	CDXLNode *dxlnode_test_expr
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	return GPOS_NEW(mp) CDXLScalarSubPlan(mp, mdid, dxl_colref_array, dxl_subplan_type, dxlnode_test_expr);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLNullTest
//
//	@doc:
//		Construct a scalar NullTest
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLNullTest
	(
	CDXLMemoryManager *dxl_memory_manager,
	const BOOL is_null
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	return GPOS_NEW(mp) CDXLScalarNullTest(mp, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLCast
//
//	@doc:
//		Construct a scalar RelabelType
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLCast
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// parse type id and function id
	IMDId *mdid_type = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenTypeId,
							EdxltokenScalarCast
							);

	IMDId *mdid_func = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenFuncId,
							EdxltokenScalarCast
							);

	return GPOS_NEW(mp) CDXLScalarCast(mp, mdid_type, mdid_func);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLCoerceToDomain
//
//	@doc:
//		Construct a scalar coerce
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLCoerceToDomain
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	// parse type id and function id
	IMDId *mdid_type = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenTypeId, EdxltokenScalarCoerceToDomain);
	INT type_modifier = ExtractConvertAttrValueToInt(dxl_memory_manager, attrs, EdxltokenTypeMod, EdxltokenScalarCoerceToDomain, true, default_type_modifier);
	ULONG coercion_form = ExtractConvertAttrValueToUlong(dxl_memory_manager, attrs, EdxltokenCoercionForm, EdxltokenScalarCoerceToDomain);
	INT location = ExtractConvertAttrValueToInt(dxl_memory_manager, attrs, EdxltokenLocation, EdxltokenScalarCoerceToDomain);

	return GPOS_NEW(mp) CDXLScalarCoerceToDomain(mp, mdid_type, type_modifier, (EdxlCoercionForm) coercion_form, location);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLCoerceViaIO
//
//	@doc:
//		Construct a scalar coerce
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLCoerceViaIO
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	// parse type id and function id
	IMDId *mdid_type = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenTypeId, EdxltokenScalarCoerceViaIO);
	INT type_modifier = ExtractConvertAttrValueToInt(dxl_memory_manager, attrs, EdxltokenTypeMod, EdxltokenScalarCoerceViaIO, true, default_type_modifier);
	ULONG coercion_form = ExtractConvertAttrValueToUlong(dxl_memory_manager, attrs, EdxltokenCoercionForm, EdxltokenScalarCoerceViaIO);
	INT location = ExtractConvertAttrValueToInt(dxl_memory_manager, attrs, EdxltokenLocation, EdxltokenScalarCoerceViaIO);

	return GPOS_NEW(mp) CDXLScalarCoerceViaIO(mp, mdid_type, type_modifier, (EdxlCoercionForm) coercion_form, location);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLArrayCoerceExpr
//
//	@doc:
//		Construct a scalar array coerce expression
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLArrayCoerceExpr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	IMDId *element_func = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenElementFunc, EdxltokenScalarArrayCoerceExpr);
	IMDId *mdid_type = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenTypeId, EdxltokenScalarArrayCoerceExpr);
	INT type_modifier = ExtractConvertAttrValueToInt(dxl_memory_manager, attrs, EdxltokenTypeMod, EdxltokenScalarArrayCoerceExpr, true, default_type_modifier);
	BOOL is_explicit = ExtractConvertAttrValueToBool(dxl_memory_manager, attrs, EdxltokenIsExplicit, EdxltokenScalarArrayCoerceExpr);
	ULONG coercion_form = ExtractConvertAttrValueToUlong(dxl_memory_manager, attrs, EdxltokenCoercionForm, EdxltokenScalarArrayCoerceExpr);
	INT location = ExtractConvertAttrValueToInt(dxl_memory_manager, attrs, EdxltokenLocation, EdxltokenScalarArrayCoerceExpr);

	return GPOS_NEW(mp) CDXLScalarArrayCoerceExpr(mp, element_func, mdid_type, type_modifier, is_explicit, (EdxlCoercionForm) coercion_form, location);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLConstValue
//
//	@doc:
//		Construct a scalar Const
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLConstValue
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	CDXLDatum *dxl_datum = GetDatumVal(dxl_memory_manager, attrs, EdxltokenScalarConstValue);
	
	return GPOS_NEW(mp) CDXLScalarConstValue(mp, dxl_datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLIfStmt
//
//	@doc:
//		Construct an if statement operator
//
//---------------------------------------------------------------------------
CDXLScalar*
CDXLOperatorFactory::MakeDXLIfStmt
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	// get the type id
	IMDId *mdid_type = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenTypeId,
							EdxltokenScalarIfStmt
							);

	return GPOS_NEW(mp) CDXLScalarIfStmt(mp, mdid_type);
}


//		Construct an funcexpr operator
CDXLScalar*
CDXLOperatorFactory::MakeDXLFuncExpr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	IMDId *mdid_func = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenFuncId,
							EdxltokenScalarFuncExpr
							);

	BOOL is_retset = ExtractConvertAttrValueToBool
					(
					dxl_memory_manager,
					attrs,
					EdxltokenFuncRetSet,
					EdxltokenScalarFuncExpr
					);

	IMDId *mdid_return_type = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenTypeId,
							EdxltokenScalarFuncExpr
							);

	INT type_modifier = ExtractConvertAttrValueToInt
						(
						dxl_memory_manager,
						attrs,
						EdxltokenTypeMod,
						EdxltokenScalarCast,
						true,
						default_type_modifier
						);

	return GPOS_NEW(mp) CDXLScalarFuncExpr(mp, mdid_func, mdid_return_type, type_modifier, is_retset);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLAggFunc
//
//	@doc:
//		Construct an AggRef operator
//
//---------------------------------------------------------------------------
CDXLScalar*
CDXLOperatorFactory::MakeDXLAggFunc
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	IMDId *agg_mdid = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenAggrefOid,
							EdxltokenScalarAggref
							);

	const XMLCh *agg_stage_xml  = ExtractAttrValue
									(
									attrs,
									EdxltokenAggrefStage,
									EdxltokenScalarAggref
									);
	
	BOOL is_distinct = ExtractConvertAttrValueToBool
						(
						dxl_memory_manager,
						attrs,
						EdxltokenAggrefDistinct,
						EdxltokenScalarAggref
						);

	EdxlAggrefStage agg_stage = EdxlaggstageFinal;

	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenAggrefStageNormal), agg_stage_xml))
	{
		agg_stage = EdxlaggstageNormal;
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenAggrefStagePartial), agg_stage_xml))
	{
		agg_stage = EdxlaggstagePartial;
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenAggrefStageIntermediate), agg_stage_xml))
	{
		agg_stage = EdxlaggstageIntermediate;
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenAggrefStageFinal), agg_stage_xml))
	{
		agg_stage = EdxlaggstageFinal;
	}
	else
	{
		// turn Xerces exception in optimizer exception	
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenAggrefStage)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenScalarAggref)->GetBuffer()
			);
	}

	IMDId *resolved_rettype = NULL;
	const XMLCh *return_type_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenTypeId));
	if (NULL != return_type_xml)
	{
		resolved_rettype = ExtractConvertAttrValueToMdId
									(
									dxl_memory_manager,
									attrs,
									EdxltokenTypeId,
									EdxltokenScalarAggref
									);
	}

	return GPOS_NEW(mp) CDXLScalarAggref(mp, agg_mdid, resolved_rettype, is_distinct, agg_stage);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseDXLFrameBoundary
//
//	@doc:
//		Parse the frame boundary
//
//---------------------------------------------------------------------------
EdxlFrameBoundary
CDXLOperatorFactory::ParseDXLFrameBoundary
	(
	const Attributes& attrs,
	Edxltoken token_type
	)
{
	const XMLCh *frame_boundary_xml  = ExtractAttrValue(attrs, token_type, EdxltokenWindowFrame);

	EdxlFrameBoundary frame_boundary = EdxlfbSentinel;
	ULONG window_frame_boundary_to_frame_boundary_mapping[][2] =
					{
					{EdxlfbUnboundedPreceding, EdxltokenWindowBoundaryUnboundedPreceding},
					{EdxlfbBoundedPreceding, EdxltokenWindowBoundaryBoundedPreceding},
					{EdxlfbCurrentRow, EdxltokenWindowBoundaryCurrentRow},
					{EdxlfbUnboundedFollowing, EdxltokenWindowBoundaryUnboundedFollowing},
					{EdxlfbBoundedFollowing, EdxltokenWindowBoundaryBoundedFollowing},
					{EdxlfbDelayedBoundedPreceding, EdxltokenWindowBoundaryDelayedBoundedPreceding},
					{EdxlfbDelayedBoundedFollowing, EdxltokenWindowBoundaryDelayedBoundedFollowing}
					};

	const ULONG arity = GPOS_ARRAY_SIZE(window_frame_boundary_to_frame_boundary_mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *current = window_frame_boundary_to_frame_boundary_mapping[ul];
		Edxltoken current_window = (Edxltoken) current[1];
		if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(current_window), frame_boundary_xml))
		{
			frame_boundary = (EdxlFrameBoundary) current[0];
			break;
		}
	}

	if (EdxlfbSentinel == frame_boundary)
	{
		// turn Xerces exception in optimizer exception
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(token_type)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenWindowFrame)->GetBuffer()
			);
	}

	return frame_boundary;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseDXLFrameSpec
//
//	@doc:
//		Parse the frame specification
//
//---------------------------------------------------------------------------
EdxlFrameSpec
CDXLOperatorFactory::ParseDXLFrameSpec
	(
	const Attributes& attrs
	)
{
	const XMLCh *frame_spec_xml  = ExtractAttrValue(attrs, EdxltokenWindowFrameSpec, EdxltokenWindowFrame);

	EdxlFrameSpec frame_spec = EdxlfsSentinel;
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowFSRow), frame_spec_xml))
	{
		frame_spec = EdxlfsRow;
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowFSRange), frame_spec_xml))
	{
		frame_spec = EdxlfsRange;
	}
	else
	{
		// turn Xerces exception in optimizer exception
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenWindowFrameSpec)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenWindowFrame)->GetBuffer()
			);
	}

	return frame_spec;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseFrameExclusionStrategy
//
//	@doc:
//		Parse the frame exclusion strategy
//
//---------------------------------------------------------------------------
EdxlFrameExclusionStrategy
CDXLOperatorFactory::ParseFrameExclusionStrategy
	(
	const Attributes& attrs
	)
{
	const XMLCh *frame_exc_strategy_xml  = ExtractAttrValue(attrs, EdxltokenWindowExclusionStrategy, EdxltokenWindowFrame);

	ULONG window_frame_boundary_to_frame_boundary_mapping[][2] =
			{
			{EdxlfesNone, EdxltokenWindowESNone},
			{EdxlfesNulls, EdxltokenWindowESNulls},
			{EdxlfesCurrentRow, EdxltokenWindowESCurrentRow},
			{EdxlfesGroup, EdxltokenWindowESGroup},
			{EdxlfesTies, EdxltokenWindowESTies}
			};

	EdxlFrameExclusionStrategy frame_exc_strategy = EdxlfesSentinel;
	const ULONG arity = GPOS_ARRAY_SIZE(window_frame_boundary_to_frame_boundary_mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *current = window_frame_boundary_to_frame_boundary_mapping[ul];
		Edxltoken current_window = (Edxltoken) current[1];
		if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(current_window), frame_exc_strategy_xml))
		{
			frame_exc_strategy = (EdxlFrameExclusionStrategy) current[0];
			break;
		}
	}

	if (EdxlfesSentinel == frame_exc_strategy)
	{
		// turn Xerces exception in optimizer exception
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenWindowExclusionStrategy)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenWindowFrame)->GetBuffer()
			);
	}

	return frame_exc_strategy;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLArray
//
//	@doc:
//		Construct an array operator
//
//---------------------------------------------------------------------------
CDXLScalar*
CDXLOperatorFactory::MakeDXLArray
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	IMDId *elem_type_mdid = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenArrayElementType, EdxltokenScalarArray);
	IMDId *array_type_mdid = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenArrayType, EdxltokenScalarArray);
	BOOL is_multidimenstional = ExtractConvertAttrValueToBool(dxl_memory_manager, attrs, EdxltokenArrayMultiDim, EdxltokenScalarArray);

	return GPOS_NEW(mp) CDXLScalarArray(mp, elem_type_mdid, array_type_mdid, is_multidimenstional);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLScalarIdent
//
//	@doc:
//		Construct a scalar identifier operator
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLScalarIdent
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	CDXLColRef *dxl_colref = MakeDXLColRef(dxl_memory_manager, attrs, EdxltokenScalarIdent);

	return GPOS_NEW(mp) CDXLScalarIdent(mp, dxl_colref);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLProjElem
//
//	@doc:
//		Construct a proj elem operator
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLProjElem
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// parse alias from attributes	
	const XMLCh *xml_alias = ExtractAttrValue
								(
								attrs,
								EdxltokenAlias,
								EdxltokenScalarProjElem
								);
	
	// parse column id
	ULONG id = ExtractConvertAttrValueToUlong
					(
					dxl_memory_manager,
					attrs,
					EdxltokenColId,
					EdxltokenScalarProjElem
					);
	
	CWStringDynamic *alias = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, xml_alias);

	// create a copy of the string in the CMDName constructor
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, alias);
	
	GPOS_DELETE(alias);
	
	return GPOS_NEW(mp) CDXLScalarProjElem(mp, id, mdname);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLHashExpr
//
//	@doc:
//		Construct a hash expr operator
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLHashExpr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// get column type id and type name from attributes

	IMDId *mdid_type = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenOpfamily,
							EdxltokenScalarHashExpr,
							true /* is_optional */,
							NULL /* default_val */
							);

	return GPOS_NEW(mp) CDXLScalarHashExpr(mp, mdid_type);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLSortCol
//
//	@doc:
//		Construct a sorting column description
//
//---------------------------------------------------------------------------
CDXLScalar *
CDXLOperatorFactory::MakeDXLSortCol
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// get column id from attributes
	ULONG colid = ExtractConvertAttrValueToUlong
						(
						dxl_memory_manager,
						attrs,
						EdxltokenColId,
						EdxltokenScalarSortCol
						);

	// get sorting operator name
	const XMLCh *sort_op_xml = ExtractAttrValue
										(
										attrs,
										EdxltokenSortOpName,
										EdxltokenScalarSortCol
										);
	CWStringDynamic *sort_op_name = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, sort_op_xml);

	// get null first property
	BOOL nulls_first = ExtractConvertAttrValueToBool
						(
						dxl_memory_manager,
						attrs,
						EdxltokenSortNullsFirst,
						EdxltokenPhysicalSort
						);
	
	// parse sorting operator id
	IMDId *sort_op_id = ExtractConvertAttrValueToMdId
								(
								dxl_memory_manager,
								attrs,
								EdxltokenSortOpId,
								EdxltokenPhysicalSort
								);

	// copy dynamic string into const string
	CWStringConst *sort_op_name_copy = GPOS_NEW(mp) CWStringConst(mp, sort_op_name->GetBuffer());

	GPOS_DELETE(sort_op_name);

	return GPOS_NEW(mp) CDXLScalarSortCol(mp, colid, sort_op_id, sort_op_name_copy, nulls_first);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLOperatorCost
//
//	@doc:
//		Construct a cost estimates element
//
//---------------------------------------------------------------------------
CDXLOperatorCost *
CDXLOperatorFactory::MakeDXLOperatorCost
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	const XMLCh *startup_cost_xml = ExtractAttrValue
										(
										attrs,
										EdxltokenStartupCost,
										EdxltokenCost
										);
	
	const XMLCh *total_cost_xml = ExtractAttrValue
										(
										attrs,
										EdxltokenTotalCost,
										EdxltokenCost
										);
	
	const XMLCh *rows_xml = ExtractAttrValue
								(
								attrs,
								EdxltokenRows,
								EdxltokenCost
								);
	
	const XMLCh *width_xml = ExtractAttrValue
								(
								attrs,
								EdxltokenWidth,
								EdxltokenCost
								);
	
	CWStringDynamic *startup_cost_str = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, startup_cost_xml);
	CWStringDynamic *total_cost_str = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, total_cost_xml);
	CWStringDynamic *rows_out_str = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, rows_xml);
	CWStringDynamic *width_str = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, width_xml);
	
	return GPOS_NEW(mp) CDXLOperatorCost
						(
						startup_cost_str,
						total_cost_str,
						rows_out_str,
						width_str
						);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLTableDescr
//
//	@doc:
//		Construct a table descriptor
//
//---------------------------------------------------------------------------
CDXLTableDescr *
CDXLOperatorFactory::MakeDXLTableDescr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// parse table descriptor from attributes
	const XMLCh *xml_str_table_name = ExtractAttrValue
									(
									attrs,
									EdxltokenTableName,
									EdxltokenTableDescr
									);

	CMDName *mdname = CDXLUtils::CreateMDNameFromXMLChar(dxl_memory_manager, xml_str_table_name);
	
	// parse metadata id
	IMDId *mdid = ExtractConvertAttrValueToMdId
						(
						dxl_memory_manager,
						attrs,
						EdxltokenMdid,
						EdxltokenTableDescr
						);

	// parse execute as user value if the attribute is specified
	ULONG user_id = GPDXL_DEFAULT_USERID;
	const XMLCh *execute_as_user_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenExecuteAsUser));

	if (NULL != execute_as_user_xml)
	{
		user_id = ConvertAttrValueToUlong(dxl_memory_manager, execute_as_user_xml, EdxltokenExecuteAsUser, EdxltokenTableDescr);
	}
					
	return GPOS_NEW(mp) CDXLTableDescr(mp, mdid, mdname, user_id);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLIndexDescr
//
//	@doc:
//		Construct an index descriptor
//
//---------------------------------------------------------------------------
CDXLIndexDescr *
CDXLOperatorFactory::MakeDXLIndexDescr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	// parse index descriptor from attributes
	const XMLCh *index_name_xml = ExtractAttrValue
									(
									attrs,
									EdxltokenIndexName,
									EdxltokenIndexDescr
									);

	CWStringDynamic *index_name = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, index_name_xml);

	// parse metadata id
	IMDId *mdid = ExtractConvertAttrValueToMdId
						(
						dxl_memory_manager,
						attrs,
						EdxltokenMdid,
						EdxltokenIndexDescr
						);

	// create a copy of the string in the CMDName constructor
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, index_name);
	GPOS_DELETE(index_name);

	return GPOS_NEW(mp) CDXLIndexDescr(mp, mdid, mdname);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeColumnDescr
//
//	@doc:
//		Construct a column descriptor
//
//---------------------------------------------------------------------------
CDXLColDescr *
CDXLOperatorFactory::MakeColumnDescr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	// parse column name from attributes
	const XMLCh *column_name_xml = ExtractAttrValue
										(
										attrs,
										EdxltokenColName,
										EdxltokenColDescr
										);

	// parse column id
	ULONG id = ExtractConvertAttrValueToUlong
					(
					dxl_memory_manager,
					attrs,
					EdxltokenColId,
					EdxltokenColDescr
					);
	
	// parse attno
	INT attno = ExtractConvertAttrValueToInt
					(
					dxl_memory_manager,
					attrs,
					EdxltokenAttno,
					EdxltokenColDescr
					);
	
	if (0 == attno)
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenAttno)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenColDescr)->GetBuffer()
			);
	}
	
	// parse column type id
	IMDId *mdid_type = ExtractConvertAttrValueToMdId
							(
							dxl_memory_manager,
							attrs,
							EdxltokenTypeId,
							EdxltokenColDescr
							);

	// parse optional type modifier from attributes
	INT type_modifier = ExtractConvertAttrValueToInt
						(
						dxl_memory_manager,
						attrs,
						EdxltokenTypeMod,
						EdxltokenColDescr,
						true,
						default_type_modifier
						);

	BOOL col_dropped = false;
	
	const XMLCh *col_dropped_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColDropped));
	
	if (NULL != col_dropped_xml)
	{
		// attribute is present: get value
		col_dropped = ConvertAttrValueToBool
						(
						dxl_memory_manager,
						col_dropped_xml,
						EdxltokenColDropped,
						EdxltokenColDescr
						);
	}

	ULONG col_len = gpos::ulong_max;

	// parse column length from attributes
	const XMLCh *col_len_xml =  attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColWidth));

	if (NULL != col_len_xml)
	{
		col_len = ConvertAttrValueToUlong
					(
					dxl_memory_manager,
					col_len_xml,
					EdxltokenColWidth,
					EdxltokenColDescr
					);
	}

	CWStringDynamic *col_name = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager,column_name_xml);

	// create a copy of the string in the CMDName constructor
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, col_name);
	
	GPOS_DELETE(col_name);
	
	return GPOS_NEW(mp) CDXLColDescr(mp, mdname, id, attno, mdid_type, type_modifier, col_dropped, col_len);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeDXLColRef
//
//	@doc:
//		Construct a column reference
//
//---------------------------------------------------------------------------
CDXLColRef *
CDXLOperatorFactory::MakeDXLColRef
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_elem
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	// parse column name from attributes
	const XMLCh *column_name_xml = ExtractAttrValue
									(
									attrs,
									EdxltokenColName,
									target_elem
									);

	// parse column id
	ULONG id = 0;
	const XMLCh *colid_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColId));
	if (NULL == colid_xml)
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLMissingAttribute,
			CDXLTokens::GetDXLTokenStr(EdxltokenColRef)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(target_elem)->GetBuffer()
			);
	}
	
	id = XMLString::parseInt(colid_xml, dxl_memory_manager);
		
	CWStringDynamic *col_name =  CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager,column_name_xml);

	// create a copy of the string in the CMDName constructor
	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, col_name);
	
	GPOS_DELETE(col_name);

	IMDId *mdid_type = ExtractConvertAttrValueToMdId
						(
						dxl_memory_manager,
						attrs,
						EdxltokenTypeId,
						target_elem
						);

	// parse optional type modifier
	INT type_modifier = ExtractConvertAttrValueToInt
						(
						dxl_memory_manager,
						attrs,
						EdxltokenTypeMod,
						target_elem,
						true,
						default_type_modifier
						);
	
	return GPOS_NEW(mp) CDXLColRef(mp, mdname, id, mdid_type, type_modifier);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseOutputSegId
//
//	@doc:
//		Parse an output segment index
//
//---------------------------------------------------------------------------
INT
CDXLOperatorFactory::ParseOutputSegId
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get output segment index from attributes
	const XMLCh *seg_id_xml = ExtractAttrValue
									(
									attrs,
									EdxltokenSegId,
									EdxltokenSegment
									);

	// parse segment id from string
	INT segment_id = -1;
	try
	{
		segment_id = XMLString::parseInt(seg_id_xml, dxl_memory_manager);
	}
	catch (const NumberFormatException& toCatch)
	{
		// turn Xerces exception into GPOS exception
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenSegId)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenSegment)->GetBuffer()
			);
	}

	return segment_id;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractAttrValue
//
//	@doc:
//    	Extracts the value for the given attribute.
// 		If there is no such attribute defined, and the given optional
// 		flag is set to false then it will raise an exception
//---------------------------------------------------------------------------
const XMLCh *
CDXLOperatorFactory::ExtractAttrValue
	(
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional
	)
{
	const XMLCh *attribute_val_xml = attrs.getValue(CDXLTokens::XmlstrToken(target_attr));

	if (NULL == attribute_val_xml && !is_optional)
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLMissingAttribute,
			CDXLTokens::GetDXLTokenStr(target_attr)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(target_elem)->GetBuffer()
			);
	}

	return attribute_val_xml;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToUlong
//
//	@doc:
//	  	Converts the attribute value to ULONG
//
//---------------------------------------------------------------------------
ULONG
CDXLOperatorFactory::ConvertAttrValueToUlong
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *attribute_val_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(attribute_val_xml != NULL);
	ULONG attr = 0;
	try
	{
		attr = XMLString::parseInt(attribute_val_xml, dxl_memory_manager);
	}
	catch (const NumberFormatException& toCatch)
	{
		// turn Xerces exception into GPOS exception
		GPOS_RAISE
		(
		gpdxl::ExmaDXL,
		gpdxl::ExmiDXLInvalidAttributeValue,
		CDXLTokens::GetDXLTokenStr(target_attr)->GetBuffer(),
		CDXLTokens::GetDXLTokenStr(target_elem)->GetBuffer()
		);
	}
	return attr;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToUllong
//
//	@doc:
//	  	Converts the attribute value to ULLONG
//
//---------------------------------------------------------------------------
ULLONG
CDXLOperatorFactory::ConvertAttrValueToUllong
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *attribute_val_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(attribute_val_xml != NULL);

	CHAR *attr = XMLString::transcode(attribute_val_xml, dxl_memory_manager);
	GPOS_ASSERT(NULL != attr);

	CHAR **end = NULL;
	LINT converted_val = clib::Strtoll(attr, end, 10 /*ulBase*/);

	if ((NULL != end && attr == *end) ||
		gpos::lint_max == converted_val || gpos::lint_min == converted_val || 0 > converted_val)
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(target_attr)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(target_elem)->GetBuffer()
			);
	}

	XMLString::release(&attr, dxl_memory_manager);

	return (ULLONG) converted_val;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToBool
//
//	@doc:
//	  	Converts the attribute value to BOOL
//
//---------------------------------------------------------------------------
BOOL
CDXLOperatorFactory::ConvertAttrValueToBool
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *attribute_val_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(attribute_val_xml != NULL);
	BOOL flag = false;
	CHAR *attr = XMLString::transcode(attribute_val_xml, dxl_memory_manager);

	if (0 == strncasecmp(attr, "true", 4))
	{
		flag = true;
	}else if (0 != strncasecmp(attr, "false", 5))
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(target_attr)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(target_elem)->GetBuffer()
			);
	}

	XMLString::release(&attr, dxl_memory_manager);
	return flag;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToInt
//
//	@doc:
//	  	Converts the attribute value from xml string to INT
//
//---------------------------------------------------------------------------
INT
CDXLOperatorFactory::ConvertAttrValueToInt
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *attribute_val_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(attribute_val_xml != NULL);
	INT attr = 0;
	try
	{
		attr = XMLString::parseInt(attribute_val_xml, dxl_memory_manager);
	}
	catch (const NumberFormatException& toCatch)
	{
		// turn Xerces exception into GPOS exception
		GPOS_RAISE
		(
		gpdxl::ExmaDXL,
		gpdxl::ExmiDXLInvalidAttributeValue,
		CDXLTokens::GetDXLTokenStr(target_attr)->GetBuffer(),
		CDXLTokens::GetDXLTokenStr(target_elem)->GetBuffer()
		);
	}
	return attr;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToInt
//
//	@doc:
//	  	Extracts the value for the given attribute and converts it into INT
//
//---------------------------------------------------------------------------
INT
CDXLOperatorFactory::ExtractConvertAttrValueToInt
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	INT default_val
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem,
											is_optional
											);

	if (NULL == attr_val_xml)
	{
		return default_val;
	}

	return ConvertAttrValueToInt
			(
			dxl_memory_manager,
			attr_val_xml,
			target_attr,
			target_elem
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToShortInt
//
//	@doc:
//	  	Converts the attribute value from xml string to short int
//
//---------------------------------------------------------------------------
SINT
CDXLOperatorFactory::ConvertAttrValueToShortInt
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *attribute_val_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(attribute_val_xml != NULL);
	SINT attr = 0;
	try
	{
		attr = (SINT) XMLString::parseInt(attribute_val_xml, dxl_memory_manager);
	}
	catch (const NumberFormatException& toCatch)
	{
		// turn Xerces exception into GPOS exception
		GPOS_RAISE
		(
		gpdxl::ExmaDXL,
		gpdxl::ExmiDXLInvalidAttributeValue,
		CDXLTokens::GetDXLTokenStr(target_attr)->GetBuffer(),
		CDXLTokens::GetDXLTokenStr(target_elem)->GetBuffer()
		);
	}
	return attr;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToShortInt
//
//	@doc:
//	  	Extracts the value for the given attribute and converts it into short
//		int
//
//---------------------------------------------------------------------------
SINT
CDXLOperatorFactory::ExtractConvertAttrValueToShortInt
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	SINT default_val
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem,
											is_optional
											);

	if (NULL == attr_val_xml)
	{
		return default_val;
	}

	return ConvertAttrValueToShortInt
			(
			dxl_memory_manager,
			attr_val_xml,
			target_attr,
			target_elem
			);
}

// Converts the attribute value from xml string to char
CHAR
CDXLOperatorFactory::ConvertAttrValueToChar
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *xml_val,
	Edxltoken , // target_attr,
	Edxltoken  // target_elem
	)
{
	GPOS_ASSERT(xml_val != NULL);
	CHAR *attr = XMLString::transcode(xml_val, dxl_memory_manager);
	CHAR val = *attr;
	XMLString::release(&attr, dxl_memory_manager);
	return val;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToOid
//
//	@doc:
//	  	Converts the attribute value to OID
//
//---------------------------------------------------------------------------
OID
CDXLOperatorFactory::ConvertAttrValueToOid
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *attribute_val_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(attribute_val_xml != NULL);
	OID oid = 0;
	try
	{
		oid = XMLString::parseInt(attribute_val_xml, dxl_memory_manager);
	}
	catch (const NumberFormatException& toCatch)
	{
		// turn Xerces exception into GPOS exception
		GPOS_RAISE
		(
		gpdxl::ExmaDXL,
		gpdxl::ExmiDXLInvalidAttributeValue,
		CDXLTokens::GetDXLTokenStr(target_attr)->GetBuffer(),
		CDXLTokens::GetDXLTokenStr(target_elem)->GetBuffer()
		);
	}
	return oid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToOid
//
//	@doc:
//	  	Extracts the value for the given attribute and converts it into OID
//
//---------------------------------------------------------------------------
OID
CDXLOperatorFactory::ExtractConvertAttrValueToOid
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	OID OidDefaultValue
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem,
											is_optional
											);

	if (NULL == attr_val_xml)
	{
		return OidDefaultValue;
	}

	return ConvertAttrValueToOid(dxl_memory_manager, attr_val_xml, target_attr, target_elem);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToSz
//
//	@doc:
//	  	Converts the string attribute value
//
//---------------------------------------------------------------------------
CHAR *
CDXLOperatorFactory::ConvertAttrValueToSz
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *xml_val,
	Edxltoken , // target_attr,
	Edxltoken  // target_elem
	)
{
	GPOS_ASSERT(NULL != xml_val);
	return XMLString::transcode(xml_val, dxl_memory_manager);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToSz
//
//	@doc:
//	  	Extracts the value for the given attribute and converts it into CHAR*
//
//---------------------------------------------------------------------------
CHAR *
CDXLOperatorFactory::ExtractConvertAttrValueToSz
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	CHAR *default_value
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem,
											is_optional
											);

	if (NULL == attr_val_xml)
	{
		return default_value;
	}

	return CDXLOperatorFactory::ConvertAttrValueToSz
			(
			dxl_memory_manager,
			attr_val_xml,
			target_attr,
			target_elem
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToStr
//
//	@doc:
//	  	Extracts the string value for the given attribute
//
//---------------------------------------------------------------------------
CWStringDynamic *
CDXLOperatorFactory::ExtractConvertAttrValueToStr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem
											);
	return CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, attr_val_xml);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToBool
//
//	@doc:
//	  	Extracts the value for the given attribute and converts it into BOOL
//
//---------------------------------------------------------------------------
BOOL
CDXLOperatorFactory::ExtractConvertAttrValueToBool
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	BOOL default_value
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem,
											is_optional
											);

	if (NULL == attr_val_xml)
	{
		return default_value;
	}

	return ConvertAttrValueToBool
			(
			dxl_memory_manager,
			attr_val_xml,
			target_attr,
			target_elem
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToUlong
//
//	@doc:
//	  	Extracts the value for the given attribute and converts it into ULONG
//
//---------------------------------------------------------------------------
ULONG
CDXLOperatorFactory::ExtractConvertAttrValueToUlong
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	ULONG default_value
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem,
											is_optional
											);

	if (NULL == attr_val_xml)
	{
		return default_value;
	}

	return ConvertAttrValueToUlong
			(
			dxl_memory_manager,
			attr_val_xml,
			target_attr,
			target_elem
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToUllong
//
//	@doc:
//	  	Extracts the value for the given attribute and converts it into ULLONG
//
//---------------------------------------------------------------------------
ULLONG
CDXLOperatorFactory::ExtractConvertAttrValueToUllong
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	ULLONG default_value
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem,
											is_optional
											);

	if (NULL == attr_val_xml)
	{
		return default_value;
	}

	return ConvertAttrValueToUllong
			(
			dxl_memory_manager,
			attr_val_xml,
			target_attr,
			target_elem
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseGroupingColId
//
//	@doc:
//		Parse a grouping column id
//
//---------------------------------------------------------------------------
ULONG
CDXLOperatorFactory::ParseGroupingColId
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	const CWStringConst *grouping_colid_str = CDXLTokens::GetDXLTokenStr(EdxltokenGroupingCol);
	const CWStringConst *colid_str = CDXLTokens::GetDXLTokenStr(EdxltokenColId);

	// get grouping column id from attributes	
	INT colid = ExtractConvertAttrValueToInt(dxl_memory_manager, attrs, EdxltokenColId, EdxltokenGroupingCol);
	
	if (colid < 0)
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			colid_str->GetBuffer(),
			grouping_colid_str->GetBuffer()
			);
	}
	
	return (ULONG) colid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToMdId
//
//	@doc:
//		Parse a metadata id object from the XML attributes of the specified element.
//
//---------------------------------------------------------------------------
IMDId *
CDXLOperatorFactory::ExtractConvertAttrValueToMdId
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	IMDId *default_val
	)
{
	// extract mdid
	const XMLCh *mdid_xml = ExtractAttrValue(attrs, target_attr, target_elem, is_optional);

	if (NULL == mdid_xml)
	{
		if (NULL != default_val)
		{
			default_val->AddRef();
		}

		return default_val;
	}
	
	return MakeMdIdFromStr(dxl_memory_manager, mdid_xml, target_attr, target_elem);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeMdIdFromStr
//
//	@doc:
//		Parse a metadata id object from the XML attributes of the specified element.
//
//---------------------------------------------------------------------------
IMDId *
CDXLOperatorFactory::MakeMdIdFromStr
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *mdid_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	// extract mdid's components: MdidType.Oid.Major.Minor
	XMLStringTokenizer mdid_components(mdid_xml, CDXLTokens::XmlstrToken(EdxltokenDotSemicolon));

	GPOS_ASSERT(1 < mdid_components.countTokens());
	
	// get mdid type from first component
	XMLCh *mdid_type = mdid_components.nextToken();
	
	// collect the remaining tokens in an array
	XMLChArray *remaining_tokens = GPOS_NEW(dxl_memory_manager->Pmp()) XMLChArray(dxl_memory_manager->Pmp());
	
	XMLCh *xml_val = NULL;
	while (NULL != (xml_val = mdid_components.nextToken()))
	{
		remaining_tokens->Append(xml_val);
	}
	
	IMDId::EMDIdType typ = (IMDId::EMDIdType) ConvertAttrValueToUlong(dxl_memory_manager, mdid_type, target_attr, target_elem);

	IMDId *mdid = NULL;
	switch (typ)
	{
		case IMDId::EmdidGPDB:
			mdid = GetGPDBMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
			break;
			
		case IMDId::EmdidGPDBCtas:
			mdid = GetGPDBCTASMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
			break;
			
		case IMDId::EmdidColStats:
			mdid = GetColStatsMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
			break;
			
		case IMDId::EmdidRelStats:
			mdid = GetRelStatsMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
			break;
			
		case IMDId::EmdidCastFunc:
			mdid = GetCastFuncMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
			break;
			
		case IMDId::EmdidScCmp:
			mdid = GetScCmpMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
			break;
			
		default:
			GPOS_ASSERT(!"Unrecognized mdid type");		
	}
	
	remaining_tokens->Release();
	
	return mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetGPDBMdId
//
//	@doc:
//		Construct a GPDB mdid from an array of XML string components.
//
//---------------------------------------------------------------------------
CMDIdGPDB *
CDXLOperatorFactory::GetGPDBMdId
	(
	CDXLMemoryManager *dxl_memory_manager,
	XMLChArray *remaining_tokens,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(GPDXL_GPDB_MDID_COMPONENTS <= remaining_tokens->Size());

	XMLCh *xml_oid = (*remaining_tokens)[0];
	ULONG oid_colid = ConvertAttrValueToUlong(dxl_memory_manager, xml_oid, target_attr, target_elem);

	XMLCh *version_major_xml = (*remaining_tokens)[1];
	ULONG version_major = ConvertAttrValueToUlong(dxl_memory_manager, version_major_xml, target_attr, target_elem);

	XMLCh *xmlszVersionMinor = (*remaining_tokens)[2];
	;
	ULONG version_minor = ConvertAttrValueToUlong(dxl_memory_manager, xmlszVersionMinor, target_attr, target_elem);

	// construct metadata id object
	return GPOS_NEW(dxl_memory_manager->Pmp()) CMDIdGPDB(oid_colid, version_major, version_minor);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetGPDBCTASMdId
//
//	@doc:
//		Construct a GPDB CTAS mdid from an array of XML string components.
//
//---------------------------------------------------------------------------
CMDIdGPDB *
CDXLOperatorFactory::GetGPDBCTASMdId
	(
	CDXLMemoryManager *dxl_memory_manager,
	XMLChArray *remaining_tokens,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(GPDXL_GPDB_MDID_COMPONENTS <= remaining_tokens->Size());

	XMLCh *xml_oid = (*remaining_tokens)[0];
	ULONG oid_colid = ConvertAttrValueToUlong(dxl_memory_manager, xml_oid, target_attr, target_elem);

	// construct metadata id object
	return GPOS_NEW(dxl_memory_manager->Pmp()) CMDIdGPDBCtas(oid_colid);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetColStatsMdId
//
//	@doc:
//		Construct a column stats mdid from an array of XML string components.
//
//---------------------------------------------------------------------------
CMDIdColStats *
CDXLOperatorFactory::GetColStatsMdId
	(
	CDXLMemoryManager *dxl_memory_manager,
	XMLChArray *remaining_tokens,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(GPDXL_GPDB_MDID_COMPONENTS + 1 == remaining_tokens->Size());

	CMDIdGPDB *rel_mdid = GetGPDBMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
	
	XMLCh *attno_xml = (*remaining_tokens)[3];
	ULONG attno = ConvertAttrValueToUlong(dxl_memory_manager, attno_xml, target_attr, target_elem);

	// construct metadata id object
	return GPOS_NEW(dxl_memory_manager->Pmp()) CMDIdColStats(rel_mdid, attno);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetRelStatsMdId
//
//	@doc:
//		Construct a relation stats mdid from an array of XML string components.
//
//---------------------------------------------------------------------------
CMDIdRelStats *
CDXLOperatorFactory::GetRelStatsMdId
	(
	CDXLMemoryManager *dxl_memory_manager,
	XMLChArray *remaining_tokens,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(GPDXL_GPDB_MDID_COMPONENTS == remaining_tokens->Size());

	CMDIdGPDB *rel_mdid = GetGPDBMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
	
	// construct metadata id object
	return GPOS_NEW(dxl_memory_manager->Pmp()) CMDIdRelStats(rel_mdid);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetCastFuncMdId
//
//	@doc:
//		Construct a cast function mdid from the array of XML string components.
//
//---------------------------------------------------------------------------
CMDIdCast *
CDXLOperatorFactory::GetCastFuncMdId
	(
	CDXLMemoryManager *dxl_memory_manager,
	XMLChArray *remaining_tokens,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(2 * GPDXL_GPDB_MDID_COMPONENTS == remaining_tokens->Size());

	CMDIdGPDB *mdid_src = GetGPDBMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
	XMLChArray *dest_xml = GPOS_NEW(dxl_memory_manager->Pmp()) XMLChArray(dxl_memory_manager->Pmp());
	
	for (ULONG ul = GPDXL_GPDB_MDID_COMPONENTS; ul < GPDXL_GPDB_MDID_COMPONENTS * 2; ul++)
	{
		dest_xml->Append((*remaining_tokens)[ul]);
	}
	
	CMDIdGPDB *mdid_dest = GetGPDBMdId(dxl_memory_manager, dest_xml, target_attr, target_elem);
	dest_xml->Release();
	
	return GPOS_NEW(dxl_memory_manager->Pmp()) CMDIdCast(mdid_src, mdid_dest);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetScCmpMdId
//
//	@doc:
//		Construct a scalar comparison operator mdid from the array of XML string components.
//
//---------------------------------------------------------------------------
CMDIdScCmp *
CDXLOperatorFactory::GetScCmpMdId
	(
	CDXLMemoryManager *dxl_memory_manager,
	XMLChArray *remaining_tokens,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	GPOS_ASSERT(2 * GPDXL_GPDB_MDID_COMPONENTS + 1 == remaining_tokens->Size());

	CMDIdGPDB *left_mdid = GetGPDBMdId(dxl_memory_manager, remaining_tokens, target_attr, target_elem);
	XMLChArray *right_xml = GPOS_NEW(dxl_memory_manager->Pmp()) XMLChArray(dxl_memory_manager->Pmp());
	
	for (ULONG ul = GPDXL_GPDB_MDID_COMPONENTS; ul < GPDXL_GPDB_MDID_COMPONENTS * 2 + 1; ul++)
	{
		right_xml->Append((*remaining_tokens)[ul]);
	}
	
	CMDIdGPDB *right_mdid = GetGPDBMdId(dxl_memory_manager, right_xml, target_attr, target_elem);
	
	// parse the comparison type from the last component of the mdid
	XMLCh *xml_str_comp_type = (*right_xml)[right_xml->Size() - 1];
	IMDType::ECmpType cmp_type = (IMDType::ECmpType) ConvertAttrValueToUlong(dxl_memory_manager, xml_str_comp_type, target_attr, target_elem);
	GPOS_ASSERT(IMDType::EcmptOther > cmp_type);
	
	right_xml->Release();
	
	return GPOS_NEW(dxl_memory_manager->Pmp()) CMDIdScCmp(left_mdid, right_mdid, cmp_type);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumVal
//
//	@doc:
//		Parses a DXL datum from the given attributes
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumVal
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_elem
	)
{	
	// get the type id and value of the datum from attributes
	IMDId *mdid = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenTypeId, target_elem);
	GPOS_ASSERT(IMDId::EmdidGPDB == mdid->MdidType());
	CMDIdGPDB *gpdb_mdid = CMDIdGPDB::CastMdid(mdid);

	// get the type id from string
	BOOL is_const_null = ExtractConvertAttrValueToBool(dxl_memory_manager, attrs, EdxltokenIsNull, target_elem, true, false);

	SDXLDatumFactoryElem translators_mapping[] =
	{
		// native support
		{CMDIdGPDB::m_mdid_int2.Oid(), &CDXLOperatorFactory::GetDatumInt2},
		{CMDIdGPDB::m_mdid_int4.Oid(), &CDXLOperatorFactory::GetDatumInt4},
		{CMDIdGPDB::m_mdid_int8.Oid(), &CDXLOperatorFactory::GetDatumInt8},
		{CMDIdGPDB::m_mdid_bool.Oid(), &CDXLOperatorFactory::GetDatumBool},
		{CMDIdGPDB::m_mdid_oid.Oid(), &CDXLOperatorFactory::GetDatumOid},
		// types with long int mapping
		{CMDIdGPDB::m_mdid_bpchar.Oid(), &CDXLOperatorFactory::GetDatumStatsLintMappable},
		{CMDIdGPDB::m_mdid_varchar.Oid(), &CDXLOperatorFactory::GetDatumStatsLintMappable},
		{CMDIdGPDB::m_mdid_text.Oid(), &CDXLOperatorFactory::GetDatumStatsLintMappable},
		{CMDIdGPDB::m_mdid_cash.Oid(), &CDXLOperatorFactory::GetDatumStatsLintMappable},
		{CMDIdGPDB::m_mdid_uuid.Oid(), &CDXLOperatorFactory::GetDatumStatsLintMappable},
		// non-integer numeric types
		{CMDIdGPDB::m_mdid_numeric.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_float4.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_float8.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		// network-related types
		{CMDIdGPDB::m_mdid_inet.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_cidr.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_macaddr.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		// time-related types
		{CMDIdGPDB::m_mdid_date.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_time.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_timeTz.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_timestamp.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_timestampTz.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_abs_time.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_relative_time.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_interval.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable},
		{CMDIdGPDB::m_mdid_time_interval.Oid(), &CDXLOperatorFactory::GetDatumStatsDoubleMappable}
	};
	
	const ULONG translators_mapping_len = GPOS_ARRAY_SIZE(translators_mapping);
	// find translator for the datum type
	PfPdxldatum *func = NULL;
	for (ULONG ul = 0; ul < translators_mapping_len; ul++)
	{
		SDXLDatumFactoryElem elem = translators_mapping[ul];
		if (gpdb_mdid->Oid() == elem.oid)
		{
			func = elem.pf;
			break;
		}
	}
		
	if (NULL == func)
	{
		const XMLCh *attr_val_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenLintValue));
		if (attr_val_xml)
		{
			return GetDatumStatsLintMappable(dxl_memory_manager, attrs, target_elem, mdid, is_const_null);
		}
		// generate a datum of generic type
		return GetDatumGeneric(dxl_memory_manager, attrs, target_elem, mdid, is_const_null);
	}
	else
	{
		return (*func)(dxl_memory_manager, attrs, target_elem, mdid, is_const_null);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumOid
//
//	@doc:
//		Parses a DXL datum of oid type
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumOid
(
 CDXLMemoryManager *dxl_memory_manager,
 const Attributes &attrs,
 Edxltoken target_elem,
 IMDId *mdid,
 BOOL is_const_null
 )
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	OID val = 0;
	if (!is_const_null)
	{
		val = ExtractConvertAttrValueToOid(dxl_memory_manager, attrs, EdxltokenValue, target_elem);
	}

	return GPOS_NEW(mp) CDXLDatumOid(mp, mdid, is_const_null, val);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumInt2
//
//	@doc:
//		Parses a DXL datum of int2 type
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumInt2
(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_elem,
	IMDId *mdid,
	BOOL is_const_null
 )
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	SINT val = 0;
	if (!is_const_null)
	{
		val = ExtractConvertAttrValueToShortInt(dxl_memory_manager, attrs, EdxltokenValue, target_elem);
	}

	return GPOS_NEW(mp) CDXLDatumInt2(mp, mdid, is_const_null, val);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumInt4
//
//	@doc:
//		Parses a DXL datum of int4 type
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumInt4
(
 CDXLMemoryManager *dxl_memory_manager,
 const Attributes &attrs,
 Edxltoken target_elem,
 IMDId *mdid,
 BOOL is_const_null
 )
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	INT val = 0;
	if (!is_const_null)
	{
		val = ExtractConvertAttrValueToInt(dxl_memory_manager, attrs, EdxltokenValue, target_elem);
	}
	
	return GPOS_NEW(mp) CDXLDatumInt4(mp, mdid, is_const_null, val);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumInt8
//
//	@doc:
//		Parses a DXL datum of int8 type
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumInt8
(
 CDXLMemoryManager *dxl_memory_manager,
 const Attributes &attrs,
 Edxltoken target_elem,
 IMDId *mdid,
 BOOL is_const_null
 )
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	LINT val = 0;
	if (!is_const_null)
	{
		val = ExtractConvertAttrValueToLint(dxl_memory_manager, attrs, EdxltokenValue, target_elem);
	}
	
	return GPOS_NEW(mp) CDXLDatumInt8(mp, mdid, is_const_null, val);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumBool
//
//	@doc:
//		Parses a DXL datum of boolean type
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumBool
(
 CDXLMemoryManager *dxl_memory_manager,
 const Attributes &attrs,
 Edxltoken target_elem,
 IMDId *mdid,
 BOOL is_const_null
 )
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	BOOL value = false;
	if (!is_const_null)
	{
		value = ExtractConvertAttrValueToBool(dxl_memory_manager, attrs, EdxltokenValue, target_elem);
	}
	
	return GPOS_NEW(mp) CDXLDatumBool(mp, mdid, is_const_null, value);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumGeneric
//
//	@doc:
//		Parses a DXL datum of generic type
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumGeneric
(
 CDXLMemoryManager *dxl_memory_manager,
 const Attributes &attrs,
 Edxltoken target_elem,
 IMDId *mdid,
 BOOL is_const_null
 )
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	ULONG len = 0;
	BYTE *data = NULL;
	
	if (!is_const_null)
	{
		data = GetByteArray(dxl_memory_manager, attrs, target_elem, &len);
		if (NULL == data)
		{
			// unable to decode value. probably not Base64 encoded.
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLInvalidAttributeValue, CDXLTokens::XmlstrToken(EdxltokenValue), CDXLTokens::GetDXLTokenStr(target_elem));
		}
	}

	INT type_modifier = ExtractConvertAttrValueToInt
						(
						dxl_memory_manager,
						attrs,
						EdxltokenTypeMod,
						EdxltokenScalarCast,
						true,
						default_type_modifier
						);

	return GPOS_NEW(mp) CDXLDatumGeneric(mp, mdid, type_modifier, is_const_null, data, len);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumStatsLintMappable
//
//	@doc:
//		Parses a DXL datum of types having lint mapping
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumStatsLintMappable
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_elem,
	IMDId *mdid,
	BOOL is_const_null
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	ULONG len = 0;
	BYTE *data = NULL;

	LINT value = 0;
	if (!is_const_null)
	{
		data = GetByteArray(dxl_memory_manager, attrs, target_elem, &len);
		value = Value(dxl_memory_manager, attrs, target_elem, data);
	}

	INT type_modifier = ExtractConvertAttrValueToInt
						(
						dxl_memory_manager,
						attrs,
						EdxltokenTypeMod,
						EdxltokenScalarCast,
						true,
													 -1 /* default_val value */
						);

	return GPOS_NEW(mp) CDXLDatumStatsLintMappable(mp, mdid, type_modifier, is_const_null, data, len, value);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::Value
//
//	@doc:
//		Return the LINT value of byte array
//
//---------------------------------------------------------------------------
LINT
CDXLOperatorFactory::Value
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_elem,
	BYTE *data
	)
{
	if (NULL == data)
	{
		// unable to decode value. probably not Base64 encoded.
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLInvalidAttributeValue, CDXLTokens::XmlstrToken(EdxltokenValue), CDXLTokens::GetDXLTokenStr(target_elem));
	}

	return ExtractConvertAttrValueToLint(dxl_memory_manager, attrs, EdxltokenLintValue, target_elem);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetByteArray
//
//	@doc:
//		Parses a byte array representation of the datum
//
//---------------------------------------------------------------------------
BYTE *
CDXLOperatorFactory::GetByteArray
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_elem,
	ULONG *length
	)
{
	const XMLCh *attr_val_xml  = ExtractAttrValue(attrs, EdxltokenValue, target_elem);

	return CDXLUtils::CreateStringFrom64XMLStr(dxl_memory_manager, attr_val_xml, length);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::GetDatumStatsDoubleMappable
//
//	@doc:
//		Parses a DXL datum of types that need double mapping
//
//---------------------------------------------------------------------------
CDXLDatum *
CDXLOperatorFactory::GetDatumStatsDoubleMappable
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_elem,
	IMDId *mdid,
	BOOL is_const_null
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	ULONG len = 0;
	BYTE *data = NULL;
	CDouble value = 0;
	
	if (!is_const_null)
	{
		data = GetByteArray(dxl_memory_manager, attrs, target_elem, &len);
		
		if (NULL == data)
		{
			// unable to decode value. probably not Base64 encoded.
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLInvalidAttributeValue, CDXLTokens::XmlstrToken(EdxltokenValue), CDXLTokens::GetDXLTokenStr(target_elem));
		}

		value = ExtractConvertAttrValueToDouble(dxl_memory_manager, attrs, EdxltokenDoubleValue, target_elem);
	}
	INT type_modifier = ExtractConvertAttrValueToInt
						(
						dxl_memory_manager,
						attrs,
						EdxltokenTypeMod,
						EdxltokenScalarCast,
						true,
													 -1 /* default_val value */
						);
	return GPOS_NEW(mp) CDXLDatumStatsDoubleMappable(mp, mdid, type_modifier, is_const_null, data, len, value);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertValuesToArray
//
//	@doc:
//		Parse a comma-separated list of unsigned long integers ids into a dynamic array.
//		Will raise an exception if list is not well-formed
//
//---------------------------------------------------------------------------
ULongPtrArray *
CDXLOperatorFactory::ExtractConvertValuesToArray
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	const XMLCh *xml_val = CDXLOperatorFactory::ExtractAttrValue(attrs, target_attr, target_elem);

	return ExtractIntsToUlongArray(dxl_memory_manager, xml_val, target_attr, target_elem);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertMdIdsToArray
//
//	@doc:
//		Parse a comma-separated list of MDids into a dynamic array.
//		Will raise an exception if list is not well-formed
//
//---------------------------------------------------------------------------
IMdIdArray *
CDXLOperatorFactory::ExtractConvertMdIdsToArray
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *mdid_list_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	IMdIdArray *mdid_array = GPOS_NEW(mp) IMdIdArray(mp);

	XMLStringTokenizer mdid_components(mdid_list_xml, CDXLTokens::XmlstrToken(EdxltokenComma));
	const ULONG num_tokens = mdid_components.countTokens();

	for (ULONG ul = 0; ul < num_tokens; ul++)
	{
		XMLCh *mdid_xml = mdid_components.nextToken();
		GPOS_ASSERT(NULL != mdid_xml);

		IMDId *mdid = MakeMdIdFromStr(dxl_memory_manager, mdid_xml, target_attr, target_elem);
		mdid_array->Append(mdid);
	}

	return mdid_array;
}

// Parse a comma-separated list of CHAR partition types into a dynamic array.
// Will raise an exception if list is not well-formed
CharPtrArray *
CDXLOperatorFactory::ExtractConvertPartitionTypeToArray
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *xml_val,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	return ExtractIntsToArray<CHAR, CleanupDelete, ConvertAttrValueToChar>
			(
			dxl_memory_manager,
			xml_val,
			target_attr,
			target_elem
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertUlongTo2DArray
//
//	@doc:
//		Parse a semicolon-separated list of comma-separated unsigned long 
//		integers into a dynamic array.
//		Will raise an exception if list is not well-formed
//
//---------------------------------------------------------------------------
ULongPtr2dArray *
CDXLOperatorFactory::ExtractConvertUlongTo2DArray
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *xml_val,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
		
	ULongPtr2dArray *array_2D = GPOS_NEW(mp) ULongPtr2dArray(mp);
	
	XMLStringTokenizer mdid_components(xml_val, CDXLTokens::XmlstrToken(EdxltokenSemicolon));
	const ULONG num_tokens = mdid_components.countTokens();
	
	for (ULONG ul = 0; ul < num_tokens; ul++)
	{
		XMLCh *comp_xml = mdid_components.nextToken();
		
		GPOS_ASSERT(NULL != comp_xml);
		
		ULongPtrArray *array_1D = ExtractIntsToUlongArray(dxl_memory_manager, comp_xml, target_attr, target_elem);
		array_2D->Append(array_1D);
	}
	
	return array_2D;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertSegmentIdsToArray
//
//	@doc:
//		Parse a comma-separated list of segment ids into a dynamic array.
//		Will raise an exception if list is not well-formed
//
//---------------------------------------------------------------------------
IntPtrArray *
CDXLOperatorFactory::ExtractConvertSegmentIdsToArray
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *seg_id_list_xml,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	
	GPOS_ASSERT(NULL != seg_id_list_xml);
	
	IntPtrArray *seg_ids = GPOS_NEW(mp) IntPtrArray(mp);
	
	XMLStringTokenizer mdid_components(seg_id_list_xml, CDXLTokens::XmlstrToken(EdxltokenComma));
	
	const ULONG num_of_segments = mdid_components.countTokens();
	GPOS_ASSERT(0 < num_of_segments);
	
	for (ULONG ul = 0; ul < num_of_segments; ul++)
	{
		XMLCh *seg_id_xml = mdid_components.nextToken();
		
		GPOS_ASSERT(NULL != seg_id_xml);
		
		INT *seg_id = GPOS_NEW(mp) INT(ConvertAttrValueToInt(dxl_memory_manager, seg_id_xml, target_attr, target_elem));
		seg_ids->Append(seg_id);
	}
	
	return seg_ids;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertStrsToArray
//
//	@doc:
//		Parse a semicolon-separated list of strings into a dynamic array.
//
//---------------------------------------------------------------------------
StringPtrArray *
CDXLOperatorFactory::ExtractConvertStrsToArray
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *xml_val
	)
{
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	StringPtrArray *array_strs = GPOS_NEW(mp) StringPtrArray(mp);

	XMLStringTokenizer mdid_components(xml_val, CDXLTokens::XmlstrToken(EdxltokenSemicolon));
	const ULONG num_tokens = mdid_components.countTokens();

	for (ULONG ul = 0; ul < num_tokens; ul++)
	{
		XMLCh *current_str = mdid_components.nextToken();
		GPOS_ASSERT(NULL != current_str);

		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, current_str);
		array_strs->Append(str);
	}

	return array_strs;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::SetSegmentInfo
//
//	@doc:
//		Parses the input and output segment ids from Xerces attributes and
//		stores them in the provided DXL Motion operator.
//		Will raise an exception if lists are not well-formed 
//
//---------------------------------------------------------------------------
void
CDXLOperatorFactory::SetSegmentInfo
	(
	CDXLMemoryManager *dxl_memory_manager,
	CDXLPhysicalMotion *motion,
	const Attributes &attrs,
	Edxltoken target_elem
	)
{
	const XMLCh *input_seglist_xml = ExtractAttrValue
										(
										attrs,
										EdxltokenInputSegments,
										target_elem
										);
	IntPtrArray *input_segments = ExtractConvertSegmentIdsToArray
									(
									dxl_memory_manager,
									input_seglist_xml,
									EdxltokenInputSegments,
									target_elem
									);
	motion->SetInputSegIds(input_segments);

	const XMLCh *output_seglist_xml = ExtractAttrValue
										(
										attrs,
										EdxltokenOutputSegments,
										target_elem
										);
	IntPtrArray *output_segments = ExtractConvertSegmentIdsToArray
									(
									dxl_memory_manager,
									output_seglist_xml,
									EdxltokenOutputSegments,
									target_elem
									);
	motion->SetOutputSegIds(output_segments);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseJoinType
//
//	@doc:
//		Parse a join type from the attribute value.
//		Raise an exception if join type value is invalid.
//
//---------------------------------------------------------------------------
EdxlJoinType
CDXLOperatorFactory::ParseJoinType
	(
	const XMLCh *join_type_xml,
	const CWStringConst *join_name
	)
{
	EdxlJoinType join_type = EdxljtSentinel;
	
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenJoinInner),
			join_type_xml))
	{
		join_type = EdxljtInner;
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenJoinLeft),
					join_type_xml))
	{
		join_type = EdxljtLeft;
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenJoinFull),
			join_type_xml))
	{
		join_type = EdxljtFull;
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenJoinRight),
				join_type_xml))
	{
		join_type = EdxljtRight;
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenJoinIn),
					join_type_xml))
	{
		join_type = EdxljtIn;
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenJoinLeftAntiSemiJoin),
					join_type_xml))
	{
		join_type = EdxljtLeftAntiSemijoin;
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenJoinLeftAntiSemiJoinNotIn),
					join_type_xml))
	{
		join_type = EdxljtLeftAntiSemijoinNotIn;
	}
	else
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenJoinType)->GetBuffer(),
			join_name->GetBuffer()
			);		
	}
	
	return join_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseIndexScanDirection
//
//	@doc:
//		Parse the index scan direction from the attribute value. Raise
//		exception if it is invalid
//
//---------------------------------------------------------------------------
EdxlIndexScanDirection
CDXLOperatorFactory::ParseIndexScanDirection
	(
	const XMLCh *direction_xml,
	const CWStringConst *index_scan
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenIndexScanDirectionBackward),
			direction_xml))
	{
		return EdxlisdBackward;
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenIndexScanDirectionForward),
					direction_xml))
	{
		return EdxlisdForward;
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenIndexScanDirectionNoMovement),
			direction_xml))
	{
		return EdxlisdNoMovement;
	}
	else
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenIndexScanDirection)->GetBuffer(),
			index_scan->GetBuffer()
			);
	}

	return EdxlisdSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeLogicalJoin
//
//	@doc:
//		Construct a logical join operator
//
//---------------------------------------------------------------------------
CDXLLogical*
CDXLOperatorFactory::MakeLogicalJoin
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	const XMLCh *join_type_xml = ExtractAttrValue(attrs, EdxltokenJoinType, EdxltokenLogicalJoin);
	EdxlJoinType join_type = ParseJoinType(join_type_xml, CDXLTokens::GetDXLTokenStr(EdxltokenLogicalJoin));

	return GPOS_NEW(mp) CDXLLogicalJoin(mp, join_type);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToDouble
//
//	@doc:
//	  Converts the attribute value to CDouble
//
//---------------------------------------------------------------------------
CDouble
CDXLOperatorFactory::ConvertAttrValueToDouble
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *attribute_val_xml,
	Edxltoken ,//target_attr,
	Edxltoken //target_elem
	)
{
	GPOS_ASSERT(attribute_val_xml != NULL);
	CHAR *sz = XMLString::transcode(attribute_val_xml, dxl_memory_manager);

	CDouble value(clib::Strtod(sz));

	XMLString::release(&sz, dxl_memory_manager);
	return value;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToDouble
//
//	@doc:
//	  Extracts the value for the given attribute and converts it into CDouble
//
//---------------------------------------------------------------------------
CDouble
CDXLOperatorFactory::ExtractConvertAttrValueToDouble
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue
											(
											attrs,
											target_attr,
											target_elem
											);
	return ConvertAttrValueToDouble
			(
			dxl_memory_manager,
			attr_val_xml,
			target_attr,
			target_elem
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ConvertAttrValueToLint
//
//	@doc:
//	  Converts the attribute value to LINT
//
//---------------------------------------------------------------------------
LINT
CDXLOperatorFactory::ConvertAttrValueToLint
	(
	CDXLMemoryManager *dxl_memory_manager,
	const XMLCh *attribute_val_xml,
	Edxltoken ,//target_attr,
	Edxltoken //target_elem
	)
{
	GPOS_ASSERT(NULL != attribute_val_xml);
	CHAR *sz = XMLString::transcode(attribute_val_xml, dxl_memory_manager);
	CHAR *szEnd = NULL;

	LINT value = clib::Strtoll(sz, &szEnd, 10);
	XMLString::release(&sz, dxl_memory_manager);

	return value;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ExtractConvertAttrValueToLint
//
//	@doc:
//	  Extracts the value for the given attribute and converts it into LINT
//
//---------------------------------------------------------------------------
LINT
CDXLOperatorFactory::ExtractConvertAttrValueToLint
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem,
	BOOL is_optional,
	LINT default_value
	)
{
	const XMLCh *attr_val_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, target_attr, target_elem, is_optional);

	if (NULL == attr_val_xml)
	{
		return default_value;
	}

	return ConvertAttrValueToLint(dxl_memory_manager, attr_val_xml, target_attr, target_elem);
}


CSystemId
CDXLOperatorFactory::Sysid
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs,
	Edxltoken target_attr,
	Edxltoken target_elem
	)
{
	// extract systemids
	const XMLCh *xml_val = ExtractAttrValue(attrs, target_attr, target_elem);

	// get sysid components
	XMLStringTokenizer sys_id_components(xml_val, CDXLTokens::XmlstrToken(EdxltokenDot));
	GPOS_ASSERT(2 == sys_id_components.countTokens());
	
	XMLCh *sys_id_comp = sys_id_components.nextToken();
	ULONG type = CDXLOperatorFactory::ConvertAttrValueToUlong(dxl_memory_manager, sys_id_comp, target_attr, target_elem);

	XMLCh *xml_str_name = sys_id_components.nextToken();
	CWStringDynamic *str_name = CDXLUtils::CreateDynamicStringFromXMLChArray(dxl_memory_manager, xml_str_name);
	
	CSystemId sys_id((IMDId::EMDIdType) type, str_name->GetBuffer(), str_name->Length());
	GPOS_DELETE(str_name);
	
	return sys_id;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::MakeWindowRef
//
//	@doc:
//		Construct an WindowRef operator
//
//---------------------------------------------------------------------------
CDXLScalar*
CDXLOperatorFactory::MakeWindowRef
	(
	CDXLMemoryManager *dxl_memory_manager,
	const Attributes &attrs
	)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();
	IMDId *mdid_func = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenWindowrefOid, EdxltokenScalarWindowref);
	IMDId *mdid_return_type = ExtractConvertAttrValueToMdId(dxl_memory_manager, attrs, EdxltokenTypeId, EdxltokenScalarWindowref);
	BOOL is_distinct = ExtractConvertAttrValueToBool(dxl_memory_manager, attrs, EdxltokenWindowrefDistinct, EdxltokenScalarWindowref);
	BOOL is_star_arg = ExtractConvertAttrValueToBool(dxl_memory_manager, attrs, EdxltokenWindowrefStarArg, EdxltokenScalarWindowref);
	BOOL is_simple_agg = ExtractConvertAttrValueToBool(dxl_memory_manager, attrs, EdxltokenWindowrefSimpleAgg, EdxltokenScalarWindowref);
	ULONG win_spec_pos = ExtractConvertAttrValueToUlong(dxl_memory_manager, attrs, EdxltokenWindowrefWinSpecPos, EdxltokenScalarWindowref);

	const XMLCh *agg_stage_xml = ExtractAttrValue(attrs, EdxltokenWindowrefStrategy, EdxltokenScalarWindowref);
	EdxlWinStage dxl_win_stage = EdxlwinstageSentinel;

	ULONG window_frame_boundary_to_frame_boundary_mapping[][2] =
					{
					{EdxlwinstageImmediate, EdxltokenWindowrefStageImmediate},
					{EdxlwinstagePreliminary, EdxltokenWindowrefStagePreliminary},
					{EdxlwinstageRowKey, EdxltokenWindowrefStageRowKey}
					};

	const ULONG arity = GPOS_ARRAY_SIZE(window_frame_boundary_to_frame_boundary_mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *mapping = window_frame_boundary_to_frame_boundary_mapping[ul];
		Edxltoken frame_bound = (Edxltoken) mapping[1];
		if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(frame_bound), agg_stage_xml))
		{
			dxl_win_stage = (EdxlWinStage) mapping[0];
			break;
		}
	}
	GPOS_ASSERT(EdxlwinstageSentinel != dxl_win_stage);

	return GPOS_NEW(mp) CDXLScalarWindowRef(mp, mdid_func, mdid_return_type, is_distinct, is_star_arg, is_simple_agg, dxl_win_stage, win_spec_pos);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseCmpType
//
//	@doc:
//		Parse comparison type
//
//---------------------------------------------------------------------------
IMDType::ECmpType
CDXLOperatorFactory::ParseCmpType
	(
	const XMLCh* xml_str_comp_type
	)
{
	ULONG parse_cmp_type_mapping[][2] =
	{
	{EdxltokenCmpEq, IMDType::EcmptEq},
	{EdxltokenCmpNeq,IMDType::EcmptNEq},
	{EdxltokenCmpLt, IMDType::EcmptL},
	{EdxltokenCmpLeq, IMDType::EcmptLEq},
	{EdxltokenCmpGt, IMDType::EcmptG},
	{EdxltokenCmpGeq,IMDType::EcmptGEq}, 
	{EdxltokenCmpIDF, IMDType::EcmptIDF},
	{EdxltokenCmpOther, IMDType::EcmptOther}
	};
	
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(parse_cmp_type_mapping); ul++)
	{
		ULONG *mapping = parse_cmp_type_mapping[ul];
		Edxltoken cmp_type = (Edxltoken) mapping[0];
		
		if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(cmp_type),
				xml_str_comp_type))
		{
			return (IMDType::ECmpType) mapping[1];
		}
	}
	
	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLInvalidAttributeValue, CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOpCmpType)->GetBuffer(), CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOp)->GetBuffer());
	return (IMDType::ECmpType) IMDType::EcmptOther;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseRelationDistPolicy
//
//	@doc:
//		Parse relation distribution policy from XML string
//
//---------------------------------------------------------------------------
IMDRelation::Ereldistrpolicy
CDXLOperatorFactory::ParseRelationDistPolicy
	(
	const XMLCh* xml_val
	)
{
	GPOS_ASSERT(NULL != xml_val);
	IMDRelation::Ereldistrpolicy rel_distr_policy = IMDRelation::EreldistrSentinel;
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelDistrMasterOnly)))
	{
		rel_distr_policy = IMDRelation::EreldistrMasterOnly;
	}
	else if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelDistrHash)))
	{
		rel_distr_policy = IMDRelation::EreldistrHash;
	}
	else if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelDistrRandom)))
	{
		rel_distr_policy = IMDRelation::EreldistrRandom;
	}
	else if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelDistrReplicated)))
	{
		rel_distr_policy = IMDRelation::EreldistrReplicated;
	}

	return rel_distr_policy;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseRelationStorageType
//
//	@doc:
//		Parse relation storage type from XML string
//
//---------------------------------------------------------------------------
IMDRelation::Erelstoragetype
CDXLOperatorFactory::ParseRelationStorageType
	(
	const XMLCh* xml_val
	)
{
	GPOS_ASSERT(NULL != xml_val);
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelStorageHeap)))
	{
		return IMDRelation::ErelstorageHeap;
	}
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelStorageAppendOnlyCols)))
	{
		return IMDRelation::ErelstorageAppendOnlyCols;
	}
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelStorageAppendOnlyRows)))
	{
		return IMDRelation::ErelstorageAppendOnlyRows;
	}
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelStorageAppendOnlyParquet)))
	{
		return IMDRelation::ErelstorageAppendOnlyParquet;
	}
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelStorageExternal)))
	{
		return IMDRelation::ErelstorageExternal;
	}
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenRelStorageVirtual)))
	{
		return IMDRelation::ErelstorageVirtual;
	}
	
	GPOS_ASSERT(!"Unrecognized storage type");
	
	return IMDRelation::ErelstorageSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseOnCommitActionSpec
//
//	@doc:
//		Parse on commit action spec from XML attributes
//
//---------------------------------------------------------------------------
CDXLCtasStorageOptions::ECtasOnCommitAction
CDXLOperatorFactory::ParseOnCommitActionSpec
	(
	const Attributes &attrs
	)
{
	const XMLCh *xml_val = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenOnCommitAction));

	if (NULL == xml_val)
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLMissingAttribute,
			CDXLTokens::GetDXLTokenStr(EdxltokenOnCommitAction)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenCTASOptions)->GetBuffer()
			);
	}
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenOnCommitPreserve)))
	{
		return CDXLCtasStorageOptions::EctascommitPreserve;
	}
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenOnCommitDelete)))
	{
		return CDXLCtasStorageOptions::EctascommitDelete;
	}
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenOnCommitDrop)))
	{
		return CDXLCtasStorageOptions::EctascommitDrop;
	}
	
	if (0 != XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenOnCommitNOOP)))
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenOnCommitAction)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenCTASOptions)->GetBuffer()
			);	
	}
	
	return CDXLCtasStorageOptions::EctascommitNOOP;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorFactory::ParseIndexType
//
//	@doc:
//		Parse index type from XML attributes
//
//---------------------------------------------------------------------------
IMDIndex::EmdindexType
CDXLOperatorFactory::ParseIndexType
	(
	const Attributes &attrs
	)
{
	const XMLCh *xml_val = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenIndexType));

	if (NULL == xml_val || 0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenIndexTypeBtree)))
	{
		// default_val is btree
		return IMDIndex::EmdindBtree;
	}
	
	
	if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenIndexTypeBitmap)))
	{
		return IMDIndex::EmdindBitmap;
	}
	else if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenIndexTypeGist)))
	{
		return IMDIndex::EmdindGist;
	}
	else if (0 == XMLString::compareString(xml_val, CDXLTokens::XmlstrToken(EdxltokenIndexTypeGin)))
	{
		return IMDIndex::EmdindGin;
	}
	
	GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenIndexType)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenIndex)->GetBuffer()
			);	
	
	return IMDIndex::EmdindSentinel;
}

// EOF
