//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarBoolExpr.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing scalar BoolExpr.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarBoolExpr.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBoolExpr::CParseHandlerScalarBoolExpr
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerScalarBoolExpr::CParseHandlerScalarBoolExpr(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_bool_type(Edxland)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBoolExpr::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarBoolExpr::StartElement(const XMLCh *const element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const element_qname,
										  const Attributes &attrs)
{
	if ((0 == XMLString::compareString(
				  CDXLTokens::XmlstrToken(EdxltokenScalarBoolAnd),
				  element_local_name)) ||
		(0 == XMLString::compareString(
				  CDXLTokens::XmlstrToken(EdxltokenScalarBoolOr),
				  element_local_name)) ||
		(0 == XMLString::compareString(
				  CDXLTokens::XmlstrToken(EdxltokenScalarBoolNot),
				  element_local_name)))
	{
		if (NULL == m_dxl_node)
		{
			if (0 == XMLString::compareString(
						 CDXLTokens::XmlstrToken(EdxltokenScalarBoolNot),
						 element_local_name))
			{
				m_dxl_bool_type = Edxlnot;
			}
			else if (0 == XMLString::compareString(
							  CDXLTokens::XmlstrToken(EdxltokenScalarBoolOr),
							  element_local_name))
			{
				m_dxl_bool_type = Edxlor;
			}

			// parse and create scalar BoolExpr
			CDXLScalarBoolExpr *dxl_op =
				(CDXLScalarBoolExpr *) CDXLOperatorFactory::MakeDXLBoolExpr(
					m_parse_handler_mgr->GetDXLMemoryManager(),
					m_dxl_bool_type);

			// construct node from the created child nodes
			m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
		}
		else
		{
			// This is to support nested BoolExpr. TODO:  - create a separate xml tag for boolean expression
			CParseHandlerBase *bool_expr_parse_handler =
				CParseHandlerFactory::GetParseHandler(
					m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarBoolOr),
					m_parse_handler_mgr, this);
			m_parse_handler_mgr->ActivateParseHandler(bool_expr_parse_handler);

			// store parse handlers
			this->Append(bool_expr_parse_handler);

			bool_expr_parse_handler->startElement(
				element_uri, element_local_name, element_qname, attrs);
		}
	}
	else
	{
		if (NULL == m_dxl_node)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
					   CDXLUtils::CreateDynamicStringFromXMLChArray(
						   m_parse_handler_mgr->GetDXLMemoryManager(),
						   element_local_name)
						   ->GetBuffer());
		}

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
//		CParseHandlerScalarBoolExpr::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarBoolExpr::EndElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	EdxlBoolExprType edxlBoolType =
		CParseHandlerScalarBoolExpr::GetDxlBoolTypeStr(element_local_name);

	if (EdxlBoolExprTypeSentinel == edxlBoolType ||
		m_dxl_bool_type != edxlBoolType)
	{
		GPOS_RAISE(
			gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
			CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name)
				->GetBuffer());
	}

	const ULONG size = this->Length();
	// If the operation is NOT then it only has one child.
	if (((((CDXLScalarBoolExpr *) m_dxl_node->GetOperator())
			  ->GetDxlBoolTypeStr() == Edxlnot) &&
		 (1 != size)) ||
		((((CDXLScalarBoolExpr *) m_dxl_node->GetOperator())
			  ->GetDxlBoolTypeStr() != Edxlnot) &&
		 (2 > size)))
	{
		GPOS_RAISE(
			gpdxl::ExmaDXL, gpdxl::ExmiDXLIncorrectNumberOfChildren,
			CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name)
				->GetBuffer());
	}

	// add constructed children from child parse handlers
	for (ULONG ul = 0; ul < size; ul++)
	{
		CParseHandlerScalarOp *pph =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		AddChildFromParseHandler(pph);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarBoolExpr::GetDxlBoolTypeStr
//
//	@doc:
//		Parse the bool type from the attribute value. Raise exception if it is invalid
//
//---------------------------------------------------------------------------
EdxlBoolExprType
CParseHandlerScalarBoolExpr::GetDxlBoolTypeStr(const XMLCh *xmlszBoolType)
{
	if (0 ==
		XMLString::compareString(
			CDXLTokens::XmlstrToken(EdxltokenScalarBoolNot), xmlszBoolType))
	{
		return Edxlnot;
	}

	if (0 ==
		XMLString::compareString(
			CDXLTokens::XmlstrToken(EdxltokenScalarBoolAnd), xmlszBoolType))
	{
		return Edxland;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarBoolOr), xmlszBoolType))
	{
		return Edxlor;
	}

	return EdxlBoolExprTypeSentinel;
}

// EOF
