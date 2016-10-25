//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    pg_func_generator.h
//
//  @doc:
//    Object to manage code generation for Greenplum's function
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_PG_FUNC_GENERATOR_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_PG_FUNC_GENERATOR_H_

#include <string>
#include <vector>
#include <memory>

#include "codegen/pg_func_generator_interface.h"
#include "codegen/utils/gp_codegen_utils.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

namespace gpcodegen {

// Helper template classes for gp_func_generators nested in here
namespace pg_func_generator_detail {

// PGFuncArgPreProcessor is a variadic template to assist in pre-processing a
// variable number of arguments to PGFuncBaseGenerator. Each specialization has
// a method called 'PreProcessArgs' that takes a pointer to 'CodegenUtils', an
// input vector of 'llvm:Values*' and another vector of 'llvm::Values*' that it
// populates by pre-processing the input vector. During pre-processsing, we
// generate LLVM instructions to convert the input 'llvm::Value*' values (that
// are expected to be of the type 'Datum') to the appropriate CppType from the
// variadic template list, and populate the output vector with coverted
// 'llvm::Values*' in the same order as input.
//
// e.g. PGFuncArgPreProcessor<int, bool>::PreProcessArgs(cu, vin, vout) will
// create instructions to convert vin[0] to int and vin[1] to bool and populate
// 'vout'.
template <typename... ArgumentTypes>
class PGFuncArgPreProcessor;

// Base version for zero argument types does nothing.
template <>
class PGFuncArgPreProcessor<> {
 private:
  static bool PreProcessArgsInternal(
      gpcodegen::GpCodegenUtils* codegen_utils,
      std::vector<llvm::Value*>::const_iterator llvm_in_args_iter,
      std::vector<llvm::Value*>* llvm_out_args) {
    return true;
  }

  // Make all template specializations of PGFuncArgPreProcessor friends
  template <typename ... T>
  friend class PGFuncArgPreProcessor;
};

// Version for 1+ arguments that recurses
template <typename HeadType, typename ... TailTypes>
class PGFuncArgPreProcessor<HeadType, TailTypes...> {
 public:
  static bool PreProcessArgs(
        gpcodegen::GpCodegenUtils* codegen_utils,
        const std::vector<llvm::Value*>& llvm_in_args,
        std::vector<llvm::Value*>* llvm_out_args) {
    assert(nullptr != codegen_utils &&
           nullptr != llvm_out_args);


    if (llvm_in_args.size() != 1 /* HeadType */ + sizeof...(TailTypes)) {
      return false;
    }

    return PreProcessArgsInternal(
        codegen_utils, llvm_in_args.begin(), llvm_out_args);
  }

 private:
  static bool PreProcessArgsInternal(
      gpcodegen::GpCodegenUtils* codegen_utils,
      std::vector<llvm::Value*>::const_iterator llvm_in_args_iter,
      std::vector<llvm::Value*>* llvm_out_args) {
    assert(nullptr != codegen_utils);
    assert(nullptr != llvm_out_args);
    assert(nullptr != *llvm_in_args_iter);

    assert(codegen_utils->GetType<Datum>() == (*llvm_in_args_iter)->getType());

    // Convert Datum to given cpp type.
    llvm_out_args->push_back(
        codegen_utils->CreateDatumToCppTypeCast<HeadType>(*llvm_in_args_iter));
    return PGFuncArgPreProcessor<TailTypes...>::PreProcessArgsInternal(
        codegen_utils, ++llvm_in_args_iter, llvm_out_args);
  }

  // Make all template specializations of PGFuncArgPreProcessor friends
  template <typename ... T>
  friend class PGFuncArgPreProcessor;
};

}  // namespace pg_func_generator_detail


/** \addtogroup gpcodegen
 *  @{
 */

typedef bool (*PGFuncGeneratorFn)(gpcodegen::GpCodegenUtils* codegen_utils,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value);

typedef bool (*PGCheckNullFuncGeneratorFn)(
    gpcodegen::GpCodegenUtils* codegen_utils,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value* llvm_out_value_ptr,
    llvm::Value* llvm_is_set_ptr);


/**
 * @brief Object that use an IRBuilder member function that to generate code for
 *        given binary GPDB function
 *
 * @tparam ReturnType   C++ type of return type of the GPDB function
 * @tparam Arg0         C++ type of 1st argument of the GPDB function
 * @tparam Arg1         C++ type of 2nd argument of the GPDB function
 *
 **/
template <typename ReturnType, typename Arg0, typename Arg1>
class PGIRBuilderFuncGenerator
    : public PGFuncGeneratorInterface {
 public:
  // Type of the IRBuilder function we need to call
  using IRBuilderFuncPtrType = llvm::Value* (llvm::IRBuilder<>::*) (
      llvm::Value*, llvm::Value*, const llvm::Twine&);

  /**
   * @brief Constructor.
   *
   * @param pg_func_oid   Greenplum function Oid
   * @param pg_func_name  Greenplum function name
   * @param mem_func_ptr  IRBuilder member function pointer
   **/
  PGIRBuilderFuncGenerator(int pg_func_oid,
                           const std::string& pg_func_name,
                           IRBuilderFuncPtrType mem_func_ptr,
                           bool pg_func_strict)
  : pg_func_oid_(pg_func_oid),
    pg_func_name_(pg_func_name),
    func_ptr_(mem_func_ptr),
    pg_func_strict_(pg_func_strict) {
  }

  std::string GetName() final {
    return pg_func_name_;
  }

  size_t GetTotalArgCount() final {
    // Support Binary IRBuilder functions only
    return 2;
  }

  bool IsStrict() final {
    return pg_func_strict_;
  }

  bool GenerateCode(gpcodegen::GpCodegenUtils* codegen_utils,
                    const PGFuncGeneratorInfo& pg_func_info,
                    llvm::Value** llvm_out_value) final {
    assert(nullptr != llvm_out_value);
    std::vector<llvm::Value*> llvm_preproc_args;

    if (!pg_func_generator_detail::PGFuncArgPreProcessor<Arg0, Arg1>::
        PreProcessArgs(
            codegen_utils, pg_func_info.llvm_args, &llvm_preproc_args)) {
      return false;
    }

    llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

    if (!IsStrict()) {
      // We do not support non-strict PGIRBuilderFuncGenerator
      elog(DEBUG1, "Non-strict PGIRBuilderFuncGenerators are not supported");
      return false;
    } else {
      // Block that will be the entry point of the implementation of argument
      // checks for strict built-in functions.
      llvm::BasicBlock* strict_logic_entry_block = codegen_utils->
          CreateBasicBlock("strict_logic_entry_block",
                           pg_func_info.llvm_main_func);
      // Block for generating the code of the function.
      llvm::BasicBlock* generate_function_block = codegen_utils->
          CreateBasicBlock("generate_function_block",
                           pg_func_info.llvm_main_func);
      // Block that returns (Datum) 0 if there is NULL argument.
      llvm::BasicBlock* null_argument_block = codegen_utils->CreateBasicBlock(
          "null_argument_block", pg_func_info.llvm_main_func);
      // Block that receives values for llvm_out_value using a phi node that
      // has incoming edges from the previous two blocks
      llvm::BasicBlock* set_llvm_out_value_block = codegen_utils->
          CreateBasicBlock("PGGenericFuncGenerator_set_llvm_out_value_block",
                           pg_func_info.llvm_main_func);
      irb->CreateBr(strict_logic_entry_block);

      // strict_logic_block
      // --------------------
      // Checks if there is a NULL argument block. If yes then go to
      // null_argument_block; generate_function_block otherwise.
      irb->SetInsertPoint(strict_logic_entry_block);
      // Stores if there is a NULL argument
      llvm::Value* llvm_there_is_null_arg_ptr = irb->CreateAlloca(
          codegen_utils->GetType<bool>(),
          nullptr, "llvm_there_is_null_arg_ptr");
      irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                       llvm_there_is_null_arg_ptr);
      for (int i = 0; i < pg_func_info.llvm_args_isNull.size(); ++i) {
        irb->CreateStore(
            irb->CreateOr(pg_func_info.llvm_args_isNull[i],
                          irb->CreateLoad(llvm_there_is_null_arg_ptr)),
                          llvm_there_is_null_arg_ptr);
      }
      irb->CreateCondBr(irb->CreateLoad(llvm_there_is_null_arg_ptr),
                        null_argument_block /*true*/,
                        generate_function_block /*false*/);

      // generate_function_block
      // -----------------------
      // Generate code for the built-in function
      irb->SetInsertPoint(generate_function_block);
      // Pointer to the temporary value of llvm_out_value after built-in
      // function's execution
      llvm::Value* llvm_func_tmp_value = nullptr;
      // Generate code for the built-in function
      llvm_func_tmp_value = (irb->*this->func_ptr_)(llvm_preproc_args[0],
          llvm_preproc_args[1], "");
      // Keep track of the last created block during execution of the built-in
      // function. This will be used as an incoming edge to the phi node.
      llvm::BasicBlock* func_generation_last_block = irb->GetInsertBlock();
      assert(llvm_func_tmp_value->getType() ==
          codegen_utils->GetType<ReturnType>());
      irb->CreateBr(set_llvm_out_value_block);

      // null_argument_block
      // -------------------
      // Actions to be takes when there is a null argument
      irb->SetInsertPoint(null_argument_block);
      // *isNull = true;
      irb->CreateStore(
          codegen_utils->GetConstant<bool>(true),
          pg_func_info.llvm_isNull_ptr);
      llvm::Value* zero_value = codegen_utils->GetConstant<ReturnType>(0);
      irb->CreateBr(set_llvm_out_value_block);

      // set_llvm_out_value_block
      // ------------------------
      // Create the phi node and set the value of llvm_out_value accordingly
      irb->SetInsertPoint(set_llvm_out_value_block);
      llvm::PHINode* llvm_out_value_phinode = irb->CreatePHI(
          codegen_utils->GetType<Datum>(), 2);
      llvm_out_value_phinode->addIncoming(zero_value,
                                          null_argument_block);
      llvm_out_value_phinode->addIncoming(llvm_func_tmp_value,
                                          func_generation_last_block);
      *llvm_out_value = llvm_out_value_phinode;

      return true;
    }
  }

 private:
  int pg_func_oid_;
  std::string pg_func_name_;
  IRBuilderFuncPtrType func_ptr_;
  bool pg_func_strict_;
};

/**
 * @brief Object that uses a static member function to generate code for
 *        given Greenplum's function
 *
 * @tparam FuncPtrType  Function type that generate code
 * @tparam Args         Argument CppTypes
 *
 **/
template <typename ReturnType, typename ... Args>
class PGGenericFuncGenerator : public  PGFuncGeneratorInterface {
 public:
  /**
   * @brief Constructor.
   *
   * @param pg_func_oid         Greenplum function Oid
   * @param pg_func_name        Greenplum function name
   * @param func_ptr            function pointer to static function
   * @param check_null_func_ptr function pointer to static function that
   *                            generates code for nullity checks
   * @param pg_func_strict      records if function is strict
   **/
  PGGenericFuncGenerator(int pg_func_oid,
                         const std::string& pg_func_name,
                         PGFuncGeneratorFn func_ptr,
                         PGCheckNullFuncGeneratorFn check_null_func_ptr,
                         bool pg_func_strict)
  : pg_func_oid_(pg_func_oid),
    pg_func_name_(pg_func_name),
    func_ptr_(func_ptr),
    check_null_func_ptr_(check_null_func_ptr),
    pg_func_strict_(pg_func_strict) {
  }

  std::string GetName() final {
    return pg_func_name_;
  }

  size_t GetTotalArgCount() final {
    return sizeof...(Args);
  }

  bool IsStrict() final {
    return pg_func_strict_;
  }

  bool GenerateCode(gpcodegen::GpCodegenUtils* codegen_utils,
                    const PGFuncGeneratorInfo& pg_func_info,
                    llvm::Value** llvm_out_value) final {
    assert(nullptr != codegen_utils);
    assert(nullptr != llvm_out_value);
    auto irb = codegen_utils->ir_builder();
    std::vector<llvm::Value*> llvm_preproc_args;

    if (!pg_func_generator_detail::PGFuncArgPreProcessor<Args...>::
        PreProcessArgs(
            codegen_utils, pg_func_info.llvm_args, &llvm_preproc_args)) {
      return false;
    }

    PGFuncGeneratorInfo pg_processed_func_info(pg_func_info.llvm_main_func,
                                               pg_func_info.llvm_error_block,
                                               llvm_preproc_args,
                                               pg_func_info.llvm_args_isNull,
                                               pg_func_info.llvm_isNull_ptr);

    // Block that is the entry point to built-in function's generated code
    llvm::BasicBlock* generate_function_block = codegen_utils->
        CreateBasicBlock("PGGenericFuncGenerator_generate_function_block",
                         pg_func_info.llvm_main_func);
    // Block that receives values for llvm_out_value using a phi node that
    // has incoming edges from the previous two blocks
    llvm::BasicBlock* set_llvm_out_value_block = codegen_utils->
        CreateBasicBlock("PGGenericFuncGenerator_set_llvm_out_value_block",
                         pg_func_info.llvm_main_func);
    // Block that implements the required actions if there is a null argument.
    // The implementation depends if function is strict.
    llvm::BasicBlock* null_argument_block;
    // It contains the value of llvm_out_value when there is a null attribute.
    llvm::Value* llvm_check_null_tmp_value;

    // In both cases we create blocks that implement nullity checks.
    // The generation of functions's code along with the set of llvm_out
    // value happens after the if...else statements for both scenarios.
    if (!IsStrict()) {
      // If function is not strict then we have to generate code that examines
      // if there are NULL arguments and acts accordingly.
      assert(nullptr != check_null_func_ptr_);
      // Block that is the entry point to generated code that examine if there
      // are NULL arguments
      llvm::BasicBlock* non_strict_logic_entry_block = codegen_utils->
          CreateBasicBlock("non_strict_logic_entry_block",
                           pg_func_info.llvm_main_func);
      irb->CreateBr(non_strict_logic_entry_block);

      // generate_check_null_block
      // -------------------------
      // Generate check for NULL arguments logic
      irb->SetInsertPoint(non_strict_logic_entry_block);
      // This variable will tell us if we set the llvm_out_value during
      // the check for NULL attributes.
      llvm::Value* llvm_is_set_ptr = irb->CreateAlloca(
          codegen_utils->GetType<bool>(), nullptr, "llvm_is_set_ptr");
      // Initially, set it to false
      irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                       llvm_is_set_ptr);
      // Pointer to the temporary value of llvm_out_value after check for NULLs
      llvm::Value* llvm_check_null_value_ptr = irb->CreateAlloca(
          codegen_utils->GetType<Datum>(), nullptr,
          "llvm_check_null_value_ptr");
      // Invoke CheckNulls function
      this->check_null_func_ptr_(codegen_utils,
                                 pg_processed_func_info,
                                 llvm_check_null_value_ptr,
                                 llvm_is_set_ptr);
      // Keep track of the last created block during the check for NULL
      // attributes. This will be used as an incoming edge to the phi node.
      null_argument_block = irb->GetInsertBlock();
      // Temporary value of llvm_out_value after check for NULLs
      llvm_check_null_tmp_value =
          irb->CreateLoad(llvm_check_null_value_ptr);
      assert(llvm_check_null_tmp_value->getType() ==
    		  codegen_utils->GetType<ReturnType>());
      irb->CreateCondBr(irb->CreateLoad(llvm_is_set_ptr),
                        set_llvm_out_value_block /* true */,
                        generate_function_block /* false */);
    } else {
      // Block that will be the entry point of the implementation of argument
      // checks for strict built-in functions.
      llvm::BasicBlock* strict_logic_entry_block = codegen_utils->
          CreateBasicBlock("strict_logic_entry_block",
                           pg_func_info.llvm_main_func);
      // Block that returns (Datum) 0 if there is NULL argument.
      null_argument_block = codegen_utils->CreateBasicBlock(
          "null_argument_block", pg_func_info.llvm_main_func);
      irb->CreateBr(strict_logic_entry_block);

      // strict_logic_block
      // --------------------
      // Checks if there is a NULL argument block. If yes then go to
      // null_argument_block; generate_function_block otherwise.
      irb->SetInsertPoint(strict_logic_entry_block);
      // Stores if there is a NULL argument
      llvm::Value* llvm_there_is_null_arg_ptr = irb->CreateAlloca(
          codegen_utils->GetType<bool>(),
          nullptr, "llvm_there_is_null_arg_ptr");
      irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                       llvm_there_is_null_arg_ptr);
      for (int i = 0; i < pg_func_info.llvm_args_isNull.size(); ++i) {
        irb->CreateStore(
            irb->CreateOr(pg_func_info.llvm_args_isNull[i],
                          irb->CreateLoad(llvm_there_is_null_arg_ptr)),
                          llvm_there_is_null_arg_ptr);
      }
      irb->CreateCondBr(irb->CreateLoad(llvm_there_is_null_arg_ptr),
                        null_argument_block /*true*/,
                        generate_function_block /*false*/);

      // null_argument_block
      // -------------------
      // Actions to be taken when there is a null argument
      irb->SetInsertPoint(null_argument_block);
      // *isNull = true;
      irb->CreateStore(
          codegen_utils->GetConstant<bool>(true),
          pg_func_info.llvm_isNull_ptr);
      llvm_check_null_tmp_value = codegen_utils->GetConstant<ReturnType>(0);
      irb->CreateBr(set_llvm_out_value_block);
    }

    // generate_function_block
    // -----------------------
    // Generate code for the built-in function
    irb->SetInsertPoint(generate_function_block);
    // Pointer to the temporary value of llvm_out_value after built-in
    // function's execution
    llvm::Value* llvm_func_generation_tmp_value = nullptr;
    // Generate code for the built-in function
    this->func_ptr_(codegen_utils,
                    pg_processed_func_info,
                    &llvm_func_generation_tmp_value);
    // Keep track of the last created block during execution of the built-in
    // function. This will be used as an incoming edge to the phi node.
    llvm::BasicBlock* func_generation_last_block = irb->GetInsertBlock();
    assert(llvm_func_generation_tmp_value->getType() ==
           codegen_utils->GetType<ReturnType>());
    irb->CreateBr(set_llvm_out_value_block);

    // set_llvm_out_value_block
    // ------------------------
    // Create the phi node and set the value of llvm_out_value accordingly
    irb->SetInsertPoint(set_llvm_out_value_block);
    llvm::PHINode* llvm_out_value_phinode = irb->CreatePHI(
        codegen_utils->GetType<ReturnType>(), 2);
    llvm_out_value_phinode->addIncoming(llvm_check_null_tmp_value,
                                        null_argument_block);
    llvm_out_value_phinode->addIncoming(llvm_func_generation_tmp_value,
                                        func_generation_last_block);
    *llvm_out_value = llvm_out_value_phinode;
    return true;
  }

 private:
  int pg_func_oid_;
  const std::string& pg_func_name_;
  PGFuncGeneratorFn func_ptr_;
  PGCheckNullFuncGeneratorFn check_null_func_ptr_;
  bool pg_func_strict_;
};



/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_PG_FUNC_GENERATOR_H_
