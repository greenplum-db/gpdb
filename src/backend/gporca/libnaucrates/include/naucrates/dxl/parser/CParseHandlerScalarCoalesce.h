//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarCoalesce.h
//
//	@doc:
//
//		SAX parse handler class for parsing coalesce operator
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerScalarCoalesce_H
#define GPDXL_CParseHandlerScalarCoalesce_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/operators/CDXLScalarCoalesce.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarCoalesce
//
//	@doc:
//		Parse handler for parsing a coalesce operator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarCoalesce : public CParseHandlerScalarOp
{
private:
	// return type
	IMDId *m_mdid_type;

	// private copy ctor
	CParseHandlerScalarCoalesce(const CParseHandlerScalarCoalesce &);

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
	CParseHandlerScalarCoalesce(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarCoalesce_H

//EOF
