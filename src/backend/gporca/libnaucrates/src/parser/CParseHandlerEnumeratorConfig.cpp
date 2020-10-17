//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerEnumeratorConfig.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing enumerator
//		configuratiom params
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerEnumeratorConfig.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/xml/dxltokens.h"

#include "gpopt/engine/CEnumeratorConfig.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerEnumeratorConfig::CParseHandlerEnumeratorConfig
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerEnumeratorConfig::CParseHandlerEnumeratorConfig(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_enumerator_cfg(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerEnumeratorConfig::~CParseHandlerEnumeratorConfig
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerEnumeratorConfig::~CParseHandlerEnumeratorConfig()
{
	CRefCount::SafeRelease(m_enumerator_cfg);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerEnumeratorConfig::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerEnumeratorConfig::StartElement(
	const XMLCh *const,	 //element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 //element_qname,
	const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenEnumeratorConfig),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse enumerator config options
	ULLONG plan_id = CDXLOperatorFactory::ExtractConvertAttrValueToUllong(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenPlanId,
		EdxltokenOptimizerConfig);
	ULLONG num_of_plan_samples =
		CDXLOperatorFactory::ExtractConvertAttrValueToUllong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenPlanSamples, EdxltokenOptimizerConfig);
	CDouble cost_threshold =
		CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenCostThreshold, EdxltokenOptimizerConfig);

	m_enumerator_cfg = GPOS_NEW(m_mp)
		CEnumeratorConfig(m_mp, plan_id, num_of_plan_samples, cost_threshold);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerEnumeratorConfig::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerEnumeratorConfig::EndElement(const XMLCh *const,  // element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenEnumeratorConfig),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_enumerator_cfg);
	GPOS_ASSERT(0 == this->Length());

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerEnumeratorConfig::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerEnumeratorConfig::GetParseHandlerType() const
{
	return EdxlphEnumeratorConfig;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerEnumeratorConfig::GetEnumeratorCfg
//
//	@doc:
//		Returns the enumerator configuration
//
//---------------------------------------------------------------------------
CEnumeratorConfig *
CParseHandlerEnumeratorConfig::GetEnumeratorCfg() const
{
	return m_enumerator_cfg;
}

// EOF
