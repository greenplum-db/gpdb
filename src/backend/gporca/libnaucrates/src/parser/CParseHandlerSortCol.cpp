//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerSortCol.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing sorting columns.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerSortCol.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarSortCol.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSortCol::CParseHandlerSortCol
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerSortCol::CParseHandlerSortCol
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
//		CParseHandlerSortCol::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSortCol::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& attrs
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSortCol), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
	
	// parse and create sort col operator
	CDXLScalarSortCol *dxl_op = (CDXLScalarSortCol *) CDXLOperatorFactory::MakeDXLSortCol(m_parse_handler_mgr->GetDXLMemoryManager(), attrs);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSortCol::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSortCol::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSortCol), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
	
	GPOS_ASSERT(NULL != m_dxl_node);
	
#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG
	
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
