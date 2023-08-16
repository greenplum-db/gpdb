//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CParseHandlerDynamicIndexOnlyScan.h
//
//	@doc:
//		SAX parse handler class for parsing dynamic index only scan operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerDynamicIndexOnlyScan_H
#define GPDXL_CParseHandlerDynamicIndexOnlyScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalIndexOnlyScan.h"
#include "naucrates/dxl/parser/CParseHandlerIndexOnlyScan.h"
#include "naucrates/dxl/xml/dxltokens.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerDynamicIndexOnlyScan
//
//	@doc:
//		Parse handler for index scan operator nodes
//
//---------------------------------------------------------------------------
class CParseHandlerDynamicIndexOnlyScan : public CParseHandlerIndexOnlyScan
{
private:
	ULongPtrArray *m_selector_ids;

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
	CParseHandlerDynamicIndexOnlyScan(
		const CParseHandlerDynamicIndexOnlyScan &) = delete;

	// ctor
	CParseHandlerDynamicIndexOnlyScan(CMemoryPool *mp,
									  CParseHandlerManager *parse_handler_mgr,
									  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerDynamicIndexOnlyScan_H

// EOF
