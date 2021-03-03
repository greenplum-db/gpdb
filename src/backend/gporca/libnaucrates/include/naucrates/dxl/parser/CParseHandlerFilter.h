//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerFilter.h
//
//	@doc:
//		SAX parse handler class for parsing filter operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerFilter_H
#define GPDXL_CParseHandlerFilter_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerFilter
//
//	@doc:
//		Parse handler for filters and join filters in physical DXL operator nodes
//
//---------------------------------------------------------------------------
class CParseHandlerFilter : public CParseHandlerScalarOp
{
private:
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
	CParseHandlerFilter(const CParseHandlerFilter &) = delete;

	// ctor/dtor
	CParseHandlerFilter(CMemoryPool *mp,
						CParseHandlerManager *parse_handler_mgr,
						CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerFilter_H

// EOF
