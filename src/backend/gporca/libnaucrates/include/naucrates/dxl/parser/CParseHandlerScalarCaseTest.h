//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarCaseTest.h
//
//	@doc:
//
//		SAX parse handler class for parsing case test
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerScalarCaseTest_H
#define GPDXL_CParseHandlerScalarCaseTest_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarCaseTest.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarCaseTest
//
//	@doc:
//		Parse handler for parsing a case test
//
//---------------------------------------------------------------------------
class CParseHandlerScalarCaseTest : public CParseHandlerScalarOp
{
private:
	// return type
	IMDId *m_mdid_type;

	// private copy ctor
	CParseHandlerScalarCaseTest(const CParseHandlerScalarCaseTest &);

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
	CParseHandlerScalarCaseTest(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarCaseTest_H

//EOF
