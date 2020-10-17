//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalSetOp.h
//
//	@doc:
//		Parse handler for parsing a logical set operator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalSetOp_H
#define GPDXL_CParseHandlerLogicalSetOp_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"
#include "naucrates/dxl/operators/CDXLLogicalSetOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalSetOp
//
//	@doc:
//		Parse handler for parsing a logical set operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalSetOp : public CParseHandlerLogicalOp
{
private:
	// set operation type
	EdxlSetOpType m_setop_type;

	// array of input column id arrays
	ULongPtr2dArray *m_input_colids_arrays;

	// do the columns across inputs need to be casted
	BOOL m_cast_across_input_req;

	// private copy ctor
	CParseHandlerLogicalSetOp(const CParseHandlerLogicalSetOp &);

	// return the set operation type
	EdxlSetOpType GetSetOpType(const XMLCh *const element_local_name);

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
	CParseHandlerLogicalSetOp(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerLogicalSetOp();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalSetOp_H

// EOF
