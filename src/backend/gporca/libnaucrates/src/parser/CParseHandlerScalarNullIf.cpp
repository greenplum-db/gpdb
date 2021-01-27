//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarNullIf.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for scalar NullIf
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarNullIf.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarNullIf::CParseHandlerScalarNullIf
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarNullIf::CParseHandlerScalarNullIf(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarNullIf::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarNullIf::StartElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const,	 // element_qname
										const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarNullIf),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	IMDId *mdid_op = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenOpNo,
		EdxltokenScalarNullIf);
	IMDId *mdid_return_type =
		CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeId,
			EdxltokenScalarNullIf);

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(
		m_mp, GPOS_NEW(m_mp) CDXLScalarNullIf(m_mp, mdid_op, mdid_return_type));

	// create and activate the parse handler for the children nodes in reverse
	// order of their expected appearance

	// parse handler for right scalar node
	CParseHandlerBase *right_child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr,
			this);
	m_parse_handler_mgr->ActivateParseHandler(right_child_parse_handler);

	// parse handler for left scalar node
	CParseHandlerBase *left_child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr,
			this);
	m_parse_handler_mgr->ActivateParseHandler(left_child_parse_handler);

	// store parse handlers
	this->Append(left_child_parse_handler);
	this->Append(right_child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarNullIf::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarNullIf::EndElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarNullIf),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerScalarOp *left_child_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
	CParseHandlerScalarOp *right_child_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[1]);

	// add constructed children
	AddChildFromParseHandler(left_child_parse_handler);
	AddChildFromParseHandler(right_child_parse_handler);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
