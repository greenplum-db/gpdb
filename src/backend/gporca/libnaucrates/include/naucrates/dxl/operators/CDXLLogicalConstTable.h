//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDXLLogicalConstTable.h
//
//	@doc:
//		Class for representing DXL logical constant tables
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalConstTable_H
#define GPDXL_CDXLLogicalConstTable_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLLogical.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalConstTable
//
//	@doc:
//		Class for representing DXL logical const table operators
//
//---------------------------------------------------------------------------
class CDXLLogicalConstTable : public CDXLLogical
{
private:
	// list of column descriptors
	CDXLColDescrArray *m_col_descr_array;

	// array of datum arrays (const tuples)
	CDXLDatum2dArray *m_const_tuples_datum_array;

	// private copy ctor
	CDXLLogicalConstTable(CDXLLogicalConstTable &);

public:
	// ctor
	CDXLLogicalConstTable(CMemoryPool *mp,
						  CDXLColDescrArray *dxl_col_descr_array,
						  CDXLDatum2dArray *pdrgpdrgpdxldatum);

	//dtor
	virtual ~CDXLLogicalConstTable();

	// accessors

	// operator type
	Edxlopid GetDXLOperator() const;

	// operator name
	const CWStringConst *GetOpNameStr() const;

	// column descriptors
	const CDXLColDescrArray *
	GetDXLColumnDescrArray() const
	{
		return m_col_descr_array;
	}

	// return the column descriptor at a given position
	CDXLColDescr *GetColumnDescrAt(ULONG ul) const;

	// number of columns
	ULONG Arity() const;

	// number of constant tuples
	ULONG
	GetConstTupleCount() const
	{
		return m_const_tuples_datum_array->Size();
	}

	// return the const tuple (datum array) at a given position
	const CDXLDatumArray *
	GetConstTupleDatumArrayAt(ULONG ulTuplePos) const
	{
		return (*m_const_tuples_datum_array)[ulTuplePos];
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// check if given column is defined by operator
	virtual BOOL IsColDefined(ULONG colid) const;

	// conversion function
	static CDXLLogicalConstTable *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalConstTable == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalConstTable *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalConstTable_H

// EOF
