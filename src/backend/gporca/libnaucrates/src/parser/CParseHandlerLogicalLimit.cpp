//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalLimit.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		limit operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalLimit.h"
#include "naucrates/dxl/parser/CParseHandlerSortColList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/operators/CDXLLogicalLimit.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalLimit::CParseHandlerLogicalLimit
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalLimit::CParseHandlerLogicalLimit
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
//		CParseHandlerLogicalLimit::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalLimit::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& attrs
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalLimit), element_local_name))
	{
		const XMLCh *non_removable_limit_str = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenTopLimitUnderDML));
		BOOL keep_limit = false;
		if (non_removable_limit_str)
		{
			keep_limit = CDXLOperatorFactory::ConvertAttrValueToBool
										(
										m_parse_handler_mgr->GetDXLMemoryManager(),
										non_removable_limit_str,
										EdxltokenTopLimitUnderDML,
										EdxltokenLogicalLimit
										);
		}

		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLLogicalLimit(m_mp, keep_limit));

		// create child node parsers

		// parse handler for logical operator
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		CParseHandlerBase *offset_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarLimitOffset), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(offset_parse_handler);

		CParseHandlerBase *count_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarLimitCount), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(count_parse_handler);

		// parse handler for the sorting column list
		CParseHandlerBase *sort_col_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSortColList), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(sort_col_list_parse_handler);

		// store child parse handler in array
		this->Append(sort_col_list_parse_handler);
		this->Append(count_parse_handler);
		this->Append(offset_parse_handler);
		this->Append(child_parse_handler);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalLimit::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalLimit::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalLimit), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_dxl_node );
	GPOS_ASSERT(4 == this->Length());

	CParseHandlerSortColList *sort_col_list_parse_handler = dynamic_cast<CParseHandlerSortColList*>((*this)[0]);
	CParseHandlerScalarOp *count_parse_handler = dynamic_cast<CParseHandlerScalarOp *>((*this)[1]);
	CParseHandlerScalarOp *offset_parse_handler = dynamic_cast<CParseHandlerScalarOp *>((*this)[2]);
	CParseHandlerLogicalOp *child_parse_handler = dynamic_cast<CParseHandlerLogicalOp*>((*this)[3]);

	GPOS_ASSERT(NULL != child_parse_handler->CreateDXLNode());

	AddChildFromParseHandler(sort_col_list_parse_handler);
	AddChildFromParseHandler(count_parse_handler);
	AddChildFromParseHandler(offset_parse_handler);
	AddChildFromParseHandler(child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}
// EOF
