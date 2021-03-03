//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalSetOp.h
//
//	@doc:
//		Class for representing DXL logical set operators
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLLogicalSetOp_H
#define GPDXL_CDXLLogicalSetOp_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLColDescr.h"
#include "naucrates/dxl/operators/CDXLLogical.h"

namespace gpdxl
{
enum EdxlSetOpType
{
	EdxlsetopUnion,
	EdxlsetopUnionAll,
	EdxlsetopIntersect,
	EdxlsetopIntersectAll,
	EdxlsetopDifference,
	EdxlsetopDifferenceAll,
	EdxlsetopSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalSetOp
//
//	@doc:
//		Class for representing DXL logical set operators
//
//---------------------------------------------------------------------------
class CDXLLogicalSetOp : public CDXLLogical
{
private:
	// set operation type
	EdxlSetOpType m_set_operation_dxl_type;

	// list of output column descriptors
	CDXLColDescrArray *m_col_descr_array;

	// array of input colid arrays
	ULongPtr2dArray *m_input_colids_arrays;

	// do the columns need to be casted accross inputs
	BOOL m_cast_across_input_req;

public:
	CDXLLogicalSetOp(CDXLLogicalSetOp &) = delete;

	// ctor
	CDXLLogicalSetOp(CMemoryPool *mp, EdxlSetOpType edxlsetoptype,
					 CDXLColDescrArray *pdrgdxlcd, ULongPtr2dArray *array_2D,
					 BOOL fCastAcrossInput);

	// dtor
	~CDXLLogicalSetOp() override;

	// operator id
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// set operator type
	EdxlSetOpType
	GetSetOpType() const
	{
		return m_set_operation_dxl_type;
	}

	// array of output columns
	const CDXLColDescrArray *
	GetDXLColumnDescrArray() const
	{
		return m_col_descr_array;
	}

	// number of output columns
	ULONG
	Arity() const
	{
		return m_col_descr_array->Size();
	}

	// output column descriptor at a given position
	const CDXLColDescr *
	GetColumnDescrAt(ULONG idx) const
	{
		return (*m_col_descr_array)[idx];
	}

	// number of inputs to the n-ary set operation
	ULONG
	ChildCount() const
	{
		return m_input_colids_arrays->Size();
	}

	// column array of the input at a given position
	const ULongPtrArray *
	GetInputColIdArrayAt(ULONG idx) const
	{
		GPOS_ASSERT(idx < ChildCount());

		return (*m_input_colids_arrays)[idx];
	}

	// do the columns across inputs need to be casted
	BOOL
	IsCastAcrossInputReq() const
	{
		return m_cast_across_input_req;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// check if given column is defined by operator
	BOOL IsColDefined(ULONG colid) const override;

	// conversion function
	static CDXLLogicalSetOp *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopLogicalSetOp == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalSetOp *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalSetOp_H

// EOF
