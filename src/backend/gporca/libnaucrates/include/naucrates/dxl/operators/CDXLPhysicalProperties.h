//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalProperties.h
//
//	@doc:
//		Representation of properties of a physical DXL operator
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLPhysicalProperties_H
#define GPDXL_CDXLPhysicalProperties_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLOperatorCost.h"
#include "naucrates/dxl/operators/CDXLProperties.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalProperties
//
//	@doc:
//		Container for the properties of a physical operator node, such as
//		cost and output columns
//
//---------------------------------------------------------------------------
class CDXLPhysicalProperties : public CDXLProperties
{
private:
	// cost estimate
	CDXLOperatorCost *m_operator_cost_dxl;

public:
	CDXLPhysicalProperties(const CDXLPhysicalProperties &) = delete;

	// ctor
	explicit CDXLPhysicalProperties(CDXLOperatorCost *cost);

	// dtor
	~CDXLPhysicalProperties() override;

	// serialize properties in DXL format
	void SerializePropertiesToDXL(
		CXMLSerializer *xml_serializer) const override;

	// accessors
	// the cost estimates for the operator node
	CDXLOperatorCost *GetDXLOperatorCost() const;

	Edxlproperty
	GetDXLPropertyType() const override
	{
		return EdxlpropertyPhysical;
	}

	// conversion function
	static CDXLPhysicalProperties *
	PdxlpropConvert(CDXLProperties *dxl_properties)
	{
		GPOS_ASSERT(nullptr != dxl_properties);
		GPOS_ASSERT(EdxlpropertyPhysical ==
					dxl_properties->GetDXLPropertyType());
		return dynamic_cast<CDXLPhysicalProperties *>(dxl_properties);
	}
};

}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalProperties_H

// EOF
