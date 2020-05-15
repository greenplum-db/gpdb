//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerPhysicalTVF.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing table-valued
//		functions
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalTVF.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLPhysicalTVF.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalTVF::CParseHandlerPhysicalTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerPhysicalTVF::CParseHandlerPhysicalTVF
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_func_mdid(NULL),
	m_return_type_mdid(NULL),
	m_pstr(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalTVF::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalTVF::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes &attrs
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalTVF), element_local_name))
	{
		// parse function id
		m_func_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenFuncId, EdxltokenPhysicalTVF);

		// parse function name
		const XMLCh *xml_str_func_name = CDXLOperatorFactory::ExtractAttrValue
																(
																attrs,
																EdxltokenName,
																EdxltokenPhysicalTVF
																);

		CWStringDynamic *str_func_name = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_func_name);
		m_pstr = GPOS_NEW(m_mp) CWStringConst(m_mp, str_func_name->GetBuffer());
		GPOS_DELETE(str_func_name);

		// parse return type
		m_return_type_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeId, EdxltokenPhysicalTVF);

		// parse handler for the proj list
		CParseHandlerBase *proj_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

		//parse handler for the properties of the operator
		CParseHandlerBase *prop_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

		// store parse handlers
		this->Append(prop_parse_handler);
		this->Append(proj_list_parse_handler);
	}
	else
	{
		// parse scalar child
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalTVF::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalTVF::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalTVF), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	CDXLPhysicalTVF *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalTVF(m_mp, m_func_mdid, m_return_type_mdid, m_pstr);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	CParseHandlerProperties *prop_parse_handler = dynamic_cast<CParseHandlerProperties *>((*this)[0]);

	// set statistics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	CParseHandlerProjList *proj_list_parse_handler = dynamic_cast<CParseHandlerProjList*>((*this)[1]);
	AddChildFromParseHandler(proj_list_parse_handler);

	const ULONG size = this->Length();
	for (ULONG ul = 2; ul < size; ul++)
	{
		CParseHandlerScalarOp *child_parse_handler = dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		AddChildFromParseHandler(child_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF

