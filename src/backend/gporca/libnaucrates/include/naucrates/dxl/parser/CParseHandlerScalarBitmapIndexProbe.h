//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CParseHandlerScalarBitmapIndexProbe.h
//
//	@doc:
//		SAX parse handler class for parsing bitmap index probe operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarBitmapIndexProbe_H
#define GPDXL_CParseHandlerScalarBitmapIndexProbe_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarBitmapIndexProbe
//
//	@doc:
//		Parse handler for bitmap index probe operator nodes
//
//---------------------------------------------------------------------------
class CParseHandlerScalarBitmapIndexProbe : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarBitmapIndexProbe(
		const CParseHandlerScalarBitmapIndexProbe &);

	// process the start of an element
	virtual void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	virtual void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor
	CParseHandlerScalarBitmapIndexProbe(CMemoryPool *mp,
										CParseHandlerManager *parse_handler_mgr,
										CParseHandlerBase *parse_handler_root);
};	// class CParseHandlerScalarBitmapIndexProbe
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarBitmapIndexProbe_H

// EOF
