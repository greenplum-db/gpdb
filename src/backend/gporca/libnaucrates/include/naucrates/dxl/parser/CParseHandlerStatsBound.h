//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerStatsBound.h
//
//	@doc:
//		SAX parse handler class for parsing a bounds of a bucket
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerStatsBound_H
#define GPDXL_CParseHandlerStatsBound_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpopt;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerStatsBound
//
//	@doc:
//		Parse handler for parsing the upper/lower bounds of a bucket
//
//---------------------------------------------------------------------------
class CParseHandlerStatsBound : public CParseHandlerBase
{
private:
	// dxl datum representing the bound
	CDXLDatum *m_dxl_datum;

	// is stats bound closed
	BOOL m_is_stats_bound_closed;

	// private copy ctor
	CParseHandlerStatsBound(const CParseHandlerStatsBound &);

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
	CParseHandlerStatsBound(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerStatsBound();

	// return the dxl datum representing the bound point
	CDXLDatum *
	GetDatumVal() const
	{
		return m_dxl_datum;
	}

	// is stats bound closed
	BOOL
	IsStatsBoundClosed() const
	{
		return m_is_stats_bound_closed;
	}
};
}  // namespace gpdxl

#endif	// GPDXL_CParseHandlerStatsBound_H

// EOF
