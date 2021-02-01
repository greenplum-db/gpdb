//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerSort.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing sort operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerSort.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerScalarLimitCount.h"
#include "naucrates/dxl/parser/CParseHandlerScalarLimitOffset.h"
#include "naucrates/dxl/parser/CParseHandlerSortColList.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSort::CParseHandlerSort
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerSort::CParseHandlerSort(CMemoryPool *mp,
									 CParseHandlerManager *parse_handler_mgr,
									 CParseHandlerBase *parse_handler_root)
	: CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_op(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSort::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSort::StartElement(const XMLCh *const,	 // element_uri,
								const XMLCh *const element_local_name,
								const XMLCh *const,	 // element_qname
								const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalSort),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse and create Sort operator
	m_dxl_op = (CDXLPhysicalSort *) CDXLOperatorFactory::MakeDXLSort(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

	// create and activate the parse handler for the children nodes in reverse
	// order of their expected appearance

	// parse handler for the child
	CParseHandlerBase *child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	// create parse handlers for the limit count and offset expressions
	CParseHandlerBase *offset_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarLimitOffset),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(offset_parse_handler);

	CParseHandlerBase *count_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarLimitCount),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(count_parse_handler);

	// parse handler for the sorting column list
	CParseHandlerBase *sort_col_list_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSortColList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(sort_col_list_parse_handler);

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
	this->Append(sort_col_list_parse_handler);
	this->Append(count_parse_handler);
	this->Append(offset_parse_handler);
	this->Append(child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerSort::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerSort::EndElement(const XMLCh *const,  // element_uri,
							  const XMLCh *const element_local_name,
							  const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalSort),
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
	CParseHandlerSortColList *sort_col_list_parse_handler =
		dynamic_cast<CParseHandlerSortColList *>((*this)[3]);
	CParseHandlerScalarLimitCount *count_parse_handler =
		dynamic_cast<CParseHandlerScalarLimitCount *>((*this)[4]);
	CParseHandlerScalarLimitOffset *offset_parse_handler =
		dynamic_cast<CParseHandlerScalarLimitOffset *>((*this)[5]);
	CParseHandlerPhysicalOp *child_parse_handler =
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[6]);

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);
	// set statictics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// add children
	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(filter_parse_handler);
	AddChildFromParseHandler(sort_col_list_parse_handler);
	AddChildFromParseHandler(count_parse_handler);
	AddChildFromParseHandler(offset_parse_handler);
	AddChildFromParseHandler(child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif	// GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
