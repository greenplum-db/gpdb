//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerFactory.h
//
//	@doc:
//		Factory methods for creating SAX parse handlers
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerFactory_H
#define GPDXL_CParseHandlerFactory_H

#include "gpos/base.h"
#include "gpos/common/CHashMap.h"

#include "naucrates/exception.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/dxl/xml/dxltokens.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

// shorthand for functions creating operator parse handlers
typedef CParseHandlerBase *(ParseHandlerOpCreatorFunc)(CMemoryPool *mp,
													   CParseHandlerManager *,
													   CParseHandlerBase *);

// fwd decl
class CDXLTokens;

const ULONG HASH_MAP_SIZE = 128;

// function for hashing xerces strings
inline ULONG
GetHashXMLStr(const XMLCh *xml_str)
{
	return (ULONG) XMLString::hash(xml_str, HASH_MAP_SIZE);
}

// function for equality on xerces strings
inline BOOL
IsXMLStrEqual(const XMLCh *xml_str1, const XMLCh *xml_str2)
{
	return (0 == XMLString::compareString(xml_str1, xml_str2));
}


//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerFactory
//
//	@doc:
//		Factory class for creating DXL SAX parse handlers
//
//---------------------------------------------------------------------------
class CParseHandlerFactory
{
	typedef CHashMap<const XMLCh, ParseHandlerOpCreatorFunc, GetHashXMLStr,
					 IsXMLStrEqual, CleanupNULL, CleanupNULL>
		TokenParseHandlerFuncMap;

	// pair of DXL token type and the corresponding parse handler
	struct SParseHandlerMapping
	{
		// type
		Edxltoken token_type;

		// translator function pointer
		ParseHandlerOpCreatorFunc *parse_handler_op_func;
	};

private:
	// mappings DXL token -> ParseHandler creator
	static TokenParseHandlerFuncMap *m_token_parse_handler_func_map;

	static void AddMapping(Edxltoken token_type,
						   ParseHandlerOpCreatorFunc *parse_handler_op_func);

	// construct a physical op parse handlers
	static CParseHandlerBase *CreatePhysicalOpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a GPDB plan parse handler
	static CParseHandlerBase *CreatePlanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a metadata parse handler
	static CParseHandlerBase *CreateMetadataParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a metadata request parse handler
	static CParseHandlerBase *CreateMDRequestParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *pph);

	// construct a parse handler for the optimizer configuration
	static CParseHandlerBase *CreateOptimizerCfgParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler for the enumerator configuration
	static CParseHandlerBase *CreateEnumeratorCfgParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler for the statistics configuration
	static CParseHandlerBase *CreateStatisticsCfgParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler for the CTE configuration
	static CParseHandlerBase *CreateCTECfgParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler for the cost model configuration
	static CParseHandlerBase *CreateCostModelCfgParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct hint parse handler
	static CParseHandlerBase *CreateHintParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct window oids parse handler
	static CParseHandlerBase *CreateWindowOidsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a trace flag parse handler
	static CParseHandlerBase *CreateTraceFlagsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a MD relation parse handler
	static CParseHandlerBase *CreateMDRelationParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a MD external relation parse handler
	static CParseHandlerBase *CreateMDRelationExtParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a MD CTAS relation parse handler
	static CParseHandlerBase *CreateMDRelationCTASParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD index parse handler
	static CParseHandlerBase *CreateMDIndexParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a relation stats parse handler
	static CParseHandlerBase *CreateRelStatsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a column stats parse handler
	static CParseHandlerBase *CreateColStatsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a column stats bucket parse handler
	static CParseHandlerBase *CreateColStatsBucketParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD type parse handler
	static CParseHandlerBase *CreateMDTypeParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD scalarop parse handler
	static CParseHandlerBase *CreateMDScalarOpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD function parse handler
	static CParseHandlerBase *CreateMDFuncParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD aggregate operation parse handler
	static CParseHandlerBase *CreateMDAggParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD trigger parse handler
	static CParseHandlerBase *CreateMDTriggerParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD cast parse handler
	static CParseHandlerBase *CreateMDCastParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD scalar comparison parse handler
	static CParseHandlerBase *CreateMDScCmpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an MD check constraint parse handler
	static CParseHandlerBase *CreateMDChkConstraintParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler for a list of MD ids
	static CParseHandlerBase *CreateMDIdListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a metadata columns parse handler
	static CParseHandlerBase *CreateMDColsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	static CParseHandlerBase *CreateMDIndexInfoListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a column MD parse handler
	static CParseHandlerBase *CreateMDColParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a column default value expression parse handler
	static CParseHandlerBase *CreateColDefaultValExprParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar operator parse handler
	static CParseHandlerBase *CreateScalarOpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a properties parse handler
	static CParseHandlerBase *CreatePropertiesParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a filter operator parse handler
	static CParseHandlerBase *CreateFilterParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a table scan parse handler
	static CParseHandlerBase *CreateTableScanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a bitmap table scan parse handler
	static CParseHandlerBase *CreateBitmapTableScanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a dynamic bitmap table scan parse handler
	static CParseHandlerBase *CreateDynBitmapTableScanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an external scan parse handler
	static CParseHandlerBase *CreateExternalScanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a subquery scan parse handler
	static CParseHandlerBase *CreateSubqueryScanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a result node parse handler
	static CParseHandlerBase *CreateResultParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a HJ parse handler
	static CParseHandlerBase *CreateHashJoinParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a NLJ parse handler
	static CParseHandlerBase *CreateNLJoinParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a merge join parse handler
	static CParseHandlerBase *CreateMergeJoinParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a sort parse handler
	static CParseHandlerBase *CreateSortParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an append parse handler
	static CParseHandlerBase *CreateAppendParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a materialize parse handler
	static CParseHandlerBase *CreateMaterializeParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a dynamic table scan parse handler
	static CParseHandlerBase *CreateDTSParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a dynamic index scan parse handler
	static CParseHandlerBase *CreateDynamicIdxScanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a partition selector parse handler
	static CParseHandlerBase *CreatePartitionSelectorParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a sequence parse handler
	static CParseHandlerBase *CreateSequenceParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a limit (physical) parse handler
	static CParseHandlerBase *CreateLimitParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a limit count parse handler
	static CParseHandlerBase *CreateLimitCountParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a limit offset parse handler
	static CParseHandlerBase *CreateLimitOffsetParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a subquery parse handler
	static CParseHandlerBase *CreateScSubqueryParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a subquery parse handler
	static CParseHandlerBase *CreateScBitmapBoolOpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an array parse handler
	static CParseHandlerBase *CreateScArrayParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an arrayref parse handler
	static CParseHandlerBase *CreateScArrayRefParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an arrayref index list parse handler
	static CParseHandlerBase *CreateScArrayRefIdxListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an assert predicate parse handler
	static CParseHandlerBase *CreateScAssertConstraintListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);


	// construct a DML action parse handler
	static CParseHandlerBase *CreateScDMLActionParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar operator list
	static CParseHandlerBase *CreateScOpListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar part oid
	static CParseHandlerBase *CreateScPartOidParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar part default
	static CParseHandlerBase *CreateScPartDefaultParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar part bound
	static CParseHandlerBase *CreateScPartBoundParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar part bound inclusion
	static CParseHandlerBase *CreateScPartBoundInclParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar part bound openness
	static CParseHandlerBase *CreateScPartBoundOpenParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar part list values
	static CParseHandlerBase *CreateScPartListValuesParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar part list null test
	static CParseHandlerBase *CreateScPartListNullTestParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a direct dispatch info parse handler
	static CParseHandlerBase *CreateDirectDispatchParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a gather motion parse handler
	static CParseHandlerBase *CreateGatherMotionParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a broadcast motion parse handler
	static CParseHandlerBase *CreateBroadcastMotionParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a redistribute motion parse handler
	static CParseHandlerBase *CreateRedistributeMotionParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a routed motion parse handler
	static CParseHandlerBase *CreateRoutedMotionParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a random motion parse handler
	static CParseHandlerBase *CreateRandomMotionParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical aggregate parse handler
	static CParseHandlerBase *CreateAggParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an aggregate function parse handler
	static CParseHandlerBase *CreateAggRefParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler for a physical window node
	static CParseHandlerBase *CreateWindowParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an window function parse handler
	static CParseHandlerBase *CreateWindowRefParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an window frame parse handler
	static CParseHandlerBase *CreateWindowFrameParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an window key parse handler
	static CParseHandlerBase *CreateWindowKeyParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler to parse the list of window keys
	static CParseHandlerBase *CreateWindowKeyListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an window specification parse handler
	static CParseHandlerBase *CreateWindowSpecParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler to parse the list of window specifications
	static CParseHandlerBase *CreateWindowSpecListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a grouping column list parse handler
	static CParseHandlerBase *CreateGroupingColListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a comparison operator parse handler
	static CParseHandlerBase *CreateScCmpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a distinct compare parse handler
	static CParseHandlerBase *CreateDistinctCmpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar identifier parse handler
	static CParseHandlerBase *CreateScIdParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar operator parse handler
	static CParseHandlerBase *CreateScOpExprParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an array compare parse handler
	static CParseHandlerBase *CreateScArrayCmpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a boolean expression parse handler
	static CParseHandlerBase *CreateScBoolExprParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a min/max parse handler
	static CParseHandlerBase *CreateScMinMaxParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a boolean test parse handler
	static CParseHandlerBase *CreateBooleanTestParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a null test parse handler
	static CParseHandlerBase *CreateScNullTestParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a nullif parse handler
	static CParseHandlerBase *CreateScNullIfParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a cast parse handler
	static CParseHandlerBase *CreateScCastParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a coerce parse handler
	static CParseHandlerBase *CreateScCoerceToDomainParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a coerceviaio parse handler
	static CParseHandlerBase *CreateScCoerceViaIOParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a ArrayCoerceExpr parse handler
	static CParseHandlerBase *CreateScArrayCoerceExprParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a sub plan parse handler
	static CParseHandlerBase *CreateScSubPlanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// create a parse handler for parsing a SubPlan test expression
	static CParseHandlerBase *CreateScSubPlanTestExprParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a sub plan params parse handler
	static CParseHandlerBase *CreateScSubPlanParamListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a sub plan param parse handler
	static CParseHandlerBase *CreateScSubPlanParamParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical TVF parse handler
	static CParseHandlerBase *CreateLogicalTVFParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical TVF parse handler
	static CParseHandlerBase *CreatePhysicalTVFParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a coalesce parse handler
	static CParseHandlerBase *CreateScCoalesceParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a switch parse handler
	static CParseHandlerBase *CreateScSwitchParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a switch case parse handler
	static CParseHandlerBase *CreateScSwitchCaseParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a case test parse handler
	static CParseHandlerBase *CreateScCaseTestParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a constant parse handler
	static CParseHandlerBase *CreateScConstValueParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an if statement parse handler
	static CParseHandlerBase *CreateIfStmtParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a function parse handler
	static CParseHandlerBase *CreateScFuncExprParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a project list parse handler
	static CParseHandlerBase *CreateProjListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a project element parse handler
	static CParseHandlerBase *CreateProjElemParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a hash expression list parse handler
	static CParseHandlerBase *CreateHashExprListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a hash expression parse handler
	static CParseHandlerBase *CreateHashExprParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a condition list parse handler
	static CParseHandlerBase *CreateCondListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a sort column list parse handler
	static CParseHandlerBase *CreateSortColListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a sort column parse handler
	static CParseHandlerBase *CreateSortColParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a cost parse handler
	static CParseHandlerBase *CreateCostParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a table descriptor parse handler
	static CParseHandlerBase *CreateTableDescParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a column descriptor parse handler
	static CParseHandlerBase *CreateColDescParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an index scan list parse handler
	static CParseHandlerBase *CreateIdxScanListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an index only scan parse handler
	static CParseHandlerBase *CreateIdxOnlyScanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a bitmap index scan list parse handler
	static CParseHandlerBase *CreateBitmapIdxProbeParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an index descriptor list parse handler
	static CParseHandlerBase *CreateIdxDescrParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct an index condition list parse handler
	static CParseHandlerBase *CreateIdxCondListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);


	// construct a query parse handler
	static CParseHandlerBase *CreateQueryParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical get parse handler
	static CParseHandlerBase *CreateLogicalGetParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical external get parse handler
	static CParseHandlerBase *CreateLogicalExtGetParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical operator parse handler
	static CParseHandlerBase *CreateLogicalOpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical project parse handler
	static CParseHandlerBase *CreateLogicalProjParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical CTE producer parse handler
	static CParseHandlerBase *CreateLogicalCTEProdParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical CTE consumer parse handler
	static CParseHandlerBase *CreateLogicalCTEConsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical CTE anchor parse handler
	static CParseHandlerBase *CreateLogicalCTEAnchorParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a CTE list
	static CParseHandlerBase *CreateCTEListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical window parse handler
	static CParseHandlerBase *CreateLogicalWindowParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical insert parse handler
	static CParseHandlerBase *CreateLogicalInsertParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical delete parse handler
	static CParseHandlerBase *CreateLogicalDeleteParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical update parse handler
	static CParseHandlerBase *CreateLogicalUpdateParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical CTAS parse handler
	static CParseHandlerBase *CreateLogicalCTASParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical CTAS parse handler
	static CParseHandlerBase *CreatePhysicalCTASParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a parse handler for parsing CTAS storage options
	static CParseHandlerBase *CreateCTASOptionsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical CTE producer parse handler
	static CParseHandlerBase *CreatePhysicalCTEProdParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical CTE consumer parse handler
	static CParseHandlerBase *CreatePhysicalCTEConsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical DML parse handler
	static CParseHandlerBase *CreatePhysicalDMLParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical split parse handler
	static CParseHandlerBase *CreatePhysicalSplitParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical row trigger parse handler
	static CParseHandlerBase *CreatePhysicalRowTriggerParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a physical assert parse handler
	static CParseHandlerBase *CreatePhysicalAssertParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical set operator parse handler
	static CParseHandlerBase *CreateLogicalSetOpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical select parse handler
	static CParseHandlerBase *CreateLogicalSelectParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical join parse handler
	static CParseHandlerBase *CreateLogicalJoinParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical query output parse handler
	static CParseHandlerBase *CreateLogicalQueryOpParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical groupby parse handler
	static CParseHandlerBase *CreateLogicalGrpByParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical limit parse handler
	static CParseHandlerBase *CreateLogicalLimitParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a logical const table parse handler
	static CParseHandlerBase *CreateLogicalConstTableParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a quantified subquery parse handler
	static CParseHandlerBase *CreateScScalarSubqueryQuantifiedParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a subquery parse handler
	static CParseHandlerBase *CreateScScalarSubqueryExistsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a pass-through parse handler for stack traces
	static CParseHandlerBase *CreateStackTraceParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a statistics parse handler
	static CParseHandlerBase *CreateStatsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a derived column parse handler
	static CParseHandlerBase *CreateStatsDrvdColParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a derived relation stats parse handler
	static CParseHandlerBase *CreateStatsDrvdRelParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a bucket bound parse handler
	static CParseHandlerBase *CreateStatsBucketBoundParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a trailing window frame edge parser
	static CParseHandlerBase *CreateFrameTrailingEdgeParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a leading window frame edge parser
	static CParseHandlerBase *CreateFrameLeadingEdgeParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct search strategy parse handler
	static CParseHandlerBase *CreateSearchStrategyParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct search stage parse handler
	static CParseHandlerBase *CreateSearchStageParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct xform parse handler
	static CParseHandlerBase *CreateXformParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct cost params parse handler
	static CParseHandlerBase *CreateCostParamsParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct cost param parse handler
	static CParseHandlerBase *CreateCostParamParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar expression parse handler
	static CParseHandlerBase *CreateScExprParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a scalar values list parse handler
	static CParseHandlerBase *CreateScValuesListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a values scan parse handler
	static CParseHandlerBase *CreateValuesScanParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a md array coerce cast parse handler
	static CParseHandlerBase *CreateMDArrayCoerceCastParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// construct a nested loop param list parse handler
	static CParseHandlerBase *CreateNLJIndexParamListParseHandler(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_manager,
		CParseHandlerBase *parse_handler_root);

	// construct a nested loop param parse handler
	static CParseHandlerBase *CreateNLJIndexParamParseHandler(
		CMemoryPool *pmp, CParseHandlerManager *parse_handler_manager,
		CParseHandlerBase *parse_handler_root);

public:
	// initialize mappings of tokens to parse handlers
	static void Init(CMemoryPool *mp);

	// return the parse handler creator for operator with the given name
	static CParseHandlerBase *GetParseHandler(
		CMemoryPool *mp, const XMLCh *xml_str,
		CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);

	// factory methods for creating parse handlers
	static CParseHandlerDXL *GetParseHandlerDXL(CMemoryPool *mp,
												CParseHandlerManager *);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerFactory_H

// EOF
