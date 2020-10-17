//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerCost.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing cost estimates.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerCost.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCost::CParseHandlerCost
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerCost::CParseHandlerCost(CMemoryPool *mp,
									 CParseHandlerManager *parse_handler_mgr,
									 CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_operator_cost_dxl(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCost::~CParseHandlerCost
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerCost::~CParseHandlerCost()
{
	CRefCount::SafeRelease(m_operator_cost_dxl);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCost::MakeDXLOperatorCost
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLOperatorCost *
CParseHandlerCost::GetDXLOperatorCost()
{
	return m_operator_cost_dxl;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCost::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCost::StartElement(const XMLCh *const,	 // element_uri,
								const XMLCh *const element_local_name,
								const XMLCh *const,	 // element_qname
								const Attributes &attrs)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCost),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// get cost estimates from attributes
	m_operator_cost_dxl = CDXLOperatorFactory::MakeDXLOperatorCost(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCost::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCost::EndElement(const XMLCh *const,  // element_uri,
							  const XMLCh *const element_local_name,
							  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCost),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
