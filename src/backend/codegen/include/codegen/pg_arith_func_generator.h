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
   * @brief Create LLVM instructions to check if arguments are NULL.
   *
   * @param codegen_utils      Utility for easy code generation.
   * @param pg_func_info       Details for pgfunc generation
   * @param llvm_isnull_ptr    Records if result is NULL
   * @param llvm_out_value_ptr Store location for the result
   * @param llvm_is_set_ptr    Pointer to flag that shows if a value has been
   *                           assigned to the contents of llvm_out_value_ptr
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  It is used only if built-in function is not strict.
   *        This function implements the first part of int4_sum built-in
   *        function that checks for NULL arguments.
   **/
  static bool CheckNull(gpcodegen::GpCodegenUtils* codegen_utils,
                        const PGFuncGeneratorInfo& pg_func_info,
                        llvm::Value* llvm_out_value_ptr,
                        llvm::Value* const llvm_isnull_ptr,
                        llvm::Value* llvm_is_set_ptr) {
    assert(nullptr != codegen_utils);
    assert(nullptr != llvm_out_value_ptr);
    assert(nullptr != llvm_is_set_ptr);
    assert(codegen_utils->GetType<rtype>() ==
    		codegen_utils->GetType<int64_t>());
    assert(pg_func_info.llvm_args.size() > 1);
    assert(pg_func_info.llvm_args_isNull.size() > 1);
    auto irb = codegen_utils->ir_builder();

    // Entry block that shows that clearly shows the beginning of CheckNull
    llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
        "CheckNull_entry_block", pg_func_info.llvm_main_func);
    // Block that includes instructions in case that arg0 is null
    llvm::BasicBlock* arg0_is_null_block = codegen_utils->
        CreateBasicBlock("CheckNull_arg0_is_null_block",
                         pg_func_info.llvm_main_func);
    // Block that includes instructions in case that arg0 is not null
    llvm::BasicBlock* arg0_is_not_null_block = codegen_utils->
        CreateBasicBlock("CheckNull_arg0_is_not_null_block",
                         pg_func_info.llvm_main_func);
    // Block that includes instructions in case that arg1 is null
    llvm::BasicBlock* arg1_is_null_block = codegen_utils->
        CreateBasicBlock("CheckNull_arg1_is_null_block",
                         pg_func_info.llvm_main_func);
    // Block that includes instructions in case that arg1 is not null
    llvm::BasicBlock* arg1_is_not_null_block = codegen_utils->
        CreateBasicBlock("CheckNull_arg1_is_not_null_block",
                         pg_func_info.llvm_main_func);
    // Block that includes instructions in case that there is a null argument
    llvm::BasicBlock* return_null_block = codegen_utils->CreateBasicBlock(
        "CheckNull_return_null_block", pg_func_info.llvm_main_func);
    // All CheckNull blocks create branch to continue_block (final block)
    llvm::BasicBlock* continue_block = codegen_utils->CreateBasicBlock(
        "CheckNull_continue_block", pg_func_info.llvm_main_func);
    irb->CreateBr(entry_block);

    // entry_block
    // -----------
    // Generate code that examines if arg0 is null
    irb->SetInsertPoint(entry_block);
    // if (PG_ARGISNULL(0))
    irb->CreateCondBr(pg_func_info.llvm_args_isNull[0],
        arg0_is_null_block /* true */,
        arg0_is_not_null_block /* false */);

    // arg0_is_null_block
    // ------------------
    // arg0 is NULL, so examine if arg1 is NULL too.
    irb->SetInsertPoint(arg0_is_null_block);
    // if (PG_ARGISNULL(1))
    irb->CreateCondBr(pg_func_info.llvm_args_isNull[1],
                      return_null_block /* true */,
                      arg1_is_not_null_block /* false */);

    // return_null_block
    // -----------------
    // Generate code for PG_RETURN_NULL()
    irb->SetInsertPoint(return_null_block);
    // fcinfo->isnull = true;
    irb->CreateStore(codegen_utils->GetConstant<bool>(true),
                     llvm_isnull_ptr);
    // similar to: return (Datum) 0
    irb->CreateStore(codegen_utils->GetConstant<rtype>(0),
                     llvm_out_value_ptr);
    // set the content of llvm_is_set_ptr to true, so that we do not need to
    // execute the code of non-strict built-in function
    irb->CreateStore(codegen_utils->GetConstant<bool>(true),
                     llvm_is_set_ptr);
    irb->CreateBr(continue_block);

    // arg1_is_not_null_block
    // ----------------------
    // In this case, arg0 is NULL and arg1 is not NULL.
    irb->SetInsertPoint(arg1_is_not_null_block);
    // val = (int64) PG_GETARG_INT32(1)
    // PG_RETURN_INT64(val)
    irb->CreateStore(codegen_utils->CreateCast<rtype, int32>(
        codegen_utils->CreateCast<int32, Arg1>(pg_func_info.llvm_args[1])),
                     llvm_out_value_ptr);
    // set the content of llvm_is_set_ptr to true, so that we do not need to
    // execute the code of non-strict built-in function
    irb->CreateStore(codegen_utils->GetConstant<bool>(true),
                     llvm_is_set_ptr);
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_isnull_ptr);
    irb->CreateBr(continue_block);

    // arg0_is_not_null_block
    // ----------------------
    // arg0 is not null, examine if arg1 is null
    // If both arguments are not NULL, then just continue and execute the
    // generated code of the built-in function
    irb->SetInsertPoint(arg0_is_not_null_block);
    // if (PG_ARGISNULL(1))
    irb->CreateCondBr(pg_func_info.llvm_args_isNull[1],
                      arg1_is_null_block,
                      continue_block);

    // arg1_is_null_block
    // ------------------
    // arg0 is not NULL, but arg1 is NULL
    // Generate code for PG_RETURN_INT64(val), where val = arg0
    irb->SetInsertPoint(arg1_is_null_block);

    irb->CreateStore(codegen_utils->CreateCast<rtype, Arg0>(
        pg_func_info.llvm_args[0]),
                     llvm_out_value_ptr);
    // set the content of llvm_is_set_ptr to true, so that we do not need to
    // execute the code of non-strict built-in function
    irb->CreateStore(codegen_utils->GetConstant<bool>(true),
                         llvm_is_set_ptr);
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_isnull_ptr);
    irb->CreateBr(continue_block);

    // continue_block
    // --------------
    // Continue with the rest of the generated code
    irb->SetInsertPoint(continue_block);

    return true;
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
 * @brief Class with Static member function to generate code for unary
 *        operator such as ++, -- etc.
 *
 * @tparam rtype  Return type of function
 * @tparam Arg    Argument's type
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
   * @param codegen_utils     Utility for easy code generation.
   * @param pg_func_info      Details for pgfunc generation
   * @param llvm_out_value    Store location for the result
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

  /**
   * @brief Dummy function that does not perform any actions
   *
   * @param codegen_utils      Utility for easy code generation.
   * @param pg_func_info       Details for pgfunc generation
   * @param llvm_out_value_ptr Store location for the result
   * @param llvm_is_set_ptr    Pointer to flag that shows if a value has been
   *                           assigned to the contents of llvm_out_value_ptr
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  It is passed as an argument to PGGenericFuncGenerator, when a
   *        a built-in function is not strict and it does not perform
   *        any nullity checks.
   **/
  static bool DoNothing(gpcodegen::GpCodegenUtils* codegen_utils,
                        const PGFuncGeneratorInfo& pg_func_info,
                        llvm::Value* llvm_out_value_ptr,
                        llvm::Value* const llvm_isnull_ptr,
                        llvm::Value* llvm_out_value_is_set_ptr) {
    return true;
  }
};

/**
 * @brief Utility function for generating code for overflow checking. This
 * includes an overflow block and non-overflow block, check and conditional
 * branch instructions.
 *
 * @param codegen_utils     Utility for easy code generation.
 * @param pg_func_info      Details for pgfunc generation
 * @param llvm_arith_output Result of the arithmetic operation
 * @param llvm_error_msg    Error message to use when overflow occurs
 * @param llvm_out_value    Store location for the result
 *
 * @return true if generation was successful otherwise return false
 *
 * @note  Only integral types are currently supported.
 **/
template<typename rtype>
static bool CreateOverflowCheckLogic(
    gpcodegen::GpCodegenUtils* codegen_utils,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value* llvm_arith_output,
    llvm::Value* llvm_error_msg,
    llvm::Value** llvm_out_value) {
  assert(nullptr != codegen_utils);
  assert(nullptr != llvm_arith_output);
  assert(nullptr != llvm_error_msg);

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

    irb->CreateCondBr(llvm_overflow_flag, llvm_overflow_block,
                      llvm_non_overflow_block);

    irb->SetInsertPoint(llvm_overflow_block);
    codegen_utils->CreateElog(
        ERROR,
        "%s", llvm_error_msg);
    irb->CreateBr(pg_func_info.llvm_error_block);

    irb->SetInsertPoint(llvm_non_overflow_block);
  } else {
    *llvm_out_value = llvm_arith_output;
  }

  return true;
}

template <typename rtype, typename Arg>
bool PGArithUnaryFuncGenerator<rtype, Arg>::ArithOpWithOverflow(
    gpcodegen::GpCodegenUtils* codegen_utils,
    CGArithOpFunc codegen_mem_funcptr,
    llvm::Value* llvm_error_msg,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value) {
  assert(nullptr != codegen_utils);
  assert(nullptr != llvm_out_value);
  assert(nullptr != codegen_mem_funcptr);
  assert(nullptr != llvm_error_msg);

  // Assumed caller checked vector size and nullptr for codegen_utils
  llvm::Value* casted_arg =
      codegen_utils->CreateCast<rtype, Arg>(pg_func_info.llvm_args[0]);

  llvm::Value* llvm_arith_output = (codegen_utils->*codegen_mem_funcptr)(
      casted_arg);

  return CreateOverflowCheckLogic<rtype>(codegen_utils,
                                         pg_func_info,
                                         llvm_arith_output,
                                         llvm_error_msg,
                                         llvm_out_value);
}


template <typename rtype, typename Arg0, typename Arg1>
bool PGArithFuncGenerator<rtype, Arg0, Arg1>::ArithOpWithOverflow(
    gpcodegen::GpCodegenUtils* codegen_utils,
    CGArithOpFunc codegen_mem_funcptr,
    llvm::Value* llvm_error_msg,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value) {
  assert(nullptr != codegen_utils);
  assert(nullptr != llvm_out_value);
  assert(nullptr != codegen_mem_funcptr);
  assert(nullptr != llvm_error_msg);

  // Assumed caller checked vector size and nullptr for codegen_utils
  llvm::Value* casted_arg0 =
      codegen_utils->CreateCast<rtype, Arg0>(pg_func_info.llvm_args[0]);
  llvm::Value* casted_arg1 =
      codegen_utils->CreateCast<rtype, Arg1>(pg_func_info.llvm_args[1]);

  llvm::Value* llvm_arith_output =
      (codegen_utils->*codegen_mem_funcptr)(casted_arg0, casted_arg1);

  return CreateOverflowCheckLogic<rtype>(codegen_utils,
                                         pg_func_info,
                                         llvm_arith_output,
                                         llvm_error_msg,
                                         llvm_out_value);
}

/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_PG_ARITH_FUNC_GENERATOR_H_
