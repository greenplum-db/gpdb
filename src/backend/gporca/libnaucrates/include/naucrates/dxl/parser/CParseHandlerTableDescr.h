//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerTableDescr.h
//
//	@doc:
//		SAX parse handler class for parsing table descriptor portions
//		of scan operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerTableDescriptor_H
#define GPDXL_CParseHandlerTableDescriptor_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerTableDescr
//
//	@doc:
//		Parse handler for parsing a table descriptor
//
//---------------------------------------------------------------------------
class CParseHandlerTableDescr : public CParseHandlerBase
{
private:
	// the table descriptor to construct
	CDXLTableDescr *m_dxl_table_descr;

	// private copy ctor
	CParseHandlerTableDescr(const CParseHandlerTableDescr &);

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
	CParseHandlerTableDescr(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	~CParseHandlerTableDescr();

	CDXLTableDescr *GetDXLTableDescr();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerTableDescriptor_H

// EOF
