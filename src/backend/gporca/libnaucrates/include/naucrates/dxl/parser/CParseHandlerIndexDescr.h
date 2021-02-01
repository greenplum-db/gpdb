//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerIndexDescr.h
//
//	@doc:
//		SAX parse handler class for parsing the index descriptor portions
//		of index scan operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerIndexDescriptor_H
#define GPDXL_CParseHandlerIndexDescriptor_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLIndexDescr.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerIndexDescr
//
//	@doc:
//		Parse handler for parsing an index descriptor
//
//---------------------------------------------------------------------------
class CParseHandlerIndexDescr : public CParseHandlerBase
{
private:
	// the index descriptor to construct
	CDXLIndexDescr *m_dxl_index_descr;

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
	CParseHandlerIndexDescr(const CParseHandlerIndexDescr &) = delete;

	// ctor
	CParseHandlerIndexDescr(CMemoryPool *, CParseHandlerManager *,
							CParseHandlerBase *);

	//dtor
	~CParseHandlerIndexDescr() override;

	CDXLIndexDescr *GetDXLIndexDescr();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerIndexDescriptor_H

// EOF
