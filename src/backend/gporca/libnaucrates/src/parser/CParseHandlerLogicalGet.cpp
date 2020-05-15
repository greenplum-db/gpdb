//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerLogicalGet.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical get operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalGet.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerTableDescr.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

#include "naucrates/dxl/operators/CDXLLogicalExternalGet.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalGet::CParseHandlerLogicalGet
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalGet::CParseHandlerLogicalGet
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalGet::StartElement
//
//	@doc:
//		Start element helper function
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalGet::StartElement
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

	// create child node parsers

	// parse handler for table descriptor
	CParseHandlerBase *table_descr_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenTableDescr), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(table_descr_parse_handler);

	// store child parse handlers in array
	this->Append(table_descr_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalGet::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalGet::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& //attrs
	)
{
	StartElement(element_local_name, EdxltokenLogicalGet);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalGet::EndElement
//
//	@doc:
//		End element helper function
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalGet::EndElement
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

	CParseHandlerTableDescr *table_descr_parse_handler = dynamic_cast<CParseHandlerTableDescr*>((*this)[0]);

	GPOS_ASSERT(NULL != table_descr_parse_handler->GetDXLTableDescr());

	CDXLTableDescr *table_descr = table_descr_parse_handler->GetDXLTableDescr();
	table_descr->AddRef();

	if (EdxltokenLogicalGet == token_type)
	{
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLLogicalGet(m_mp, table_descr));
	}
	else
	{
		GPOS_ASSERT(EdxltokenLogicalExternalGet == token_type);
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLLogicalExternalGet(m_mp, table_descr));
	}

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalGet::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalGet::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	EndElement(element_local_name, EdxltokenLogicalGet);
}

// EOF
