//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerSearchStrategy.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing search strategy.
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerSearchStrategy.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerSearchStage.h"

using namespace gpdxl;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSearchStrategy::CParseHandlerSearchStrategy
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerSearchStrategy::CParseHandlerSearchStrategy(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_search_stage_array(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSearchStrategy::~CParseHandlerSearchStrategy
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerSearchStrategy::~CParseHandlerSearchStrategy()
{
	CRefCount::SafeRelease(m_search_stage_array);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSearchStrategy::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSearchStrategy::StartElement(const XMLCh *const element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const element_qname,
										  const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenSearchStrategy),
				 element_local_name))
	{
		m_search_stage_array = GPOS_NEW(m_mp) CSearchStageArray(m_mp);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenSearchStage),
					  element_local_name))
	{
		GPOS_ASSERT(NULL != m_search_stage_array);

		// start new search stage
		CParseHandlerBase *search_stage_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenSearchStage),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(search_stage_parse_handler);

		// store parse handler
		this->Append(search_stage_parse_handler);

		search_stage_parse_handler->startElement(
			element_uri, element_local_name, element_qname, attrs);
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
//		CParseHandlerSearchStrategy::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSearchStrategy::EndElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenSearchStrategy),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG size = this->Length();
	for (ULONG idx = 0; idx < size; idx++)
	{
		CParseHandlerSearchStage *search_stage_parse_handler =
			dynamic_cast<CParseHandlerSearchStage *>((*this)[idx]);
		CXformSet *xform_set = search_stage_parse_handler->GetXformSet();
		xform_set->AddRef();
		CSearchStage *search_stage = GPOS_NEW(m_mp)
			CSearchStage(xform_set, search_stage_parse_handler->TimeThreshold(),
						 search_stage_parse_handler->CostThreshold());
		m_search_stage_array->Append(search_stage);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
