//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarCast.h
//
//	@doc:
//
//		SAX parse handler class for parsing relabel node to a CDXLScalarCast operator.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarCast_H
#define GPDXL_CParseHandlerScalarCast_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarCast.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarCast
//
//	@doc:
//		Parse handler for parsing a scalar relabeltype expression
//
//---------------------------------------------------------------------------
class CParseHandlerScalarCast : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarCast(const CParseHandlerScalarCast &);

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
	// ctor/dtor
	CParseHandlerScalarCast(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerScalarCast(){};
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerScalarCast_H

//EOF
