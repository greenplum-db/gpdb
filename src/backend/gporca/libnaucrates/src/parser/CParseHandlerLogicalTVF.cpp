//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalTVF.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing table-valued
//		functions
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalTVF.h"
#include "naucrates/dxl/parser/CParseHandlerColDescr.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLLogicalTVF.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalTVF::CParseHandlerLogicalTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalTVF::CParseHandlerLogicalTVF
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_func_mdid(NULL),
	m_return_type_mdid(NULL),
	m_mdname(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalTVF::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalTVF::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes &attrs
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalTVF), element_local_name))
	{
		// parse function id
		m_func_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenFuncId, EdxltokenLogicalTVF);

		// parse function name
		const XMLCh *func_name = CDXLOperatorFactory::ExtractAttrValue
																(
																attrs,
																EdxltokenName,
																EdxltokenLogicalTVF
																);

		CWStringDynamic *func_name_str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), func_name);
		m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, func_name_str);
		GPOS_DELETE(func_name_str);

		// parse function return type
		m_return_type_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeId, EdxltokenLogicalTVF);

	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenColumns), element_local_name))
	{
		// parse handler for columns
		CParseHandlerBase *cold_descr_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenColumns), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(cold_descr_parse_handler);

		// store parse handlers
		this->Append(cold_descr_parse_handler);

		cold_descr_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
	else
	{
		// parse scalar child
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handlers
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalTVF::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalTVF::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalTVF), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	CParseHandlerColDescr *col_descr_parse_handler = dynamic_cast<CParseHandlerColDescr *>((*this)[0]);

	GPOS_ASSERT(NULL != col_descr_parse_handler);

	// get column descriptors
	CDXLColDescrArray *cold_descr_dxl_array = col_descr_parse_handler->GetDXLColumnDescrArray();
	GPOS_ASSERT(NULL != cold_descr_dxl_array);

	cold_descr_dxl_array->AddRef();
	CDXLLogicalTVF *lg_tvf_op = GPOS_NEW(m_mp) CDXLLogicalTVF(m_mp, m_func_mdid, m_return_type_mdid, m_mdname, cold_descr_dxl_array);

	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, lg_tvf_op);

	const ULONG length = this->Length();
	// loop over arglist children and add them to this parsehandler
	for (ULONG idx = 1; idx < length; idx++)
	{
		CParseHandlerScalarOp *child_parse_handler = dynamic_cast<CParseHandlerScalarOp *>((*this)[idx]);
		AddChildFromParseHandler(child_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
