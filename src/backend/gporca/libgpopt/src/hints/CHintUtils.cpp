//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CHintUtils.cpp
//
//	@doc:
//		Utilitiies for plan hint objects
//---------------------------------------------------------------------------

#include "gpopt/hints/CHintUtils.h"

#include "gpos/common/clibwrapper.h"
#include "gpos/memory/queue.h"

using namespace gpopt;


// Find a scan hint that matches the operator based on the relation or alias
// name.
template <typename T>
static CScanHint *
GetScanHint(T *pop, CPlanHint *plan_hint)
{
	GPOS_ASSERT(nullptr != pop);

	if (nullptr == plan_hint)
	{
		// no parsed hints
		return nullptr;
	}

	const CWStringConst *name = pop->Ptabdesc()->Name().Pstr();

	return plan_hint->GetScanHint(name);
}


BOOL
CHintUtils::SatisfiesPlanHints(CLogicalGet *pop, CPlanHint *plan_hint)
{
	GPOS_ASSERT(nullptr != pop);

	CScanHint *scan_hint = GetScanHint(pop, plan_hint);
	if (scan_hint == nullptr)
	{
		// no matched hint, so everything goes...
		return true;
	}

	// If opertor matches hint operator _or_ it doesn't match and is a not.
	return scan_hint->SatisfiesOperator(pop);
}

BOOL
CHintUtils::SatisfiesPlanHints(CLogicalIndexGet *pop, CPlanHint *plan_hint)
{
	GPOS_ASSERT(nullptr != pop);

	CScanHint *scan_hint = GetScanHint(pop, plan_hint);
	if (scan_hint == nullptr)
	{
		// no matched hint, so everything goes...
		return true;
	}

	for (ULONG ul = 0; ul < scan_hint->GetIndexNames()->Size(); ul++)
	{
		if (pop->Pindexdesc()->Name().Pstr()->Equals(
				(*scan_hint->GetIndexNames())[ul]))
		{
			// If opertor matches hint operator and index matches hint index.
			return scan_hint->SatisfiesOperator(pop);
		}
	}

	return scan_hint->GetIndexNames()->Size() == 0 &&
		   scan_hint->SatisfiesOperator(pop);
}


BOOL
CHintUtils::SatisfiesPlanHints(CLogicalDynamicGet *pop, CPlanHint *plan_hint)
{
	GPOS_ASSERT(nullptr != pop);

	CScanHint *scan_hint = GetScanHint(pop, plan_hint);
	if (scan_hint == nullptr)
	{
		// no matched hint, so everything goes...
		return true;
	}

	return scan_hint->SatisfiesOperator(pop);
}


BOOL
CHintUtils::SatisfiesPlanHints(CLogicalDynamicIndexGet *pop,
							   CPlanHint *plan_hint)
{
	GPOS_ASSERT(nullptr != pop);

	CScanHint *scan_hint = GetScanHint(pop, plan_hint);
	if (scan_hint == nullptr)
	{
		// no matched hint, so everything goes...
		return true;
	}

	for (ULONG ul = 0; ul < scan_hint->GetIndexNames()->Size(); ul++)
	{
		if (pop->Pindexdesc()->Name().Pstr()->Equals(
				(*scan_hint->GetIndexNames())[ul]))
		{
			// If opertor matches hint operator and index matches hint index.
			return scan_hint->SatisfiesOperator(pop);
		}
	}

	return scan_hint->GetIndexNames()->Size() == 0 &&
		   scan_hint->SatisfiesOperator(pop);
}


BOOL
CHintUtils::SatisfiesPlanHints(CScalarBitmapIndexProbe *pop,
							   CPlanHint *plan_hint)
{
	GPOS_ASSERT(nullptr != pop);

	CScanHint *scan_hint = GetScanHint(pop, plan_hint);
	if (scan_hint == nullptr)
	{
		// no matched hint, so everything goes...
		return true;
	}

	for (ULONG ul = 0; ul < scan_hint->GetIndexNames()->Size(); ul++)
	{
		if (pop->Pindexdesc()->Name().Pstr()->Equals(
				(*scan_hint->GetIndexNames())[ul]))
		{
			// If opertor matches hint operator and index matches hint index.
			return scan_hint->SatisfiesOperator(pop);
		}
	}

	return scan_hint->GetIndexNames()->Size() == 0 &&
		   scan_hint->SatisfiesOperator(pop);
}

const WCHAR *
CHintUtils::ScanHintEnumToString(CScanHint::EType type)
{
	switch (type)
	{
		case CScanHint::SeqScan:
		{
			return GPOS_WSZ_LIT("SeqScan");
		}
		case CScanHint::NoSeqScan:
		{
			return GPOS_WSZ_LIT("NoSeqScan");
		}
		case CScanHint::IndexScan:
		{
			return GPOS_WSZ_LIT("IndexScan");
		}
		case CScanHint::NoIndexScan:
		{
			return GPOS_WSZ_LIT("NoIndexScan");
		}
		case CScanHint::IndexOnlyScan:
		{
			return GPOS_WSZ_LIT("IndexOnlyScan");
		}
		case CScanHint::NoIndexOnlyScan:
		{
			return GPOS_WSZ_LIT("NoIndexOnlyScan");
		}
		case CScanHint::BitmapScan:
		{
			return GPOS_WSZ_LIT("BitmapScan");
		}
		case CScanHint::NoBitmapScan:
		{
			return GPOS_WSZ_LIT("NoBitmapScan");
		}
		default:
		{
			return nullptr;
		}
	}
}

CScanHint::EType
CHintUtils::ScanHintStringToEnum(const WCHAR *type)
{
	if (0 ==
		clib::Wcsncmp(type, GPOS_WSZ_LIT("SeqScan"), gpos::clib::Wcslen(type)))
	{
		return CScanHint::SeqScan;
	}
	if (0 == clib::Wcsncmp(type, GPOS_WSZ_LIT("NoSeqScan"),
						   gpos::clib::Wcslen(type)))
	{
		return CScanHint::NoSeqScan;
	}
	if (0 == clib::Wcsncmp(type, GPOS_WSZ_LIT("IndexScan"),
						   gpos::clib::Wcslen(type)))
	{
		return CScanHint::IndexScan;
	}
	if (0 == clib::Wcsncmp(type, GPOS_WSZ_LIT("NoIndexScan"),
						   gpos::clib::Wcslen(type)))
	{
		return CScanHint::NoIndexScan;
	}
	if (0 == clib::Wcsncmp(type, GPOS_WSZ_LIT("IndexOnlyScan"),
						   gpos::clib::Wcslen(type)))
	{
		return CScanHint::IndexOnlyScan;
	}
	if (0 == clib::Wcsncmp(type, GPOS_WSZ_LIT("NoIndexOnlyScan"),
						   gpos::clib::Wcslen(type)))
	{
		return CScanHint::NoIndexOnlyScan;
	}
	if (0 == clib::Wcsncmp(type, GPOS_WSZ_LIT("BitmapScan"),
						   gpos::clib::Wcslen(type)))
	{
		return CScanHint::BitmapScan;
	}
	if (0 == clib::Wcsncmp(type, GPOS_WSZ_LIT("NoBitmapScan"),
						   gpos::clib::Wcslen(type)))
	{
		return CScanHint::NoBitmapScan;
	}
	return CScanHint::Sentinal;
}

//---------------------------------------------------------------------------
//	@function:
//		CHintUtils::GetAliasesFromTableDescriptors
//
//	@doc:
//		Returns a set containing all the aliases referenced in the table
//		descriptor hash set.
//---------------------------------------------------------------------------
CWStringConstHashSet *
CHintUtils::GetAliasesFromTableDescriptors(CMemoryPool *mp,
										   CTableDescriptorHashSet *ptabs)
{
	CWStringConstHashSet *pexpr_aliases = GPOS_NEW(mp) CWStringConstHashSet(mp);
	CTableDescriptorHashSetIter tabiter(ptabs);
	while (tabiter.Advance())
	{
		const CTableDescriptor *tabdesc = tabiter.Get();
		pexpr_aliases->Insert(tabdesc->Name().Pstr());
	}

	return pexpr_aliases;
}

//---------------------------------------------------------------------------
//	@function:
//		CHintUtils::GetAliasesFromHint
//
//	@doc:
//		Returns a set containing all the aliases referenced in the JoinPair.
//---------------------------------------------------------------------------
CWStringConstHashSet *
CHintUtils::GetAliasesFromHint(CMemoryPool *mp,
							   const CJoinHint::JoinPair *joinpair)
{
	CWStringConstHashSet *aliases = GPOS_NEW(mp) CWStringConstHashSet(mp);

	CAutoMemoryPool amp;
	gpos::queue<const CJoinHint::JoinPair *> q(amp.Pmp());
	q.push(joinpair);

	while (q.size() > 0)
	{
		const CJoinHint::JoinPair *pair = q.front();
		q.pop();
		if (nullptr != pair->GetName())
		{
			aliases->Insert(pair->GetName());
		}
		else
		{
			q.push(pair->GetOuter());
			q.push(pair->GetInner());
		}
	}

	return aliases;
}
