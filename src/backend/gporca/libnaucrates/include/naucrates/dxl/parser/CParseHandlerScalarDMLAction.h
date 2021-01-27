//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarDMLAction.h
//
//	@doc:
//		SAX parse handler class for parsing scalar DML action expression
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarDMLAction_H
#define GPDXL_CParseHandlerScalarDMLAction_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarDMLAction
//
//	@doc:
//		Parse handler for parsing a scalar DML action expression
//
//---------------------------------------------------------------------------
class CParseHandlerScalarDMLAction : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarDMLAction(const CParseHandlerScalarDMLAction &);

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
	CParseHandlerScalarDMLAction(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarDMLAction_H

// EOF
