//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerHashExpr.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing hash expression operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerHashExpr.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashExpr::CParseHandlerHashExpr
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerHashExpr::CParseHandlerHashExpr(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_op(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashExpr::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerHashExpr::StartElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const,	 // element_qname
									const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarHashExpr),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse and create hash expr operator
	m_dxl_op = (CDXLScalarHashExpr *) CDXLOperatorFactory::MakeDXLHashExpr(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

	// create and activate the parse handler for the child scalar expression node

	CParseHandlerBase *op_parse_handler = CParseHandlerFactory::GetParseHandler(
		m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr,
		this);
	m_parse_handler_mgr->ActivateParseHandler(op_parse_handler);

	// store child parse handler
	this->Append(op_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashExpr::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerHashExpr::EndElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarHashExpr),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerScalarOp *op_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);

	GPOS_ASSERT(NULL != op_parse_handler->CreateDXLNode());

	// construct node from the parsed expression node
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);

	AddChildFromParseHandler(op_parse_handler);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
