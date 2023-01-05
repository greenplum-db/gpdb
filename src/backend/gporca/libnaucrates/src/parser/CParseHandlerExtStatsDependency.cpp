//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsDependency.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a extended
//		stat dependency object
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerExtStatsDependency.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLColStats.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

CParseHandlerExtStatsDependency::CParseHandlerExtStatsDependency(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_base)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_base),
	  m_degree(0.0),
	  m_from_attnos(nullptr),
	  m_to_attno(0),
	  m_dependency(nullptr)
{
}

CParseHandlerExtStatsDependency::~CParseHandlerExtStatsDependency()
{
	m_dependency->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsDependency::GetDependencyAt
//
//	@doc:
//		The dependency constructed by the parse handler
//
//---------------------------------------------------------------------------
CMDDependency *
CParseHandlerExtStatsDependency::GetDependencyAt() const
{
	return m_dependency;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsDependency::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsDependency::StartElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname,
	const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenMVDependency),
								 element_local_name))
	{
		// parse degree and relationship values
		m_degree = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenDegree,
			EdxltokenMVDependency);
		m_from_attnos = CDXLOperatorFactory::ExtractConvertValuesToIntArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenFrom,
			EdxltokenMVDependency);
		m_to_attno = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTo,
			EdxltokenMVDependency);
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
//		CParseHandlerExtStatsDependency::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsDependency::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenMVDependency),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	m_dependency = GPOS_NEW(m_mp)
		CMDDependency(m_mp, m_degree.Get(), m_from_attnos, m_to_attno);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
