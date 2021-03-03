//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDPartConstraintGPDB.h
//
//	@doc:
//		Class representing a GPDB partition constraint in the MD cache
//---------------------------------------------------------------------------

#ifndef GPMD_CMDPartConstraintGPDB_H
#define GPMD_CMDPartConstraintGPDB_H

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDName.h"
#include "naucrates/md/IMDPartConstraint.h"

// fwd decl
namespace gpdxl
{
class CXMLSerializer;
}

namespace gpopt
{
class CExpression;
class CMDAccessor;
}  // namespace gpopt

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CMDPartConstraintGPDB
//
//	@doc:
//		Class representing a GPDB partition constraint in the MD cache
//
//---------------------------------------------------------------------------
class CMDPartConstraintGPDB : public IMDPartConstraint
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// included default partitions
	ULongPtrArray *m_level_with_default_part_array;

	// is constraint unbounded
	BOOL m_is_unbounded;

	// the DXL representation of the part constraint
	CDXLNode *m_dxl_node;

public:
	// ctor
	CMDPartConstraintGPDB(CMemoryPool *mp,
						  ULongPtrArray *level_with_default_part_array,
						  BOOL is_unbounded, CDXLNode *dxlnode);

	// dtor
	~CMDPartConstraintGPDB() override;

	// serialize constraint in DXL format
	void Serialize(CXMLSerializer *xml_serializer) const override;

	// the scalar expression of the part constraint
	CExpression *GetPartConstraintExpr(
		CMemoryPool *mp, CMDAccessor *md_accessor,
		CColRefArray *colref_array) const override;

	// included default partitions
	ULongPtrArray *GetDefaultPartsArray() const override;

	// is constraint unbounded
	BOOL IsConstraintUnbounded() const override;
};
}  // namespace gpmd

#endif	// !GPMD_CMDPartConstraintGPDB_H

// EOF
