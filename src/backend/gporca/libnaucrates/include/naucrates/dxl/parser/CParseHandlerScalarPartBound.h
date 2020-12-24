//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarPartBound.h
//
//	@doc:
//		SAX parse handler class for parsing scalar part bound
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarScalarPartBound_H
#define GPDXL_CParseHandlerScalarScalarPartBound_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarPartBound
//
//	@doc:
//		Parse handler class for parsing scalar part bound
//
//---------------------------------------------------------------------------
class CParseHandlerScalarPartBound : public CParseHandlerScalarOp
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
	CParseHandlerScalarPartBound(const CParseHandlerScalarPartBound &) = delete;

	// ctor
	CParseHandlerScalarPartBound(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarScalarPartBound_H

// EOF
