//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDatumGenericGPDB.cpp
//
//	@doc:
//		Implementation of GPDB generic datum
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/clibwrapper.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/base/CDatumGenericGPDB.h"

#include "naucrates/md/IMDType.h"
#include "naucrates/md/CMDIdGPDB.h"

#include "naucrates/statistics/CScaleFactorUtils.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/base/COptCtxt.h"

using namespace gpnaucrates;
using namespace gpmd;

// selectivities needed for LIKE predicate statistics evaluation
const CDouble CDatumGenericGPDB::DefaultFixedCharSelectivity(0.20);
const CDouble CDatumGenericGPDB::DefaultCharRangeSelectivity(0.25);
const CDouble CDatumGenericGPDB::DefaultAnyCharSelectivity(0.99);
const CDouble CDatumGenericGPDB::DefaultCdbRanchorSelectivity(0.95);
const CDouble CDatumGenericGPDB::DefaultCdbRolloffSelectivity(0.14);

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::CDatumGenericGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumGenericGPDB::CDatumGenericGPDB(CMemoryPool *mp, IMDId *mdid,
									 INT type_modifier, const void *src,
									 ULONG size, BOOL is_null,
									 LINT stats_comp_val_int,
									 CDouble stats_comp_val_double)
	: m_mp(mp),
	  m_size(size),
	  m_bytearray_value(NULL),
	  m_is_null(is_null),
	  m_mdid(mdid),
	  m_type_modifier(type_modifier),
	  m_stats_comp_val_int(stats_comp_val_int),
	  m_stats_comp_val_double(stats_comp_val_double)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(mdid->IsValid());

	if (!IsNull())
	{
		GPOS_ASSERT(0 < size);

		m_bytearray_value = GPOS_NEW_ARRAY(m_mp, BYTE, size);
		(void) clib::Memcpy(m_bytearray_value, src, size);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::~CDatumGenericGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDatumGenericGPDB::~CDatumGenericGPDB()
{
	GPOS_DELETE_ARRAY(m_bytearray_value);
	m_mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::IsNull
//
//	@doc:
//		Accessor of is null
//
//---------------------------------------------------------------------------
BOOL
CDatumGenericGPDB::IsNull() const
{
	return m_is_null;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::Size
//
//	@doc:
//		Accessor of size
//
//---------------------------------------------------------------------------
ULONG
CDatumGenericGPDB::Size() const
{
	return m_size;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::MDId
//
//	@doc:
//		Accessor of the type information
//
//---------------------------------------------------------------------------
IMDId *
CDatumGenericGPDB::MDId() const
{
	return m_mdid;
}


INT
CDatumGenericGPDB::TypeModifier() const
{
	return m_type_modifier;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CDatumGenericGPDB::HashValue() const
{
	ULONG hash = 0;
	if (IsNull())
	{
		hash = gpos::HashValue<ULONG>(&hash);
	}
	else
	{
		hash = gpos::HashValue<BYTE>(&m_bytearray_value[0]);
		ULONG size = Size();
		for (ULONG i = 1; i < size; i++)
		{
			hash = gpos::CombineHashes(
				hash, gpos::HashValue<BYTE>(&m_bytearray_value[i]));
		}
	}

	return gpos::CombineHashes(m_mdid->HashValue(), hash);
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::GetMDName
//
//	@doc:
//		Return string representation
//
//---------------------------------------------------------------------------
const CWStringConst *
CDatumGenericGPDB::GetStrRepr(CMemoryPool *mp) const
{
	CWStringDynamic str(mp);

	if (IsNull())
	{
		str.AppendFormat(GPOS_WSZ_LIT("null"));
		return GPOS_NEW(mp) CWStringConst(mp, str.GetBuffer());
	}

	// pretty print datums that can be mapped to LINTs or CDoubles
	if (IsDatumMappableToLINT())
	{
		str.AppendFormat(GPOS_WSZ_LIT("%0.3f"), (double) GetLINTMapping());
		return GPOS_NEW(mp) CWStringConst(mp, str.GetBuffer());
	}
	else if (IsDatumMappableToDouble())
	{
		str.AppendFormat(GPOS_WSZ_LIT("%0.3f"), GetDoubleMapping().Get());
		return GPOS_NEW(mp) CWStringConst(mp, str.GetBuffer());
	}

	// print hex representation of bytes
	ULONG size = Size();
	for (ULONG i = 0; i < size; i++)
	{
		str.AppendFormat(GPOS_WSZ_LIT("%02X"), m_bytearray_value[i]);
	}

	return GPOS_NEW(mp) CWStringConst(mp, str.GetBuffer());
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::Matches
//
//	@doc:
//		Matches the values of datums
//
//---------------------------------------------------------------------------
BOOL
CDatumGenericGPDB::Matches(const IDatum *datum) const
{
	if (!datum->MDId()->Equals(m_mdid) || (datum->Size() != Size()))
	{
		return false;
	}

	const CDatumGenericGPDB *datum_generic =
		dynamic_cast<const CDatumGenericGPDB *>(datum);

	if (datum_generic->IsNull() && IsNull())
	{
		return true;
	}

	if (!datum_generic->IsNull() && !IsNull())
	{
		if (0 == clib::Memcmp(datum_generic->m_bytearray_value,
							  m_bytearray_value, Size()))
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::MakeCopy
//
//	@doc:
//		Returns a copy of the datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumGenericGPDB::MakeCopy(CMemoryPool *mp) const
{
	m_mdid->AddRef();

	// CDatumGenericGPDB makes a copy of the buffer
	return GPOS_NEW(mp) CDatumGenericGPDB(
		mp, m_mdid, m_type_modifier, m_bytearray_value, m_size, m_is_null,
		m_stats_comp_val_int, m_stats_comp_val_double);
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CDatumGenericGPDB::OsPrint(IOstream &os) const
{
	const CWStringConst *str = GetStrRepr(m_mp);
	os << str->GetBuffer();
	GPOS_DELETE(str);

	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::IsDatumMappableToDouble
//
//	@doc:
//		For statistics computation, can this datum be mapped to a CDouble
//
//---------------------------------------------------------------------------
BOOL
CDatumGenericGPDB::IsDatumMappableToDouble() const
{
	return CMDTypeGenericGPDB::HasByte2DoubleMapping(this->MDId());
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::IsDatumMappableToLINT
//
//	@doc:
//		For statistics computation, can this datum be mapped to a LINT
//
//---------------------------------------------------------------------------
BOOL
CDatumGenericGPDB::IsDatumMappableToLINT() const
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDType *type = md_accessor->RetrieveType(MDId());
	return CMDTypeGenericGPDB::HasByte2IntMapping(type);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::MakeCopyOfValue
//
//	@doc:
//		For statistics computation, return the byte array representation of
//		the datum
//---------------------------------------------------------------------------
const BYTE *
CDatumGenericGPDB::GetByteArrayValue() const
{
	return m_bytearray_value;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::StatsAreEqual
//
//	@doc:
//		Are datums statistically equal?
//
//---------------------------------------------------------------------------
BOOL
CDatumGenericGPDB::StatsAreEqual(const IDatum *datum) const
{
	// if mapping exists, use that to compute equality
	if (IsDatumMappableToLINT() || IsDatumMappableToDouble())
	{
		return IDatum::StatsAreEqual(datum);
	}

	// take special care of nulls
	if (IsNull() || datum->IsNull())
	{
		return IsNull() && datum->IsNull();
	}

	// fall back to memcmp
	const CDatumGenericGPDB *datum_generic_gpdb =
		dynamic_cast<const CDatumGenericGPDB *>(datum);

	ULONG size = this->Size();
	if (size == datum_generic_gpdb->Size())
	{
		const BYTE *s1 = m_bytearray_value;
		const BYTE *s2 = datum_generic_gpdb->m_bytearray_value;
		return (clib::Memcmp(s1, s2, size) == 0);
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::MakeCopyOfValue
//
//	@doc:
//		Accessor of byte array
//
//---------------------------------------------------------------------------
BYTE *
CDatumGenericGPDB::MakeCopyOfValue(CMemoryPool *mp, ULONG *dest_length) const
{
	ULONG length = 0;
	BYTE *dest = NULL;

	if (!IsNull())
	{
		length = this->Size();
		;
		GPOS_ASSERT(length > 0);
		dest = GPOS_NEW_ARRAY(mp, BYTE, length);
		(void) clib::Memcpy(dest, this->m_bytearray_value, length);
	}

	*dest_length = length;
	return dest;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::NeedsPadding
//
//	@doc:
//		Does the datum need to be padded before statistical derivation
//
//---------------------------------------------------------------------------
BOOL
CDatumGenericGPDB::NeedsPadding() const
{
	return MDId()->Equals(&CMDIdGPDB::m_mdid_bpchar);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::MakePaddedDatum
//
//	@doc:
//		Return the padded datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumGenericGPDB::MakePaddedDatum(CMemoryPool *mp, ULONG col_len) const
{
	// in GPDB the first four bytes of the datum are used for the header
	const ULONG adjusted_col_width = col_len + GPDB_DATUM_HDRSZ;

	if (this->IsNull() || (gpos::ulong_max == col_len))
	{
		return this->MakeCopy(mp);
	}

	const ULONG datum_len = this->Size();
	if (gpos::ulong_max != adjusted_col_width && datum_len < adjusted_col_width)
	{
		const BYTE *original = this->GetByteArrayValue();
		BYTE *dest = NULL;

		dest = GPOS_NEW_ARRAY(m_mp, BYTE, adjusted_col_width);
		(void) clib::Memcpy(dest, original, datum_len);

		// datum's length smaller than column's size, therefore pad the input datum
		(void) clib::Memset(dest + datum_len, ' ',
							adjusted_col_width - datum_len);

		// create a new datum
		this->MDId()->AddRef();
		CDatumGenericGPDB *datum_new = GPOS_NEW(m_mp) CDatumGenericGPDB(
			mp, this->MDId(), this->TypeModifier(), dest, adjusted_col_width,
			this->IsNull(), this->GetLINTMapping(), 0 /* dValue */
		);

		// clean up the input byte array as the constructor creates a copy
		GPOS_DELETE_ARRAY(dest);

		return datum_new;
	}

	return this->MakeCopy(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::GetLikePredicateScaleFactor
//
//	@doc:
//		Return the scale factor of the like predicate by checking the pattern
//		that is being matched in the LIKE predicate
//---------------------------------------------------------------------------
CDouble
CDatumGenericGPDB::GetLikePredicateScaleFactor() const
{
	if (this->IsNull())
	{
		return CDouble(1.0);
	}

	const ULONG datum_len = this->Size();
	const BYTE *dest = this->GetByteArrayValue();

	ULONG pos = 0;

	// skip any leading %; it's already factored into initial selectivity (DDefaultScaleFactorLike).
	// In GPDB the first four bytes of the datum are used for the header
	for (pos = GPDB_DATUM_HDRSZ; pos < datum_len; pos++)
	{
		if ('%' != dest[pos] && '_' != dest[pos])
		{
			break;
		}
	}

	CDouble selectivity(1.0);
	CDouble fixed_char_selectivity =
		CDatumGenericGPDB::DefaultFixedCharSelectivity;
	while (pos < datum_len)
	{
		// % and _ are wildcard characters in LIKE
		if ('_' == dest[pos])
		{
			selectivity =
				selectivity * CDatumGenericGPDB::DefaultAnyCharSelectivity;
		}
		else if ('%' != dest[pos])
		{
			if ('\\' == dest[pos])
			{
				// backslash quotes the next character
				pos++;
				if (pos >= datum_len)
				{
					break;
				}
			}

			selectivity = selectivity * fixed_char_selectivity;
			fixed_char_selectivity =
				fixed_char_selectivity +
				(1.0 - fixed_char_selectivity) *
					CDatumGenericGPDB::DefaultCdbRolloffSelectivity;
		}

		pos++;
	}

	selectivity = selectivity * GetTrailingWildcardSelectivity(dest, pos);

	return 1 / std::max(selectivity,
						1 / CScaleFactorUtils::DDefaultScaleFactorLike);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumGenericGPDB::GetTrailingWildcardSelectivity
//
//	@doc:
//		Return the selectivity of the trailing wildcards
//
//---------------------------------------------------------------------------
CDouble
CDatumGenericGPDB::GetTrailingWildcardSelectivity(const BYTE *dest,
												  ULONG pos) const
{
	GPOS_ASSERT(NULL != dest);

	// If no trailing wildcard, reduce selectivity
	BOOL wildcard = (0 < pos) && ('%' != dest[pos - 1]);
	BOOL backslash = (2 <= pos) && ('\\' == dest[pos - 2]);
	if (wildcard || backslash)
	{
		return CDatumGenericGPDB::DefaultCdbRanchorSelectivity;
	}

	return CDouble(1.0);
}

// EOF
