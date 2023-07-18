//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsDependencies.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing extended stat
//		dependencies
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerExtStatsDependencies.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerExtStatsDependency.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLColStats.h"
#include "naucrates/md/CMDDependency.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

CParseHandlerExtStatsDependencies::CParseHandlerExtStatsDependencies(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_base)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_base),
	  m_dependencies(nullptr)
{
}

CParseHandlerExtStatsDependencies::~CParseHandlerExtStatsDependencies()
{
	m_dependencies->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsDependencies::GetDependencies
//
//	@doc:
//		The dependencies constructed by the parse handler
//
//---------------------------------------------------------------------------
CMDDependencyArray *
CParseHandlerExtStatsDependencies::GetDependencies() const
{
	return m_dependencies;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsDependencies::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsDependencies::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenMVDependencyList),
				 element_local_name) &&
		0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenMVDependency),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenMVDependency),
					  element_local_name))
	{
		// new bucket
		CParseHandlerBase *parse_handler_base_dependency =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenMVDependency),
				m_parse_handler_mgr, this);
		this->Append(parse_handler_base_dependency);

		m_parse_handler_mgr->ActivateParseHandler(
			parse_handler_base_dependency);
		parse_handler_base_dependency->startElement(
			element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsDependencies::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsDependencies::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenMVDependencyList),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	m_dependencies = GPOS_NEW(m_mp) CMDDependencyArray(m_mp);
	for (ULONG ul = 0; ul < this->Length(); ul++)
	{
		CParseHandlerExtStatsDependency *parse_handler_ext_stats_dep =
			dynamic_cast<CParseHandlerExtStatsDependency *>((*this)[ul]);

		CMDDependency *dep = parse_handler_ext_stats_dep->GetDependencyAt();
		dep->AddRef();

		m_dependencies->Append(dep);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
