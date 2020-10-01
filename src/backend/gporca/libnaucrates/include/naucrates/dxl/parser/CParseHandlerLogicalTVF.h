//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalTVF.h
//
//	@doc:
//
//		SAX parse handler class for parsing Logical TVF
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerLogicalTVF_H
#define GPDXL_CParseHandlerLogicalTVF_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalTVF
//
//	@doc:
//		Parse handler for parsing a Logical TVF
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalTVF : public CParseHandlerLogicalOp
{
private:
	// catalog id of the function
	IMDId *m_func_mdid;

	// return type
	IMDId *m_return_type_mdid;

	// function name
	CMDName *m_mdname;

	// private copy ctor
	CParseHandlerLogicalTVF(const CParseHandlerLogicalTVF &);

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
	// ctor
	CParseHandlerLogicalTVF(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerLogicalTVF_H

// EOF
