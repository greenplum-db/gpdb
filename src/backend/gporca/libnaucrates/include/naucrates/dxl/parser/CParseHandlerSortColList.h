//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerSortColList.h
//
//	@doc:
//		SAX parse handler class for parsing the list of sorting columns in sort
//		and motion nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerSortColList_H
#define GPDXL_CParseHandlerSortColList_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerSortColList
//
//	@doc:
//		SAX parse handler class for parsing the list of sorting columns
//		in sort and motion nodes.
//
//---------------------------------------------------------------------------
class CParseHandlerSortColList : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerSortColList(const CParseHandlerSortColList &);

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
	CParseHandlerSortColList(CMemoryPool *mp,
							 CParseHandlerManager *parse_handler_mgr,
							 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerSortColList_H

// EOF
