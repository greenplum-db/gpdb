//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerLogicalJoin.h
//
//	@doc:
//		Parse handler for parsing a logical join operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalJoin_H
#define GPDXL_CParseHandlerLogicalJoin_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogicalJoin.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalJoin
//
//	@doc:
//		Parse handler for parsing a logical join operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalJoin : public CParseHandlerLogicalOp
{
private:
	// private copy ctor
	CParseHandlerLogicalJoin(const CParseHandlerLogicalJoin &);

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
	CParseHandlerLogicalJoin(CMemoryPool *mp,
							 CParseHandlerManager *parse_handler_mgr,
							 CParseHandlerBase *parse_handler_root);

	~CParseHandlerLogicalJoin();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalJoin_H

// EOF
