//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerHashExprList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing hash expression lists.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerHashExprList.h"
#include "naucrates/dxl/parser/CParseHandlerHashExpr.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarHashExprList.h"
#include "naucrates/dxl/operators/CDXLScalarProjList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashExprList::CParseHandlerHashExprList
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerHashExprList::CParseHandlerHashExprList
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}



//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashExprList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerHashExprList::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes& attrs
	)
{
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarHashExprList), element_local_name))
	{
		// start the hash expr list
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode (m_mp, GPOS_NEW(m_mp) CDXLScalarHashExprList(m_mp));
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarHashExpr), element_local_name))
	{
		// we must have seen a hash expr list already and initialized the hash expr list node
		GPOS_ASSERT(NULL != m_dxl_node);
		// start new hash expr element
		CParseHandlerBase *hash_expr_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarHashExpr), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(hash_expr_parse_handler);
		
		// store parse handler
		this->Append(hash_expr_parse_handler);
		
		hash_expr_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashExprList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerHashExprList::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarHashExprList), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
	
	const ULONG length = this->Length();
	// add hash expressions from child parse handlers
	for (ULONG ul = 0; ul < length; ul++)
	{
		CParseHandlerHashExpr *hash_expr_parse_handler = dynamic_cast<CParseHandlerHashExpr *>((*this)[ul]);
		
		AddChildFromParseHandler(hash_expr_parse_handler);
	}
		
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
