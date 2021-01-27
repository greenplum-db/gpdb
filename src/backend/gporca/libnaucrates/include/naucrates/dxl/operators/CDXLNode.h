//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CDXLNode.h
//
//	@doc:
//		Basic tree/DAG-based representation for DXL tree nodes
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLNode_H
#define GPDXL_CDXLNode_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CHashMap.h"

#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/operators/CDXLProperties.h"

namespace gpdxl
{
using namespace gpos;

// fwd decl
class CDXLNode;
class CDXLOperator;
class CXMLSerializer;
class CDXLDirectDispatchInfo;

typedef CDynamicPtrArray<CDXLNode, CleanupRelease> CDXLNodeArray;

// arrays of OID
typedef CDynamicPtrArray<OID, CleanupDelete> OidArray;

typedef CHashMap<ULONG, CDXLNode, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
				 CleanupDelete<ULONG>, CleanupRelease<CDXLNode> >
	IdToCDXLNodeMap;

//---------------------------------------------------------------------------
//	@class:
//		CDXLNode
//
//	@doc:
//		Class for representing nodes in a DXL tree.
//		Each node specifies an operator at that node, and has an array of children nodes.
//
//---------------------------------------------------------------------------
class CDXLNode : public CRefCount
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// dxl tree operator class
	CDXLOperator *m_dxl_op;

	// properties of the operator
	CDXLProperties *m_dxl_properties;

	// array of children
	CDXLNodeArray *m_dxl_array;

	// direct dispatch spec
	CDXLDirectDispatchInfo *m_direct_dispatch_info;

	// private copy ctor
	CDXLNode(const CDXLNode &);

public:
	// ctors

	explicit CDXLNode(CMemoryPool *mp);
	CDXLNode(CMemoryPool *mp, CDXLOperator *dxl_op);
	CDXLNode(CMemoryPool *mp, CDXLOperator *dxl_op, CDXLNode *child_dxlnode);
	CDXLNode(CMemoryPool *mp, CDXLOperator *dxl_op,
			 CDXLNode *first_child_dxlnode, CDXLNode *second_child_dxlnode);
	CDXLNode(CMemoryPool *mp, CDXLOperator *dxl_op,
			 CDXLNode *first_child_dxlnode, CDXLNode *second_child_dxlnode,
			 CDXLNode *third_child_dxlnode);
	CDXLNode(CMemoryPool *mp, CDXLOperator *dxl_op, CDXLNodeArray *dxl_array);

	// dtor
	virtual ~CDXLNode();

	// shorthand to access children
	inline CDXLNode *
	operator[](ULONG idx) const
	{
		GPOS_ASSERT(NULL != m_dxl_array);
		CDXLNode *dxl_node = (*m_dxl_array)[idx];
		GPOS_ASSERT(NULL != dxl_node);
		return dxl_node;
	};

	// arity function, returns the number of children this node has
	inline ULONG
	Arity() const
	{
		return (m_dxl_array == NULL) ? 0 : m_dxl_array->Size();
	}

	// accessor for operator
	inline CDXLOperator *
	GetOperator() const
	{
		return m_dxl_op;
	}

	// return properties
	CDXLProperties *
	GetProperties() const
	{
		return m_dxl_properties;
	}

	// accessor for children nodes
	const CDXLNodeArray *
	GetChildDXLNodeArray() const
	{
		return m_dxl_array;
	}

	// accessor to direct dispatch info
	CDXLDirectDispatchInfo *
	GetDXLDirectDispatchInfo() const
	{
		return m_direct_dispatch_info;
	}

	// setters
	void AddChild(CDXLNode *child_dxlnode);

	void SetOperator(CDXLOperator *dxl_op);

	void SerializeToDXL(CXMLSerializer *) const;

	// replace a given child of this DXL node with the given node
	void ReplaceChild(ULONG idx, CDXLNode *child_dxlnode);

	void SerializeChildrenToDXL(CXMLSerializer *xml_serializer) const;

	// setter
	void SetProperties(CDXLProperties *dxl_properties);

	// setter for direct dispatch info
	void SetDirectDispatchInfo(
		CDXLDirectDispatchInfo *dxl_direct_dispatch_info);

	// serialize properties in DXL format
	void SerializePropertiesToDXL(CXMLSerializer *xml_serializer) const;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(BOOL validate_children) const;
#endif	// GPOS_DEBUG

};	// class CDXLNode
}  // namespace gpdxl


#endif	// !GPDXL_CDXLNode_H

// EOF
