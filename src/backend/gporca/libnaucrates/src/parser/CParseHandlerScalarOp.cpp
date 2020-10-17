//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarOp.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing scalar operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOp::CParseHandlerScalarOp
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarOp::CParseHandlerScalarOp(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOp::~CParseHandlerScalarOp
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarOp::~CParseHandlerScalarOp()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOp::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag for a scalar operator.
//		The function serves as a dispatcher for invoking the correct processing
//		function of the actual operator type.
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarOp::StartElement(const XMLCh *const element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const element_qname,
									const Attributes &attrs)
{
	// instantiate the parse handler
	CParseHandlerBase *parse_handler_base =
		CParseHandlerFactory::GetParseHandler(m_mp, element_local_name,
											  m_parse_handler_mgr, this);

	GPOS_ASSERT(NULL != parse_handler_base);

	// activate the specialized parse handler
	m_parse_handler_mgr->ReplaceHandler(parse_handler_base,
										m_parse_handler_root);

	// pass the startElement message for the specialized parse handler to process
	parse_handler_base->startElement(element_uri, element_local_name,
									 element_qname, attrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOp::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag.
//		This function should never be called. Instead, the endElement function
//		of the parse handler for the actual physical operator type is called.
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarOp::EndElement(const XMLCh *const,  //= element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const  // element_qname,
)
{
	CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
}


// EOF
