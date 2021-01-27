//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerMDGPDBTrigger.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing metadata for
//		GPDB triggers
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDGPDBTrigger.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#define GPMD_TRIGGER_ROW_BIT 0
#define GPMD_TRIGGER_BEFORE_BIT 1
#define GPMD_TRIGGER_INSERT_BIT 2
#define GPMD_TRIGGER_DELETE_BIT 3
#define GPMD_TRIGGER_UPDATE_BIT 4
#define GPMD_TRIGGER_BITMAP_LEN 5

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBTrigger::CParseHandlerMDGPDBTrigger
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerMDGPDBTrigger::CParseHandlerMDGPDBTrigger(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid(NULL),
	  m_mdname(NULL),
	  m_rel_mdid(NULL),
	  m_func_mdid(NULL),
	  m_type(0),
	  m_is_enabled(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBTrigger::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDGPDBTrigger::StartElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const,  // element_qname
										 const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBTrigger),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		EdxltokenGPDBTrigger);

	const XMLCh *xml_str_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenGPDBTrigger);
	CWStringDynamic *str_name = CDXLUtils::CreateDynamicStringFromXMLChArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_name);
	m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, str_name);
	GPOS_DELETE(str_name);
	GPOS_ASSERT(m_mdid->IsValid() && NULL != m_mdname);

	m_rel_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenRelationMdid, EdxltokenGPDBTrigger);
	m_func_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenFuncId,
		EdxltokenGPDBTrigger);

	BOOL gpmd_trigger_properties[GPMD_TRIGGER_BITMAP_LEN];
	gpmd_trigger_properties[GPMD_TRIGGER_ROW_BIT] =
		CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBTriggerRow, EdxltokenGPDBTrigger);
	gpmd_trigger_properties[GPMD_TRIGGER_BEFORE_BIT] =
		CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBTriggerBefore, EdxltokenGPDBTrigger);
	gpmd_trigger_properties[GPMD_TRIGGER_INSERT_BIT] =
		CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBTriggerInsert, EdxltokenGPDBTrigger);
	gpmd_trigger_properties[GPMD_TRIGGER_DELETE_BIT] =
		CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBTriggerDelete, EdxltokenGPDBTrigger);
	gpmd_trigger_properties[GPMD_TRIGGER_UPDATE_BIT] =
		CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBTriggerUpdate, EdxltokenGPDBTrigger);

	for (ULONG idx = 0; idx < GPMD_TRIGGER_BITMAP_LEN; idx++)
	{
		// if the current property flag is true then set the corresponding bit
		if (gpmd_trigger_properties[idx])
		{
			m_type |= (1 << idx);
		}
	}

	m_is_enabled = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBTriggerEnabled, EdxltokenGPDBTrigger);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBTrigger::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDGPDBTrigger::EndElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBTrigger),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct the MD trigger object
	m_imd_obj = GPOS_NEW(m_mp) CMDTriggerGPDB(
		m_mp, m_mdid, m_mdname, m_rel_mdid, m_func_mdid, m_type, m_is_enabled);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
