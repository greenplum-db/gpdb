//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    ExecQual_codegen.h
//
//  @doc:
//    Headers for ExecQual codegen.
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_EXECQUAL_CODEGEN_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_EXECQUAL_CODEGEN_H_

#include "codegen/codegen_wrapper.h"
#include "codegen/base_codegen.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

class ExecQualCodegen: public BaseCodegen<ExecQualFn> {
 public:
  /**
   * @brief Constructor
   *
   * @param regular_func_ptr       Regular version of the target function.
   * @param ptr_to_chosen_func_ptr Reference to the function pointer that the caller will call.
   * @param slot         The slot to use for generating code.
   *
   * @note 	The ptr_to_chosen_func_ptr can refer to either the generated function or the
   * 			corresponding regular version.
   *
   **/
  explicit ExecQualCodegen(ExecQualFn regular_func_ptr,
                                   ExecQualFn* ptr_to_regular_func_ptr,
                                   PlanState *planstate);

  virtual ~ExecQualCodegen() = default;

 protected:
  /**
   * @brief Generate code for ExecQual.
   *
   * @param codegen_utils
   *
   * @return true on successful generation; false otherwise.
   *
   * @note Currently, it simply falls back to regular ExecQual.
   */
  bool GenerateCodeInternal(gpcodegen::CodegenUtils* codegen_utils) final;

 private:
  PlanState *planstate_;


  static constexpr char kExecQualPrefix[] = "ExecQual";

  /**
   * @brief Generates runtime code that implements ExecQual.
   *
   * @param codegen_utils Utility to ease the code generation process.
   * @return true on successful generation.
   **/
  bool GenerateExecQual(gpcodegen::CodegenUtils* codegen_utils);
};

/** @} */

}  // namespace gpcodegen
#endif  // GPCODEGEN_EXECQUAL_CODEGEN_H_
