//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerHashJoin.h
//
//	@doc:
//		SAX parse handler class for parsing hash join operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerHashJoin_H
#define GPDXL_CParseHandlerHashJoin_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

#include "naucrates/dxl/operators/CDXLPhysicalHashJoin.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerHashJoin
//
//	@doc:
//		Parse handler for hash join operators
//
//---------------------------------------------------------------------------
class CParseHandlerHashJoin : public CParseHandlerPhysicalOp
{
private:
	// the hash join operator
	CDXLPhysicalHashJoin *m_dxl_op;

	// private copy ctor
	CParseHandlerHashJoin(const CParseHandlerHashJoin &);

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
	CParseHandlerHashJoin(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerHashJoin_H

// EOF
