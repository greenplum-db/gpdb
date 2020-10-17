//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerStackTrace.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for stack traces.
//		This is a pass-through parse handler, since we do not do anything with
//		the stack traces
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerStacktrace.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStacktrace::CParseHandlerStacktrace
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerStacktrace::CParseHandlerStacktrace(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStacktrace::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStacktrace::StartElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const,	 // element_local_name,
	const XMLCh *const,	 // element_qname
	const Attributes &	 // attrs
)
{
	// passthrough
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStacktrace::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStacktrace::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const,	 // element_local_name,
									const XMLCh *const	 // element_qname
)
{
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
