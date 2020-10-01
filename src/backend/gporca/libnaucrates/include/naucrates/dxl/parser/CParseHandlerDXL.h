//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerDXL.h
//
//	@doc:
//		SAX parse handler class for parsing a DXL document
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerDXL_H
#define GPDXL_CParseHandlerDXL_H

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"

#include "gpopt/cost/ICostModelParams.h"
#include "gpopt/search/CSearchStage.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"

#include "naucrates/md/CDXLStatsDerivedRelation.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

// shorthand for functions for translating GPDB expressions into DXL nodes
typedef void (CParseHandlerDXL::*ParseHandler)(
	CParseHandlerBase *parse_handler_base);


//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerDXL
//
//	@doc:
//		Parse handler for DXL documents.
//		Starting point for all other parse handlers
//
//---------------------------------------------------------------------------
class CParseHandlerDXL : public CParseHandlerBase
{
private:
	// pair of parse handler type and parse handler function
	struct SParseElem
	{
		EDxlParseHandlerType parse_handler_type;  // parse handler type
		ParseHandler parse_handler_func;  // pointer to corresponding function
	};

	// traceflags
	CBitSet *m_trace_flags_bitset;

	// optimizer config
	COptimizerConfig *m_optimizer_config;

	// MD request
	CMDRequest *m_mdrequest;

	// the root of the parsed DXL query
	CDXLNode *m_query_dxl_root;

	// list of query output columns
	CDXLNodeArray *m_output_colums_dxl_array;

	// list of CTE producers
	CDXLNodeArray *m_cte_producers;

	// the root of the parsed DXL plan
	CDXLNode *m_plan_dxl_root;

	// list of parsed metadata objects
	IMDCacheObjectArray *m_mdid_cached_obj_array;

	// list of parsed metadata ids
	IMdIdArray *m_mdid_array;

	// the root of the parsed scalar expression
	CDXLNode *m_scalar_expr_dxl;

	// list of source system ids
	CSystemIdArray *m_system_id_array;

	// list of parsed statistics objects
	CDXLStatsDerivedRelationArray *m_dxl_stats_derived_rel_array;

	// search strategy
	CSearchStageArray *m_search_stage_array;

	// plan Id
	ULLONG m_plan_id;

	// plan space size
	ULLONG m_plan_space_size;

	// cost model params
	ICostModelParams *m_cost_model_params;

	// private copy ctor
	CParseHandlerDXL(const CParseHandlerDXL &);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

	// find the parse handler function for the given type
	ParseHandler FindParseHandler(EDxlParseHandlerType parse_handler_type);

	// extract traceflags
	void ExtractTraceFlags(CParseHandlerBase *parse_handler_base);

	// extract optimizer config
	void ExtractOptimizerConfig(CParseHandlerBase *parse_handler_base);

	// extract a physical plan
	void ExtractDXLPlan(CParseHandlerBase *parse_handler_base);

	// extract metadata objects
	void ExtractMetadataObjects(CParseHandlerBase *parse_handler_base);

	// extract statistics
	void ExtractStats(CParseHandlerBase *parse_handler_base);

	// extract DXL query
	void ExtractDXLQuery(CParseHandlerBase *parse_handler_base);

	// extract mdids of requested objects
	void ExtractMDRequest(CParseHandlerBase *parse_handler_base);

	// extract search strategy
	void ExtractSearchStrategy(CParseHandlerBase *parse_handler_base);

	// extract cost params
	void ExtractCostParams(CParseHandlerBase *parse_handler_base);

	// extract a top level scalar expression
	void ExtractScalarExpr(CParseHandlerBase *parse_handler_base);

	// check if given element name is valid for starting DXL document
	static BOOL IsValidStartElement(const XMLCh *const element_name);

public:
	// ctor
	CParseHandlerDXL(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr);

	//dtor
	virtual ~CParseHandlerDXL();

	// traceflag bitset
	CBitSet *Pbs() const;

	// optimizer config
	COptimizerConfig *GetOptimizerConfig() const;

	// returns the root of the parsed DXL query
	CDXLNode *GetQueryDXLRoot() const;

	// returns the list of query output columns
	CDXLNodeArray *GetOutputColumnsDXLArray() const;

	// returns the list of CTE producers
	CDXLNodeArray *GetCTEProducerDXLArray() const;

	// returns the root of the parsed DXL plan
	CDXLNode *PdxlnPlan() const;

	// return the list of parsed metadata objects
	IMDCacheObjectArray *GetMdIdCachedObjArray() const;

	// return the list of parsed metadata ids
	IMdIdArray *GetMdIdArray() const;

	// return the MD request object
	CMDRequest *GetMiniDumper() const;

	// return the root of the parsed scalar expression
	CDXLNode *GetScalarExprDXLRoot() const;

	// return the list of parsed source system id objects
	CSystemIdArray *GetSysidPtrArray() const;

	// return the list of statistics objects
	CDXLStatsDerivedRelationArray *GetStatsDerivedRelDXLArray() const;

	// return search strategy
	CSearchStageArray *GetSearchStageArray() const;

	// return plan id
	ULLONG GetPlanId() const;

	// return plan space size
	ULLONG GetPlanSpaceSize() const;

	// return cost params
	ICostModelParams *GetCostModelParams() const;

	// process the end of the document
	void endDocument();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerDXL_H

// EOF
