//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsNDistinctList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing extended stat
//		ndinstinct
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerExtStatsNDistinctList.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerExtStatsNDistinct.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLColStats.h"
#include "naucrates/md/CMDNDistinct.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

CParseHandlerExtStatsNDistinctList::CParseHandlerExtStatsNDistinctList(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_base)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_base),
	  m_ndistincts(nullptr)
{
}

CParseHandlerExtStatsNDistinctList::~CParseHandlerExtStatsNDistinctList()
{
	m_ndistincts->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsNDistinctList::GetNDistinctList
//
//	@doc:
//		The ndinstincts constructed by the parse handler
//
//---------------------------------------------------------------------------
CMDNDistinctArray *
CParseHandlerExtStatsNDistinctList::GetNDistinctList() const
{
	return m_ndistincts;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsNDistinctList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsNDistinctList::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenMVNDistinctList),
				 element_local_name) &&
		0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenMVNDistinct),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenMVNDistinct),
					  element_local_name))
	{
		// new ndistinct
		CParseHandlerBase *parse_handler_base_ndistinct =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenMVNDistinct),
				m_parse_handler_mgr, this);
		this->Append(parse_handler_base_ndistinct);

		m_parse_handler_mgr->ActivateParseHandler(parse_handler_base_ndistinct);
		parse_handler_base_ndistinct->startElement(
			element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsNDistinctList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsNDistinctList::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenMVNDistinctList),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	m_ndistincts = GPOS_NEW(m_mp) CMDNDistinctArray(m_mp);
	for (ULONG ul = 0; ul < this->Length(); ul++)
	{
		CParseHandlerExtStatsNDistinct *parse_handler_ext_stats_dep =
			dynamic_cast<CParseHandlerExtStatsNDistinct *>((*this)[ul]);

		CMDNDistinct *dep = parse_handler_ext_stats_dep->GetNDistinctAt();
		dep->AddRef();

		m_ndistincts->Append(dep);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
