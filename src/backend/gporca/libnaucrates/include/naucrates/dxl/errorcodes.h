//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		errorcodes.h
//
//	@doc:
//		Enum of errorcodes
//---------------------------------------------------------------------------
#ifndef GPOPT_errorcodes_H
#define GPOPT_errorcodes_H

namespace gpdxl
{
enum EErrorCode
{
	EerrcNotNullViolation,
	EerrcCheckConstraintViolation,
	EerrcTest,
	EerrcSentinel
};
}

#endif	// ! GPOPT_errorcodes_H

// EOF
