//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CParseHandlerScalarPartBoundInclusion.h
//
//	@doc:
//		SAX parse handler class for parsing scalar part bound inclusion
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarScalarPartBoundInclusion_H
#define GPDXL_CParseHandlerScalarScalarPartBoundInclusion_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarPartBoundInclusion
//
//	@doc:
//		Parse handler class for parsing scalar part bound inclusion
//
//---------------------------------------------------------------------------
class CParseHandlerScalarPartBoundInclusion : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarPartBoundInclusion(
		const CParseHandlerScalarPartBoundInclusion &);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor
	CParseHandlerScalarPartBoundInclusion(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarScalarPartBoundInclusion_H

// EOF
