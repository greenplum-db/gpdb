//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CParseHandlerScalarFieldSelect.h
//
//	@doc:
//		SAX parse handler class for parsing scalar FieldSelect operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarFieldSelect_H
#define GPDXL_CParseHandlerScalarFieldSelect_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarFieldSelect.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarFieldSelect
//
//	@doc:
//		Parse handler class for parsing scalar FieldSelect
//
//---------------------------------------------------------------------------
class CParseHandlerScalarFieldSelect : public CParseHandlerScalarOp
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
	CParseHandlerScalarFieldSelect(const CParseHandlerScalarFieldSelect &) =
		delete;

	// ctor/dtor
	CParseHandlerScalarFieldSelect(CMemoryPool *mp,
								   CParseHandlerManager *parse_handler_mgr,
								   CParseHandlerBase *parse_handler_root);
	~CParseHandlerScalarFieldSelect() override = default;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarFieldSelect_H

// EOF
