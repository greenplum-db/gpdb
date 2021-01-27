//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLProperties.h
//
//	@doc:
//		Representation of properties of a DXL operator node such as stats
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLProperties_H
#define GPDXL_CDXLProperties_H

#include "gpos/base.h"

#include "naucrates/md/CDXLStatsDerivedRelation.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

enum Edxlproperty
{
	EdxlpropertyLogical,
	EdxlpropertyPhysical,
	EdxlpropertySentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLProperties
//
//	@doc:
//		Container for the properties of an operator node, such as stats
//
//---------------------------------------------------------------------------
class CDXLProperties : public CRefCount
{
private:
	// derived statistics
	CDXLStatsDerivedRelation *m_dxl_stats_derived_relation;

	// private copy ctor
	CDXLProperties(const CDXLProperties &);

protected:
	// serialize statistics in DXL format
	void SerializeStatsToDXL(CXMLSerializer *xml_serializer) const;

public:
	// ctor
	explicit CDXLProperties();

	//dtor
	virtual ~CDXLProperties();

	// setter
	virtual void SetStats(CDXLStatsDerivedRelation *dxl_stats_derived_relation);

	// statistical information
	virtual const CDXLStatsDerivedRelation *GetDxlStatsDrvdRelation() const;

	virtual Edxlproperty
	GetDXLPropertyType() const
	{
		return EdxlpropertyLogical;
	}

	// serialize properties in DXL format
	virtual void SerializePropertiesToDXL(CXMLSerializer *xml_serializer) const;
};

}  // namespace gpdxl

#endif	// !GPDXL_CDXLProperties_H

// EOF
