//---------------------------------------------------------------------------
//
// funcs.h
//    API for invoking optimizer using GPDB udfs
//
// Copyright (c) 2019-Present Pivotal Software, Inc.
//
//---------------------------------------------------------------------------

#ifndef GPOPT_funcs_H
#define GPOPT_funcs_H


extern "C" {

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"

extern Datum DisableXform(PG_FUNCTION_ARGS);
extern Datum EnableXform(PG_FUNCTION_ARGS);
extern Datum LibraryVersion();
}

#endif	// GPOPT_funcs_H
