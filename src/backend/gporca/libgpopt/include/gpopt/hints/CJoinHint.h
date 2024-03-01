//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2024 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CJoinHint.h
//
//	@doc:
//		CJoinHint represents an ORCA optimizer hint that specifies an order to
//		join on a set of table(s) and/or alias(es). There are 2 syntaxes:
//		direction and direction-less.
//
//	Example (direction):
//		/*+ Leading((t3 (t2 t1))) */ SELECT * FROM t1, t2, t3;
//
//		Hint specifies that t2 and t1 are joined first where t2 is the outer
//		side and t1 is inner side. Then the result is joined with t3 where t3
//		is on the outer side.
//
//		DXL format:
//			<dxl:JoinHint Alias="t1,t2,t3" Leading="(t3 (t2 t1))"/>
//
//	Example (direction-less):
//		/*+ Leading(t3 t2 t1) */ SELECT * FROM t1, t2, t3;
//
//		Hint specifies that t3 and t2 are joined first and may be on either of
//		the join. Then the result is joined with t1 where t1 is on either side
//		of the join.
//
//		DXL format:
//			<dxl:JoinHint Alias="t1,t2,t3" Leading="t3 t2 t1"/>
//---------------------------------------------------------------------------
#ifndef GPOS_CJoinHint_H
#define GPOS_CJoinHint_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/exception.h"
#include "gpopt/hints/IHint.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/COperator.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

namespace gpopt
{
using CWStringConstHashSet =
	CHashSet<const CWStringConst, CWStringConst::HashValue,
			 CWStringConst::Equals, CleanupNULL<const CWStringConst>>;

using CWStringConstHashSetIter =
	CHashSetIter<const CWStringConst, CWStringConst::HashValue,
				 CWStringConst::Equals, CleanupNULL<const CWStringConst>>;

class CJoinHint : public IHint, public DbgPrintMixin<CJoinHint>
{
public:
	// JoinPair represent join order over a set of relations
	class JoinPair : public CRefCount, public DbgPrintMixin<JoinPair>
	{
	private:
		// alias or table name. null if non-leaf node.
		CWStringConst *m_name{nullptr};

		// outer join pair. null if leaf node.
		JoinPair *m_outer{nullptr};

		// inner join pair. null if leaf node.
		JoinPair *m_inner{nullptr};

		// directed "true" means that the inner/outer sides cannot be swapped.
		bool m_is_directed{false};

	public:
		JoinPair(CWStringConst *name) : m_name(name)
		{
		}
		JoinPair(JoinPair *outer, JoinPair *inner, bool is_directed)
			: m_outer(outer), m_inner(inner), m_is_directed(is_directed)
		{
		}

		~JoinPair() override
		{
			GPOS_DELETE(m_name);

			CRefCount::SafeRelease(m_outer);
			CRefCount::SafeRelease(m_inner);
		}

		CWStringConst *
		GetName() const
		{
			return m_name;
		}

		JoinPair *
		GetOuter() const
		{
			return m_outer;
		}

		JoinPair *
		GetInner() const
		{
			return m_inner;
		}

		bool
		IsDirected() const
		{
			return m_is_directed;
		}

		IOstream &OsPrint(IOstream &os) const;
	};

private:
	CMemoryPool *m_mp;

	// stores specified join order
	JoinPair *m_join_pair;

	// sorted list of alias names.
	StringPtrArray *m_aliases;

public:
	CJoinHint(CMemoryPool *mp, JoinPair *join_pair);

	~CJoinHint() override
	{
		m_join_pair->Release();
		m_aliases->Release();
	}

	const StringPtrArray *GetAliasNames() const;

	const CJoinHint::JoinPair *
	GetJoinPair() const
	{
		return m_join_pair;
	}

	IOstream &OsPrint(IOstream &os) const;

	void Serialize(CXMLSerializer *xml_serializer) const;
};

using JoinHintList = CDynamicPtrArray<CJoinHint, CleanupRelease>;

}  // namespace gpopt

#endif	// !GPOS_CJoinHint_H

// EOF
