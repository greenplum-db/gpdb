//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CExtendedStatsProcessor.cpp
//
//	@doc:
//		Statistics helper routines for processing extended statistics.
//
//		Many functions in this file are mirrored versions of functions in
//		dependencies.c and selfuncs.c. Ideally they should stay in sync.
//		Unfortunately, the duplication is necessary due ORCA's DXL abstraction
//		that by design it to be independent of backend core. In other words, we
//		do not necessarily have access to backend core functions. Hence the
//		need to mirror them here.
//---------------------------------------------------------------------------

#include "naucrates/statistics/CExtendedStatsProcessor.h"

#include "gpos/common/CBitSet.h"

#include "naucrates/md/CMDExtStatsInfo.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"


#define STATS_MAX_DIMENSIONS 8 /* max number of attributes */

#define InvalidOid 0


using namespace gpopt;

static BOOL
IsDependencyCapablePredicate(CStatsPred *child_pred GPOS_UNUSED)
{
	return child_pred->GetPredStatsType() == CStatsPred::EsptPoint;
}

/*
 * choose_best_statistics
 *		Look for and return statistics with the specified 'requiredkind' which
 *		have keys that match at least two of the given attnums.  Return NULL if
 *		there's no match.
 *
 * The current selection criteria is very simple - we choose the statistics
 * object referencing the most of the requested attributes, breaking ties
 * in favor of objects with fewer keys overall.
 *
 * XXX If multiple statistics objects tie on both criteria, then which object
 * is chosen depends on the order that they appear in the stats list. Perhaps
 * further tiebreakers are needed.
 *
 * NB: This function is modified version of choose_best_statistics() in
 *     dependencies.c.
 */
CMDExtStatsInfo *
choose_best_statistics(CMemoryPool *mp,
					   CMDExtStatsInfoArray *md_statsinfo_array,
					   CBitSet *attnums,
					   CMDExtStatsInfo::Estattype requiredkind)
{
	CMDExtStatsInfo *best_match = nullptr;
	int best_num_matched = 2;						  /* goal #1: maximize */
	int best_match_keys = (STATS_MAX_DIMENSIONS + 1); /* goal #2: minimize */

	for (ULONG i = 0; i < md_statsinfo_array->Size(); i++)
	{
		CMDExtStatsInfo *info = (*md_statsinfo_array)[i];
		int num_matched;
		int numkeys;
		CBitSet *matched;

		/* skip statistics that are not of the correct type */
		if (info->GetStatKind() != requiredkind)
		{
			continue;
		}

		/* determine how many attributes of these stats can be matched to */
		matched = GPOS_NEW(mp) CBitSet(mp, *attnums);
		matched->Intersection(info->GetStatKeys());
		num_matched = matched->Size();
		matched->Release();

		/*
		 * save the actual number of keys in the stats so that we can choose
		 * the narrowest stats with the most matching keys.
		 */
		numkeys = info->GetStatKeys()->Size();

		/*
		 * Use this object when it increases the number of matched clauses or
		 * when it matches the same number of attributes but these stats have
		 * fewer keys than any previous match.
		 */
		if (num_matched > best_num_matched ||
			(num_matched == best_num_matched && numkeys < best_match_keys))
		{
			best_match = info;
			best_num_matched = num_matched;
			best_match_keys = numkeys;
		}
	}

	return best_match;
}

/*
 * dependency_implies_attribute
 *		check that the attnum matches is implied by the functional dependency
 *
 * NB: This function is modified version of dependency_implies_attribute() in
 *     dependencies.c.
 */
static bool
dependency_implies_attribute(CMDDependency *dependency, INT attnum)
{
	if (attnum == dependency->GetToAttno())
	{
		return true;
	}

	return false;
}

/*
 * dependency_is_fully_matched
 *		checks that a functional dependency is fully matched given clauses on
 *		attributes (assuming the clauses are suitable equality clauses)
 *
 * NB: This function is modified version of dependency_is_fully_matched() in
 *     dependencies.c.
 */
static bool
dependency_is_fully_matched(CMDDependency *dependency, CBitSet *attnums)
{
	/*
	 * Check that the dependency actually is fully covered by clauses. We have
	 * to translate all attribute numbers, as those are referenced
	 */
	for (ULONG j = 0; j < dependency->GetNAttributes() - 1; j++)
	{
		int attnum = *(*dependency->GetFromAttno())[j];

		if (!attnums->Get(attnum))
		{
			return false;
		}
	}

	return attnums->Get(dependency->GetToAttno());
}

/*
 * find_strongest_dependency
 *		find the strongest dependency on the attributes
 *
 * When applying functional dependencies, we start with the strongest
 * dependencies. That is, we select the dependency that:
 *
 * (a) has all attributes covered by equality clauses
 *
 * (b) has the most attributes
 *
 * (c) has the highest degree of validity
 *
 * This guarantees that we eliminate the most redundant conditions first
 * (see the comment in dependencies_clauselist_selectivity).
 *
 * NB: This function is modified version of find_strongest_dependency() in
 *     dependencies.c.
 */
static CMDDependency *
find_strongest_dependency(CMDDependencyArray *dependencies, CBitSet *attnums)
{
	ULONG i;
	CMDDependency *strongest = nullptr;

	/* number of attnums in clauses */
	ULONG nattnums = attnums->Size();

	/*
	 * Iterate over the MVDependency items and find the strongest one from the
	 * fully-matched dependencies. We do the cheap checks first, before
	 * matching it against the attnums.
	 */
	for (i = 0; i < dependencies->Size(); i++)
	{
		CMDDependency *dependency = (*dependencies)[i];

		/*
		 * Skip dependencies referencing more attributes than available
		 * clauses, as those can't be fully matched.
		 */
		if (dependency->GetNAttributes() > nattnums)
		{
			continue;
		}

		if (strongest)
		{
			/* skip dependencies on fewer attributes than the strongest. */
			if (dependency->GetNAttributes() < strongest->GetNAttributes())
			{
				continue;
			}

			/* also skip weaker dependencies when attribute count matches */
			if (strongest->GetNAttributes() == dependency->GetNAttributes() &&
				strongest->GetDegree() > dependency->GetDegree())
			{
				continue;
			}
		}

		/*
		 * this dependency is stronger, but we must still check that it's
		 * fully matched to these attnums. We perform this check last as it's
		 * slightly more expensive than the previous checks.
		 */
		if (dependency_is_fully_matched(dependency, attnums))
		{
			strongest = dependency; /* save new best match */
		}
	}

	return strongest;
}

//---------------------------------------------------------------------------
//	@function:
//		CExtendedStatsProcessor::ApplyCorrelatedStatsToScaleFactorFilterCalculation
//
//	@doc:
//		This function is essentially an ORCA version of the dependencies.c
//		function dependencies_clauselist_selectivity(). It determines the most
//		suitable extended statistic to apply and computes the scale factor for
//		the conjunctive_pred_stats.
//
//---------------------------------------------------------------------------
void
CExtendedStatsProcessor::ApplyCorrelatedStatsToScaleFactorFilterCalculation(
	CDoubleArray *scale_factors, CStatsPredConj *conjunctive_pred_stats,
	const IMDExtStatsInfo *md_statsinfo, UlongToIntMap *colid_to_attno_mapping,
	CMemoryPool *mp, UlongToHistogramMap *result_histograms)
{
	GPOS_ASSERT(scale_factors->Size() == 0);

	if (!md_statsinfo || md_statsinfo->GetExtStatInfoArray()->Size() == 0)
	{
		return;
	}

	DOUBLE s1 = 1.0;
	CMDExtStatsInfo *stat;
	CMDDependencyArray *dependencies;

	CBitSet *clauses_attnums = GPOS_NEW(mp) CBitSet(mp);

	/*
	 * Pre-process the clauses list to extract the attnums seen in each item.
	 * We need to determine if there's any clauses which will be useful for
	 * dependency selectivity estimations. Along the way we'll record all of
	 * the attnums for each clause in a list which we'll reference later so we
	 * don't need to repeat the same work again. We'll also keep track of all
	 * attnums seen.
	 *
	 * We also skip clauses that we already estimated using different types of
	 * statistics (we treat them as incompatible).
	 */
	for (ULONG ul = 0; ul < conjunctive_pred_stats->GetNumPreds(); ul++)
	{
		CStatsPred *child_pred = conjunctive_pred_stats->GetPredStats(ul);
		if (!child_pred->IsAlreadyUsedInScaleFactorEstimation() &&
			IsDependencyCapablePredicate(child_pred))
		{
			ULONG colid = child_pred->GetColId();
			INT *attnum = colid_to_attno_mapping->Find(&colid);
			clauses_attnums->ExchangeSet(*attnum);
		}
	}

	/*
	 * If there's not at least two distinct attnums then reject the whole list
	 * of clauses.
	 */
	if (clauses_attnums->Size() < 2)
	{
		return;
	}

	/* find the best suited statistics object for these attnums */
	stat = choose_best_statistics(mp, md_statsinfo->GetExtStatInfoArray(),
								  clauses_attnums,
								  CMDExtStatsInfo::EstatDependencies);

	if (!stat)
	{
		return;
	}

	const COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	CMDAccessor *md_accessor = poctxt->Pmda();

	CMDIdGPDB *pmdid =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidExtStats, stat->GetStatOid());
	const IMDExtStats *extstats = md_accessor->RetrieveExtStats(pmdid);
	pmdid->Release();

	/* load the dependency items stored in the statistics object */
	dependencies = extstats->GetDependencies();

	/*
	 * Apply the dependencies recursively, starting with the widest/strongest
	 * ones, and proceeding to the smaller/weaker ones. At the end of each
	 * round we factor in the selectivity of clauses on the implied attribute,
	 * and remove the clauses from the list.
	 */
	while (true)
	{
		DOUBLE s2 = 1.0;
		CMDDependency *dependency;

		/* the widest/strongest dependency, fully matched by clauses */
		dependency = find_strongest_dependency(dependencies, clauses_attnums);

		/* if no suitable dependency was found, we're done */
		if (!dependency)
		{
			break;
		}

		/*
		 * We found an applicable dependency, so find all the clauses on the
		 * implied attribute - with dependency (a,b => c) we look for clauses
		 * on 'c'.
		 */
		const ULONG filters = conjunctive_pred_stats->GetNumPreds();
		for (ULONG ul = 0; ul < filters; ul++)
		{
			CStatsPred *child_pred = conjunctive_pred_stats->GetPredStats(ul);

			/*
			 * Skip incompatible clauses, and ones we've already estimated on.
			 */
			if (child_pred->IsAlreadyUsedInScaleFactorEstimation())
			{
				continue;
			}

			ULONG colid = child_pred->GetColId();
			INT *attnum = colid_to_attno_mapping->Find(&colid);

			/*
			 * Technically we could find more than one clause for a given
			 * attnum. Since these clauses must be equality clauses, we choose
			 * to only take the selectivity estimate from the final clause in
			 * the list for this attnum. If the attnum happens to be compared
			 * to a different Const in another clause then no rows will match
			 * anyway. If it happens to be compared to the same Const, then
			 * ignoring the additional clause is just the thing to do.
			 */
			if (dependency_implies_attribute(dependency, *attnum))
			{
				s2 = 1 / result_histograms->Find(&colid)->GetFrequency().Get();

				/* mark this one as done, so we don't touch it again. */
				child_pred->SetEstimated();

				/*
				 * Mark that we've got and used the dependency on this clause.
				 * We'll want to ignore this when looking for the next
				 * strongest dependency above.
				 */
				clauses_attnums->ExchangeClear(*attnum);
			}
		}

		/*
		 * Now factor in the selectivity for all the "implied" clauses into
		 * the final one, using this formula:
		 *
		 * P(a,b) = P(a) * (f + (1-f) * P(b))
		 *
		 * where 'f' is the degree of validity of the dependency.
		 */
		s1 *= (dependency->GetDegree().Get() +
			   (1 - dependency->GetDegree().Get()) * s2);
	}
	scale_factors->Append(GPOS_NEW(mp) CDouble(s1));

	clauses_attnums->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CExtendedStatsProcessor::ApplyCorrelatedStatsToNDistinctCalculation
//
//	@doc:
//		This function is essentially an ORCA version of the selfuncs.c
//		function estimate_multivariate_ndistinct(). It determines the most
//		suitable extended statistic to apply and calculate ndistinct.
//---------------------------------------------------------------------------
bool
CExtendedStatsProcessor::ApplyCorrelatedStatsToNDistinctCalculation(
	CMemoryPool *mp, const IMDExtStatsInfo *md_statsinfo,
	const UlongToIntMap *colid_to_attno_mapping,
	ULongPtrArray *&src_grouping_cols, DOUBLE *ndistinct)
{
	int nmatches;
	OID statOid = InvalidOid;
	const IMDExtStats *stats;
	CBitSet *attnums = nullptr;
	CBitSet *matched = nullptr;

	/* bail out immediately if the table has no extended statistics */
	if (!md_statsinfo || !colid_to_attno_mapping)
	{
		return false;
	}

	attnums = GPOS_NEW(mp) CBitSet(mp);
	for (ULONG ul = 0; ul < src_grouping_cols->Size(); ul++)
	{
		ULONG colid = *(*src_grouping_cols)[ul];

		INT *attnum = colid_to_attno_mapping->Find(&colid);
		if (!attnum)
		{
			attnums->Release();
			return false;
		}
		attnums->ExchangeSet(*attnum);
	}

	/* look for the ndistinct statistics matching the most vars */
	nmatches = 1; /* we require at least two matches */

	CMDExtStatsInfoArray *md_statsinfo_array =
		md_statsinfo->GetExtStatInfoArray();
	for (ULONG ul = 0; ul < md_statsinfo_array->Size(); ul++)
	{
		CMDExtStatsInfo *info = (*md_statsinfo_array)[ul];
		CBitSet *shared = GPOS_NEW(mp) CBitSet(mp, *attnums);
		int nshared;

		/* skip statistics of other kinds */
		if (info->GetStatKind() != CMDExtStatsInfo::EstatNDistinct)
		{
			shared->Release();
			continue;
		}

		/* compute attnums shared by the vars and the statistics object */
		shared->Intersection(info->GetStatKeys());
		nshared = shared->Size();

		/*
		 * Does this statistics object match more columns than the currently
		 * best object?  If so, use this one instead.
		 *
		 * XXX This should break ties using name of the object, or something
		 * like that, to make the outcome stable.
		 */
		if (nshared > nmatches)
		{
			CRefCount::SafeRelease(matched);

			statOid = info->GetStatOid();
			nmatches = nshared;
			matched = shared;
		}
		else
		{
			shared->Release();
		}
	}

	/* No match? */
	if (statOid == InvalidOid)
	{
		CRefCount::SafeRelease(matched);
		attnums->Release();
		return false;
	}

	const COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	CMDAccessor *md_accessor = poctxt->Pmda();

	CMDIdGPDB *pmdid = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidExtStats, statOid);
	stats = md_accessor->RetrieveExtStats(pmdid);
	pmdid->Release();

	/*
	 * If we have a match, search it for the specific item that matches (there
	 * must be one), and construct the output values.
	 */
	if (stats)
	{
		CMDNDistinct *item = nullptr;

		/* Find the specific item that exactly matches the combination */
		for (ULONG i = 0; i < stats->GetNDistinctList()->Size(); i++)
		{
			CMDNDistinct *tmpitem = (*stats->GetNDistinctList())[i];

			if (tmpitem->GetAttrs()->Equals(matched))
			{
				item = tmpitem;
				break;
			}
		}

		/* Form the output varinfo list, keeping only unmatched ones */
		ULongPtrArray *new_src_grouping_cols = GPOS_NEW(mp) ULongPtrArray(mp);
		for (ULONG ul = 0; ul < src_grouping_cols->Size(); ul++)
		{
			ULONG colid = *(*src_grouping_cols)[ul];

			INT *attnum = colid_to_attno_mapping->Find(&colid);
			if (!matched->Get(*attnum))
			{
				new_src_grouping_cols->Append(GPOS_NEW(mp) ULONG(colid));
			}
		}
		src_grouping_cols->Release();
		src_grouping_cols = new_src_grouping_cols;

		matched->Release();
		attnums->Release();

		*ndistinct = item->GetNDistinct().Get();
		return true;
	}

	matched->Release();

	attnums->Release();
	return false;
}
