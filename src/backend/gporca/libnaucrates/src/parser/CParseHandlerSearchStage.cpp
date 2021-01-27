//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerSearchStage.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing search strategy.
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerSearchStage.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerXform.h"


using namespace gpopt;
using namespace gpdxl;


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSearchStage::CParseHandlerSearchStage
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerSearchStage::CParseHandlerSearchStage(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_xforms(NULL),
	  m_cost_threshold(GPOPT_INVALID_COST)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSearchStage::~CParseHandlerSearchStage
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerSearchStage::~CParseHandlerSearchStage()
{
	CRefCount::SafeRelease(m_xforms);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSearchStage::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSearchStage::StartElement(const XMLCh *const element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const element_qname,
									   const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenSearchStage),
								 element_local_name))
	{
		// start search stage section in the DXL document
		GPOS_ASSERT(NULL == m_xforms);

		m_xforms = GPOS_NEW(m_mp) CXformSet(m_mp);

		const XMLCh *xml_str_cost = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenCostThreshold, EdxltokenSearchStage);

		m_cost_threshold = CCost(CDXLOperatorFactory::ConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_cost,
			EdxltokenCostThreshold, EdxltokenSearchStage));

		const XMLCh *xml_str_time = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenTimeThreshold, EdxltokenSearchStage);

		m_time_threshold = CDXLOperatorFactory::ConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_time,
			EdxltokenTimeThreshold, EdxltokenSearchStage);
	}
	else if (0 ==
			 XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenXform),
									  element_local_name))
	{
		GPOS_ASSERT(NULL != m_xforms);

		// start new xform
		CParseHandlerBase *xform_set_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenXform),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(xform_set_parse_handler);

		// store parse handler
		this->Append(xform_set_parse_handler);

		xform_set_parse_handler->startElement(element_uri, element_local_name,
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
//		CParseHandlerSearchStage::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSearchStage::EndElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const	 // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenSearchStage),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG size = this->Length();
	// add constructed children from child parse handlers
	for (ULONG idx = 0; idx < size; idx++)
	{
		CParseHandlerXform *xform_set_parse_handler =
			dynamic_cast<CParseHandlerXform *>((*this)[idx]);
#ifdef GPOS_DEBUG
		BOOL fSet =
#endif	// GPOS_DEBUG
			m_xforms->ExchangeSet(xform_set_parse_handler->GetXform()->Exfid());
		GPOS_ASSERT(!fSet);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
