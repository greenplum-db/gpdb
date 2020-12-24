//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerDirectDispatchInfo.h
//
//	@doc:
//		SAX parse handler class for parsing the direct dispatch info
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerDirectDispatchInfo_H
#define GPDXL_CParseHandlerDirectDispatchInfo_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

// fwd decl
class CDXLDirectDispatchInfo;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerDirectDispatchInfo
//
//	@doc:
//		Parse handler for direct dispatch info
//
//---------------------------------------------------------------------------
class CParseHandlerDirectDispatchInfo : public CParseHandlerBase
{
private:
	// current array of datums being parsed
	CDXLDatumArray *m_dxl_datum_array;

	// array of datum combinations
	CDXLDatum2dArray *m_datum_array_combination;

	// direct dispatch spec
	CDXLDirectDispatchInfo *m_direct_dispatch_info;

	// true indicates m_dispatch_identifer_datum_array contains raw
	// gp_segment_id values rather than hashable datums
	BOOL m_dispatch_is_raw;

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

public:
	CParseHandlerDirectDispatchInfo(const CParseHandlerDirectDispatchInfo &) =
		delete;

	// ctor
	CParseHandlerDirectDispatchInfo(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerDirectDispatchInfo() override;

	// accessor to the parsed direct dispatch spec
	CDXLDirectDispatchInfo *GetDXLDirectDispatchInfo() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerDirectDispatchInfo_H

// EOF
