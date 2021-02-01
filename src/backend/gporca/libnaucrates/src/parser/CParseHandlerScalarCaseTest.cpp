//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarCaseTest.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for a case test
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarCaseTest.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCaseTest::CParseHandlerScalarCaseTest
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarCaseTest::CParseHandlerScalarCaseTest(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid_type(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCaseTest::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarCaseTest::StartElement(const XMLCh *const,  //element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const,  //element_qname,
										  const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarCaseTest),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse type id
	m_mdid_type = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeId,
		EdxltokenScalarCaseTest);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCaseTest::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarCaseTest::EndElement(const XMLCh *const,	 // element_uri
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarCaseTest),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct node
	m_dxl_node = GPOS_NEW(m_mp)
		CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarCaseTest(m_mp, m_mdid_type));

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//EOF
