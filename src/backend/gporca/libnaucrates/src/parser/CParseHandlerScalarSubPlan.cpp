//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSubPlan.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing scalar SubPlan
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarSubPlan.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarSubPlan.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarSubPlanParamList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarSubPlanTestExpr.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubPlan::CParseHandlerScalarSubPlan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarSubPlan::CParseHandlerScalarSubPlan(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid_first_col(NULL),
	  m_dxl_subplan_type(EdxlSubPlanTypeSentinel)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubPlan::Edxlsubplantype
//
//	@doc:
//		Map character sequence to subplan type
//
//---------------------------------------------------------------------------
EdxlSubPlanType
CParseHandlerScalarSubPlan::GetDXLSubplanType(const XMLCh *xml_subplan_type)
{
	GPOS_ASSERT(NULL != xml_subplan_type);

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanTypeScalar),
				 xml_subplan_type))
	{
		return EdxlSubPlanTypeScalar;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanTypeExists),
				 xml_subplan_type))
	{
		return EdxlSubPlanTypeExists;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanTypeNotExists),
				 xml_subplan_type))
	{
		return EdxlSubPlanTypeNotExists;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanTypeAny),
				 xml_subplan_type))
	{
		return EdxlSubPlanTypeAny;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanTypeAll),
				 xml_subplan_type))
	{
		return EdxlSubPlanTypeAll;
	}

	// turn Xerces exception in optimizer exception
	GPOS_RAISE(
		gpdxl::ExmaDXL, gpdxl::ExmiDXLInvalidAttributeValue,
		CDXLTokens::GetDXLTokenStr(EdxltokenScalarSubPlanType)->GetBuffer(),
		CDXLTokens::GetDXLTokenStr(EdxltokenScalarSubPlan)->GetBuffer());

	return EdxlSubPlanTypeSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubPlan::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSubPlan::StartElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const,  // element_qname,
										 const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlan),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	m_mdid_first_col = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeId,
		EdxltokenScalarSubPlanParam);

	const XMLCh *xmlszSubplanType = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenScalarSubPlanType, EdxltokenScalarSubPlan);
	m_dxl_subplan_type = GetDXLSubplanType(xmlszSubplanType);

	// parse handler for child physical node
	CParseHandlerBase *child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	// parse handler for params
	CParseHandlerBase *pphParamList = CParseHandlerFactory::GetParseHandler(
		m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanParamList),
		m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(pphParamList);

	// parse handler for test expression
	CParseHandlerBase *pphTestExpr = CParseHandlerFactory::GetParseHandler(
		m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanTestExpr),
		m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(pphTestExpr);

	// store parse handlers
	this->Append(pphTestExpr);
	this->Append(pphParamList);
	this->Append(child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubPlan::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSubPlan::EndElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlan),
				 element_local_name) &&
		NULL != m_dxl_node)
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerScalarSubPlanTestExpr *parse_handler_subplan_test_expr =
		dynamic_cast<CParseHandlerScalarSubPlanTestExpr *>((*this)[0]);
	CParseHandlerScalarSubPlanParamList *parse_handler_subplan_param_list =
		dynamic_cast<CParseHandlerScalarSubPlanParamList *>((*this)[1]);
	CParseHandlerPhysicalOp *child_parse_handler =
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[2]);

	CDXLColRefArray *dxl_colref_array =
		parse_handler_subplan_param_list->GetDXLColRefArray();
	dxl_colref_array->AddRef();

	CDXLNode *dxl_subplan_test_expr =
		parse_handler_subplan_test_expr->GetDXLTestExpr();
	if (NULL != dxl_subplan_test_expr)
	{
		dxl_subplan_test_expr->AddRef();
	}
	CDXLScalarSubPlan *dxl_op =
		(CDXLScalarSubPlan *) CDXLOperatorFactory::MakeDXLSubPlan(
			m_parse_handler_mgr->GetDXLMemoryManager(), m_mdid_first_col,
			dxl_colref_array, m_dxl_subplan_type, dxl_subplan_test_expr);

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	// add constructed children
	AddChildFromParseHandler(child_parse_handler);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
