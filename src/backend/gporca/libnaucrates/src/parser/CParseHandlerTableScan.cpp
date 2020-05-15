//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerTableScan.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing table scan operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerTableScan.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerTableDescr.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLPhysicalExternalScan.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableScan::CParseHandlerTableScan
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerTableScan::CParseHandlerTableScan
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_dxl_op(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableScan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerTableScan::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& // attrs
	)
{
	StartElement(element_local_name, EdxltokenPhysicalTableScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableScan::StartElement
//
//	@doc:
//		Start element helper function
//
//---------------------------------------------------------------------------
void
CParseHandlerTableScan::StartElement
	(
	const XMLCh* const element_local_name,
	Edxltoken token_type
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(token_type), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	if (EdxltokenPhysicalTableScan == token_type)
	{
		m_dxl_op = GPOS_NEW(m_mp) CDXLPhysicalTableScan(m_mp);
	}
	else
	{
		GPOS_ASSERT(EdxltokenPhysicalExternalScan == token_type);
		m_dxl_op = GPOS_NEW(m_mp) CDXLPhysicalExternalScan(m_mp);
	}

	// create child node parsers in reverse order of their expected occurrence

	// parse handler for table descriptor
	CParseHandlerBase *table_descr_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenTableDescr), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(table_descr_parse_handler);

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
	this->Append(table_descr_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerTableScan::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	EndElement(element_local_name, EdxltokenPhysicalTableScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTableScan::EndElement
//
//	@doc:
//		End element helper function
//
//---------------------------------------------------------------------------
void
CParseHandlerTableScan::EndElement
	(
	const XMLCh* const element_local_name,
	Edxltoken token_type
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(token_type), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// construct node from the created child nodes
	CParseHandlerProperties *prop_parse_handler = dynamic_cast<CParseHandlerProperties *>((*this)[0]);
	CParseHandlerProjList *proj_list_parse_handler = dynamic_cast<CParseHandlerProjList*>((*this)[1]);
	CParseHandlerFilter *filter_parse_handler = dynamic_cast<CParseHandlerFilter *>((*this)[2]);
	CParseHandlerTableDescr *table_descr_parse_handler = dynamic_cast<CParseHandlerTableDescr*>((*this)[3]);

	GPOS_ASSERT(NULL != table_descr_parse_handler->GetDXLTableDescr());

	// set table descriptor
	CDXLTableDescr *table_descr = table_descr_parse_handler->GetDXLTableDescr();
	table_descr->AddRef();
	m_dxl_op->SetTableDescriptor(table_descr);

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);
	// set statictics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// add constructed children
	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(filter_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF

