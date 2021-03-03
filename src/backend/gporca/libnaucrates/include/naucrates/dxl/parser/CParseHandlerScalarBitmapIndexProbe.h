//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
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
	CParseHandlerScalarBitmapIndexProbe(
		const CParseHandlerScalarBitmapIndexProbe &) = delete;

	// ctor
	CParseHandlerScalarBitmapIndexProbe(CMemoryPool *mp,
										CParseHandlerManager *parse_handler_mgr,
										CParseHandlerBase *parse_handler_root);
};	// class CParseHandlerScalarBitmapIndexProbe
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarBitmapIndexProbe_H

// EOF
