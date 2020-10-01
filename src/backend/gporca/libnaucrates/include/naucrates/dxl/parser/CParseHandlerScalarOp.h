//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarOp.h
//
//	@doc:
//		SAX parse handler class for parsing scalar operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarOp_H
#define GPDXL_CParseHandlerScalarOp_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarOp
//
//	@doc:
//		Parse handler for parsing a scalar operator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarOp : public CParseHandlerOp
{
private:
	// private copy ctor
	CParseHandlerScalarOp(const CParseHandlerScalarOp &);

protected:
	// process notification of the beginning of an element.
	virtual void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process notification of the end of an element.
	virtual void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	CParseHandlerScalarOp(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerScalarOp();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarOp_H

// EOF
