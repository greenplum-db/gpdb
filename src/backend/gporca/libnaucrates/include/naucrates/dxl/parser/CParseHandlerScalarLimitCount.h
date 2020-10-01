//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarLimitCount.h
//
//	@doc:
//		SAX parse handler class for parsing LimitCount
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerScalarLimitCount_H
#define GPDXL_CParseHandlerScalarLimitCount_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarLimitCount
//
//	@doc:
//		Parse handler for parsing a LIMIT COUNT statement
//
//---------------------------------------------------------------------------
class CParseHandlerScalarLimitCount : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarLimitCount(const CParseHandlerScalarLimitCount &);

	// process the start of an element
	void StartElement(const XMLCh *const element_uri,
					  const XMLCh *const element_local_name,
					  const XMLCh *const element_qname, const Attributes &attr);

	// process the end of an element
	void EndElement(const XMLCh *const element_uri,
					const XMLCh *const element_local_name,
					const XMLCh *const element_qname);

public:
	// ctor
	CParseHandlerScalarLimitCount(CMemoryPool *mp,
								  CParseHandlerManager *parse_handler_mgr,
								  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarLimitCount_H

//EOF
