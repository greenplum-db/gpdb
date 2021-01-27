//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalMaterialize.h
//
//	@doc:
//		Class for representing DXL physical materialize operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalMaterialize_H
#define GPDXL_CDXLPhysicalMaterialize_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLSpoolInfo.h"


namespace gpdxl
{
// indices of materialize elements in the children array
enum Edxlmaterialize
{
	EdxlmatIndexProjList = 0,
	EdxlmatIndexFilter,
	EdxlmatIndexChild,
	EdxlmatIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalMaterialize
//
//	@doc:
//		Class for representing DXL materialize operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalMaterialize : public CDXLPhysical
{
private:
	// eager materialization
	BOOL m_is_eager;

	// spool info
	// id of the spooling operator
	ULONG m_spooling_op_id;

	// type of the underlying spool
	Edxlspooltype m_spool_type;

	// slice executing the underlying sort or materialize
	INT m_executor_slice;

	// number of consumers in case the materialize is a spooling operator
	ULONG m_num_consumer_slices;

	// private copy ctor
	CDXLPhysicalMaterialize(CDXLPhysicalMaterialize &);

public:
	// ctor/dtor
	CDXLPhysicalMaterialize(CMemoryPool *mp, BOOL is_eager);

	CDXLPhysicalMaterialize(CMemoryPool *mp, BOOL is_eager,
							ULONG spooling_op_id, INT executor_slice,
							ULONG num_consumer_slices);

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;
	ULONG GetSpoolingOpId() const;
	INT GetExecutorSlice() const;
	ULONG GetNumConsumerSlices() const;

	// is the operator spooling to other operators
	BOOL IsSpooling() const;

	// does the operator do eager materialization
	BOOL IsEager() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLPhysicalMaterialize *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalMaterialize == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalMaterialize *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalMaterialize_H

// EOF
