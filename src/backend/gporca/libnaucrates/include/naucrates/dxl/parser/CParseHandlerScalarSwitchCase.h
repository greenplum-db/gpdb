//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSwitchCase.h
//
//	@doc:
//
//		SAX parse handler class for parsing a SwitchCase operator
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerScalarSwitchCase_H
#define GPDXL_CParseHandlerScalarSwitchCase_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarSwitchCase.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarSwitchCase
//
//	@doc:
//		Parse handler for parsing a SwitchCase operator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarSwitchCase : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarSwitchCase(const CParseHandlerScalarSwitchCase &);

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
	CParseHandlerScalarSwitchCase(CMemoryPool *mp,
								  CParseHandlerManager *parse_handler_mgr,
								  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarSwitchCase_H

//EOF
