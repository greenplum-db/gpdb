//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerValuesScan.h
//
//	@doc:
//		SAX parse handler class for parsing ValuesScan operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerValuesScan_H
#define GPDXL_CParseHandlerValuesScan_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

#include "naucrates/dxl/operators/CDXLPhysicalValuesScan.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerValuesScan
//
//	@doc:
//		Parse handler for parsing a ValuesScan operator
//
//---------------------------------------------------------------------------
class CParseHandlerValuesScan : public CParseHandlerPhysicalOp
{
private:
	// the ValuesScan operator
	CDXLPhysicalValuesScan *m_dxl_op;

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
	CParseHandlerValuesScan(const CParseHandlerValuesScan &) = delete;

	// ctor
	CParseHandlerValuesScan(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerValuesScan_H

// EOF
