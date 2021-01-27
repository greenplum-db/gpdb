//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerWindowOids.h
//
//	@doc:
//		SAX parse handler class for parsing window function oids
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerWindowOids_H
#define GPDXL_CParseHandlerWindowOids_H

#include "gpos/base.h"

#include "gpopt/base/CWindowOids.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerWindowOids
//
//	@doc:
//		SAX parse handler class for parsing window function oids
//
//---------------------------------------------------------------------------
class CParseHandlerWindowOids : public CParseHandlerBase
{
private:
	// deafult oids
	CWindowOids *m_window_oids;

	// private copy ctor
	CParseHandlerWindowOids(const CParseHandlerWindowOids &);

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
	CParseHandlerWindowOids(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerWindowOids();

	// type of the parse handler
	virtual EDxlParseHandlerType GetParseHandlerType() const;

	// return system specific window oids
	CWindowOids *GetWindowOids() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerWindowOids_H

// EOF
