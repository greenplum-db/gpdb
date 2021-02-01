//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerAssert.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing physical
//		assert operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerAssert.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerScalarAssertConstraintList.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerAssert::CParseHandlerAssert
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerAssert::CParseHandlerAssert(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerAssert::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerAssert::StartElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const,  // element_qname,
								  const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalAssert),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CHAR *error_code = CDXLOperatorFactory::ExtractConvertAttrValueToSz(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenErrorCode,
		EdxltokenPhysicalAssert);
	if (NULL == error_code || GPOS_SQLSTATE_LENGTH != clib::Strlen(error_code))
	{
		GPOS_RAISE(
			gpdxl::ExmaDXL, gpdxl::ExmiDXLInvalidAttributeValue,
			CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalAssert)->GetBuffer(),
			CDXLTokens::GetDXLTokenStr(EdxltokenErrorCode)->GetBuffer());
	}

	m_dxl_op = GPOS_NEW(m_mp) CDXLPhysicalAssert(m_mp, error_code);

	// ctor created a copy of the error code
	GPOS_DELETE_ARRAY(error_code);

	// parse handler for child node
	CParseHandlerBase *child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	// parse handler for the predicate
	CParseHandlerBase *assert_pred_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarAssertConstraintList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(assert_pred_parse_handler);

	// parse handler for the proj list
	CParseHandlerBase *proj_list_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

	//parse handler for the properties of the operator
	CParseHandlerBase *prop_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

	this->Append(prop_parse_handler);
	this->Append(proj_list_parse_handler);
	this->Append(assert_pred_parse_handler);
	this->Append(child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerAssert::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerAssert::EndElement(const XMLCh *const,	 // element_uri,
								const XMLCh *const element_local_name,
								const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalAssert),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct node from the created child nodes
	CParseHandlerProperties *prop_parse_handler =
		dynamic_cast<CParseHandlerProperties *>((*this)[0]);
	CParseHandlerProjList *proj_list_parse_handler =
		dynamic_cast<CParseHandlerProjList *>((*this)[1]);
	CParseHandlerScalarAssertConstraintList *assert_pred_parse_handler =
		dynamic_cast<CParseHandlerScalarAssertConstraintList *>((*this)[2]);
	CParseHandlerPhysicalOp *child_parse_handler =
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[3]);

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// add constructed children
	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(assert_pred_parse_handler);
	AddChildFromParseHandler(child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif	// GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
