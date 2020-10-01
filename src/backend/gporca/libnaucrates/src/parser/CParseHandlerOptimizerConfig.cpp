//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerOptimizerConfig.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing optimizer
//		config params
//---------------------------------------------------------------------------

#include "gpos/common/CBitSet.h"

#include "naucrates/dxl/parser/CParseHandlerOptimizerConfig.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerTraceFlags.h"
#include "naucrates/dxl/parser/CParseHandlerEnumeratorConfig.h"
#include "naucrates/dxl/parser/CParseHandlerStatisticsConfig.h"
#include "naucrates/dxl/parser/CParseHandlerCTEConfig.h"
#include "naucrates/dxl/parser/CParseHandlerCostModel.h"
#include "naucrates/dxl/parser/CParseHandlerHint.h"
#include "naucrates/dxl/parser/CParseHandlerWindowOids.h"


#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/traceflags/traceflags.h"

#include "naucrates/dxl/xml/dxltokens.h"

#include "gpopt/base/CWindowOids.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/cost/ICostModel.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOptimizerConfig::CParseHandlerOptimizerConfig
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerOptimizerConfig::CParseHandlerOptimizerConfig(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_pbs(NULL),
	  m_optimizer_config(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOptimizerConfig::~CParseHandlerOptimizerConfig
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerOptimizerConfig::~CParseHandlerOptimizerConfig()
{
	CRefCount::SafeRelease(m_pbs);
	CRefCount::SafeRelease(m_optimizer_config);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOptimizerConfig::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerOptimizerConfig::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenHint),
									  element_local_name))
	{
		// install a parse handler for the hint config
		CParseHandlerBase *pphHint = CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenHint), m_parse_handler_mgr,
			this);
		m_parse_handler_mgr->ActivateParseHandler(pphHint);
		pphHint->startElement(element_uri, element_local_name, element_qname,
							  attrs);
		this->Append(pphHint);
		return;
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenCostModelConfig),
					  element_local_name))
	{
		// install a parse handler for the cost model config
		CParseHandlerBase *pphCostModel = CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenCostModelConfig),
			m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(pphCostModel);
		pphCostModel->startElement(element_uri, element_local_name,
								   element_qname, attrs);
		this->Append(pphCostModel);
		return;
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenTraceFlags),
					  element_local_name))
	{
		// install a parse handler for the trace flags
		CParseHandlerBase *pphTraceFlags =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenTraceFlags),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(pphTraceFlags);
		pphTraceFlags->startElement(element_uri, element_local_name,
									element_qname, attrs);
		this->Append(pphTraceFlags);
		return;
	}
	else if (0 != XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenOptimizerConfig),
					  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerBase *pphWindowOids = CParseHandlerFactory::GetParseHandler(
		m_mp, CDXLTokens::XmlstrToken(EdxltokenWindowOids), m_parse_handler_mgr,
		this);
	m_parse_handler_mgr->ActivateParseHandler(pphWindowOids);

	// install a parse handler for the CTE configuration
	CParseHandlerBase *pphCTEConfig = CParseHandlerFactory::GetParseHandler(
		m_mp, CDXLTokens::XmlstrToken(EdxltokenCTEConfig), m_parse_handler_mgr,
		this);
	m_parse_handler_mgr->ActivateParseHandler(pphCTEConfig);

	// install a parse handler for the statistics configuration
	CParseHandlerBase *pphStatisticsConfig =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenStatisticsConfig),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(pphStatisticsConfig);

	// install a parse handler for the enumerator configuration
	CParseHandlerBase *pphEnumeratorConfig =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenEnumeratorConfig),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(pphEnumeratorConfig);

	// store parse handlers
	this->Append(pphEnumeratorConfig);
	this->Append(pphStatisticsConfig);
	this->Append(pphCTEConfig);
	this->Append(pphWindowOids);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOptimizerConfig::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerOptimizerConfig::EndElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const	 // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenOptimizerConfig),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(NULL == m_optimizer_config);
	GPOS_ASSERT(7 >= this->Length());

	CParseHandlerEnumeratorConfig *pphEnumeratorConfig =
		dynamic_cast<CParseHandlerEnumeratorConfig *>((*this)[0]);
	CEnumeratorConfig *pec = pphEnumeratorConfig->GetEnumeratorCfg();
	pec->AddRef();

	CParseHandlerStatisticsConfig *pphStatisticsConfig =
		dynamic_cast<CParseHandlerStatisticsConfig *>((*this)[1]);
	CStatisticsConfig *stats_config = pphStatisticsConfig->GetStatsConf();
	stats_config->AddRef();

	CParseHandlerCTEConfig *pphCTEConfig =
		dynamic_cast<CParseHandlerCTEConfig *>((*this)[2]);
	CCTEConfig *pcteconfig = pphCTEConfig->GetCteConf();
	pcteconfig->AddRef();

	CParseHandlerWindowOids *pphDefoidsGPDB =
		dynamic_cast<CParseHandlerWindowOids *>((*this)[3]);
	CWindowOids *pwindowoidsGPDB = pphDefoidsGPDB->GetWindowOids();
	GPOS_ASSERT(NULL != pwindowoidsGPDB);
	pwindowoidsGPDB->AddRef();

	ICostModel *pcm = NULL;
	CHint *phint = NULL;
	if (5 == this->Length())
	{
		// no cost model: use default one
		pcm = ICostModel::PcmDefault(m_mp);
		phint = CHint::PhintDefault(m_mp);
	}
	else
	{
		CParseHandlerCostModel *pphCostModelConfig =
			dynamic_cast<CParseHandlerCostModel *>((*this)[4]);
		pcm = pphCostModelConfig->GetCostModel();
		GPOS_ASSERT(NULL != pcm);
		pcm->AddRef();

		if (6 == this->Length())
		{
			phint = CHint::PhintDefault(m_mp);
		}
		else
		{
			CParseHandlerHint *pphHint =
				dynamic_cast<CParseHandlerHint *>((*this)[5]);
			phint = pphHint->GetHint();
			GPOS_ASSERT(NULL != phint);
			phint->AddRef();
		}
	}

	m_optimizer_config = GPOS_NEW(m_mp) COptimizerConfig(
		pec, stats_config, pcteconfig, pcm, phint, pwindowoidsGPDB);

	CParseHandlerTraceFlags *pphTraceFlags =
		dynamic_cast<CParseHandlerTraceFlags *>((*this)[this->Length() - 1]);
	pphTraceFlags->GetTraceFlagBitSet()->AddRef();
	m_pbs = pphTraceFlags->GetTraceFlagBitSet();

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOptimizerConfig::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerOptimizerConfig::GetParseHandlerType() const
{
	return EdxlphOptConfig;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOptimizerConfig::Pbs
//
//	@doc:
//		Returns the bitset for the trace flags
//
//---------------------------------------------------------------------------
CBitSet *
CParseHandlerOptimizerConfig::Pbs() const
{
	return m_pbs;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerOptimizerConfig::Poc
//
//	@doc:
//		Returns the optimizer config
//
//---------------------------------------------------------------------------
COptimizerConfig *
CParseHandlerOptimizerConfig::GetOptimizerConfig() const
{
	return m_optimizer_config;
}

// EOF
