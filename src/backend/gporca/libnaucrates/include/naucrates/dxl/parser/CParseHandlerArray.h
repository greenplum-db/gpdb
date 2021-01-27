//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerArray.h
//
//	@doc:
//		SAX parse handler class for scalar array operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerArray_H
#define GPDXL_CParseHandlerArray_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalAgg.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerArray
//
//	@doc:
//		Parse handler for scalar array operators
//
//---------------------------------------------------------------------------
class CParseHandlerArray : public CParseHandlerScalarOp
{
private:
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

	// private copy ctor
	CParseHandlerArray(const CParseHandlerArray &);

public:
	// ctor
	CParseHandlerArray(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
					   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerArray_H

// EOF
