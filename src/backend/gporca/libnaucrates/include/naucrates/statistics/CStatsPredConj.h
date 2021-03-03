//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CStatsPredConj.h
//
//	@doc:
//		Conjunctive filter on statistics
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CStatsPredConj_H
#define GPNAUCRATES_CStatsPredConj_H

#include "gpos/base.h"

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CStatsPred.h"

namespace gpnaucrates
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CStatsPredConj
//
//	@doc:
//		Conjunctive filter on statistics
//---------------------------------------------------------------------------
class CStatsPredConj : public CStatsPred
{
private:
	// array of filters
	CStatsPredPtrArry *m_conj_pred_stats_array;

public:
	CStatsPredConj &operator=(CStatsPredConj &) = delete;

	CStatsPredConj(const CStatsPredConj &) = delete;

	// ctor
	explicit CStatsPredConj(CStatsPredPtrArry *pdrgpstatspred);

	// dtor
	~CStatsPredConj() override
	{
		m_conj_pred_stats_array->Release();
	}

	// the column identifier on which the predicates are on
	ULONG GetColId() const override;

	// total number of predicates in the conjunction
	ULONG
	GetNumPreds() const
	{
		return m_conj_pred_stats_array->Size();
	}

	CStatsPredPtrArry *
	GetConjPredStatsArray() const
	{
		return m_conj_pred_stats_array;
	}

	// sort the components of the conjunction
	void Sort() const;

	// return the filter at a particular position
	CStatsPred *GetPredStats(ULONG pos) const;

	// filter type id
	EStatsPredType
	GetPredStatsType() const override
	{
		return CStatsPred::EsptConj;
	}

	// conversion function
	static CStatsPredConj *
	ConvertPredStats(CStatsPred *pred_stats)
	{
		GPOS_ASSERT(nullptr != pred_stats);
		GPOS_ASSERT(CStatsPred::EsptConj == pred_stats->GetPredStatsType());

		return dynamic_cast<CStatsPredConj *>(pred_stats);
	}

};	// class CStatsPredConj
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CStatsPredConj_H

// EOF
