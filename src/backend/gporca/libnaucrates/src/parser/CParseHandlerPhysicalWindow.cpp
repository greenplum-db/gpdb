//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerPhysicalWindow.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing
//		a physical window operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPhysicalWindow.h"
#include "naucrates/dxl/parser/CParseHandlerWindowKeyList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLPhysicalWindow.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalWindow::CParseHandlerPhysicalWindow
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerPhysicalWindow::CParseHandlerPhysicalWindow
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_part_by_colid_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalWindow::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalWindow::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& attrs
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalWindow), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	const XMLCh *xml_str_part_by_cols = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenPartKeys, EdxltokenPhysicalWindow);
	m_part_by_colid_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_part_by_cols, EdxltokenPartKeys, EdxltokenPhysicalWindow);
	GPOS_ASSERT(NULL != m_part_by_colid_array);

	// parse handler for window key list
	CParseHandlerBase *window_key_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenWindowKeyList), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(window_key_list_parse_handler);

	// parse handler for child node
	CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	// parse handler for the filter
	CParseHandlerBase *filter_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarFilter), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(filter_parse_handler);

	// parse handler for the proj list
	CParseHandlerBase *proj_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

	//parse handler for the properties of the operator
	CParseHandlerBase *prop_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

	// store child parse handlers in array
	this->Append(prop_parse_handler);
	this->Append(proj_list_parse_handler);
	this->Append(filter_parse_handler);
	this->Append(child_parse_handler);
	this->Append(window_key_list_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalWindow::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalWindow::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalWindow), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(5 == this->Length());
	CParseHandlerProperties *prop_parse_handler = dynamic_cast<CParseHandlerProperties *>((*this)[0]);
	CParseHandlerProjList *proj_list_parse_handler = dynamic_cast<CParseHandlerProjList*>((*this)[1]);
	CParseHandlerFilter *filter_parse_handler = dynamic_cast<CParseHandlerFilter *>((*this)[2]);
	CParseHandlerPhysicalOp *child_parse_handler = dynamic_cast<CParseHandlerPhysicalOp *>((*this)[3]);

	CParseHandlerWindowKeyList *window_key_list_parse_handler = dynamic_cast<CParseHandlerWindowKeyList *>((*this)[4]);
	CDXLWindowKeyArray *window_key_array_dxlnode = window_key_list_parse_handler->GetDxlWindowKeyArray();
	CDXLPhysicalWindow *window_op_dxlnode = GPOS_NEW(m_mp) CDXLPhysicalWindow(m_mp, m_part_by_colid_array, window_key_array_dxlnode);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, window_op_dxlnode);

	// set statistics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// add children
	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(filter_parse_handler);
	AddChildFromParseHandler(child_parse_handler);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF

