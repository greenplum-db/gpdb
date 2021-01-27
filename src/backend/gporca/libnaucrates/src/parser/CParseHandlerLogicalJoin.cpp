//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerLogicalJoin.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		join operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalJoin.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalJoin::CParseHandlerLogicalJoin
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalJoin::CParseHandlerLogicalJoin(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalJoin::~CParseHandlerLogicalJoin
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalJoin::~CParseHandlerLogicalJoin()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalJoin::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalJoin::StartElement(const XMLCh *const element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const element_qname,
									   const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalJoin),
								 element_local_name))
	{
		if (NULL == m_dxl_node)
		{
			// parse and create logical join operator
			CDXLLogicalJoin *pdxlopJoin =
				(CDXLLogicalJoin *) CDXLOperatorFactory::MakeLogicalJoin(
					m_parse_handler_mgr->GetDXLMemoryManager(), attrs);

			// construct node from the created child nodes
			m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopJoin);
		}
		else
		{
			// This is to support nested join.
			CParseHandlerBase *lg_join_parse_handler =
				CParseHandlerFactory::GetParseHandler(
					m_mp, CDXLTokens::XmlstrToken(EdxltokenLogicalJoin),
					m_parse_handler_mgr, this);
			m_parse_handler_mgr->ActivateParseHandler(lg_join_parse_handler);

			// store parse handlers
			this->Append(lg_join_parse_handler);

			lg_join_parse_handler->startElement(element_uri, element_local_name,
												element_qname, attrs);
		}
	}
	else
	{
		if (NULL == m_dxl_node)
		{
			CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
					   str->GetBuffer());
		}

		// The child can either be a CDXLLogicalOp or CDXLScalar
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_mp, element_local_name,
												  m_parse_handler_mgr, this);

		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handlers
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name,
										  element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalJoin::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalJoin::EndElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const	 // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalJoin),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_dxl_node);
	ULONG num_of_child = this->Length();

	// Joins must atleast have 3 children (2 logical operators and 1 scalar operator)
	GPOS_ASSERT(2 < num_of_child);

	// add constructed children from child parse handlers

	// Add the first n-1 logical operator from the first n-1 child parse handler
	for (ULONG idx = 0; idx < num_of_child; idx++)
	{
		CParseHandlerOp *op_parse_handler =
			dynamic_cast<CParseHandlerOp *>((*this)[idx]);
		GPOS_ASSERT(NULL != op_parse_handler->CreateDXLNode());
		AddChildFromParseHandler(op_parse_handler);
	}

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node,
										   false /* validate_children */);
#endif	// GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}
// EOF
