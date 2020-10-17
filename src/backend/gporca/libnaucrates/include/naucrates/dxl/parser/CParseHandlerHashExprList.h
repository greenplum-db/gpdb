//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerHashExprList.h
//
//	@doc:
//		SAX parse handler class for parsing the list of hash expressions in a
//		redistribute motion node.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerHashExprList_H
#define GPDXL_CParseHandlerHashExprList_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerHashExprList
//
//	@doc:
//		SAX parse handler class for parsing the list of hash expressions in a
//		redistribute motion node.
//
//---------------------------------------------------------------------------
class CParseHandlerHashExprList : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerHashExprList(const CParseHandlerHashExprList &);

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
	CParseHandlerHashExprList(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerHashExprList_H

// EOF
