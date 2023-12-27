//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CIndexQualInfo.h
//
//	@doc:
//		Class providing access to the original index qual expression, its modified
//		version tailored for GPDB, and index strategy
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CIndexQualInfo_H
#define GPDXL_CIndexQualInfo_H

extern "C" {
#include "postgres.h"
}

namespace gpdxl
{
using namespace gpopt;

class CDXLNode;

//---------------------------------------------------------------------------
//	@class:
//		CIndexQualInfo
//
//	@doc:
//		Class providing access to the original index qual expression, its modified
//		version tailored for GPDB, and index strategy
//
//---------------------------------------------------------------------------
class CIndexQualInfo
{
public:
	// attribute number in the index
	AttrNumber m_attno;

	// index qual expression tailored for GPDB
	Expr *m_expr;

	// original index qual expression
	Expr *m_original_expr;

	// ctor
	CIndexQualInfo(AttrNumber attno, Expr *expr, Expr *original_expr)
		: m_attno(attno), m_expr(expr), m_original_expr(original_expr)
	{
		GPOS_ASSERT((IsA(m_expr, OpExpr) && IsA(m_original_expr, OpExpr)) ||
					(IsA(m_expr, ScalarArrayOpExpr) &&
					 IsA(original_expr, ScalarArrayOpExpr)) ||
					(IsA(m_expr, NullTest) && IsA(original_expr, NullTest)));
	}

	// dtor
	~CIndexQualInfo() = default;

	// comparison function for sorting index qualifiers
	static INT
	IndexQualInfoCmp(const void *p1, const void *p2)
	{
		const CIndexQualInfo *qual_info1 = *(const CIndexQualInfo **) p1;
		const CIndexQualInfo *qual_info2 = *(const CIndexQualInfo **) p2;

		return (INT) qual_info1->m_attno - (INT) qual_info2->m_attno;
	}
};
// array of index qual info
using CIndexQualInfoArray = CDynamicPtrArray<CIndexQualInfo, CleanupDelete>;
}  // namespace gpdxl

#endif	// !GPDXL_CIndexQualInfo_H

// EOF
