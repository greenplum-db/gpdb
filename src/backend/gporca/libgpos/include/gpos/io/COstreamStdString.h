//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (c) 2020 VMware, Inc. or its affiliates
//---------------------------------------------------------------------------
#ifndef COstreamStdString_H
#define COstreamStdString_H

#include <sstream>
#include <string>

#include "gpos/io/COstreamBasic.h"

namespace gpos
{
class COstreamStdString : public gpos::COstreamBasic
{
public:
	COstreamStdString();
	std::wstring Str() const;

private:
	std::wostringstream m_oss;
};
}  // namespace gpos

#endif
