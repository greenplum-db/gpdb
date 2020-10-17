//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerDefaultValueExpr.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing default value expression
//---------------------------------------------------------------------------


#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

#include "naucrates/dxl/parser/CParseHandlerDefaultValueExpr.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDefaultValueExpr::CParseHandlerDefaultValueExpr
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerDefaultValueExpr::CParseHandlerDefaultValueExpr(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  is_default_val_started(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDefaultValueExpr::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerDefaultValueExpr::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenColumnDefaultValue),
				 element_local_name))
	{
		// opening tag for a default expression: assert no other tag has been seen yet
		GPOS_ASSERT(!is_default_val_started);
		is_default_val_started = true;
	}
	else
	{
		GPOS_ASSERT(is_default_val_started);

		// install a scalar op parse handler to parse the expression
		CParseHandlerBase *scalar_op_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_mp, element_local_name,
												  m_parse_handler_mgr, this);

		GPOS_ASSERT(NULL != scalar_op_parse_handler);

		// activate the child parse handler
		m_parse_handler_mgr->ActivateParseHandler(scalar_op_parse_handler);

		// pass the startElement message for the specialized parse handler to process
		scalar_op_parse_handler->startElement(element_uri, element_local_name,
											  element_qname, attrs);

		// store parse handlers
		this->Append(scalar_op_parse_handler);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDefaultValueExpr::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerDefaultValueExpr::EndElement(const XMLCh *const,  // element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenColumnDefaultValue),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	if (0 < this->Length())
	{
		GPOS_ASSERT(1 == this->Length());

		// get node for default value expression from child parse handler
		CParseHandlerScalarOp *child_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
		m_dxl_node = child_parse_handler->CreateDXLNode();
		m_dxl_node->AddRef();
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
