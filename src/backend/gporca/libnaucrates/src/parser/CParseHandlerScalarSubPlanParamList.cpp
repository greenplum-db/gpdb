//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSubPlanParamList.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing the ParamList
//		of a scalar SubPlan
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarSubPlanParamList.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarSubPlanParam.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubPlanParamList::CParseHandlerScalarSubPlanParamList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarSubPlanParamList::CParseHandlerScalarSubPlanParamList(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root)
{
	m_dxl_colref_array = GPOS_NEW(mp) CDXLColRefArray(mp);
	m_has_param_list = false;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubPlanParamList::~CParseHandlerScalarSubPlanParamList
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerScalarSubPlanParamList::~CParseHandlerScalarSubPlanParamList()
{
	m_dxl_colref_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarSubPlanParamList::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSubPlanParamList::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanParamList),
				 element_local_name))
	{
		// we can't have seen a paramlist already
		GPOS_ASSERT(!m_has_param_list);
		// start the paramlist
		m_has_param_list = true;
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanParam),
					  element_local_name))
	{
		// we must have seen a paramlist already
		GPOS_ASSERT(m_has_param_list);

		// start new param
		CParseHandlerBase *parse_handler_subplan_param =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanParam),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(parse_handler_subplan_param);

		// store parse handler
		this->Append(parse_handler_subplan_param);

		parse_handler_subplan_param->startElement(
			element_uri, element_local_name, element_qname, attrs);
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
//		CParseHandlerScalarSubPlanParamList::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarSubPlanParamList::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenScalarSubPlanParamList),
				 element_local_name) &&
		NULL != m_dxl_node)
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const ULONG arity = this->Length();
	// add constructed children from child parse handlers
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CParseHandlerScalarSubPlanParam *parse_handler_subplan_param =
			dynamic_cast<CParseHandlerScalarSubPlanParam *>((*this)[ul]);

		CDXLColRef *dxl_colref = parse_handler_subplan_param->MakeDXLColRef();
		dxl_colref->AddRef();
		m_dxl_colref_array->Append(dxl_colref);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
