//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStats.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing extended
//		statistics.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerExtStats.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerExtStatsDependencies.h"
#include "naucrates/dxl/parser/CParseHandlerExtStatsNDistinctList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLExtStats.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStats::CParseHandlerExtStats
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerExtStats::CParseHandlerExtStats(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root)
{
}

CParseHandlerExtStats::~CParseHandlerExtStats()
{
	m_mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStats::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStats::StartElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name
										GPOS_UNUSED,
									const XMLCh *const,	 // element_qname,
									const Attributes &attrs GPOS_UNUSED)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenExtendedStats),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse metadata id info
	m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		EdxltokenExtendedStats);

	const XMLCh *parsed_stat_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenExtendedStats);

	CWStringDynamic *stat_name_str =
		CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), parsed_stat_name);

	// create a copy of the string in the CMDName constructor
	m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, stat_name_str);
	GPOS_DELETE(stat_name_str);

	CParseHandlerBase *ndistincts_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMVNDistinctList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(ndistincts_parse_handler);

	this->Append(ndistincts_parse_handler);

	CParseHandlerBase *dependencies_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMVDependencyList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(dependencies_parse_handler);

	this->Append(dependencies_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStats::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStats::EndElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenExtendedStats),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerExtStatsDependencies *dependencies_parse_handler =
		dynamic_cast<CParseHandlerExtStatsDependencies *>((*this)[1]);
	dependencies_parse_handler->GetDependencies()->AddRef();

	CParseHandlerExtStatsNDistinctList *ndistincts_parse_handler =
		dynamic_cast<CParseHandlerExtStatsNDistinctList *>((*this)[0]);
	ndistincts_parse_handler->GetNDistinctList()->AddRef();

	m_mdid->AddRef();
	m_imd_obj = GPOS_NEW(m_mp) CDXLExtStats(
		m_mp, m_mdid, m_mdname, dependencies_parse_handler->GetDependencies(),
		ndistincts_parse_handler->GetNDistinctList());

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
