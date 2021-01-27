//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalWindow.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		window operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalWindow.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerWindowSpecList.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalWindow::CParseHandlerLogicalWindow
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalWindow::CParseHandlerLogicalWindow(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root),
	  m_window_spec_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalWindow::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalWindow::StartElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const,  // element_qname
										 const Attributes &	  //attrs
)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalWindow),
				 element_local_name))
	{
		// create child node parsers
		// parse handler for logical operator
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// parse handler for the proj list
		CParseHandlerBase *proj_list_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

		// parse handler for window specification list
		CParseHandlerBase *window_speclist_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenWindowSpecList),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(
			window_speclist_parse_handler);

		// store child parse handler in array
		this->Append(window_speclist_parse_handler);
		this->Append(proj_list_parse_handler);
		this->Append(child_parse_handler);
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
//		CParseHandlerLogicalWindow::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalWindow::EndElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalWindow),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerWindowSpecList *window_speclist_parse_handler =
		dynamic_cast<CParseHandlerWindowSpecList *>((*this)[0]);
	CParseHandlerProjList *proj_list_parse_handler =
		dynamic_cast<CParseHandlerProjList *>((*this)[1]);
	CParseHandlerLogicalOp *lg_op_parse_handler =
		dynamic_cast<CParseHandlerLogicalOp *>((*this)[2]);

	CDXLWindowSpecArray *window_spec_array =
		window_speclist_parse_handler->GetDxlWindowSpecArray();
	GPOS_ASSERT(NULL != window_spec_array);

	CDXLLogicalWindow *lg_window =
		GPOS_NEW(m_mp) CDXLLogicalWindow(m_mp, window_spec_array);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, lg_window);
	GPOS_ASSERT(NULL != proj_list_parse_handler->CreateDXLNode());
	GPOS_ASSERT(NULL != lg_op_parse_handler->CreateDXLNode());

	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(lg_op_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node,
										   false /* validate_children */);
#endif	// GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}
// EOF
