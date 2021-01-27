//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerSequence.h
//
//	@doc:
//		SAX parse handler class for parsing sequence operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerSequence_H
#define GPDXL_CParseHandlerSequence_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

// fwd decl
class CDXLPhysicalDynamicTableScan;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerSequence
//
//	@doc:
//		Parse handler for parsing a sequence operator
//
//---------------------------------------------------------------------------
class CParseHandlerSequence : public CParseHandlerPhysicalOp
{
private:
	// are we already inside a sequence operator
	BOOL m_is_inside_sequence;

	// private copy ctor
	CParseHandlerSequence(const CParseHandlerSequence &);

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
	// ctor
	CParseHandlerSequence(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *pph);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerSequence_H

// EOF
