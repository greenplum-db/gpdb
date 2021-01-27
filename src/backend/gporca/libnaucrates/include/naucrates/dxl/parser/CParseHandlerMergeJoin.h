//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMergeJoin.h
//
//	@doc:
//		SAX parse handler class for parsing merge join operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMergeJoin_H
#define GPDXL_CParseHandlerMergeJoin_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalMergeJoin.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMergeJoin
//
//	@doc:
//		Parse handler for merge join operators
//
//---------------------------------------------------------------------------
class CParseHandlerMergeJoin : public CParseHandlerPhysicalOp
{
private:
	// the merge join operator
	CDXLPhysicalMergeJoin *m_dxl_op;

	// private copy ctor
	CParseHandlerMergeJoin(const CParseHandlerMergeJoin &);

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
	CParseHandlerMergeJoin(CMemoryPool *mp,
						   CParseHandlerManager *parse_handler_mgr,
						   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMergeJoin_H

// EOF
