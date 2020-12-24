//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarArrayRef.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing arrayref
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarArrayRef.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarArrayRef.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarArrayRef::CParseHandlerScalarArrayRef
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarArrayRef::CParseHandlerScalarArrayRef(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_parse_index_lists(0),
	  m_parsing_ref_expr(false),
	  m_parsing_assign_expr(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarArrayRef::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarArrayRef::StartElement(const XMLCh *const element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const element_qname,
										  const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarArrayRef),
				 element_local_name))
	{
		// initialize the arrayref node
		GPOS_ASSERT(NULL == m_dxl_node);

		// parse types
		IMDId *elem_type_mdid =
			CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenArrayElementType, EdxltokenScalarArrayRef);
		IMDId *array_type_mdid =
			CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenArrayType, EdxltokenScalarArrayRef);
		IMDId *return_type_mdid =
			CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenTypeId, EdxltokenScalarArrayRef);
		INT type_modifier = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeMod,
			EdxltokenScalarArrayRef, true, default_type_modifier);

		m_dxl_node = GPOS_NEW(m_mp)
			CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarArrayRef(
							   m_mp, elem_type_mdid, type_modifier,
							   array_type_mdid, return_type_mdid));
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefIndexList),
					  element_local_name))
	{
		GPOS_ASSERT(NULL != m_dxl_node);
		GPOS_ASSERT(2 > m_parse_index_lists);

		// parse index list
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefIndexList),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);
		m_parse_index_lists++;

		child_parse_handler->startElement(element_uri, element_local_name,
										  element_qname, attrs);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefExpr),
					  element_local_name))
	{
		GPOS_ASSERT(NULL != m_dxl_node);
		GPOS_ASSERT(2 == m_parse_index_lists);
		GPOS_ASSERT(!m_parsing_ref_expr);
		GPOS_ASSERT(!m_parsing_assign_expr);

		m_parsing_ref_expr = true;
	}
	else if (0 ==
			 XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefAssignExpr),
				 element_local_name))
	{
		GPOS_ASSERT(NULL != m_dxl_node);
		GPOS_ASSERT(2 == m_parse_index_lists);
		GPOS_ASSERT(!m_parsing_ref_expr);
		GPOS_ASSERT(!m_parsing_assign_expr);

		m_parsing_assign_expr = true;
	}
	else
	{
		// parse scalar child
		GPOS_ASSERT(m_parsing_ref_expr || m_parsing_assign_expr);

		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name,
										  element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarArrayRef::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarArrayRef::EndElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarArrayRef),
				 element_local_name))
	{
		// add constructed children from child parse handlers
		const ULONG size = this->Length();
		GPOS_ASSERT(3 == size || 4 == size);

		for (ULONG idx = 0; idx < size; idx++)
		{
			CParseHandlerScalarOp *child_parse_handler =
				dynamic_cast<CParseHandlerScalarOp *>((*this)[idx]);
			AddChildFromParseHandler(child_parse_handler);
		}

		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefExpr),
					  element_local_name))
	{
		GPOS_ASSERT(m_parsing_ref_expr);

		m_parsing_ref_expr = false;
	}
	else if (0 ==
			 XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarArrayRefAssignExpr),
				 element_local_name))
	{
		GPOS_ASSERT(m_parsing_assign_expr);

		m_parsing_assign_expr = false;
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

// EOF
