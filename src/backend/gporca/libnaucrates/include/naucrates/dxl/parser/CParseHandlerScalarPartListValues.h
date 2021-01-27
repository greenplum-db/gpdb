//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Inc.
//
//	SAX parse handler class for parsing scalar part list values
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarPartListValues_H
#define GPDXL_CParseHandlerScalarPartListValues_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerScalarPartListValues : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarPartListValues(
		const CParseHandlerScalarPartListValues &);

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
	CParseHandlerScalarPartListValues(CMemoryPool *mp,
									  CParseHandlerManager *parse_handler_mgr,
									  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarPartListValues_H

// EOF
