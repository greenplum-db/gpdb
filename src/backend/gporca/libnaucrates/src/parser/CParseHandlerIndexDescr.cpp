//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerIndexDescr.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing the index
//		 descriptor portion of an index scan node
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerIndexDescr.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexDescr::CParseHandlerIndexDescr
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerIndexDescr::CParseHandlerIndexDescr(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_index_descr(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexDescr::~CParseHandlerIndexDescr
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerIndexDescr::~CParseHandlerIndexDescr()
{
	CRefCount::SafeRelease(m_dxl_index_descr);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexDescr::MakeDXLIndexDescr
//
//	@doc:
//		Returns the index descriptor constructed by the parse handler
//
//---------------------------------------------------------------------------
CDXLIndexDescr *
CParseHandlerIndexDescr::GetDXLIndexDescr()
{
	return m_dxl_index_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexDescr::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexDescr::StartElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const,  // element_qname
									  const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenIndexDescr),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// generate the index descriptor
	m_dxl_index_descr = CDXLOperatorFactory::MakeDXLIndexDescr(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexDescr::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexDescr::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenIndexDescr),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(0 == this->Length());

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
