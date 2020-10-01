//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		init.h
//
//	@doc:
//		Initialization of the gpdxl library.
//---------------------------------------------------------------------------*/
#ifndef GPDXL_init_H
#define GPDXL_init_H

#ifdef __cplusplus
extern "C" {
#endif	// __cplusplus


// initialize DXL library support
void InitDXL();

// shutdown DXL library support
void ShutdownDXL();

// initialize Xerces parser utils
void gpdxl_init(void);

// terminate Xerces parser utils
void gpdxl_terminate(void);

#ifdef __cplusplus
}
#endif	// __cplusplus

#endif	// !GPDXL_init_H


// EOF
