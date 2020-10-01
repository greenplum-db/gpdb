//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerCTEConfig.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing CTE
//		configuration
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerCTEConfig.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

#include "naucrates/dxl/xml/dxltokens.h"

#include "gpopt/engine/CCTEConfig.h"

using namespace gpdxl;
using namespace gpopt;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCTEConfig::CParseHandlerCTEConfig
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerCTEConfig::CParseHandlerCTEConfig(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_cte_conf(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCTEConfig::~CParseHandlerCTEConfig
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerCTEConfig::~CParseHandlerCTEConfig()
{
	CRefCount::SafeRelease(m_cte_conf);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCTEConfig::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCTEConfig::StartElement(const XMLCh *const,  //element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const,  //element_qname,
									 const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCTEConfig),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse CTE configuration options
	ULONG cte_inlining_cut_off =
		CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenCTEInliningCutoff, EdxltokenCTEConfig);

	m_cte_conf = GPOS_NEW(m_mp) CCTEConfig(cte_inlining_cut_off);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCTEConfig::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCTEConfig::EndElement(const XMLCh *const,	// element_uri,
								   const XMLCh *const element_local_name,
								   const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCTEConfig),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_cte_conf);
	GPOS_ASSERT(0 == this->Length());

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCTEConfig::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerCTEConfig::GetParseHandlerType() const
{
	return EdxlphCTEConfig;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCTEConfig::GetCteConf
//
//	@doc:
//		Returns the CTE configuration
//
//---------------------------------------------------------------------------
CCTEConfig *
CParseHandlerCTEConfig::GetCteConf() const
{
	return m_cte_conf;
}

// EOF
