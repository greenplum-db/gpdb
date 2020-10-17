//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerSubqueryScan.h
//
//	@doc:
//		SAX parse handler class for parsing subquery scan operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerSubqueryScan_H
#define GPDXL_CParseHandlerSubqueryScan_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

#include "naucrates/dxl/operators/CDXLPhysicalSubqueryScan.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerSubqueryScan
//
//	@doc:
//		Parse handler for parsing a subquery scan operator
//
//---------------------------------------------------------------------------
class CParseHandlerSubqueryScan : public CParseHandlerPhysicalOp
{
private:
	// the subquery scan operator
	CDXLPhysicalSubqueryScan *m_dxl_op;

	// private copy ctor
	CParseHandlerSubqueryScan(const CParseHandlerSubqueryScan &);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor/dtor
	CParseHandlerSubqueryScan(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerSubqueryScan_H

// EOF
