//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsNDistinct.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a extended
//		stat ndistinct object
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerExtStatsNDistinct.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLColStats.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

CParseHandlerExtStatsNDistinct::CParseHandlerExtStatsNDistinct(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_base)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_base),
	  m_ndistinct(0.0),
	  m_attnos(nullptr),
	  m_ndistinct_md(nullptr)
{
}

CParseHandlerExtStatsNDistinct::~CParseHandlerExtStatsNDistinct()
{
	m_attnos->Release();
	m_ndistinct_md->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsNDistinct::GetNDistinctAt
//
//	@doc:
//		The ndistinct constructed by the parse handler
//
//---------------------------------------------------------------------------
CMDNDistinct *
CParseHandlerExtStatsNDistinct::GetNDistinctAt() const
{
	return m_ndistinct_md;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsNDistinct::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsNDistinct::StartElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname,
	const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenMVNDistinct),
								 element_local_name))
	{
		// parse degree and relationship values
		m_ndistinct = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenMVNDistinct, EdxltokenMVNDistinct);
		m_attnos = CDXLOperatorFactory::ExtractConvertValuesToIntBitSet(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenAttno,
			EdxltokenMVNDistinct);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsNDistinct::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsNDistinct::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenMVNDistinct),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	m_attnos->AddRef();
	m_ndistinct_md =
		GPOS_NEW(m_mp) CMDNDistinct(m_mp, m_ndistinct.Get(), m_attnos);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
