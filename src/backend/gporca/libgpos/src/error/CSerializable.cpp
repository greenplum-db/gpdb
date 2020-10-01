//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CSerializable.cpp
//
//	@doc:
//		Interface for serializable objects
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/error/CSerializable.h"
#include "gpos/task/CTask.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CSerializable::CSerializable
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CSerializable::CSerializable()
{
	CTask *task = CTask::Self();

	GPOS_ASSERT(NULL != task);

	task->ConvertErrCtxt()->Register(this);
}


//---------------------------------------------------------------------------
//	@function:
//		CSerializable::~CSerializable
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CSerializable::~CSerializable()
{
	CTask *task = CTask::Self();

	GPOS_ASSERT(NULL != task);

	task->ConvertErrCtxt()->Unregister(this);
}


// EOF
