//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarWindowRef.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing scalar WindowRef
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarWindowRef.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarWindowRef::CParseHandlerScalarWindowRef
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarWindowRef::CParseHandlerScalarWindowRef(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarWindowRef::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarWindowRef::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarWindowref),
				 element_local_name))
	{
		// parse and create scalar WindowRef (window function)
		CDXLScalarWindowRef *dxl_op =
			(CDXLScalarWindowRef *) CDXLOperatorFactory::MakeWindowRef(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

		// construct node from the created scalar window ref
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	}
	else
	{
		// we must have seen an window ref already and initialized the window ref node
		GPOS_ASSERT(NULL != m_dxl_node);

		CParseHandlerBase *op_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(op_parse_handler);

		// store parse handlers
		this->Append(op_parse_handler);

		op_parse_handler->startElement(element_uri, element_local_name,
									   element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarWindowRef::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarWindowRef::EndElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const	 // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarWindowref),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	const ULONG arity = this->Length();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CParseHandlerScalarOp *op_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		AddChildFromParseHandler(op_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
