//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarOpList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing scalar
//		operator lists
//	@owner:
//
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarOpList.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/operators/CDXLScalarOpList.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOpList::CParseHandlerScalarOpList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarOpList::CParseHandlerScalarOpList(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_op_list_type(CDXLScalarOpList::EdxloplistSentinel)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOpList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarOpList::StartElement(const XMLCh *const element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const element_qname,
										const Attributes &attrs)
{
	CDXLScalarOpList::EdxlOpListType dxl_op_list_type =
		GetDXLOpListType(element_local_name);
	if (NULL == m_dxl_node &&
		CDXLScalarOpList::EdxloplistSentinel > dxl_op_list_type)
	{
		// create the list
		m_dxl_op_list_type = dxl_op_list_type;
		m_dxl_node = GPOS_NEW(m_mp) CDXLNode(
			m_mp, GPOS_NEW(m_mp) CDXLScalarOpList(m_mp, m_dxl_op_list_type));
	}
	else
	{
		// we must have already initialized the list node
		GPOS_ASSERT(NULL != m_dxl_node);

		// parse scalar child
		CParseHandlerBase *child_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name,
										  element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOpList::Edxloplisttype
//
//	@doc:
//		Return the op list type corresponding to the given operator name
//
//---------------------------------------------------------------------------
CDXLScalarOpList::EdxlOpListType
CParseHandlerScalarOpList::GetDXLOpListType(
	const XMLCh *const element_local_name)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarOpList),
								 element_local_name))
	{
		return CDXLScalarOpList::EdxloplistGeneral;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPartLevelEqFilterList),
				 element_local_name))
	{
		return CDXLScalarOpList::EdxloplistEqFilterList;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPartLevelFilterList),
				 element_local_name))
	{
		return CDXLScalarOpList::EdxloplistFilterList;
	}

	return CDXLScalarOpList::EdxloplistSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarOpList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarOpList::EndElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	CDXLScalarOpList::EdxlOpListType dxl_op_list_type =
		GetDXLOpListType(element_local_name);
	if (m_dxl_op_list_type != dxl_op_list_type)
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// add constructed children from child parse handlers
	const ULONG arity = this->Length();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CParseHandlerScalarOp *child_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		AddChildFromParseHandler(child_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
