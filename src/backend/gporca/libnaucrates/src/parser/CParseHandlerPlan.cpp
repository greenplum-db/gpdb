//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerPlan.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing physical plans.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPlan.h"

#include "naucrates/dxl/operators/CDXLDirectDispatchInfo.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerDirectDispatchInfo.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPlan::CParseHandlerPlan
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerPlan::CParseHandlerPlan(CMemoryPool *mp,
									 CParseHandlerManager *parse_handler_mgr,
									 CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_plan_id(0),
	  m_plan_space_size(0),
	  m_dxl_node(NULL),
	  m_direct_dispatch_info(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPlan::~CParseHandlerPlan
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerPlan::~CParseHandlerPlan()
{
	CRefCount::SafeRelease(m_dxl_node);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPlan::CreateDXLNode
//
//	@doc:
//		Root of constructed DXL plan
//
//---------------------------------------------------------------------------
CDXLNode *
CParseHandlerPlan::CreateDXLNode()
{
	return m_dxl_node;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPlan::GetParseHandlerType
//
//	@doc:
//		Parse handler type
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerPlan::GetParseHandlerType() const
{
	return EdxlphPlan;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPlan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPlan::StartElement(const XMLCh *const element_uri,
								const XMLCh *const element_local_name,
								const XMLCh *const element_qname,
								const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenDirectDispatchInfo),
				 element_local_name))
	{
		GPOS_ASSERT(0 < this->Length());
		CParseHandlerBase *direct_dispatch_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenDirectDispatchInfo),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(
			direct_dispatch_parse_handler);

		// store parse handler
		this->Append(direct_dispatch_parse_handler);
		direct_dispatch_parse_handler->startElement(
			element_uri, element_local_name, element_qname, attrs);
		return;
	}

	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPlan),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse plan id
	const XMLCh *xml_str_plan_id = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenPlanId, EdxltokenPlan);
	m_plan_id = CDXLOperatorFactory::ConvertAttrValueToUllong(
		m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_plan_id,
		EdxltokenPlanId, EdxltokenPlan);

	// parse plan space size
	const XMLCh *xmlszPlanSpaceSize = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenPlanSpaceSize, EdxltokenPlan);
	m_plan_space_size = CDXLOperatorFactory::ConvertAttrValueToUllong(
		m_parse_handler_mgr->GetDXLMemoryManager(), xmlszPlanSpaceSize,
		EdxltokenPlanSpaceSize, EdxltokenPlan);

	// create a parse handler for physical nodes and activate it
	GPOS_ASSERT(NULL != m_mp);
	CParseHandlerBase *base_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(base_parse_handler);

	// store parse handler
	this->Append(base_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPlan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPlan::EndElement(const XMLCh *const,  // element_uri,
							  const XMLCh *const element_local_name,
							  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPlan),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerPhysicalOp *operator_parse_handler =
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[0]);

	GPOS_ASSERT(NULL != operator_parse_handler->CreateDXLNode());

	// store constructed child
	m_dxl_node = operator_parse_handler->CreateDXLNode();
	m_dxl_node->AddRef();

	if (2 == this->Length())
	{
		CParseHandlerDirectDispatchInfo *direct_dispatch_info_parse_handler =
			dynamic_cast<CParseHandlerDirectDispatchInfo *>((*this)[1]);
		CDXLDirectDispatchInfo *dxl_direct_dispatch_info =
			direct_dispatch_info_parse_handler->GetDXLDirectDispatchInfo();
		GPOS_ASSERT(NULL != dxl_direct_dispatch_info);

		dxl_direct_dispatch_info->AddRef();
		m_dxl_node->SetDirectDispatchInfo(dxl_direct_dispatch_info);
	}
	// deactivate handler

	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
