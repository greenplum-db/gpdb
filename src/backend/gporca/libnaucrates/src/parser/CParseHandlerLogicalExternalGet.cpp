//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CParseHandlerLogicalExternalGet.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical external get
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalExternalGet.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalExternalGet::CParseHandlerLogicalExternalGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalExternalGet::CParseHandlerLogicalExternalGet(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerLogicalGet(mp, parse_handler_mgr, parse_handler_root)
{
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
CParseHandlerLogicalExternalGet::StartElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname
	const Attributes &	 //attrs
)
{
	CParseHandlerLogicalGet::StartElement(element_local_name,
										  EdxltokenLogicalExternalGet);
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
CParseHandlerLogicalExternalGet::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	CParseHandlerLogicalGet::EndElement(element_local_name,
										EdxltokenLogicalExternalGet);
}

// EOF
