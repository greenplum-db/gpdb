//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarAggref.h
//
//	@doc:
//
//		SAX parse handler class for parsing scalar AggRef.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarAggref_H
#define GPDXL_CParseHandlerScalarAggref_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarAggref.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

enum EAggrefChildIndices
{
	EdxlParseHandlerAggrefIndexArgs = 0,
	EdxlParseHandlerAggrefIndexDirectArgs,
	EdxlParseHandlerAggrefIndexOrder,
	EdxlParseHandlerAggrefIndexDistinct,
	EdxlParseHandlerAggrefIndexSentinel
};

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarAggref
//
//	@doc:
//		Parse handler for parsing a scalar aggexpr expression
//
//---------------------------------------------------------------------------
class CParseHandlerScalarAggref : public CParseHandlerScalarOp
{
private:
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
	CParseHandlerScalarAggref(const CParseHandlerScalarAggref &) = delete;

	// ctor
	CParseHandlerScalarAggref(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};

}  // namespace gpdxl
#endif	// !GPDXL_CParseHandlerScalarAggref_H

//EOF
