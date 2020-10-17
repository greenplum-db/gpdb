//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalSetOp.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		set operators.
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalSetOp.h"
#include "naucrates/dxl/parser/CParseHandlerColDescr.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalSetOp::CParseHandlerLogicalSetOp
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalSetOp::CParseHandlerLogicalSetOp(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root),
	  m_setop_type(EdxlsetopSentinel),
	  m_input_colids_arrays(NULL),
	  m_cast_across_input_req(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalSetOp::~CParseHandlerLogicalSetOp
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalSetOp::~CParseHandlerLogicalSetOp()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalSetOp::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalSetOp::StartElement(const XMLCh *const element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const element_qname,
										const Attributes &attrs)
{
	if (0 == this->Length())
	{
		m_setop_type = GetSetOpType(element_local_name);

		if (EdxlsetopSentinel == m_setop_type)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
					   CDXLUtils::CreateDynamicStringFromXMLChArray(
						   m_parse_handler_mgr->GetDXLMemoryManager(),
						   element_local_name)
						   ->GetBuffer());
		}

		// parse array of input colid arrays
		const XMLCh *input_colids_array_str =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenInputCols));
		m_input_colids_arrays =
			CDXLOperatorFactory::ExtractConvertUlongTo2DArray(
				m_parse_handler_mgr->GetDXLMemoryManager(),
				input_colids_array_str, EdxltokenInputCols,
				EdxltokenLogicalSetOperation);

		// install column descriptor parsers
		CParseHandlerBase *col_descr_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenColumns),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(col_descr_parse_handler);

		m_cast_across_input_req =
			CDXLOperatorFactory::ExtractConvertAttrValueToBool(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenCastAcrossInputs, EdxltokenLogicalSetOperation);

		// store child parse handler in array
		this->Append(col_descr_parse_handler);
	}
	else
	{
		// already have seen a set operation
		GPOS_ASSERT(EdxlsetopSentinel != m_setop_type);

		// create child node parsers
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name,
										  element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalSetOp::GetSetOpType
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
EdxlSetOpType
CParseHandlerLogicalSetOp::GetSetOpType(const XMLCh *const element_local_name)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalUnion),
								 element_local_name))
	{
		return EdxlsetopUnion;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalUnionAll),
				 element_local_name))
	{
		return EdxlsetopUnionAll;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalIntersect),
				 element_local_name))
	{
		return EdxlsetopIntersect;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalIntersectAll),
				 element_local_name))
	{
		return EdxlsetopIntersectAll;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalDifference),
				 element_local_name))
	{
		return EdxlsetopDifference;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenLogicalDifferenceAll),
				 element_local_name))
	{
		return EdxlsetopDifferenceAll;
	}

	return EdxlsetopSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalSetOp::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalSetOp::EndElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	EdxlSetOpType setop_type = GetSetOpType(element_local_name);

	if (EdxlsetopSentinel == setop_type && m_setop_type != setop_type)
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG length = this->Length();
	GPOS_ASSERT(3 <= length);

	// get the columns descriptors
	CParseHandlerColDescr *col_descr_parse_handler =
		dynamic_cast<CParseHandlerColDescr *>((*this)[0]);
	GPOS_ASSERT(NULL != col_descr_parse_handler->GetDXLColumnDescrArray());
	CDXLColDescrArray *cold_descr_dxl_array =
		col_descr_parse_handler->GetDXLColumnDescrArray();

	cold_descr_dxl_array->AddRef();
	CDXLLogicalSetOp *dxl_op = GPOS_NEW(m_mp)
		CDXLLogicalSetOp(m_mp, setop_type, cold_descr_dxl_array,
						 m_input_colids_arrays, m_cast_across_input_req);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	for (ULONG idx = 1; idx < length; idx++)
	{
		// add constructed logical children from child parse handlers
		CParseHandlerLogicalOp *child_parse_handler =
			dynamic_cast<CParseHandlerLogicalOp *>((*this)[idx]);
		AddChildFromParseHandler(child_parse_handler);
	}

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node,
										   false /* validate_children */);
#endif	// GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}
// EOF
