//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    macros.h
//
//  @doc:
//    Utility macros
//
//  @test:
//
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_MACROS_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_MACROS_H_

#define DISALLOW_COPY_AND_ASSIGN(classname)   \
  classname(const classname& orig) = delete;  \
  classname& operator=(const classname &orig) = delete

#endif  // GPCODEGEN_MACROS_H_
// EOF
