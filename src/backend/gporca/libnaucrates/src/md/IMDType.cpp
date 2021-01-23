//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		IMDType.cpp
//
//	@doc:
//		Implementation
//---------------------------------------------------------------------------

#include "naucrates/md/IMDType.h"

#include "gpos/string/CWStringConst.h"

#include "naucrates/base/IDatum.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/statistics/CStatistics.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		IMDType::GetCmpTypeStr
//
//	@doc:
//		Return the comparison type as a string value
//
//---------------------------------------------------------------------------
const CWStringConst *
IMDType::GetCmpTypeStr(IMDType::ECmpType cmp_type)
{
	GPOS_ASSERT(IMDType::EcmptOther >= cmp_type);

	Edxltoken dxl_token_array[] = {
		EdxltokenCmpEq, EdxltokenCmpNeq, EdxltokenCmpLt,  EdxltokenCmpLeq,
		EdxltokenCmpGt, EdxltokenCmpGeq, EdxltokenCmpIDF, EdxltokenCmpOther};

	GPOS_ASSERT(IMDType::EcmptOther + 1 == GPOS_ARRAY_SIZE(dxl_token_array));
	return CDXLTokens::GetDXLTokenStr(dxl_token_array[cmp_type]);
}


//---------------------------------------------------------------------------
//	@function:
//		IMDType::StatsAreComparable
//
//	@doc:
//		Return true if we can perform statistical comparison between
//		datums of these two types; else return false
//
//---------------------------------------------------------------------------
BOOL
IMDType::StatsAreComparable(const IMDType *mdtype_first,
							const IMDType *mdtype_second)
{
	GPOS_ASSERT(NULL != mdtype_first);
	GPOS_ASSERT(NULL != mdtype_second);

	const IDatum *datum_first = mdtype_first->DatumNull();
	const IDatum *datum_second = mdtype_second->DatumNull();

	return datum_first->StatsAreComparable(datum_second);
}


//---------------------------------------------------------------------------
//	@function:
//		IMDType::StatsAreComparable
//
//	@doc:
//		Return true if we can perform statistical comparison between
//		datum of the given type and a given datum; else return false
//
//---------------------------------------------------------------------------
BOOL
IMDType::StatsAreComparable(const IMDType *mdtype_first,
							const IDatum *datum_second)
{
	GPOS_ASSERT(NULL != mdtype_first);
	GPOS_ASSERT(NULL != datum_second);

	const IDatum *datum_first = mdtype_first->DatumNull();

	return datum_first->StatsAreComparable(datum_second);
}


// EOF
