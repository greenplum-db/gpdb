//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    base_codegen.h
//
//  @doc:
//    Base class for all the code generators with common implementation
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_BASE_CODEGEN_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_BASE_CODEGEN_H_

extern "C" {
#include <utils/elog.h>
}

#include <string>
#include <vector>
#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/codegen_config.h"
#include "codegen/codegen_interface.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"


namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

// Forward declaration
class CodegenManager;

/**
 * @brief Base code generator with common implementation that other
 * 		  code generators can use.
 *
 * @tparam FuncPtrType Function type for regular version of target functions
 *         or generated functions.
 **/
template<class FuncPtrType>
class BaseCodegen: public CodegenInterface {
 public:
  /**
   * @brief Destroys the code generator and reverts back to using regular
   * 		    version of the target function.
   **/
  virtual ~BaseCodegen() {
    SetToRegular(regular_func_ptr_, ptr_to_chosen_func_ptr_);
  }

  bool InitDependencies() override {
    return true;
  }

  bool GenerateCode(gpcodegen::GpCodegenUtils* codegen_utils) final {
    bool valid_generated_functions = true;
    valid_generated_functions &= GenerateCodeInternal(codegen_utils);

    // Do this check only if it enabled by guc
    if (codegen_validate_functions && valid_generated_functions) {
      for (llvm::Function* function : uncompiled_generated_functions_) {
        assert(nullptr != function);
        std::string func_name = function->getName();
        std::string error_message = "";
        llvm::raw_string_ostream out(error_message);
        // Verify function returns true if there are errors.
        valid_generated_functions &= !llvm::verifyFunction(*function, &out);
        out.flush();
        if (!valid_generated_functions) {
          elog(WARNING, "Broken function found %s: %s",
               func_name.c_str(), error_message.c_str());
          break;
        }
      }
    }
    if (!valid_generated_functions) {
      // If failed to generate, or have invalid functions, make sure we
      // do clean up by erasing all the llvm functions.
      for (llvm::Function* function : uncompiled_generated_functions_) {
        assert(nullptr != function);
        function->eraseFromParent();
      }
    }
    is_generated_ = valid_generated_functions;
    // We don't need to keep these pointers any more
    std::vector<llvm::Function*>().swap(uncompiled_generated_functions_);
    return is_generated_;
  }

  bool SetToRegular() final {
    assert(nullptr != regular_func_ptr_);
    SetToRegular(regular_func_ptr_, ptr_to_chosen_func_ptr_);
    return true;
  }

  bool SetToGenerated(gpcodegen::GpCodegenUtils* codegen_utils) final {
    if (false == IsGenerated()) {
      assert(*ptr_to_chosen_func_ptr_ == regular_func_ptr_);
      return false;
    }

    FuncPtrType compiled_func_ptr = codegen_utils->GetFunctionPointer<
        FuncPtrType>(GetUniqueFuncName());

    if (nullptr != compiled_func_ptr) {
      *ptr_to_chosen_func_ptr_ = compiled_func_ptr;
      return true;
    }
    return false;
  }

  void Reset() final {
    SetToRegular();
  }

  const std::string& GetOrigFuncName() const final {
    return orig_func_name_;
  }

  const std::string& GetUniqueFuncName() const final {
    return unique_func_name_;
  }

  bool IsGenerated() const final {
    return is_generated_;
  }

  /**
   * @return Regular version of the target function.
   *
   **/
  FuncPtrType GetRegularFuncPointer() {
    return regular_func_ptr_;
  }

  /**
   * @brief Sets up the caller to use the corresponding regular version of the
   *        target function.
   *
   * @param regular_func_ptr       Regular version of the target function.
   * @param ptr_to_chosen_func_ptr Reference to caller.
   *
   * @return true on setting to regular version.
   **/
  static bool SetToRegular(FuncPtrType regular_func_ptr,
                           FuncPtrType* ptr_to_chosen_func_ptr) {
    assert(nullptr != ptr_to_chosen_func_ptr);
    assert(nullptr != regular_func_ptr);
    *ptr_to_chosen_func_ptr = regular_func_ptr;
    return true;
  }

 protected:
  /**
   * @brief Constructor
   *
   * @param manager                The manager in which this is enrolled.
   * @param orig_func_name         Original function name.
   * @param regular_func_ptr       Regular version of the target function.
   * @param ptr_to_chosen_func_ptr Reference to the function pointer that the caller will call.
   *
   * @note 	The ptr_to_chosen_func_ptr can refer to either the generated function or the
   * 			corresponding regular version.
   *
   **/
  explicit BaseCodegen(gpcodegen::CodegenManager* manager,
                       const std::string& orig_func_name,
                       FuncPtrType regular_func_ptr,
                       FuncPtrType* ptr_to_chosen_func_ptr)
  : manager_(manager),
    orig_func_name_(orig_func_name),
    unique_func_name_(CodegenInterface::GenerateUniqueName(orig_func_name)),
    regular_func_ptr_(regular_func_ptr),
    ptr_to_chosen_func_ptr_(ptr_to_chosen_func_ptr),
    is_generated_(false) {
    // Initialize the caller to use regular version of target function.
    SetToRegular(regular_func_ptr, ptr_to_chosen_func_ptr);
  }

  gpcodegen::CodegenManager* manager() const {
    return manager_;
  }

  /**
   * @brief Generates specialized code at run time.
   *
   * @note 	A template method design pattern to be overridden by the sub-class to implement
   * 			the actual code generation.
   *
   * @note  	This is being called from GenerateCode and derived class will implement actual
   *        	code generation
   *
   * @param codegen_utils Utility to ease the code generation process.
   * @return true on successful generation.
   **/
  virtual bool GenerateCodeInternal(
      gpcodegen::GpCodegenUtils* codegen_utils) = 0;

  /**
   * @brief Create llvm Function for given type and store the function pointer
   *        in vector
   *
   * @note  If generation fails, this class takes responsibility to clean up all
   *        the functions it created
   *
   * @tparam FunctionType Type of the function to create
   *
   * @param codegen_utils Utility to ease the code generation process.
   * @param function_name Name of the function to create
   * @return llvm::Function pointer
   **/
 public:
  template <typename FunctionType>
  llvm::Function* CreateFunction(gpcodegen::GpCodegenUtils* codegen_utils,
                                 const std::string& function_name) {
    assert(nullptr != codegen_utils);
    llvm::Function* function = codegen_utils->CreateFunction<FunctionType>(
        function_name);
    assert(nullptr != function);
    uncompiled_generated_functions_.push_back(function);
    return function;
  }

 private:
  gpcodegen::CodegenManager* manager_;
  std::string orig_func_name_;
  std::string unique_func_name_;
  FuncPtrType regular_func_ptr_;
  FuncPtrType* ptr_to_chosen_func_ptr_;
  bool is_generated_;
  // To track uncompiled llvm functions it creates and erase from
  // llvm module on failed generations.
  std::vector<llvm::Function*> uncompiled_generated_functions_;
};
/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_BASE_CODEGEN_H_
