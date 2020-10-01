//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerWindowSpecList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing the list of
//		window specifications in the logical window operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerWindowSpec.h"
#include "naucrates/dxl/parser/CParseHandlerWindowSpecList.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerWindowSpecList::CParseHandlerWindowSpecList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerWindowSpecList::CParseHandlerWindowSpecList(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_window_spec_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerWindowSpecList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerWindowSpecList::StartElement(const XMLCh *const element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const element_qname,
										  const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenWindowSpecList),
				 element_local_name))
	{
		m_window_spec_array = GPOS_NEW(m_mp) CDXLWindowSpecArray(m_mp);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenWindowSpec),
					  element_local_name))
	{
		// we must have seen a window specification list already
		GPOS_ASSERT(NULL != m_window_spec_array);
		// start new window specification element
		CParseHandlerBase *window_spec_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenWindowSpec),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(window_spec_parse_handler);

		// store parse handler
		this->Append(window_spec_parse_handler);

		window_spec_parse_handler->startElement(element_uri, element_local_name,
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
//		CParseHandlerWindowSpecList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerWindowSpecList::EndElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenWindowSpecList),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	GPOS_ASSERT(NULL != m_window_spec_array);

	const ULONG size = this->Length();
	// add the window specifications to the list
	for (ULONG idx = 0; idx < size; idx++)
	{
		CParseHandlerWindowSpec *window_spec_parse_handler =
			dynamic_cast<CParseHandlerWindowSpec *>((*this)[idx]);
		m_window_spec_array->Append(
			window_spec_parse_handler->GetWindowKeyAt());
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
