//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.
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
#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "gpopt/base/CWindowOids.h"

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

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

public:
	CParseHandlerWindowOids(const CParseHandlerWindowOids &) = delete;

	// ctor
	CParseHandlerWindowOids(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerWindowOids() override;

	// type of the parse handler
	EDxlParseHandlerType GetParseHandlerType() const override;

	// return system specific window oids
	CWindowOids *GetWindowOids() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerWindowOids_H

// EOF
