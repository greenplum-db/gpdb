//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarCoerceToDomain.h
//
//	@doc:
//
//		SAX parse handler class for parsing CoerceToDomain operator.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarCoerceToDomain_H
#define GPDXL_CParseHandlerScalarCoerceToDomain_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarCoerceToDomain.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarCoerceToDomain
//
//	@doc:
//		Parse handler for parsing CoerceToDomain operator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarCoerceToDomain : public CParseHandlerScalarOp
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
	CParseHandlerScalarCoerceToDomain(
		const CParseHandlerScalarCoerceToDomain &) = delete;

	// ctor/dtor
	CParseHandlerScalarCoerceToDomain(CMemoryPool *mp,
									  CParseHandlerManager *parse_handler_mgr,
									  CParseHandlerBase *parse_handler_root);

	~CParseHandlerScalarCoerceToDomain() override = default;
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerScalarCoerceToDomain_H

//EOF
