//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarOpList.h
//
//	@doc:
//		SAX parse handler class for parsing a list of scalar operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarScalarOpList_H
#define GPDXL_CParseHandlerScalarScalarOpList_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/operators/CDXLScalarOpList.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarOpList
//
//	@doc:
//		Parse handler class for parsing a list of scalar operators
//
//---------------------------------------------------------------------------
class CParseHandlerScalarOpList : public CParseHandlerScalarOp
{
private:
	// op list type
	CDXLScalarOpList::EdxlOpListType m_dxl_op_list_type;

	// return the op list type corresponding to the given operator name
	CDXLScalarOpList::EdxlOpListType GetDXLOpListType(
		const XMLCh *const element_local_name);

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
	CParseHandlerScalarOpList(const CParseHandlerScalarOpList &) = delete;

	// ctor
	CParseHandlerScalarOpList(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarScalarOpList_H

// EOF
