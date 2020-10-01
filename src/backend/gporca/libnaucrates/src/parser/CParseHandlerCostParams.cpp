//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerCostParams.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing cost parameters.
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerCostParam.h"
#include "naucrates/dxl/parser/CParseHandlerCostParams.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "gpdbcost/CCostModelParamsGPDB.h"

using namespace gpdxl;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCostParams::CParseHandlerCostParams
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerCostParams::CParseHandlerCostParams(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_cost_model_params(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCostParams::~CParseHandlerCostParams
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerCostParams::~CParseHandlerCostParams()
{
	CRefCount::SafeRelease(m_cost_model_params);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCostParams::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCostParams::StartElement(const XMLCh *const element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const element_qname,
									  const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCostParams),
								 element_local_name))
	{
		// as of now, we only parse params of GPDB cost model
		m_cost_model_params = GPOS_NEW(m_mp) CCostModelParamsGPDB(m_mp);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenCostParam),
					  element_local_name))
	{
		GPOS_ASSERT(NULL != m_cost_model_params);

		// start new search stage
		CParseHandlerBase *parse_handler_cost_params =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenCostParam),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(parse_handler_cost_params);

		// store parse handler
		this->Append(parse_handler_cost_params);

		parse_handler_cost_params->startElement(element_uri, element_local_name,
												element_qname, attrs);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCostParams::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCostParams::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCostParams),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG length = this->Length();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CParseHandlerCostParam *parse_handler_cost_params =
			dynamic_cast<CParseHandlerCostParam *>((*this)[ul]);
		m_cost_model_params->SetParam(
			parse_handler_cost_params->GetName(),
			parse_handler_cost_params->Get(),
			parse_handler_cost_params->GetLowerBoundVal(),
			parse_handler_cost_params->GetUpperBoundVal());
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
