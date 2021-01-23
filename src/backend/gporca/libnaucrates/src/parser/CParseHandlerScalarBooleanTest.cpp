//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarBooleanTest.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing scalar BooleanTest.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarBooleanTest.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBooleanTest::CParseHandlerScalarBooleanTest
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarBooleanTest::CParseHandlerScalarBooleanTest(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_boolean_test_type(EdxlbooleantestSentinel)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBooleanTest::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarBooleanTest::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	EdxlBooleanTestType dxl_boolean_test_type =
		CParseHandlerScalarBooleanTest::GetDxlBooleanTestType(
			element_local_name);

	if (EdxlbooleantestSentinel == dxl_boolean_test_type)
	{
		if (NULL == m_dxl_node)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
					   CDXLUtils::CreateDynamicStringFromXMLChArray(
						   m_parse_handler_mgr->GetDXLMemoryManager(),
						   element_local_name)
						   ->GetBuffer());
		}
		else
		{
			CParseHandlerBase *child_parse_handler =
				CParseHandlerFactory::GetParseHandler(
					m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
					m_parse_handler_mgr, this);

			m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

			// store parse handlers
			this->Append(child_parse_handler);

			child_parse_handler->startElement(element_uri, element_local_name,
											  element_qname, attrs);
		}

		return;
	}

	m_dxl_boolean_test_type = dxl_boolean_test_type;
	// parse and create scalar BooleanTest
	CDXLScalarBooleanTest *dxl_op =
		(CDXLScalarBooleanTest *) CDXLOperatorFactory::MakeDXLBooleanTest(
			m_parse_handler_mgr->GetDXLMemoryManager(),
			m_dxl_boolean_test_type);

	// construct node from the created child nodes
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBooleanTest::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarBooleanTest::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	EdxlBooleanTestType dxl_boolean_test_type =
		CParseHandlerScalarBooleanTest::GetDxlBooleanTestType(
			element_local_name);

	if (EdxlbooleantestSentinel == dxl_boolean_test_type)
	{
		GPOS_RAISE(
			gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
			CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name)
				->GetBuffer());
	}

	GPOS_ASSERT(dxl_boolean_test_type == m_dxl_boolean_test_type);
	GPOS_ASSERT(1 == this->Length());

	CParseHandlerScalarOp *pph =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
	AddChildFromParseHandler(pph);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBooleanTest::GetDxlBooleanTestType
//
//	@doc:
//		Parse the boolean test type from the attribute value
//
//---------------------------------------------------------------------------
EdxlBooleanTestType
CParseHandlerScalarBooleanTest::GetDxlBooleanTestType(
	const XMLCh *xmlszBooleanTestType)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBoolTestIsTrue),
				 xmlszBooleanTestType))
	{
		return EdxlbooleantestIsTrue;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBoolTestIsNotTrue),
				 xmlszBooleanTestType))
	{
		return EdxlbooleantestIsNotTrue;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBoolTestIsFalse),
				 xmlszBooleanTestType))
	{
		return EdxlbooleantestIsFalse;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBoolTestIsNotFalse),
				 xmlszBooleanTestType))
	{
		return EdxlbooleantestIsNotFalse;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBoolTestIsUnknown),
				 xmlszBooleanTestType))
	{
		return EdxlbooleantestIsUnknown;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBoolTestIsNotUnknown),
				 xmlszBooleanTestType))
	{
		return EdxlbooleantestIsNotUnknown;
	}

	return EdxlbooleantestSentinel;
}

// EOF
