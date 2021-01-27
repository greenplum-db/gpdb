//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerWindowKey.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing the window key
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerWindowKey.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerSortColList.h"
#include "naucrates/dxl/parser/CParseHandlerWindowFrame.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerWindowKey::CParseHandlerWindowKey
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerWindowKey::CParseHandlerWindowKey(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_window_key_gen(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerWindowKey::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerWindowKey::StartElement(const XMLCh *const element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const element_qname,
									 const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowKey),
								 element_local_name))
	{
		GPOS_ASSERT(NULL == m_dxl_window_key_gen);
		m_dxl_window_key_gen = GPOS_NEW(m_mp) CDXLWindowKey(m_mp);

		// parse handler for the sorting column list
		CParseHandlerBase *sort_col_list_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSortColList),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(sort_col_list_parse_handler);

		// store parse handler
		this->Append(sort_col_list_parse_handler);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenWindowFrame),
					  element_local_name))
	{
		GPOS_ASSERT(1 == this->Length());

		// parse handler for the leading and trailing scalar values
		CParseHandlerBase *window_frame_parse_handler_base =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenWindowFrame),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(
			window_frame_parse_handler_base);

		// store parse handler
		this->Append(window_frame_parse_handler_base);
		window_frame_parse_handler_base->startElement(
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
//		CParseHandlerWindowKey::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerWindowKey::EndElement(const XMLCh *const,	// element_uri,
								   const XMLCh *const element_local_name,
								   const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowKey),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	GPOS_ASSERT(NULL != m_dxl_window_key_gen);
	GPOS_ASSERT(1 <= this->Length());

	CParseHandlerSortColList *sort_col_list_parse_handler =
		dynamic_cast<CParseHandlerSortColList *>((*this)[0]);
	CDXLNode *sort_col_list_dxlnode =
		sort_col_list_parse_handler->CreateDXLNode();
	sort_col_list_dxlnode->AddRef();
	m_dxl_window_key_gen->SetSortColList(sort_col_list_dxlnode);

	if (2 == this->Length())
	{
		CParseHandlerWindowFrame *window_frame_parse_handler_base =
			dynamic_cast<CParseHandlerWindowFrame *>((*this)[1]);
		CDXLWindowFrame *window_frame =
			window_frame_parse_handler_base->GetWindowFrame();
		m_dxl_window_key_gen->SetWindowFrame(window_frame);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
