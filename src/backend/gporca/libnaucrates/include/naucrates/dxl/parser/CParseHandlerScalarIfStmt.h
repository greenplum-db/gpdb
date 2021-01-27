//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarIfStmt.h
//
//	@doc:
//
//		SAX parse handler class for parsing If statement operator
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerIfStmt_H
#define GPDXL_CParseHandlerIfStmt_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarIfStmt.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarIfStmt
//
//	@doc:
//		Parse handler for parsing an IF statement
//
//---------------------------------------------------------------------------
class CParseHandlerScalarIfStmt : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarIfStmt(const CParseHandlerScalarIfStmt &);

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
	CParseHandlerScalarIfStmt(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerIfStmt_H

//EOF
