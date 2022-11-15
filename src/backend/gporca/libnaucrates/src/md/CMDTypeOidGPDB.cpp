//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDTypeOidGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing GPDB-specific OID type in
//		the MD cache
//---------------------------------------------------------------------------

#include "naucrates/md/CMDTypeOidGPDB.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/base/CDatumOidGPDB.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLDatum.h"
#include "naucrates/dxl/operators/CDXLDatumOid.h"
#include "naucrates/dxl/operators/CDXLScalarConstValue.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

// static member initialization
CWStringConst CMDTypeOidGPDB::m_str = CWStringConst(GPOS_WSZ_LIT("oid"));
CMDName CMDTypeOidGPDB::m_mdname(&m_str);

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::CMDTypeOidGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDTypeOidGPDB::CMDTypeOidGPDB(CMemoryPool *mp) : m_mp(mp)
{
	m_mdid = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_OID);
	if (GPOS_FTRACE(EopttraceConsiderOpfamiliesForDistribution))
	{
		m_distr_opfamily =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_OPFAMILY);
		m_legacy_distr_opfamily = GPOS_NEW(mp)
			CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_LEGACY_OPFAMILY);
	}
	else
	{
		m_distr_opfamily = NULL;
		m_legacy_distr_opfamily = NULL;
	}
	m_mdid_op_eq = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_EQ_OP);
	m_mdid_op_neq =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_NEQ_OP);
	m_mdid_op_lt = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_LT_OP);
	m_mdid_op_leq =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_LEQ_OP);
	m_mdid_op_gt = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_GT_OP);
	m_mdid_op_geq =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_GEQ_OP);
	m_mdid_op_cmp =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_COMP_OP);
	m_mdid_type_array =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_ARRAY_TYPE);

	m_mdid_min = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_AGG_MIN);
	m_mdid_max = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_AGG_MAX);
	m_mdid_avg = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_AGG_AVG);
	m_mdid_sum = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_AGG_SUM);
	m_mdid_count =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_OID_AGG_COUNT);

	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);

	GPOS_ASSERT(GPDB_OID_OID == CMDIdGPDB::CastMdid(m_mdid)->Oid());
	m_mdid->AddRef();
	m_datum_null =
		GPOS_NEW(mp) CDatumOidGPDB(m_mdid, 1 /* value */, true /* is_null */);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::~CMDTypeOidGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDTypeOidGPDB::~CMDTypeOidGPDB()
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
	m_datum_null->Release();

	GPOS_DELETE(m_dxl_str);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::GetDatum
//
//	@doc:
//		Factory function for creating OID datums
//
//---------------------------------------------------------------------------
IDatumOid *
CMDTypeOidGPDB::CreateOidDatum(CMemoryPool *mp, OID oValue, BOOL is_null) const
{
	return GPOS_NEW(mp) CDatumOidGPDB(m_mdid->Sysid(), oValue, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::MDId
//
//	@doc:
//		Returns the metadata id of this type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeOidGPDB::MDId() const
{
	return m_mdid;
}

IMDId *
CMDTypeOidGPDB::GetDistrOpfamilyMdid() const
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

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::Mdname
//
//	@doc:
//		Returns the name of this type
//
//---------------------------------------------------------------------------
CMDName
CMDTypeOidGPDB::Mdname() const
{
	return CMDTypeOidGPDB::m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::GetMdidForCmpType
//
//	@doc:
//		Return mdid of specified comparison operator type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeOidGPDB::GetMdidForCmpType(ECmpType cmp_type) const
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
//		CMDTypeOidGPDB::GetMdidForAggType
//
//	@doc:
//		Return mdid of specified aggregate type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeOidGPDB::GetMdidForAggType(EAggType agg_type) const
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
//		CMDTypeOidGPDB::Serialize
//
//	@doc:
//		Serialize relation metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDTypeOidGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	CGPDBTypeHelper<CMDTypeOidGPDB>::Serialize(xml_serializer, this);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::GetDatumForDXLConstVal
//
//	@doc:
//		Transformation method for generating oid datum from CDXLScalarConstValue
//
//---------------------------------------------------------------------------
IDatum *
CMDTypeOidGPDB::GetDatumForDXLConstVal(const CDXLScalarConstValue *dxl_op) const
{
	CDXLDatumOid *dxl_datum =
		CDXLDatumOid::Cast(const_cast<CDXLDatum *>(dxl_op->GetDatumVal()));

	return GPOS_NEW(m_mp) CDatumOidGPDB(m_mdid->Sysid(), dxl_datum->OidValue(),
										dxl_datum->IsNull());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::GetDatumForDXLDatum
//
//	@doc:
//		Construct an oid datum from a DXL datum
//
//---------------------------------------------------------------------------
IDatum *
CMDTypeOidGPDB::GetDatumForDXLDatum(CMemoryPool *mp,
									const CDXLDatum *dxl_datum) const
{
	CDXLDatumOid *dxl_datumOid =
		CDXLDatumOid::Cast(const_cast<CDXLDatum *>(dxl_datum));
	OID oid_value = dxl_datumOid->OidValue();
	BOOL is_null = dxl_datumOid->IsNull();

	return GPOS_NEW(mp) CDatumOidGPDB(m_mdid->Sysid(), oid_value, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::GetDatumVal
//
//	@doc:
// 		Generate dxl datum
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeOidGPDB::GetDatumVal(CMemoryPool *mp, IDatum *datum) const
{
	m_mdid->AddRef();
	CDatumOidGPDB *oid_datum = dynamic_cast<CDatumOidGPDB *>(datum);

	return GPOS_NEW(mp)
		CDXLDatumOid(mp, m_mdid, oid_datum->IsNull(), oid_datum->OidValue());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::GetDXLOpScConst
//
//	@doc:
// 		Generate a dxl scalar constant from a datum
//
//---------------------------------------------------------------------------
CDXLScalarConstValue *
CMDTypeOidGPDB::GetDXLOpScConst(CMemoryPool *mp, IDatum *datum) const
{
	CDatumOidGPDB *datum_oidGPDB = dynamic_cast<CDatumOidGPDB *>(datum);

	m_mdid->AddRef();
	CDXLDatumOid *dxl_datum = GPOS_NEW(mp) CDXLDatumOid(
		mp, m_mdid, datum_oidGPDB->IsNull(), datum_oidGPDB->OidValue());

	return GPOS_NEW(mp) CDXLScalarConstValue(mp, dxl_datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::GetDXLDatumNull
//
//	@doc:
// 		Generate dxl datum
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeOidGPDB::GetDXLDatumNull(CMemoryPool *mp) const
{
	m_mdid->AddRef();

	return GPOS_NEW(mp) CDXLDatumOid(mp, m_mdid, true /*is_null*/, 1);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CMDTypeOidGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDTypeOidGPDB::DebugPrint(IOstream &os) const
{
	CGPDBTypeHelper<CMDTypeOidGPDB>::DebugPrint(os, this);
}

#endif	// GPOS_DEBUG

// EOF
