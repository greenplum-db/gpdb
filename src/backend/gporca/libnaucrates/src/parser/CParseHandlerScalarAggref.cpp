//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarAggref.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing scalar AggRef.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarAggref.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarAggref::CParseHandlerScalarAggref
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarAggref::CParseHandlerScalarAggref(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarAggref::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarAggref::StartElement(const XMLCh *const element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const element_qname,
										const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarAggref),
								 element_local_name))
	{
		// parse and create scalar AggRef
		CDXLScalarAggref *dxl_op =
			(CDXLScalarAggref *) CDXLOperatorFactory::MakeDXLAggFunc(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

		// construct node from the created scalar AggRef
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	}
	else
	{
		// we must have seen an aggref already and initialized the aggref node
		GPOS_ASSERT(NULL != m_dxl_node);

		CParseHandlerBase *parse_handler_base =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(parse_handler_base);

		// store parse handlers
		this->Append(parse_handler_base);

		parse_handler_base->startElement(element_uri, element_local_name,
										 element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarAggref::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarAggref::EndElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarAggref),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG size = this->Length();
	for (ULONG idx = 0; idx < size; idx++)
	{
		CParseHandlerScalarOp *scalar_op_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[idx]);
		AddChildFromParseHandler(scalar_op_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
