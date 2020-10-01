//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerMDIndexInfoList.h
//
//	@doc:
//		SAX parse handler class for parsing an indexinfo list
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDIndexInfoList_H
#define GPDXL_CParseHandlerMDIndexInfoList_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/IMDIndex.h"
#include "naucrates/md/IMDRelation.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

// parse handler class for parsing indexinfo list
class CParseHandlerMDIndexInfoList : public CParseHandlerBase
{
private:
	// list of indexinfo
	CMDIndexInfoArray *m_mdindex_info_array;

	// private copy ctor
	CParseHandlerMDIndexInfoList(const CParseHandlerMDIndexInfoList &);

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
	CParseHandlerMDIndexInfoList(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerMDIndexInfoList();

	// returns array of indexinfo
	CMDIndexInfoArray *GetMdIndexInfoArray();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDIndexInfoList_H

// EOF
