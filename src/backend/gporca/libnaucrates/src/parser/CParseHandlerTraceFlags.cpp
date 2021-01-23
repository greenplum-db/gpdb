//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerTraceFlags.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing trace flags
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerTraceFlags.h"

#include "gpos/common/CBitSet.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/traceflags/traceflags.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTraceFlags::CParseHandlerTraceFlags
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerTraceFlags::CParseHandlerTraceFlags(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_trace_flags_bitset(NULL)
{
	m_trace_flags_bitset = GPOS_NEW(mp) CBitSet(mp, EopttraceSentinel);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTraceFlags::~CParseHandlerTraceFlags
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerTraceFlags::~CParseHandlerTraceFlags()
{
	m_trace_flags_bitset->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTraceFlags::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerTraceFlags::StartElement(const XMLCh *const,  //element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const,  //element_qname,
									  const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenTraceFlags),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse and tokenize traceflags
	const XMLCh *xml_str_trace_flags = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenValue, EdxltokenTraceFlags);

	ULongPtrArray *trace_flag_array =
		CDXLOperatorFactory::ExtractIntsToUlongArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_trace_flags,
			EdxltokenDistrColumns, EdxltokenRelation);

	for (ULONG idx = 0; idx < trace_flag_array->Size(); idx++)
	{
		ULONG *trace_flag = (*trace_flag_array)[idx];
		m_trace_flags_bitset->ExchangeSet(*trace_flag);
	}

	trace_flag_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTraceFlags::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerTraceFlags::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenTraceFlags),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTraceFlags::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerTraceFlags::GetParseHandlerType() const
{
	return EdxlphTraceFlags;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerTraceFlags::Pbs
//
//	@doc:
//		Returns the bitset for the trace flags
//
//---------------------------------------------------------------------------
CBitSet *
CParseHandlerTraceFlags::GetTraceFlagBitSet()
{
	return m_trace_flags_bitset;
}
// EOF
