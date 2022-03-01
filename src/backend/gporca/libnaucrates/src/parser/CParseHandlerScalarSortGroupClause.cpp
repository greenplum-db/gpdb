//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarSortGroupClause.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing value list.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarSortGroupClause.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarSortGroupClause.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

// ctor
CParseHandlerScalarSortGroupClause::CParseHandlerScalarSortGroupClause(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

// invoked by Xerces to process an opening tag
void
CParseHandlerScalarSortGroupClause::StartElement(
	const XMLCh *const,	 // element_uri
	const XMLCh *const element_local_name,
	const XMLCh *const,	 //element_qname,
	const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSortGroupClause),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	INT index = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenIndex,
		EdxltokenScalarSortGroupClause);
	INT eqop = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMDTypeEqOp,
		EdxltokenScalarSortGroupClause);
	INT sortop = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenSortOpId,
		EdxltokenScalarSortGroupClause);
	BOOL nullsfirst = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenSortNullsFirst, EdxltokenScalarSortGroupClause);
	BOOL hashable = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenMDTypeHashable, EdxltokenScalarSortGroupClause);

	CDXLScalarSortGroupClause *dxl_op =
		GPOS_NEW(m_mp) CDXLScalarSortGroupClause(m_mp, index, eqop, sortop,
												 nullsfirst, hashable);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
}

// invoked by Xerces to process a closing tag
void
CParseHandlerScalarSortGroupClause::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSortGroupClause),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}
// EOF
