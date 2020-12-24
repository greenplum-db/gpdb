//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarAssertConstraintList.h
//
//	@doc:
//		SAX parse handler class for parsing assert constraint list nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarAssertConstraintList_H
#define GPDXL_CParseHandlerScalarAssertConstraintList_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarAssertConstraint.h"
#include "naucrates/dxl/operators/CDXLScalarAssertConstraintList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarAssertConstraintList
//
//	@doc:
//		Parse handler for DXL scalar assert constraint lists
//
//---------------------------------------------------------------------------
class CParseHandlerScalarAssertConstraintList : public CParseHandlerScalarOp
{
private:
	// scalar assert operator
	CDXLScalarAssertConstraintList *m_dxl_op;

	// current assert constraint
	CDXLScalarAssertConstraint *m_dxl_op_assert_constraint;

	// array of assert constraint nodes parsed so far
	CDXLNodeArray *m_dxlnode_assert_constraints_parsed_array;

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
	CParseHandlerScalarAssertConstraintList(
		const CParseHandlerScalarAssertConstraintList &) = delete;

	// ctor
	CParseHandlerScalarAssertConstraintList(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarAssertConstraintList_H

// EOF
