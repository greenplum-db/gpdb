//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerGroupingColList.h
//
//	@doc:
//		SAX parse handler class for parsing the list of grouping columns ids in an
//		aggregate operator node.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerGroupingColList_H
#define GPDXL_CParseHandlerGroupingColList_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerGroupingColList
//
//	@doc:
//		SAX parse handler class for parsing the list of grouping column ids in
//		an Aggregate operator node.
//
//---------------------------------------------------------------------------
class CParseHandlerGroupingColList : public CParseHandlerBase
{
private:
	// array of grouping column ids
	ULongPtrArray *m_grouping_colids_array;

	// private copy ctor
	CParseHandlerGroupingColList(const CParseHandlerGroupingColList &);

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
	CParseHandlerGroupingColList(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerGroupingColList();

	// accessor
	ULongPtrArray *GetGroupingColidArray();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerGroupingColList_H

// EOF
