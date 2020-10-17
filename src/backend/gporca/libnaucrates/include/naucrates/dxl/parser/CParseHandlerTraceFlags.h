//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerTraceFlags.h
//
//	@doc:
//		SAX parse handler class for parsing a set of traceflags
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerTraceFlags_H
#define GPDXL_CParseHandlerTraceFlags_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

// fwd decl
namespace gpos
{
class CBitSet;
}

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerTraceFlags
//
//	@doc:
//		SAX parse handler class for parsing the list of output segment indices in a
//		redistribute motion node.
//
//---------------------------------------------------------------------------
class CParseHandlerTraceFlags : public CParseHandlerBase
{
private:
	// trace flag bitset
	CBitSet *m_trace_flags_bitset;

	// private copy ctor
	CParseHandlerTraceFlags(const CParseHandlerTraceFlags &);

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
	CParseHandlerTraceFlags(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerTraceFlags();

	// type of the parse handler
	EDxlParseHandlerType GetParseHandlerType() const;

	// accessor
	CBitSet *GetTraceFlagBitSet();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerTraceFlags_H

// EOF
