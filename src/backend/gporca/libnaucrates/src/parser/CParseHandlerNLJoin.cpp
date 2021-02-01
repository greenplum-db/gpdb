//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerNLJoin.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing nested loop join operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerNLJoin.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerNLJIndexParamList.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJoin::CParseHandlerNLJoin
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerNLJoin::CParseHandlerNLJoin(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_op(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJoin::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerNLJoin::StartElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const,  // element_qname
								  const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalNLJoin),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse and create Hash join operator
	m_dxl_op = (CDXLPhysicalNLJoin *) CDXLOperatorFactory::MakeDXLNLJoin(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

	// create and activate the parse handler for the children nodes in reverse
	// order of their expected appearance

	// parse handler for the nest loop params list
	CParseHandlerBase *nest_params_parse_handler = NULL;
	if (m_dxl_op->NestParamsExists())
	{
		nest_params_parse_handler = CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenNLJIndexParamList),
			m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(nest_params_parse_handler);
	}

	// parse handler for right child
	CParseHandlerBase *right_child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(right_child_parse_handler);

	// parse handler for left child
	CParseHandlerBase *left_child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(left_child_parse_handler);

	// parse handler for the join filter
	CParseHandlerBase *join_filter_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarFilter),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(join_filter_parse_handler);

	// parse handler for the filter
	CParseHandlerBase *filter_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarFilter),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(filter_parse_handler);

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

	// store parse handlers
	this->Append(prop_parse_handler);
	this->Append(proj_list_parse_handler);
	this->Append(filter_parse_handler);
	this->Append(join_filter_parse_handler);
	this->Append(left_child_parse_handler);
	this->Append(right_child_parse_handler);
	if (NULL != nest_params_parse_handler)
	{
		this->Append(nest_params_parse_handler);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJoin::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerNLJoin::EndElement(const XMLCh *const,	 // element_uri,
								const XMLCh *const element_local_name,
								const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalNLJoin),
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
	CParseHandlerFilter *filter_parse_handler =
		dynamic_cast<CParseHandlerFilter *>((*this)[2]);
	CParseHandlerFilter *join_filter_parse_handler =
		dynamic_cast<CParseHandlerFilter *>((*this)[3]);
	CParseHandlerPhysicalOp *left_child_parse_handler =
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[4]);
	CParseHandlerPhysicalOp *right_child_parse_handler =
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[5]);

	if (m_dxl_op->NestParamsExists())
	{
		CParseHandlerNLJIndexParamList *nest_params_parse_handler =
			dynamic_cast<CParseHandlerNLJIndexParamList *>(
				(*this)[EdxlParseHandlerNLJIndexNestLoopParams]);
		GPOS_ASSERT(nest_params_parse_handler);
		CDXLColRefArray *nest_params_colrefs =
			nest_params_parse_handler->GetNLParamsColRefs();
		nest_params_colrefs->AddRef();
		m_dxl_op->SetNestLoopParamsColRefs(nest_params_colrefs);
	}
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);
	// set statictics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// add constructed children
	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(filter_parse_handler);
	AddChildFromParseHandler(join_filter_parse_handler);
	AddChildFromParseHandler(left_child_parse_handler);
	AddChildFromParseHandler(right_child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif	// GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
