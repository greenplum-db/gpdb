//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatum.h
//
//	@doc:
//		Class for representing DXL datums
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLDatum_H
#define GPDXL_CDXLDatum_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/string/CWStringConst.h"

#include "naucrates/md/IMDId.h"
namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
// fwd decl
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLDatum
//
//	@doc:
//		Class for representing DXL datums
//
//---------------------------------------------------------------------------
class CDXLDatum : public CRefCount
{
private:
	// private copy ctor
	CDXLDatum(const CDXLDatum &);

protected:
	// memory pool
	CMemoryPool *m_mp;

	// mdid of the datum's type
	IMDId *m_mdid_type;

	const INT m_type_modifier;

	// is the datum NULL
	BOOL m_is_null;

	// length
	const ULONG m_length;

public:
	// datum types
	enum EdxldatumType
	{
		EdxldatumInt2,
		EdxldatumInt4,
		EdxldatumInt8,
		EdxldatumBool,
		EdxldatumGeneric,
		EdxldatumStatsDoubleMappable,
		EdxldatumStatsLintMappable,
		EdxldatumOid,
		EdxldatumSentinel
	};
	// ctor
	CDXLDatum(CMemoryPool *mp, IMDId *mdid_type, INT type_modifier,
			  BOOL is_null, ULONG length);

	// dtor
	virtual ~CDXLDatum()
	{
		m_mdid_type->Release();
	}

	// mdid type of the datum
	virtual IMDId *
	MDId() const
	{
		return m_mdid_type;
	}

	INT TypeModifier() const;

	// is datum NULL
	virtual BOOL IsNull() const;

	// byte array length
	virtual ULONG Length() const;

	// serialize the datum as the given element
	virtual void Serialize(CXMLSerializer *xml_serializer,
						   const CWStringConst *datum_string);

	// serialize the datum as the given element
	virtual void Serialize(CXMLSerializer *xml_serializer) = 0;

	// ident accessors
	virtual EdxldatumType GetDatumType() const = 0;
};

// array of datums
typedef CDynamicPtrArray<CDXLDatum, CleanupRelease> CDXLDatumArray;

// dynamic array of datum arrays -- array owns elements
typedef CDynamicPtrArray<CDXLDatumArray, CleanupRelease> CDXLDatum2dArray;
}  // namespace gpdxl

#endif	// !GPDXL_CDXLDatum_H

// EOF
