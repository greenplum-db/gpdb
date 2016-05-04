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

#include <string>
#include <vector>
#include "codegen/utils/codegen_utils.h"
#include "codegen/codegen_interface.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

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

  bool GenerateCode(gpcodegen::CodegenUtils* codegen_utils) final {
    is_generated_ = GenerateCodeInternal(codegen_utils);
    return is_generated_;
  }

  bool SetToRegular() final {
    assert(nullptr != regular_func_ptr_);
    SetToRegular(regular_func_ptr_, ptr_to_chosen_func_ptr_);
    return true;
  }

  bool SetToGenerated(gpcodegen::CodegenUtils* codegen_utils) final {
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
   * @param orig_func_name         Original function name.
   * @param regular_func_ptr       Regular version of the target function.
   * @param ptr_to_chosen_func_ptr Reference to the function pointer that the caller will call.
   *
   * @note 	The ptr_to_chosen_func_ptr can refer to either the generated function or the
   * 			corresponding regular version.
   *
   **/
  explicit BaseCodegen(const std::string& orig_func_name,
                       FuncPtrType regular_func_ptr,
                       FuncPtrType* ptr_to_chosen_func_ptr)
  : orig_func_name_(orig_func_name),
    unique_func_name_(CodegenInterface::GenerateUniqueName(orig_func_name)),
    regular_func_ptr_(regular_func_ptr),
    ptr_to_chosen_func_ptr_(ptr_to_chosen_func_ptr),
    is_generated_(false) {
    // Initialize the caller to use regular version of target function.
    SetToRegular(regular_func_ptr, ptr_to_chosen_func_ptr);
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
  virtual bool GenerateCodeInternal(gpcodegen::CodegenUtils* codegen_utils) = 0;

 private:
  std::string orig_func_name_;
  std::string unique_func_name_;
  FuncPtrType regular_func_ptr_;
  FuncPtrType* ptr_to_chosen_func_ptr_;
  bool is_generated_;
};
/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_BASE_CODEGEN_H_
