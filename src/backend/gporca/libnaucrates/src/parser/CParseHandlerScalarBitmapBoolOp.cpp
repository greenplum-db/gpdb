//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CParseHandlerScalarBitmapBoolOp.cpp
//
//	@doc:
//		
//		Implementation of the SAX parse handler class for parsing scalar bitmap
//		bool op
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarBitmapBoolOp.h"

#include "naucrates/dxl/parser/CParseHandlerScalarBitmapBoolOp.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBitmapBoolOp::CParseHandlerScalarBitmapBoolOp
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarBitmapBoolOp::CParseHandlerScalarBitmapBoolOp
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBitmapBoolOp::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarBitmapBoolOp::StartElement
	(
	const XMLCh* const , // element_uri
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& attrs
	)
{
	CDXLScalarBitmapBoolOp::EdxlBitmapBoolOp bitmap_bool_dxlop = CDXLScalarBitmapBoolOp::EdxlbitmapAnd;
	Edxltoken token_type = EdxltokenScalarBitmapAnd;
	
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarBitmapOr), element_local_name))
	{
		bitmap_bool_dxlop = CDXLScalarBitmapBoolOp::EdxlbitmapOr;
		token_type = EdxltokenScalarBitmapOr;
	}
	else if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarBitmapAnd), element_local_name))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name)->GetBuffer());
	}

	IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId( m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeId, token_type);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarBitmapBoolOp(m_mp, mdid, bitmap_bool_dxlop));

	// install parse handlers for children
	CParseHandlerBase *right_child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(right_child_parse_handler);
	
	CParseHandlerBase *left_child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(left_child_parse_handler);

	this->Append(left_child_parse_handler);
	this->Append(right_child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBitmapBoolOp::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarBitmapBoolOp::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarBitmapOr), element_local_name) &&
		0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarBitmapAnd), element_local_name))
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name)->GetBuffer());
	}

	const ULONG size = this->Length();
	GPOS_ASSERT(2 == size);

	// add constructed children from child parse handlers
	for (ULONG idx = 0; idx < size; idx++)
	{
		CParseHandlerOp *op_parse_handler = dynamic_cast<CParseHandlerOp*>((*this)[idx]);
		AddChildFromParseHandler(op_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
