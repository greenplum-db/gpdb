//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerOp.h
//
//	@doc:
//		Base SAX parse handler class for parsing DXL operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerOp_H
#define GPDXL_CParseHandlerOp_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerOp
//
//	@doc:
//		Base parse handler class for DXL operators
//
//
//---------------------------------------------------------------------------
class CParseHandlerOp : public CParseHandlerBase
{
private:
	// private copy ctor
	CParseHandlerOp(const CParseHandlerOp &);


protected:
	// the root of the parsed DXL tree constructed by the parse handler
	CDXLNode *m_dxl_node;


	void AddChildFromParseHandler(const CParseHandlerOp *);

public:
	// ctor/dtor
	CParseHandlerOp(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
					CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerOp();

	// returns constructed DXL node
	CDXLNode *CreateDXLNode() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerOp_H

// EOF
