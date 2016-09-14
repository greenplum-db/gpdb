//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    pg_arith_func_generator.h
//
//  @doc:
//    Class with Static member function to generate code for +, - and * operator
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_PG_ARITH_FUNC_GENERATOR_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_PG_ARITH_FUNC_GENERATOR_H_

#include <string>
#include <vector>
#include <memory>

#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/pg_func_generator_interface.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */


namespace gpcodegen_ArithOp_detail {
// ArithOpOverFlowErrorMsg has various template specializations to
// handle error message for different C++ types. The specialized versions
// have a static method OverFlowErrMsg() that returns an overflow error message
// based on CppType as const char*
template <typename CppType, typename Enable = void>
class ArithOpOverFlowErrorMsg {};

template <typename IntType>
class ArithOpOverFlowErrorMsg<
IntType,
typename std::enable_if<std::is_integral<IntType>::value>::type> {
 public:
  static const char* OverFlowErrMsg() { return "integer out of range"; }
};

template <>
class ArithOpOverFlowErrorMsg<
float> {
 public:
  static const char* OverFlowErrMsg() { return "float4 out of range"; }
};

template <>
class ArithOpOverFlowErrorMsg<
double> {
 public:
  static const char* OverFlowErrMsg() { return "float8 out of range"; }
};
}  // namespace gpcodegen_ArithOp_detail

using gpcodegen_ArithOp_detail::ArithOpOverFlowErrorMsg;

/**
 * @brief Class with Static member function to generate code for +, - and *
 *        operator
 *
 * @tparam rtype  Return type of function
 * @tparam Arg0   First argument's type
 * @tparam Arg1   Second argument's type
 **/
template <typename rtype, typename Arg0, typename Arg1>
class PGArithFuncGenerator {
  template <typename CppType>
  using CGArithOpTemplateFunc = llvm::Value* (GpCodegenUtils::*)(
      llvm::Value* arg0, llvm::Value* arg1);
  using CGArithOpFunc = CGArithOpTemplateFunc<rtype>;

 public:
  /**
   * @brief Create LLVM Mul instruction with check for overflow
   *
   * @param codegen_utils     Utility to easy code generation.
   * @param llvm_main_func    Current function for which we are generating code
   * @param llvm_error_block  Basic Block to jump when error happens
   * @param llvm_args         Vector of llvm arguments for the function
   * @param llvm_out_value    Store the results of function
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  If there is overflow, it will do elog::ERROR and then jump to given
   *        error block.
   **/
  static bool MulWithOverflow(gpcodegen::GpCodegenUtils* codegen_utils,
                              const PGFuncGeneratorInfo& pg_func_info,
                               llvm::Value** llvm_out_value) {
    llvm::Value* llvm_err_msg = codegen_utils->GetConstant(
        ArithOpOverFlowErrorMsg<rtype>::OverFlowErrMsg());
    return ArithOpWithOverflow(
        codegen_utils,
        &gpcodegen::GpCodegenUtils::CreateMulOverflow<rtype>,
        llvm_err_msg,
        pg_func_info,
        llvm_out_value);
  }

  /**
   * @brief Create LLVM Add instruction with check for overflow
   *
   * @param codegen_utils     Utility to easy code generation.
   * @param llvm_main_func    Current function for which we are generating code
   * @param llvm_error_block  Basic Block to jump when error happens
   * @param llvm_args         Vector of llvm arguments for the function
   * @param llvm_out_value    Store the results of function
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  If there is overflow, it will do elog::ERROR and then jump to given
   *        error block.
   **/
  static bool AddWithOverflow(gpcodegen::GpCodegenUtils* codegen_utils,
                              const PGFuncGeneratorInfo& pg_func_info,
                              llvm::Value** llvm_out_value) {
    llvm::Value* llvm_err_msg = codegen_utils->GetConstant(
        ArithOpOverFlowErrorMsg<rtype>::OverFlowErrMsg());
    return ArithOpWithOverflow(
        codegen_utils,
        &gpcodegen::GpCodegenUtils::CreateAddOverflow<rtype>,
        llvm_err_msg,
        pg_func_info,
        llvm_out_value);
  }

  /**
   * @brief Create LLVM Sub instruction with check for overflow
   *
   * @param codegen_utils     Utility to easy code generation.
   * @param llvm_main_func    Current function for which we are generating code
   * @param llvm_error_block  Basic Block to jump when error happens
   * @param llvm_args         Vector of llvm arguments for the function
   * @param llvm_out_value    Store the results of function
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  If there is overflow, it will do elog::ERROR and then jump to given
   *        error block.
   **/
  static bool SubWithOverflow(gpcodegen::GpCodegenUtils* codegen_utils,
                              const PGFuncGeneratorInfo& pg_func_info,
                              llvm::Value** llvm_out_value) {
    llvm::Value* llvm_err_msg = codegen_utils->GetConstant(
        ArithOpOverFlowErrorMsg<rtype>::OverFlowErrMsg());
    return ArithOpWithOverflow(
        codegen_utils,
        &gpcodegen::GpCodegenUtils::CreateSubOverflow<rtype>,
        llvm_err_msg,
        pg_func_info,
        llvm_out_value);
  }

  static bool ArithOpWithOverflow(gpcodegen::GpCodegenUtils* codegen_utils,
                                  CGArithOpFunc codegen_mem_funcptr,
                                  llvm::Value* llvm_error_msg,
                                  const PGFuncGeneratorInfo& pg_func_info,
                                  llvm::Value** llvm_out_value);
};


/**
 * @brief Class with Static member function to generate code for ++
 *        operator
 *
 * @tparam rtype  Return type of function
 * @tparam Arg   Argument's type
 **/
template <typename rtype, typename Arg>
class PGArithUnaryFuncGenerator {
  template <typename CppType>
  using CGArithOpTemplateFunc = llvm::Value* (GpCodegenUtils::*)(
      llvm::Value* arg);
  using CGArithOpFunc = CGArithOpTemplateFunc<rtype>;

 public:


  /**
   * @brief Increase the value of a variable with check for overflow
   *
   * @param codegen_utils     Utility to easy code generation.
   * @param llvm_main_func    Current function for which we are generating code
   * @param llvm_error_block  Basic Block to jump when error happens
   * @param llvm_args         Vector that contains the llvm argument for the
   *                          function
   * @param llvm_out_value    Store the results of function
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  If there is overflow, it will do elog::ERROR and then jump to given
   *        error block.
   **/
  static bool IncWithOverflow(gpcodegen::GpCodegenUtils* codegen_utils,
                              const PGFuncGeneratorInfo& pg_func_info,
                              llvm::Value** llvm_out_value) {
    llvm::Value* llvm_err_msg = codegen_utils->GetConstant(
        ArithOpOverFlowErrorMsg<rtype>::OverFlowErrMsg());
    return PGArithUnaryFuncGenerator<rtype, Arg>::ArithOpWithOverflow(
        codegen_utils,
        &gpcodegen::GpCodegenUtils::CreateIncOverflow<rtype>,
        llvm_err_msg,
        pg_func_info,
        llvm_out_value);
  }

  static bool ArithOpWithOverflow(gpcodegen::GpCodegenUtils* codegen_utils,
                                   CGArithOpFunc codegen_mem_funcptr,
                                   llvm::Value* llvm_error_msg,
                                   const PGFuncGeneratorInfo& pg_func_info,
                                   llvm::Value** llvm_out_value);

};


template <typename rtype, typename Arg>
bool PGArithUnaryFuncGenerator<rtype, Arg>::ArithOpWithOverflow(
    gpcodegen::GpCodegenUtils* codegen_utils,
    CGArithOpFunc codegen_mem_funcptr,
    llvm::Value* llvm_error_msg,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value) {

  assert(nullptr != llvm_out_value);
  assert(nullptr != codegen_mem_funcptr);
  assert(nullptr != llvm_error_msg);
  // Assumed caller checked vector size and nullptr for codegen_utils
  llvm::Value* casted_arg =
      codegen_utils->CreateCast<rtype, Arg>(pg_func_info.llvm_args[0]);

  llvm::Value* llvm_arith_output = (codegen_utils->*codegen_mem_funcptr)(
      casted_arg);

  llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

  // Check if it is a Integer type
  if (std::is_integral<rtype>::value) {
    llvm::BasicBlock* llvm_non_overflow_block = codegen_utils->CreateBasicBlock(
        "arith_non_overflow_block", pg_func_info.llvm_main_func);
    llvm::BasicBlock* llvm_overflow_block = codegen_utils->CreateBasicBlock(
        "arith_overflow_block", pg_func_info.llvm_main_func);

    *llvm_out_value = irb->CreateExtractValue(llvm_arith_output, 0);
    llvm::Value* llvm_overflow_flag =
        irb->CreateExtractValue(llvm_arith_output, 1);

    irb->CreateCondBr(llvm_overflow_flag,
                      llvm_overflow_block,
                      llvm_non_overflow_block);

    irb->SetInsertPoint(llvm_overflow_block);
    codegen_utils->CreateElog(
        ERROR, "%s", llvm_error_msg);
    irb->CreateBr(pg_func_info.llvm_error_block);

    irb->SetInsertPoint(llvm_non_overflow_block);
  } else {
    *llvm_out_value = llvm_arith_output;
  }
  return true;
}


template <typename rtype, typename Arg0, typename Arg1>
bool PGArithFuncGenerator<rtype, Arg0, Arg1>::ArithOpWithOverflow(
    gpcodegen::GpCodegenUtils* codegen_utils,
    CGArithOpFunc codegen_mem_funcptr,
    llvm::Value* llvm_error_msg,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value) {

  assert(nullptr != llvm_out_value);
  assert(nullptr != codegen_mem_funcptr);
  assert(nullptr != llvm_error_msg);
  // Assumed caller checked vector size and nullptr for codegen_utils
  llvm::Value* casted_arg0 =
      codegen_utils->CreateCast<rtype, Arg0>(pg_func_info.llvm_args[0]);
  llvm::Value* casted_arg1 =
      codegen_utils->CreateCast<rtype, Arg1>(pg_func_info.llvm_args[1]);

  llvm::Value* llvm_arith_output = (codegen_utils->*codegen_mem_funcptr)(
      casted_arg0, casted_arg1);

  llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

  // Check if it is a Integer type
  if (std::is_integral<rtype>::value) {
    llvm::BasicBlock* llvm_non_overflow_block = codegen_utils->CreateBasicBlock(
        "arith_non_overflow_block", pg_func_info.llvm_main_func);
    llvm::BasicBlock* llvm_overflow_block = codegen_utils->CreateBasicBlock(
        "arith_overflow_block", pg_func_info.llvm_main_func);

    *llvm_out_value = irb->CreateExtractValue(llvm_arith_output, 0);
    llvm::Value* llvm_overflow_flag =
        irb->CreateExtractValue(llvm_arith_output, 1);

    irb->CreateCondBr(llvm_overflow_flag,
                      llvm_overflow_block,
                      llvm_non_overflow_block);

    irb->SetInsertPoint(llvm_overflow_block);
    codegen_utils->CreateElog(
        ERROR, "%s", llvm_error_msg);
    irb->CreateBr(pg_func_info.llvm_error_block);

    irb->SetInsertPoint(llvm_non_overflow_block);
  } else {
    *llvm_out_value = llvm_arith_output;
  }
  return true;
}

/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_PG_ARITH_FUNC_GENERATOR_H_
