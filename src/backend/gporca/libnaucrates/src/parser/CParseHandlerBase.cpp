//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerBase.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing the list of
//		column descriptors in a table descriptor node.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"


using namespace gpdxl;
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerBase::CParseHandlerBase
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerBase::CParseHandlerBase(CMemoryPool *mp,
									 CParseHandlerManager *parse_handler_mgr,
									 CParseHandlerBase *parse_handler_root)
	: m_mp(mp),
	  m_parse_handler_mgr(parse_handler_mgr),
	  m_parse_handler_root(parse_handler_root)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != parse_handler_mgr);

	m_parse_handler_base_array = GPOS_NEW(m_mp) CParseHandlerBaseArray(m_mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerBase::~CParseHandlerBase
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------

CParseHandlerBase::~CParseHandlerBase()
{
	m_parse_handler_base_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerBase::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler. Currently we overload this method to
//		return a specific type for the plan, query, metadata and traceflags parse handlers.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerBase::GetParseHandlerType() const
{
	return EdxlphOther;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerBase::ReplaceParseHandler
//
//	@doc:
//		Replaces a parse handler in the parse handler array with a new one
//
//---------------------------------------------------------------------------
void
CParseHandlerBase::ReplaceParseHandler(
	CParseHandlerBase *parse_handler_base_old,
	CParseHandlerBase *parse_handler_base_new)
{
	ULONG idx = 0;

	GPOS_ASSERT(NULL != m_parse_handler_base_array);

	for (idx = 0; idx < m_parse_handler_base_array->Size(); idx++)
	{
		if ((*m_parse_handler_base_array)[idx] == parse_handler_base_old)
		{
			break;
		}
	}

	// assert old parse handler was found in array
	GPOS_ASSERT(idx < m_parse_handler_base_array->Size());

	m_parse_handler_base_array->Replace(idx, parse_handler_base_new);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerBase::startElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerBase::startElement(const XMLCh *const element_uri,
								const XMLCh *const element_local_name,
								const XMLCh *const element_qname,
								const Attributes &attrs)
{
	StartElement(element_uri, element_local_name, element_qname, attrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerBase::endElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerBase::endElement(const XMLCh *const element_uri,
							  const XMLCh *const element_local_name,
							  const XMLCh *const element_qname)
{
	EndElement(element_uri, element_local_name, element_qname);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerBase::ProcessError
//
//	@doc:
//		Invoked by Xerces to process an ProcessError
//
//---------------------------------------------------------------------------
void
CParseHandlerBase::error(const SAXParseException &to_catch)
{
	CHAR *message = XMLString::transcode(
		to_catch.getMessage(), m_parse_handler_mgr->GetDXLMemoryManager());
	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLValidationError, message);
}

// EOF
