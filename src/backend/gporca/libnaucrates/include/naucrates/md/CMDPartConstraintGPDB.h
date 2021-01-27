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
	virtual ~CMDPartConstraintGPDB();

	// serialize constraint in DXL format
	virtual void Serialize(CXMLSerializer *xml_serializer) const;

	// the scalar expression of the part constraint
	virtual CExpression *GetPartConstraintExpr(
		CMemoryPool *mp, CMDAccessor *md_accessor,
		CColRefArray *colref_array) const;

	// included default partitions
	virtual ULongPtrArray *GetDefaultPartsArray() const;

	// is constraint unbounded
	virtual BOOL IsConstraintUnbounded() const;
};
}  // namespace gpmd

#endif	// !GPMD_CMDPartConstraintGPDB_H

// EOF
