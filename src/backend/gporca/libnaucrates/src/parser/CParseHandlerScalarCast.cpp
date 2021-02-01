//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarCast.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing scalar cast operator.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarCast.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCast::CParseHandlerScalarCast
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarCast::CParseHandlerScalarCast(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCast::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarCast::StartElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const,  // element_qname
									  const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarCast),
								 element_local_name))
	{
		if (NULL != m_dxl_node)
		{
			CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
					   str->GetBuffer());
		}

		// parse and create scalar cast
		CDXLScalarCast *dxl_op =
			(CDXLScalarCast *) CDXLOperatorFactory::MakeDXLCast(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

		// parse handler for child scalar node
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCast::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarCast::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarCast),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(1 == this->Length());


	// add constructed child from child parse handlers
	CParseHandlerScalarOp *child_parse_handler =
		dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
	AddChildFromParseHandler(child_parse_handler);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
