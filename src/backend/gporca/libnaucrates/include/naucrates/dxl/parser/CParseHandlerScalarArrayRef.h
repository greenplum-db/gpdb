//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarArrayRef.h
//
//	@doc:
//		SAX parse handler class for parsing scalar arrayref
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarScalarArrayRef_H
#define GPDXL_CParseHandlerScalarScalarArrayRef_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarArrayRef
//
//	@doc:
//		Parse handler class for parsing scalar arrayref
//
//---------------------------------------------------------------------------
class CParseHandlerScalarArrayRef : public CParseHandlerScalarOp
{
private:
	// number of index lists parsed
	ULONG m_parse_index_lists;

	// whether the parser is currently parsing the ref expr
	BOOL m_parsing_ref_expr;

	// whether the parser is currently parsing the assign expr
	BOOL m_parsing_assign_expr;

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
	CParseHandlerScalarArrayRef(const CParseHandlerScalarArrayRef &) = delete;

	// ctor
	CParseHandlerScalarArrayRef(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarScalarArrayRef_H

// EOF
