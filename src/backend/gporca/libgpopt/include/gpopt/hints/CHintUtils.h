//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CHintUtils.h
//---------------------------------------------------------------------------
#ifndef GPOS_CHintUtils_H
#define GPOS_CHintUtils_H

#include "gpos/base.h"

#include "gpopt/hints/CPlanHint.h"
#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/operators/CLogicalDynamicIndexGet.h"
#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalIndexGet.h"
#include "gpopt/operators/COperator.h"
#include "gpopt/operators/CScalarBitmapIndexProbe.h"

namespace gpopt
{
class CHintUtils
{
public:
	// Check if CLogicalGet operator satisfies plan hints
	static BOOL SatisfiesPlanHints(CLogicalGet *pop, CPlanHint *plan_hint);

	// Check if CLogicalIndexGet operator satisfies plan hints
	static BOOL SatisfiesPlanHints(CLogicalIndexGet *pop, CPlanHint *plan_hint);

	// Check if CLogicalDynamicGet operator satisfies plan hints
	static BOOL SatisfiesPlanHints(CLogicalDynamicGet *pop,
								   CPlanHint *plan_hint);

	// Check if CLogicalDynamicIndexGet operator satisfies plan hints
	static BOOL SatisfiesPlanHints(CLogicalDynamicIndexGet *pop,
								   CPlanHint *plan_hint);

	// Check if CScalarBitmapIndexProbe operator satisfies plan hints
	static BOOL SatisfiesPlanHints(CScalarBitmapIndexProbe *pop,
								   CPlanHint *plan_hint);

	static const WCHAR *ScanHintEnumToString(CScanHint::EType type);

	static CScanHint::EType ScanHintStringToEnum(const WCHAR *type);
};

}  // namespace gpopt

#endif	// !GPOS_CHintUtils_H

// EOF
