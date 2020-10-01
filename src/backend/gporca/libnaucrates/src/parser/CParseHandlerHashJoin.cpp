//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerHashJoin.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing hash join operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerHashJoin.h"

#include "naucrates/dxl/parser/CParseHandlerCondList.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashJoin::CParseHandlerHashJoin
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerHashJoin::CParseHandlerHashJoin(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_op(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashJoin::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerHashJoin::StartElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const,	 // element_qname
									const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalHashJoin),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse and create Hash join operator
	m_dxl_op = (CDXLPhysicalHashJoin *) CDXLOperatorFactory::MakeDXLHashJoin(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

	// create and activate the parse handler for the children nodes in reverse
	// order of their expected appearance

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

	// parse handler for the hash clauses
	CParseHandlerBase *hash_clauses_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarHashCondList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(hash_clauses_parse_handler);

	// parse handler for the join filter
	CParseHandlerBase *hashjoin_filter_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarFilter),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(hashjoin_filter_parse_handler);

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
	this->Append(hashjoin_filter_parse_handler);
	this->Append(hash_clauses_parse_handler);
	this->Append(left_child_parse_handler);
	this->Append(right_child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHashJoin::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerHashJoin::EndElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalHashJoin),
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
	CParseHandlerFilter *hashjoin_filter_parse_handler =
		dynamic_cast<CParseHandlerFilter *>((*this)[3]);
	CParseHandlerCondList *hash_clauses_parse_handler =
		dynamic_cast<CParseHandlerCondList *>((*this)[4]);
	CParseHandlerPhysicalOp *left_child_parse_handler =
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[5]);
	CParseHandlerPhysicalOp *right_child_parse_handler =
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[6]);

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);
	// set statictics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// add children
	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(filter_parse_handler);
	AddChildFromParseHandler(hashjoin_filter_parse_handler);
	AddChildFromParseHandler(hash_clauses_parse_handler);
	AddChildFromParseHandler(left_child_parse_handler);
	AddChildFromParseHandler(right_child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif	// GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
