//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerPhysicalOp.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing physical operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalOp::CParseHandlerPhysicalOp
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerPhysicalOp::CParseHandlerPhysicalOp(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalOp::~CParseHandlerPhysicalOp
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerPhysicalOp::~CParseHandlerPhysicalOp()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalOp::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag of a physical operator node.
//		This function serves as a dispatcher for invoking the correct processing
//		function for the respective operator type.
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalOp::StartElement(const XMLCh *const element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const element_qname,
									  const Attributes &attrs)
{
	// instantiate the parse handler
	CParseHandlerBase *parse_handler_base =
		CParseHandlerFactory::GetParseHandler(m_mp, element_local_name,
											  m_parse_handler_mgr, this);

	GPOS_ASSERT(NULL != parse_handler_base);

	// activate the parse handler
	m_parse_handler_mgr->ReplaceHandler(parse_handler_base,
										m_parse_handler_root);

	// pass the startElement message for the specialized parse handler to process
	parse_handler_base->startElement(element_uri, element_local_name,
									 element_qname, attrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalOp::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag.
//		This function should never be called. Instead, the endElement function
//		of the parse handler for the actual physical operator type is called.
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalOp::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const,	 // element_local_name,
									const XMLCh *const	 // element_qname
)
{
	GPOS_ASSERT(!"Invalid call of endElement inside CParseHandlerPhysicalOp");
}



// EOF
