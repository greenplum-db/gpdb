//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerWindowOids.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing window oids
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerWindowOids.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;
using namespace gpopt;

XERCES_CPP_NAMESPACE_USE

CParseHandlerWindowOids::CParseHandlerWindowOids(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_window_oids(NULL)
{
}

CParseHandlerWindowOids::~CParseHandlerWindowOids()
{
	CRefCount::SafeRelease(m_window_oids);
}

void
CParseHandlerWindowOids::StartElement(const XMLCh *const,  //element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const,  //element_qname,
									  const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowOids),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse window function oids
	OID row_number_oid = CDXLOperatorFactory::ExtractConvertAttrValueToOid(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenOidRowNumber, EdxltokenWindowOids);
	OID rank_oid = CDXLOperatorFactory::ExtractConvertAttrValueToOid(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenOidRank,
		EdxltokenWindowOids);

	m_window_oids = GPOS_NEW(m_mp) CWindowOids(row_number_oid, rank_oid);
}

// invoked by Xerces to process a closing tag
void
CParseHandlerWindowOids::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenWindowOids),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_window_oids);
	GPOS_ASSERT(0 == this->Length());

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// return the type of the parse handler.
EDxlParseHandlerType
CParseHandlerWindowOids::GetParseHandlerType() const
{
	return EdxlphWindowOids;
}

CWindowOids *
CParseHandlerWindowOids::GetWindowOids() const
{
	return m_window_oids;
}

// EOF
