//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerProjList.h
//
//	@doc:
//		SAX parse handler class for parsing the list of projection elements
//		in a DXL physical operator node.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarProjList_H
#define GPDXL_CParseHandlerScalarProjList_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerProjList
//
//	@doc:
//		Parse handler class for parsing the list of projection elements
//		in a DXL physical operator node.
//
//---------------------------------------------------------------------------
class CParseHandlerProjList : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerProjList(const CParseHandlerProjList &);

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
	CParseHandlerProjList(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarProjList_H

// EOF
