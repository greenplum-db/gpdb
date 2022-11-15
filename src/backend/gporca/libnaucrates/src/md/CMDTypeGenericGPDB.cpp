//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDTypeGenericGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing GPDB generic types
//---------------------------------------------------------------------------

#include "naucrates/md/CMDTypeGenericGPDB.h"

#include "gpos/string/CWStringDynamic.h"

#include "gpopt/base/COptCtxt.h"
#include "naucrates/base/CDatumGenericGPDB.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLDatumGeneric.h"
#include "naucrates/dxl/operators/CDXLDatumStatsDoubleMappable.h"
#include "naucrates/dxl/operators/CDXLDatumStatsLintMappable.h"
#include "naucrates/dxl/operators/CDXLScalarConstValue.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/statistics/CStatsPredUtils.h"

using namespace gpnaucrates;
using namespace gpdxl;
using namespace gpmd;

#define GPDB_ANYELEMENT_OID OID(2283)  // oid of GPDB ANYELEMENT type

#define GPDB_ANYARRAY_OID OID(2277)	 // oid of GPDB ANYARRAY type

#define GPDB_ANYNONARRAY_OID OID(2776)	// oid of GPDB ANYNONARRAY type

#define GPDB_ANYENUM_OID OID(3500)	// oid of GPDB ANYENUM type

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::CMDTypeGenericGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDTypeGenericGPDB::CMDTypeGenericGPDB(
	CMemoryPool *mp, IMDId *mdid, CMDName *mdname, BOOL is_redistributable,
	BOOL is_fixed_length, ULONG length, BOOL is_passed_by_value,
	IMDId *mdid_distr_opfamily, IMDId *mdid_legacy_distr_opfamily,
	IMDId *mdid_op_eq, IMDId *mdid_op_neq, IMDId *mdid_op_lt,
	IMDId *mdid_op_leq, IMDId *mdid_op_gt, IMDId *mdid_op_geq,
	IMDId *mdid_op_cmp, IMDId *mdid_op_min, IMDId *mdid_op_max,
	IMDId *mdid_op_avg, IMDId *mdid_op_sum, IMDId *mdid_op_count,
	BOOL is_hashable, BOOL is_merge_joinable, BOOL is_composite_type,
	BOOL is_text_related, IMDId *mdid_base_relation, IMDId *mdid_type_array,
	INT gpdb_length)
	: m_mp(mp),
	  m_mdid(mdid),
	  m_mdname(mdname),
	  m_is_redistributable(is_redistributable),
	  m_is_fixed_length(is_fixed_length),
	  m_length(length),
	  m_is_passed_by_value(is_passed_by_value),
	  m_distr_opfamily(mdid_distr_opfamily),
	  m_legacy_distr_opfamily(mdid_legacy_distr_opfamily),
	  m_mdid_op_eq(mdid_op_eq),
	  m_mdid_op_neq(mdid_op_neq),
	  m_mdid_op_lt(mdid_op_lt),
	  m_mdid_op_leq(mdid_op_leq),
	  m_mdid_op_gt(mdid_op_gt),
	  m_mdid_op_geq(mdid_op_geq),
	  m_mdid_op_cmp(mdid_op_cmp),
	  m_mdid_min(mdid_op_min),
	  m_mdid_max(mdid_op_max),
	  m_mdid_avg(mdid_op_avg),
	  m_mdid_sum(mdid_op_sum),
	  m_mdid_count(mdid_op_count),
	  m_is_hashable(is_hashable),
	  m_is_merge_joinable(is_merge_joinable),
	  m_is_composite_type(is_composite_type),
	  m_is_text_related(is_text_related),
	  m_mdid_base_relation(mdid_base_relation),
	  m_mdid_type_array(mdid_type_array),
	  m_gpdb_length(gpdb_length),
	  m_datum_null(NULL)
{
	GPOS_ASSERT_IMP(m_is_fixed_length, 0 < m_length);
	GPOS_ASSERT_IMP(!m_is_fixed_length, 0 > m_gpdb_length);
	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);

	m_mdid->AddRef();
	m_datum_null = GPOS_NEW(m_mp) CDatumGenericGPDB(
		m_mp, m_mdid, default_type_modifier, NULL /*pba*/, 0 /*length*/,
		true /*constNull*/, 0 /*lValue */, 0 /*dValue */);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::~CMDTypeGenericGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDTypeGenericGPDB::~CMDTypeGenericGPDB()
{
	m_mdid->Release();
	CRefCount::SafeRelease(m_distr_opfamily);
	CRefCount::SafeRelease(m_legacy_distr_opfamily);
	m_mdid_op_eq->Release();
	m_mdid_op_neq->Release();
	m_mdid_op_lt->Release();
	m_mdid_op_leq->Release();
	m_mdid_op_gt->Release();
	m_mdid_op_geq->Release();
	m_mdid_op_cmp->Release();
	m_mdid_type_array->Release();
	m_mdid_min->Release();
	m_mdid_max->Release();
	m_mdid_avg->Release();
	m_mdid_sum->Release();
	m_mdid_count->Release();
	CRefCount::SafeRelease(m_mdid_base_relation);
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
	m_datum_null->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::GetMdidForAggType
//
//	@doc:
//		Return mdid of specified aggregate type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeGenericGPDB::GetMdidForAggType(EAggType agg_type) const
{
	switch (agg_type)
	{
		case EaggMin:
			return m_mdid_min;
		case EaggMax:
			return m_mdid_max;
		case EaggAvg:
			return m_mdid_avg;
		case EaggSum:
			return m_mdid_sum;
		case EaggCount:
			return m_mdid_count;
		default:
			GPOS_ASSERT(!"Invalid aggregate type");
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::MDId
//
//	@doc:
//		Returns the metadata id of this type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeGenericGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::Mdname
//
//	@doc:
//		Returns the name of this type
//
//---------------------------------------------------------------------------
CMDName
CMDTypeGenericGPDB::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::GetMdidForCmpType
//
//	@doc:
//		Return mdid of specified comparison operator type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeGenericGPDB::GetMdidForCmpType(ECmpType cmp_type) const
{
	switch (cmp_type)
	{
		case EcmptEq:
			return m_mdid_op_eq;
		case EcmptNEq:
			return m_mdid_op_neq;
		case EcmptL:
			return m_mdid_op_lt;
		case EcmptLEq:
			return m_mdid_op_leq;
		case EcmptG:
			return m_mdid_op_gt;
		case EcmptGEq:
			return m_mdid_op_geq;
		default:
			GPOS_ASSERT(!"Invalid operator type");
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::Serialize
//
//	@doc:
//		Serialize metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDTypeGenericGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	CGPDBTypeHelper<CMDTypeGenericGPDB>::Serialize(xml_serializer, this);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::PGetDatumForDXLConstValdatum
//
//	@doc:
//		Factory method for generating generic datum from CDXLScalarConstValue
//
//---------------------------------------------------------------------------
IDatum *
CMDTypeGenericGPDB::GetDatumForDXLConstVal(
	const CDXLScalarConstValue *dxl_op) const
{
	CDXLDatumGeneric *dxl_datum =
		CDXLDatumGeneric::Cast(const_cast<CDXLDatum *>(dxl_op->GetDatumVal()));
	GPOS_ASSERT(NULL != dxl_op);

	LINT lint_value = 0;
	if (dxl_datum->IsDatumMappableToLINT())
	{
		lint_value = dxl_datum->GetLINTMapping();
	}

	CDouble double_value = 0;
	if (dxl_datum->IsDatumMappableToDouble())
	{
		double_value = dxl_datum->GetDoubleMapping();
	}

	m_mdid->AddRef();
	return GPOS_NEW(m_mp) CDatumGenericGPDB(
		m_mp, m_mdid, dxl_datum->TypeModifier(), dxl_datum->GetByteArray(),
		dxl_datum->Length(), dxl_datum->IsNull(), lint_value, double_value);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::GetDatumForDXLDatum
//
//	@doc:
//		Construct a datum from a DXL datum
//
//---------------------------------------------------------------------------
IDatum *
CMDTypeGenericGPDB::GetDatumForDXLDatum(CMemoryPool *mp,
										const CDXLDatum *dxl_datum) const
{
	m_mdid->AddRef();
	CDXLDatumGeneric *dxl_datum_generic =
		CDXLDatumGeneric::Cast(const_cast<CDXLDatum *>(dxl_datum));

	LINT lint_value = 0;
	if (dxl_datum_generic->IsDatumMappableToLINT())
	{
		lint_value = dxl_datum_generic->GetLINTMapping();
	}

	CDouble double_value = 0;
	if (dxl_datum_generic->IsDatumMappableToDouble())
	{
		double_value = dxl_datum_generic->GetDoubleMapping();
	}

	return GPOS_NEW(m_mp) CDatumGenericGPDB(
		mp, m_mdid, dxl_datum_generic->TypeModifier(),
		dxl_datum_generic->GetByteArray(), dxl_datum_generic->Length(),
		dxl_datum_generic->IsNull(), lint_value, double_value);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::GetDatumVal
//
//	@doc:
// 		Generate dxl datum
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeGenericGPDB::GetDatumVal(CMemoryPool *mp, IDatum *datum) const
{
	m_mdid->AddRef();
	CDatumGenericGPDB *datum_generic = dynamic_cast<CDatumGenericGPDB *>(datum);
	ULONG length = 0;
	BYTE *pba = NULL;
	if (!datum_generic->IsNull())
	{
		pba = datum_generic->MakeCopyOfValue(mp, &length);
	}

	LINT lValue = 0;
	if (datum_generic->IsDatumMappableToLINT())
	{
		lValue = datum_generic->GetLINTMapping();
	}

	CDouble dValue = 0;
	if (datum_generic->IsDatumMappableToDouble())
	{
		dValue = datum_generic->GetDoubleMapping();
	}

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDType *md_type = md_accessor->RetrieveType(m_mdid);
	return CreateDXLDatumVal(mp, m_mdid, md_type, datum_generic->TypeModifier(),
							 datum_generic->IsNull(), pba, length, lValue,
							 dValue);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::IsAmbiguous
//
//	@doc:
// 		Is type an ambiguous one? I.e. a "polymorphic" type in GPDB terms
//
//---------------------------------------------------------------------------
BOOL
CMDTypeGenericGPDB::IsAmbiguous() const
{
	OID oid = CMDIdGPDB::CastMdid(m_mdid)->Oid();
	// This should match the IsPolymorphicType() macro in GPDB's pg_type.h
	return (GPDB_ANYELEMENT_OID == oid || GPDB_ANYARRAY_OID == oid ||
			GPDB_ANYNONARRAY_OID == oid || GPDB_ANYENUM_OID == oid);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::CreateDXLDatumVal
//
//	@doc:
// 		Create a dxl datum
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeGenericGPDB::CreateDXLDatumVal(CMemoryPool *mp, IMDId *mdid,
									  const IMDType *md_type, INT type_modifier,
									  BOOL is_null, BYTE *pba, ULONG length,
									  LINT lValue, CDouble dValue)
{
	GPOS_ASSERT(IMDId::EmdidGeneral == mdid->MdidType());

	if (HasByte2DoubleMapping(mdid))
	{
		return CMDTypeGenericGPDB::CreateDXLDatumStatsDoubleMappable(
			mp, mdid, type_modifier, is_null, pba, length, lValue, dValue);
	}

	if (HasByte2IntMapping(md_type))
	{
		return CMDTypeGenericGPDB::CreateDXLDatumStatsIntMappable(
			mp, mdid, type_modifier, is_null, pba, length, lValue, dValue);
	}

	return GPOS_NEW(mp)
		CDXLDatumGeneric(mp, mdid, type_modifier, is_null, pba, length);
}


//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::CreateDXLDatumStatsDoubleMappable
//
//	@doc:
// 		Create a dxl datum of types that need double mapping
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeGenericGPDB::CreateDXLDatumStatsDoubleMappable(
	CMemoryPool *mp, IMDId *mdid, INT type_modifier, BOOL is_null,
	BYTE *byte_array, ULONG length, LINT, CDouble double_value)
{
	GPOS_ASSERT(CMDTypeGenericGPDB::HasByte2DoubleMapping(mdid));
	return GPOS_NEW(mp) CDXLDatumStatsDoubleMappable(
		mp, mdid, type_modifier, is_null, byte_array, length, double_value);
}


//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::CreateDXLDatumStatsIntMappable
//
//	@doc:
// 		Create a dxl datum of types having lint mapping
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeGenericGPDB::CreateDXLDatumStatsIntMappable(
	CMemoryPool *mp, IMDId *mdid, INT type_modifier, BOOL is_null,
	BYTE *byte_array, ULONG length, LINT lint_value,
	CDouble	 // double_value
)
{
	return GPOS_NEW(mp) CDXLDatumStatsLintMappable(
		mp, mdid, type_modifier, is_null, byte_array, length, lint_value);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::GetDXLOpScConst
//
//	@doc:
// 		Generate a dxl scalar constant from a datum
//
//---------------------------------------------------------------------------
CDXLScalarConstValue *
CMDTypeGenericGPDB::GetDXLOpScConst(CMemoryPool *mp, IDatum *datum) const
{
	CDXLDatum *dxl_datum = GetDatumVal(mp, datum);

	return GPOS_NEW(mp) CDXLScalarConstValue(mp, dxl_datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::GetDXLDatumNull
//
//	@doc:
// 		Generate dxl datum
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeGenericGPDB::GetDXLDatumNull(CMemoryPool *mp) const
{
	m_mdid->AddRef();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDType *md_type = md_accessor->RetrieveType(m_mdid);
	return CreateDXLDatumVal(mp, m_mdid, md_type, default_type_modifier,
							 true /*fConstNull*/, NULL /*byte_array*/,
							 0 /*length*/, 0 /*lint_value */,
							 0 /*double_value */);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::HasByte2IntMapping
//
//	@doc:
//		Does the datum of this type need bytea -> Lint mapping for
//		statistics computation
//---------------------------------------------------------------------------
BOOL
CMDTypeGenericGPDB::HasByte2IntMapping(const IMDType *mdtype)
{
	IMDId *mdid = mdtype->MDId();
	return mdtype->IsTextRelated() || mdid->Equals(&CMDIdGPDB::m_mdid_uuid) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_cash) ||
		   IsTimeRelatedTypeMappableToLint(mdid);
}

IDatum *
CMDTypeGenericGPDB::CreateGenericNullDatum(CMemoryPool *mp,
										   INT type_modifier) const
{
	return GPOS_NEW(mp) CDatumGenericGPDB(mp, MDId(), type_modifier,
										  NULL,	 // source value buffer
										  0,	 // source value buffer length
										  true,	 // is NULL
										  0,	 // LINT mapping for stats
										  0.0);	 // CDouble mapping for stats
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::HasByte2DoubleMapping
//
//	@doc:
//		Does the datum of this type need bytea -> double mapping for
//		statistics computation
//---------------------------------------------------------------------------
BOOL
CMDTypeGenericGPDB::HasByte2DoubleMapping(const IMDId *mdid)
{
	return mdid->Equals(&CMDIdGPDB::m_mdid_numeric) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_float4) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_float8) ||
		   IsTimeRelatedTypeMappableToDouble(mdid) ||
		   IsNetworkRelatedType(mdid);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::IsTimeRelatedType
//
//	@doc:
//		is this a time-related type
//---------------------------------------------------------------------------
BOOL
CMDTypeGenericGPDB::IsTimeRelatedType(const IMDId *mdid)
{
	return mdid->Equals(&CMDIdGPDB::m_mdid_date) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_time) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_timeTz) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_timestamp) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_timestampTz) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_abs_time) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_relative_time) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_interval) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_time_interval);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::IsNetworkRelatedType
//
//	@doc:
//		is this a network-related type
//---------------------------------------------------------------------------
BOOL
CMDTypeGenericGPDB::IsNetworkRelatedType(const IMDId *mdid)
{
	return mdid->Equals(&CMDIdGPDB::m_mdid_inet) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_cidr) ||
		   mdid->Equals(&CMDIdGPDB::m_mdid_macaddr);
}

IMDId *
CMDTypeGenericGPDB::GetDistrOpfamilyMdid() const
{
	if (GPOS_FTRACE(EopttraceUseLegacyOpfamilies))
	{
		return m_legacy_distr_opfamily;
	}
	else
	{
		return m_distr_opfamily;
	}
}

BOOL
CMDTypeGenericGPDB::IsRedistributable() const
{
	if (GPOS_FTRACE(EopttraceConsiderOpfamiliesForDistribution) &&
		GPOS_FTRACE(EopttraceUseLegacyOpfamilies))
	{
		return (NULL != m_legacy_distr_opfamily);
	}
	// If EopttraceConsiderOpfamiliesForDistribution is set, m_is_redistributable
	// is redundant. It's still used here for MDP tests where the traceflag is
	// unset and/or opfamilies are not available for all types.
	return m_is_redistributable;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CMDTypeGenericGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDTypeGenericGPDB::DebugPrint(IOstream &os) const
{
	CGPDBTypeHelper<CMDTypeGenericGPDB>::DebugPrint(os, this);
}

#endif	// GPOS_DEBUG

// EOF
