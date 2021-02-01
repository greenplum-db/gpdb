//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerDXL.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a DXL document
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerDXL.h"

#include "gpos/task/CWorker.h"

#include "gpopt/optimizer/COptimizerConfig.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerCostParams.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerMDRequest.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadata.h"
#include "naucrates/dxl/parser/CParseHandlerOptimizerConfig.h"
#include "naucrates/dxl/parser/CParseHandlerPlan.h"
#include "naucrates/dxl/parser/CParseHandlerQuery.h"
#include "naucrates/dxl/parser/CParseHandlerScalarExpr.h"
#include "naucrates/dxl/parser/CParseHandlerSearchStrategy.h"
#include "naucrates/dxl/parser/CParseHandlerStatistics.h"
#include "naucrates/dxl/parser/CParseHandlerTraceFlags.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::CParseHandlerDXL
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerDXL::CParseHandlerDXL(CMemoryPool *mp,
								   CParseHandlerManager *parse_handler_mgr)
	: CParseHandlerBase(mp, parse_handler_mgr, NULL),
	  m_trace_flags_bitset(NULL),
	  m_optimizer_config(NULL),
	  m_mdrequest(NULL),
	  m_query_dxl_root(NULL),
	  m_output_colums_dxl_array(NULL),
	  m_cte_producers(NULL),
	  m_plan_dxl_root(NULL),
	  m_mdid_cached_obj_array(NULL),
	  m_mdid_array(NULL),
	  m_scalar_expr_dxl(NULL),
	  m_system_id_array(NULL),
	  m_dxl_stats_derived_rel_array(NULL),
	  m_search_stage_array(NULL),
	  m_plan_id(gpos::ullong_max),
	  m_plan_space_size(gpos::ullong_max),
	  m_cost_model_params(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::~CParseHandlerDXL
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerDXL::~CParseHandlerDXL()
{
	CRefCount::SafeRelease(m_trace_flags_bitset);
	CRefCount::SafeRelease(m_optimizer_config);
	CRefCount::SafeRelease(m_mdrequest);
	CRefCount::SafeRelease(m_query_dxl_root);
	CRefCount::SafeRelease(m_output_colums_dxl_array);
	CRefCount::SafeRelease(m_cte_producers);
	CRefCount::SafeRelease(m_plan_dxl_root);
	CRefCount::SafeRelease(m_mdid_cached_obj_array);
	CRefCount::SafeRelease(m_mdid_array);
	CRefCount::SafeRelease(m_scalar_expr_dxl);
	CRefCount::SafeRelease(m_system_id_array);
	CRefCount::SafeRelease(m_dxl_stats_derived_rel_array);
	CRefCount::SafeRelease(m_search_stage_array);
	CRefCount::SafeRelease(m_cost_model_params);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::Pbs
//
//	@doc:
//		Returns the bitset of traceflags
//
//---------------------------------------------------------------------------
CBitSet *
CParseHandlerDXL::Pbs() const
{
	return m_trace_flags_bitset;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::Poc
//
//	@doc:
//		Returns the optimizer config object
//
//---------------------------------------------------------------------------
COptimizerConfig *
CParseHandlerDXL::GetOptimizerConfig() const
{
	return m_optimizer_config;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetQueryDXLRoot
//
//	@doc:
//		Returns the root of the DXL query constructed by this parser
//
//---------------------------------------------------------------------------
CDXLNode *
CParseHandlerDXL::GetQueryDXLRoot() const
{
	return m_query_dxl_root;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetOutputColumnsDXLArray
//
//	@doc:
//		Returns the list of query output objects constructed by the parser
//
//---------------------------------------------------------------------------
CDXLNodeArray *
CParseHandlerDXL::GetOutputColumnsDXLArray() const
{
	return m_output_colums_dxl_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetCTEProducerDXLArray
//
//	@doc:
//		Returns the list of CTE producers
//
//---------------------------------------------------------------------------
CDXLNodeArray *
CParseHandlerDXL::GetCTEProducerDXLArray() const
{
	return m_cte_producers;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::PdxlnPlan
//
//	@doc:
//		Returns the root of the DXL plan constructed by this parser
//
//---------------------------------------------------------------------------
CDXLNode *
CParseHandlerDXL::PdxlnPlan() const
{
	return m_plan_dxl_root;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetMdIdCachedObjArray
//
//	@doc:
//		Returns the list of metadata objects constructed by the parser
//
//---------------------------------------------------------------------------
IMDCacheObjectArray *
CParseHandlerDXL::GetMdIdCachedObjArray() const
{
	return m_mdid_cached_obj_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetMdIdArray
//
//	@doc:
//		Returns the list of metadata ids constructed by the parser
//
//---------------------------------------------------------------------------
IMdIdArray *
CParseHandlerDXL::GetMdIdArray() const
{
	return m_mdid_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetMiniDumper
//
//	@doc:
//		Return the md request
//
//---------------------------------------------------------------------------
CMDRequest *
CParseHandlerDXL::GetMiniDumper() const
{
	return m_mdrequest;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetScalarExprDXLRoot
//
//	@doc:
//		Returns the DXL node representing the parsed scalar expression
//
//---------------------------------------------------------------------------
CDXLNode *
CParseHandlerDXL::GetScalarExprDXLRoot() const
{
	return m_scalar_expr_dxl;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetSysidPtrArray
//
//	@doc:
//		Returns the list of source system ids for the metadata
//
//---------------------------------------------------------------------------
CSystemIdArray *
CParseHandlerDXL::GetSysidPtrArray() const
{
	return m_system_id_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetStatsDerivedRelDXLArray
//
//	@doc:
//		Returns the list of statistics objects constructed by the parser
//
//---------------------------------------------------------------------------
CDXLStatsDerivedRelationArray *
CParseHandlerDXL::GetStatsDerivedRelDXLArray() const
{
	return m_dxl_stats_derived_rel_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetSearchStageArray
//
//	@doc:
//		Returns search strategy
//
//---------------------------------------------------------------------------
CSearchStageArray *
CParseHandlerDXL::GetSearchStageArray() const
{
	return m_search_stage_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetPlanId
//
//	@doc:
//		Returns plan id
//
//---------------------------------------------------------------------------
ULLONG
CParseHandlerDXL::GetPlanId() const
{
	return m_plan_id;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetPlanId
//
//	@doc:
//		Returns plan space size
//
//---------------------------------------------------------------------------
ULLONG
CParseHandlerDXL::GetPlanSpaceSize() const
{
	return m_plan_space_size;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::GetCostModelParams
//
//	@doc:
//		Returns cost params
//
//---------------------------------------------------------------------------
ICostModelParams *
CParseHandlerDXL::GetCostModelParams() const
{
	return m_cost_model_params;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::IsValidStartElement
//
//	@doc:
//		Return true if given element name is valid to start DXL document
//
//---------------------------------------------------------------------------
BOOL
CParseHandlerDXL::IsValidStartElement(const XMLCh *const element_name)
{
	// names of valid start elements of DXL document
	const XMLCh *valid_start_element_array[] = {
		CDXLTokens::XmlstrToken(EdxltokenTraceFlags),
		CDXLTokens::XmlstrToken(EdxltokenOptimizerConfig),
		CDXLTokens::XmlstrToken(EdxltokenPlan),
		CDXLTokens::XmlstrToken(EdxltokenQuery),
		CDXLTokens::XmlstrToken(EdxltokenMetadata),
		CDXLTokens::XmlstrToken(EdxltokenMDRequest),
		CDXLTokens::XmlstrToken(EdxltokenStatistics),
		CDXLTokens::XmlstrToken(EdxltokenStackTrace),
		CDXLTokens::XmlstrToken(EdxltokenSearchStrategy),
		CDXLTokens::XmlstrToken(EdxltokenCostParams),
		CDXLTokens::XmlstrToken(EdxltokenScalarExpr),
	};

	BOOL is_valid_start_element = false;
	for (ULONG idx = 0; !is_valid_start_element &&
						idx < GPOS_ARRAY_SIZE(valid_start_element_array);
		 idx++)
	{
		is_valid_start_element =
			(0 == XMLString::compareString(element_name,
										   valid_start_element_array[idx]));
	}

	return is_valid_start_element;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::StartElement(const XMLCh *const element_uri,
							   const XMLCh *const element_local_name,
							   const XMLCh *const element_qname,
							   const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 element_local_name,
				 CDXLTokens::XmlstrToken(EdxltokenDXLMessage)) ||
		0 ==
			XMLString::compareString(
				element_local_name, CDXLTokens::XmlstrToken(EdxltokenThread)) ||
		0 == XMLString::compareString(
				 element_local_name, CDXLTokens::XmlstrToken(EdxltokenComment)))
	{
		// beginning of DXL document or a new thread info
		;
	}
	else
	{
		GPOS_ASSERT(IsValidStartElement(element_local_name));

		// install a parse handler for the given element
		CParseHandlerBase *parse_handler_base =
			CParseHandlerFactory::GetParseHandler(m_mp, element_local_name,
												  m_parse_handler_mgr, this);

		m_parse_handler_mgr->ActivateParseHandler(parse_handler_base);

		// store parse handler
		this->Append(parse_handler_base);

		parse_handler_base->startElement(element_uri, element_local_name,
										 element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::EndElement(const XMLCh *const,  // element_uri,
							 const XMLCh *const,  // element_local_name,
							 const XMLCh *const	  // element_qname
)
{
	// ignore
}

void
CParseHandlerDXL::endDocument()
{
	// retrieve plan and/or query and/or list of metadata objects from child parse handler
	for (ULONG idx = 0; idx < this->Length(); idx++)
	{
		CParseHandlerBase *parse_handler_base = (*this)[idx];

		EDxlParseHandlerType parse_handler_type =
			parse_handler_base->GetParseHandlerType();

		// find parse handler for the current type
		ParseHandler parse_handler_func = FindParseHandler(parse_handler_type);

		if (NULL != parse_handler_func)
		{
			(this->*parse_handler_func)(parse_handler_base);
		}
	}

	m_parse_handler_mgr->DeactivateHandler();
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::FindParseHandler
//
//	@doc:
//		Find the parse handler function for the given type
//
//---------------------------------------------------------------------------
ParseHandler
CParseHandlerDXL::FindParseHandler(EDxlParseHandlerType parse_handler_type)
{
	SParseElem parse_element_type_func_map[] = {
		{EdxlphTraceFlags, &CParseHandlerDXL::ExtractTraceFlags},
		{EdxlphOptConfig, &CParseHandlerDXL::ExtractOptimizerConfig},
		{EdxlphPlan, &CParseHandlerDXL::ExtractDXLPlan},
		{EdxlphMetadata, &CParseHandlerDXL::ExtractMetadataObjects},
		{EdxlphStatistics, &CParseHandlerDXL::ExtractStats},
		{EdxlphQuery, &CParseHandlerDXL::ExtractDXLQuery},
		{EdxlphMetadataRequest, &CParseHandlerDXL::ExtractMDRequest},
		{EdxlphSearchStrategy, &CParseHandlerDXL::ExtractSearchStrategy},
		{EdxlphCostParams, &CParseHandlerDXL::ExtractCostParams},
		{EdxlphScalarExpr, &CParseHandlerDXL::ExtractScalarExpr},
	};

	const ULONG num_of_parse_handler =
		GPOS_ARRAY_SIZE(parse_element_type_func_map);
	ParseHandler parse_handler_func = NULL;
	for (ULONG idx = 0; idx < num_of_parse_handler; idx++)
	{
		SParseElem elem = parse_element_type_func_map[idx];
		if (parse_handler_type == elem.parse_handler_type)
		{
			parse_handler_func = elem.parse_handler_func;
			break;
		}
	}

	return parse_handler_func;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractTraceFlags
//
//	@doc:
//		Extract traceflags
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractTraceFlags(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerTraceFlags *parse_handler_tf =
		(CParseHandlerTraceFlags *) parse_handler_base;
	GPOS_ASSERT(NULL != parse_handler_tf);

	GPOS_ASSERT(NULL == m_trace_flags_bitset && "Traceflags already set");

	m_trace_flags_bitset = parse_handler_tf->GetTraceFlagBitSet();
	m_trace_flags_bitset->AddRef();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractOptimizerConfig
//
//	@doc:
//		Extract optimizer config
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractOptimizerConfig(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerOptimizerConfig *parse_handler_opt_config =
		(CParseHandlerOptimizerConfig *) parse_handler_base;
	GPOS_ASSERT(NULL != parse_handler_opt_config);

	GPOS_ASSERT(NULL == m_trace_flags_bitset && "Traceflags already set");

	m_trace_flags_bitset = parse_handler_opt_config->Pbs();
	m_trace_flags_bitset->AddRef();

	GPOS_ASSERT(NULL == m_optimizer_config &&
				"Optimizer configuration already set");

	m_optimizer_config = parse_handler_opt_config->GetOptimizerConfig();
	m_optimizer_config->AddRef();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractDXLPlan
//
//	@doc:
//		Extract a physical plan
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractDXLPlan(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerPlan *parse_handler_plan =
		(CParseHandlerPlan *) parse_handler_base;
	GPOS_ASSERT(NULL != parse_handler_plan &&
				NULL != parse_handler_plan->CreateDXLNode());

	m_plan_dxl_root = parse_handler_plan->CreateDXLNode();
	m_plan_dxl_root->AddRef();

	m_plan_id = parse_handler_plan->PlanId();
	m_plan_space_size = parse_handler_plan->PlanSpaceSize();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractMetadataObjects
//
//	@doc:
//		Extract metadata objects
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractMetadataObjects(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerMetadata *parse_handler_md =
		dynamic_cast<CParseHandlerMetadata *>(parse_handler_base);
	GPOS_ASSERT(NULL != parse_handler_md &&
				NULL != parse_handler_md->GetMdIdCachedObjArray());

	m_mdid_cached_obj_array = parse_handler_md->GetMdIdCachedObjArray();
	m_mdid_cached_obj_array->AddRef();

	m_mdid_array = parse_handler_md->GetMdIdArray();
	m_mdid_array->AddRef();

	m_system_id_array = parse_handler_md->GetSysidPtrArray();

	if (NULL != m_system_id_array)
	{
		m_system_id_array->AddRef();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractStats
//
//	@doc:
//		Extract statistics
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractStats(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerStatistics *parse_handler_stats =
		dynamic_cast<CParseHandlerStatistics *>(parse_handler_base);
	GPOS_ASSERT(NULL != parse_handler_stats);

	CDXLStatsDerivedRelationArray *dxl_derived_rel_stats_array =
		parse_handler_stats->GetStatsDerivedRelDXLArray();
	GPOS_ASSERT(NULL != dxl_derived_rel_stats_array);

	dxl_derived_rel_stats_array->AddRef();
	m_dxl_stats_derived_rel_array = dxl_derived_rel_stats_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractDXLQuery
//
//	@doc:
//		Extract DXL query
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractDXLQuery(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerQuery *parse_handler_query =
		dynamic_cast<CParseHandlerQuery *>(parse_handler_base);
	GPOS_ASSERT(NULL != parse_handler_query &&
				NULL != parse_handler_query->CreateDXLNode());

	m_query_dxl_root = parse_handler_query->CreateDXLNode();
	m_query_dxl_root->AddRef();

	GPOS_ASSERT(NULL != parse_handler_query->GetOutputColumnsDXLArray());

	m_output_colums_dxl_array = parse_handler_query->GetOutputColumnsDXLArray();
	m_output_colums_dxl_array->AddRef();

	m_cte_producers = parse_handler_query->GetCTEProducerDXLArray();
	m_cte_producers->AddRef();
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractMDRequest
//
//	@doc:
//		Extract mdids
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractMDRequest(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerMDRequest *parse_handler_mdrequest =
		dynamic_cast<CParseHandlerMDRequest *>(parse_handler_base);
	GPOS_ASSERT(NULL != parse_handler_mdrequest &&
				NULL != parse_handler_mdrequest->GetMdIdArray());

	IMdIdArray *mdid_array = parse_handler_mdrequest->GetMdIdArray();
	CMDRequest::SMDTypeRequestArray *md_type_request_array =
		parse_handler_mdrequest->GetMDTypeRequestArray();

	mdid_array->AddRef();
	md_type_request_array->AddRef();

	m_mdrequest =
		GPOS_NEW(m_mp) CMDRequest(m_mp, mdid_array, md_type_request_array);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractSearchStrategy
//
//	@doc:
//		Extract search strategy
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractSearchStrategy(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerSearchStrategy *parse_handler_search_strategy =
		dynamic_cast<CParseHandlerSearchStrategy *>(parse_handler_base);
	GPOS_ASSERT(NULL != parse_handler_search_strategy &&
				NULL != parse_handler_search_strategy->GetSearchStageArray());

	CSearchStageArray *search_stage_array =
		parse_handler_search_strategy->GetSearchStageArray();

	search_stage_array->AddRef();
	m_search_stage_array = search_stage_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractCostParams
//
//	@doc:
//		Extract cost params
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractCostParams(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerCostParams *parse_handler_cost_params =
		dynamic_cast<CParseHandlerCostParams *>(parse_handler_base);
	GPOS_ASSERT(NULL != parse_handler_cost_params &&
				NULL != parse_handler_cost_params->GetCostModelParams());

	ICostModelParams *cost_model_params =
		parse_handler_cost_params->GetCostModelParams();

	cost_model_params->AddRef();
	m_cost_model_params = cost_model_params;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDXL::ExtractScalarExpr
//
//	@doc:
//		Extract scalar expressions
//
//---------------------------------------------------------------------------
void
CParseHandlerDXL::ExtractScalarExpr(CParseHandlerBase *parse_handler_base)
{
	CParseHandlerScalarExpr *parse_handler_scalar_expr =
		dynamic_cast<CParseHandlerScalarExpr *>(parse_handler_base);
	GPOS_ASSERT(NULL != parse_handler_scalar_expr &&
				NULL != parse_handler_scalar_expr->CreateDXLNode());

	m_scalar_expr_dxl = parse_handler_scalar_expr->CreateDXLNode();
	m_scalar_expr_dxl->AddRef();
}

// EOF
