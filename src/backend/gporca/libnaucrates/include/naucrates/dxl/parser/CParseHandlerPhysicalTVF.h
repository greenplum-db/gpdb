//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerPhysicalTVF.h
//
//	@doc:
//
//		SAX parse handler class for parsing Physical TVF
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerPhysicalTVF_H
#define GPDXL_CParseHandlerPhysicalTVF_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerPhysicalTVF
//
//	@doc:
//		Parse handler for parsing a Physical TVF
//
//---------------------------------------------------------------------------
class CParseHandlerPhysicalTVF : public CParseHandlerPhysicalOp
{
private:
	// catalog id of the function
	IMDId *m_func_mdid;

	// return type
	IMDId *m_return_type_mdid;

	// function name
	CWStringConst *m_pstr;

	// private copy ctor
	CParseHandlerPhysicalTVF(const CParseHandlerPhysicalTVF &);

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
	CParseHandlerPhysicalTVF(CMemoryPool *mp,
							 CParseHandlerManager *parse_handler_mgr,
							 CParseHandlerBase *parse_handler_root);
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerPhysicalTVF_H

// EOF
