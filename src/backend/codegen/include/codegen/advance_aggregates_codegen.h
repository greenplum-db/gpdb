//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    advance_aggregates_codegen.h
//
//  @doc:
//    Headers for AdvanceAggregates codegen.
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_ADVANCEAGGREGATES_CODEGEN_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_ADVANCEAGGREGATES_CODEGEN_H_

#include "codegen/base_codegen.h"
#include "codegen/codegen_wrapper.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

class AdvanceAggregatesCodegen: public BaseCodegen<AdvanceAggregatesFn> {
 public:
  /**
   * @brief Constructor
   *
   * @param regular_func_ptr        Regular version of the target function.
   * @param ptr_to_chosen_func_ptr  Reference to the function pointer that the
   *                                caller will call.
   * @param aggstate                The AggState to use for generating code.
   *
   * @note 	The ptr_to_chosen_func_ptr can refer to either the generated
   *        function or the corresponding regular version.
   *
   **/
  explicit AdvanceAggregatesCodegen(CodegenManager* manager,
                                    AdvanceAggregatesFn regular_func_ptr,
                                    AdvanceAggregatesFn* ptr_to_regular_func_ptr,
                                    AggState *aggstate);

  virtual ~AdvanceAggregatesCodegen() = default;

 protected:
  /**
   * @brief Generate code for advance_aggregates.
   *
   * @param codegen_utils
   *
   * @return true on successful generation; false otherwise.
   *
   * This implementation does not support percentile and ordered aggregates.
   *
   * If at execution time, we see any of the above types of attributes,
   * we fall backs to the regular function.
   *
   */
  bool GenerateCodeInternal(gpcodegen::GpCodegenUtils* codegen_utils) final;

 private:
  AggState *aggstate_;

  static constexpr char kAdvanceAggregatesPrefix[] = "AdvanceAggregates";

  /**
   * @brief Generates runtime code that implements advance_aggregates.
   *
   * @param codegen_utils Utility to ease the code generation process.
   * @return true on successful generation.
   **/
  bool GenerateAdvanceAggregates(gpcodegen::GpCodegenUtils* codegen_utils);

  /**
   * @brief Generates runtime code that implements advance_transition_function.
   * It is called by GenerateAdvanceAggregates to generate code for a given
   * aggregate function.
   *
   * @param codegen_utils Utility to ease the code generation process.
   * @param llvm_pergroup_arg LLVM pointer to pergroup argument of regular
   *        advance_aggregates
   * @param aggno ith aggregate function
   * @param advance_aggregates_func LLVM function pointer to the code generated
   *        advance_aggregate function
   * @param error_block LLVM block for treating errors
   * @param llvm_arg argument of aggregate function
   *
   * @return true on successful generation; false otherwise.
   */
  bool GenerateAdvanceTransitionFunction(
      gpcodegen::GpCodegenUtils* codegen_utils,
      llvm::Value* llvm_pergroup_arg,
      int aggno,
      llvm::Function* advance_aggregates_func,
      llvm::BasicBlock* error_block,
      llvm::Value *llvm_arg);

};

/** @} */

}  // namespace gpcodegen
#endif  // GPCODEGEN_ADVANCEAGGREGATES_CODEGEN_H_
