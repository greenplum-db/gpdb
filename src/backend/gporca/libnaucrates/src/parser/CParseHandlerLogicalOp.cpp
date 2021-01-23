//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerLogicalOp.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalOp::CParseHandlerLogicalOp
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalOp::CParseHandlerLogicalOp(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalOp::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag of a logical operator node.
//		This function serves as a dispatcher for invoking the correct processing
//		function for the respective operator type.
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalOp::StartElement(const XMLCh *const element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const element_qname,
									 const Attributes &attrs)
{
	// instantiate the parse handler
	CParseHandlerBase *logical_op_parse_handler =
		CParseHandlerFactory::GetParseHandler(m_mp, element_local_name,
											  m_parse_handler_mgr, this);

	GPOS_ASSERT(NULL != logical_op_parse_handler);

	// activate the parse handler
	m_parse_handler_mgr->ReplaceHandler(logical_op_parse_handler,
										m_parse_handler_root);

	// pass the startElement message for the specialized parse handler to process
	logical_op_parse_handler->startElement(element_uri, element_local_name,
										   element_qname, attrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalOp::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag.
//		This function should never be called. Instead, the endElement function
//		of the parse handler for the actual physical operator type is called.
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalOp::EndElement(const XMLCh *const,	// element_uri,
								   const XMLCh *const,	// element_local_name,
								   const XMLCh *const	// element_qname
)
{
	GPOS_ASSERT(!"Invalid call of endElement inside CParseHandlerLogicalOp");
}

// EOF
