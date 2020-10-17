//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerHint.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing hint
//		configuration
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerHint.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

#include "naucrates/dxl/xml/dxltokens.h"

#include "gpopt/engine/CHint.h"

using namespace gpdxl;
using namespace gpopt;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHint::CParseHandlerHint
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerHint::CParseHandlerHint(CMemoryPool *mp,
									 CParseHandlerManager *parse_handler_mgr,
									 CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root), m_hint(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHint::~CParseHandlerHint
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerHint::~CParseHandlerHint()
{
	CRefCount::SafeRelease(m_hint);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHint::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerHint::StartElement(const XMLCh *const,	 //element_uri,
								const XMLCh *const element_local_name,
								const XMLCh *const,	 //element_qname,
								const Attributes &attrs)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenHint),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse hint configuration options
	ULONG min_num_of_parts_to_require_sort_on_insert =
		CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenMinNumOfPartsToRequireSortOnInsert, EdxltokenHint);
	ULONG join_arity_for_associativity_commutativity =
		CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenJoinArityForAssociativityCommutativity, EdxltokenHint,
			true, gpos::int_max);
	ULONG array_expansion_threshold =
		CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenArrayExpansionThreshold, EdxltokenHint, true,
			gpos::int_max);
	ULONG join_order_dp_threshold =
		CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenJoinOrderDPThreshold, EdxltokenHint, true,
			JOIN_ORDER_DP_THRESHOLD);
	ULONG broadcast_threshold =
		CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenBroadcastThreshold, EdxltokenHint, true,
			BROADCAST_THRESHOLD);
	BOOL enforce_constraint_on_dml =
		CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenEnforceConstraintsOnDML, EdxltokenHint, true, true);
	ULONG push_group_by_below_setop_threshold =
		CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenPushGroupByBelowSetopThreshold, EdxltokenHint, true,
			PUSH_GROUP_BY_BELOW_SETOP_THRESHOLD);

	m_hint = GPOS_NEW(m_mp) CHint(
		min_num_of_parts_to_require_sort_on_insert,
		join_arity_for_associativity_commutativity, array_expansion_threshold,
		join_order_dp_threshold, broadcast_threshold, enforce_constraint_on_dml,
		push_group_by_below_setop_threshold);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHint::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerHint::EndElement(const XMLCh *const,  // element_uri,
							  const XMLCh *const element_local_name,
							  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenHint),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_hint);
	GPOS_ASSERT(0 == this->Length());

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHint::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerHint::GetParseHandlerType() const
{
	return EdxlphHint;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerHint::GetHint
//
//	@doc:
//		Returns the hint configuration
//
//---------------------------------------------------------------------------
CHint *
CParseHandlerHint::GetHint() const
{
	return m_hint;
}

// EOF
