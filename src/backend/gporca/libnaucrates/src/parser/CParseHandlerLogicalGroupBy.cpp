//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalGroupBy.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		GroupBy operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalGroupBy.h"

#include "naucrates/dxl/parser/CParseHandlerGroupingColList.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

#include "naucrates/dxl/operators/CDXLLogicalGroupBy.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalGroupBy::CParseHandlerLogicalGroupBy
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalGroupBy::CParseHandlerLogicalGroupBy
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalGroupBy::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalGroupBy::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& //attrs
	)
{
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalGrpBy), element_local_name))
	{
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLLogicalGroupBy(m_mp));

		// create child node parsers

		// parse handler for logical operator
		CParseHandlerBase *lg_op_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(lg_op_parse_handler);

		// parse handler for the proj list
		CParseHandlerBase *proj_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

		//parse handler for the grouping columns list
		CParseHandlerBase *grouping_col_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarGroupingColList), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(grouping_col_list_parse_handler);

		// store child parse handler in array
		this->Append(grouping_col_list_parse_handler);
		this->Append(proj_list_parse_handler);
		this->Append(lg_op_parse_handler);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalGroupBy::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalGroupBy::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalGrpBy), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_dxl_node );
	GPOS_ASSERT(3 == this->Length());

	CParseHandlerGroupingColList *grouping_col_parse_handler = dynamic_cast<CParseHandlerGroupingColList*>((*this)[0]);
	CParseHandlerProjList *proj_list_parse_handler = dynamic_cast<CParseHandlerProjList*>((*this)[1]);
	CParseHandlerLogicalOp *lg_op_parse_handler = dynamic_cast<CParseHandlerLogicalOp*>((*this)[2]);

	GPOS_ASSERT(NULL != proj_list_parse_handler->CreateDXLNode());
	GPOS_ASSERT(NULL != lg_op_parse_handler->CreateDXLNode());

	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(lg_op_parse_handler);

	CDXLLogicalGroupBy *lg_group_by_dxl = static_cast<CDXLLogicalGroupBy*>(m_dxl_node->GetOperator());

	// set grouping cols list
	GPOS_ASSERT(NULL != grouping_col_parse_handler->GetGroupingColidArray());

	ULongPtrArray *grouping_col_array = grouping_col_parse_handler->GetGroupingColidArray();
	grouping_col_array->AddRef();
	lg_group_by_dxl->SetGroupingColumns(grouping_col_array);


#ifdef GPOS_DEBUG
	lg_group_by_dxl->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}
// EOF
