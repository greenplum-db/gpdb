//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		init.h
//
//	@doc:
//		Initialization of the gpopt library.
//---------------------------------------------------------------------------
#ifndef GPOPT_init_H
#define GPOPT_init_H

#ifdef __cplusplus
extern "C" {
#endif	// __cplusplus

// initialize gpopt library
void gpopt_init();


// terminate gpopt library
void gpopt_terminate(void);

#ifdef __cplusplus
}
#endif	// __cplusplus

#endif	// !GPOPT_init_H


// EOF
