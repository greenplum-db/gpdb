//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerCondList.h
//
//	@doc:
//		SAX parse handler class for parsing the list of conditions in a
//		hash join or merge join node.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerCondList_H
#define GPDXL_CParseHandlerCondList_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerCondList
//
//	@doc:
//		SAX parse handler class for parsing the list of conditions in a
//		hash join or merge join node.
//
//---------------------------------------------------------------------------
class CParseHandlerCondList : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerCondList(const CParseHandlerCondList &);

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
	CParseHandlerCondList(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerCondList_H

// EOF
