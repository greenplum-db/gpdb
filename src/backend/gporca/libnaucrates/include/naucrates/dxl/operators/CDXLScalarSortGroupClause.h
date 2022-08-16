//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarSortGroupClause.h
//
//	@doc:
//		Class for representing sort group clause.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLScalarSortGroupClause_H
#define GPDXL_CDXLScalarSortGroupClause_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

namespace gpdxl
{
using namespace gpmd;
using namespace gpos;

// fwd decl
class CXMLSerializer;
class CDXLScalarSortGroupClause;

// arrays of column references
using CDXLScalarSortGroupClauseArray =
	CDynamicPtrArray<CDXLScalarSortGroupClause, CleanupRelease>;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSortGroupClause
//
//	@doc:
//		Class for representing references to columns in DXL trees
//
//---------------------------------------------------------------------------
class CDXLScalarSortGroupClause : public CDXLScalar
{
private:
	INT m_tle_sort_group_ref;
	INT m_eqop;
	INT m_sortop;
	BOOL m_nulls_first;
	BOOL m_hashable;

public:
	CDXLScalarSortGroupClause(const CDXLScalarSortGroupClause &) = delete;

	// ctor/dtor
	CDXLScalarSortGroupClause(CMemoryPool *mp, INT tle_sort_group_ref, INT eqop,
							  INT sortop, BOOL nulls_first, BOOL hashable)
		: CDXLScalar(mp),
		  m_tle_sort_group_ref(tle_sort_group_ref),
		  m_eqop(eqop),
		  m_sortop(sortop),
		  m_nulls_first(nulls_first),
		  m_hashable(hashable)
	{
	}

	// accessors
	INT
	Index() const
	{
		return m_tle_sort_group_ref;
	}

	INT
	EqOp() const
	{
		return m_eqop;
	}

	INT
	SortOp() const
	{
		return m_sortop;
	}

	BOOL
	NullsFirst() const
	{
		return m_nulls_first;
	}

	BOOL
	IsHashable() const
	{
		return m_hashable;
	}

	static CDXLScalarSortGroupClause *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarSortGroupClause == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSortGroupClause *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *) const override
	{
		return false;
	}

#ifdef GPOS_DEBUG
	void
	AssertValid(const CDXLNode *, BOOL) const override
	{
	}
#endif	// GPOS_DEBUG

	Edxlopid
	GetDXLOperator() const override
	{
		return EdxlopScalarSortGroupClause;
	}

	const CWStringConst *
	GetOpNameStr() const override
	{
		return CDXLTokens::GetDXLTokenStr(EdxltokenScalarSortGroupClause);
	}

	void
	SerializeToDXL(CXMLSerializer *xml_serializer,
				   const CDXLNode *) const override
	{
		const CWStringConst *element_name = GetOpNameStr();

		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenIndex),
									 Index());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeEqOp), EqOp());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenSortOpId), SortOp());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenSortNullsFirst), NullsFirst());
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeHashable), IsHashable());

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	}
};
}  // namespace gpdxl



#endif	// !GPDXL_CDXLScalarSortGroupClause_H

// EOF
