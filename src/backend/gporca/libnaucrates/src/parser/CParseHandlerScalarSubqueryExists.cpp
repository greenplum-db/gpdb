//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSubqueryExists.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing EXISTS and NOT EXISTS 
//		 subquery operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarSubqueryExists.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarSubqueryNotExists.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubqueryExists::CParseHandlerScalarSubqueryExists
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarSubqueryExists::CParseHandlerScalarSubqueryExists
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	m_dxl_op(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubqueryExists::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSubqueryExists::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& // attrs
	)
{
	
	GPOS_ASSERT(NULL == m_dxl_op);
		
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSubqueryExists), element_local_name))
	{
		m_dxl_op = GPOS_NEW(m_mp) CDXLScalarSubqueryExists(m_mp);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSubqueryNotExists), element_local_name))
	{
		m_dxl_op = GPOS_NEW(m_mp) CDXLScalarSubqueryNotExists(m_mp);	
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// parse handler for the child node
	CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	// store child parse handler in array
	this->Append(child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubqueryExists::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSubqueryExists::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSubqueryExists), element_local_name) &&
		0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSubqueryNotExists), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// construct node from parsed components
	GPOS_ASSERT(NULL != m_dxl_op);
	GPOS_ASSERT(1 == this->Length());
	
	CParseHandlerLogicalOp *child_parse_handler = dynamic_cast<CParseHandlerLogicalOp *>((*this)[0]);
		
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);

	// add constructed child
	AddChildFromParseHandler(child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG
	
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF

