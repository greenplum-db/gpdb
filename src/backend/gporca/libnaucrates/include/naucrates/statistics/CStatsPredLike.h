//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CStatsPredLike.h
//
//	@doc:
//		LIKE filter for statistics
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CStatsPredLike_H
#define GPNAUCRATES_CStatsPredLike_H

#include "gpos/base.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/statistics/CStatsPred.h"

// fwd decl
namespace gpopt
{
class CExpression;
}

namespace gpnaucrates
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CStatsPredLike
//
//	@doc:
//		LIKE filter for statistics
//---------------------------------------------------------------------------
class CStatsPredLike : public CStatsPred
{
private:
	// private copy ctor
	CStatsPredLike(const CStatsPredLike &);

	// private assignment operator
	CStatsPredLike &operator=(CStatsPredLike &);

	// left hand side of the LIKE expression
	CExpression *m_expr_left;

	// right hand side of the LIKE expression
	CExpression *m_expr_right;

	// default scale factor
	CDouble m_default_scale_factor;

public:
	// ctor
	CStatsPredLike(ULONG colid, CExpression *expr_left, CExpression *expr_right,
				   CDouble default_scale_factor);

	// dtor
	virtual ~CStatsPredLike();

	// the column identifier on which the predicates are on
	virtual ULONG GetColId() const;

	// filter type id
	virtual EStatsPredType
	GetPredStatsType() const
	{
		return CStatsPred::EsptLike;
	}

	// left hand side of the LIKE expression
	virtual CExpression *
	GetExprOnLeft() const
	{
		return m_expr_left;
	}

	// right hand side of the LIKE expression
	virtual CExpression *
	GetExprOnRight() const
	{
		return m_expr_right;
	}

	// default scale factor
	virtual CDouble DefaultScaleFactor() const;

	// conversion function
	static CStatsPredLike *
	ConvertPredStats(CStatsPred *pred_stats)
	{
		GPOS_ASSERT(NULL != pred_stats);
		GPOS_ASSERT(CStatsPred::EsptLike == pred_stats->GetPredStatsType());

		return dynamic_cast<CStatsPredLike *>(pred_stats);
	}

};	// class CStatsPredLike
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CStatsPredLike_H

// EOF
