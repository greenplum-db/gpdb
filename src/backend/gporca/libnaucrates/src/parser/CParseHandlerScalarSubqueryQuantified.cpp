//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSubqueryQuantified.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for ANY and ALL subquery 
//		operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarSubqueryQuantified.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarSubqueryAll.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubqueryQuantified::CParseHandlerScalarSubqueryQuantified
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarSubqueryQuantified::CParseHandlerScalarSubqueryQuantified
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	m_dxl_op(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubqueryQuantified::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSubqueryQuantified::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& attrs
	)
{
	
	GPOS_ASSERT(NULL == m_dxl_op);
		
	// is this a subquery any or subquery all operator
	Edxltoken dxl_token = EdxltokenScalarSubqueryAll;
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSubqueryAny), element_local_name))
	{
		dxl_token = EdxltokenScalarSubqueryAny;
	}
	else if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSubqueryAll), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// parse operator id
	IMDId *mdid_op = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
							(
							m_parse_handler_mgr->GetDXLMemoryManager(),
							attrs,
							EdxltokenOpNo,
							dxl_token
							);

	// parse operator name
	const XMLCh *xmlszScalarOpName = CDXLOperatorFactory::ExtractAttrValue
										(
										attrs,
										EdxltokenOpName,
										dxl_token
										);
	
	CWStringDynamic *op_name_str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), xmlszScalarOpName);
	CMDName *md_op_name = GPOS_NEW(m_mp) CMDName(m_mp, op_name_str);
	GPOS_DELETE(op_name_str);
		
	// parse column id
	ULONG colid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong
										(
										m_parse_handler_mgr->GetDXLMemoryManager(),
										attrs, 
										EdxltokenColId,
										dxl_token
										);
	
	if (EdxltokenScalarSubqueryAny == dxl_token)
	{
		m_dxl_op = GPOS_NEW(m_mp) CDXLScalarSubqueryAny(m_mp, mdid_op, md_op_name, colid);
	}
	else
	{
		m_dxl_op = GPOS_NEW(m_mp) CDXLScalarSubqueryAll(m_mp, mdid_op, md_op_name, colid);
	}
	
	// parse handler for the child nodes
	CParseHandlerBase *parse_handler_logical_child = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(parse_handler_logical_child);

	CParseHandlerBase *parse_handler_scalar_child = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(parse_handler_scalar_child);

	// store child parse handler in array
	this->Append(parse_handler_scalar_child);
	this->Append(parse_handler_logical_child);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubqueryQuantified::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSubqueryQuantified::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSubqueryAll), element_local_name) &&
		0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarSubqueryAny), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// construct node from parsed components
	GPOS_ASSERT(NULL != m_dxl_op);
	GPOS_ASSERT(2 == this->Length());
	
	CParseHandlerScalarOp *parse_handler_scalar_child = dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
	CParseHandlerLogicalOp *parse_handler_logical_child = dynamic_cast<CParseHandlerLogicalOp *>((*this)[1]);
		
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, m_dxl_op);

	// add constructed child
	AddChildFromParseHandler(parse_handler_scalar_child);
	AddChildFromParseHandler(parse_handler_logical_child);

#ifdef GPOS_DEBUG
	m_dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG
	
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF

