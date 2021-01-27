//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CParseHandlerScalarPartDefault.h
//
//	@doc:
//		SAX parse handler class for parsing scalar part default
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarScalarPartDefault_H
#define GPDXL_CParseHandlerScalarScalarPartDefault_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarPartDefault
//
//	@doc:
//		Parse handler class for parsing scalar part default
//
//---------------------------------------------------------------------------
class CParseHandlerScalarPartDefault : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarPartDefault(const CParseHandlerScalarPartDefault &);

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
	CParseHandlerScalarPartDefault(CMemoryPool *mp,
								   CParseHandlerManager *parse_handler_mgr,
								   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarScalarPartDefault_H

// EOF
