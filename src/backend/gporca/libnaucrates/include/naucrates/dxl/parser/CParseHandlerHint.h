//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerHint.h
//
//	@doc:
//		SAX parse handler class for parsing hint configuration
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerHint_H
#define GPDXL_CParseHandlerHint_H

#include "gpos/base.h"

#include "gpopt/engine/CHint.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerHint
//
//	@doc:
//		SAX parse handler class for parsing hint configuration options
//
//---------------------------------------------------------------------------
class CParseHandlerHint : public CParseHandlerBase
{
private:
	// hint configuration
	CHint *m_hint;

	// private copy ctor
	CParseHandlerHint(const CParseHandlerHint &);

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
	CParseHandlerHint(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
					  CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerHint();

	// type of the parse handler
	virtual EDxlParseHandlerType GetParseHandlerType() const;

	// hint configuration
	CHint *GetHint() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerHint_H

// EOF
