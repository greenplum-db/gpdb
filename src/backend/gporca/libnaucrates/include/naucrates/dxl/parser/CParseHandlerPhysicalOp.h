//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerPhysicalOp.h
//
//	@doc:
//		SAX parse handler class for parsing physical operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerPhysicalOp_H
#define GPDXL_CParseHandlerPhysicalOp_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerPhysicalOp
//
//	@doc:
//		Parse handler for physical operators
//
//
//---------------------------------------------------------------------------
class CParseHandlerPhysicalOp : public CParseHandlerOp
{
private:
	// private copy ctor
	CParseHandlerPhysicalOp(const CParseHandlerPhysicalOp &);


protected:
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

public:
	// ctor/dtor
	CParseHandlerPhysicalOp(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerPhysicalOp();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerPhysicalOp_H

// EOF
