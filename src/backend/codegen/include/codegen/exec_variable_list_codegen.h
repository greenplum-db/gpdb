//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    exec_variable_list.h
//
//  @doc:
//    Headers for slot_deform_tuple codegen
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_EXECVARIABLELIST_CODEGEN_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_EXECVARIABLELIST_CODEGEN_H_

#include "codegen/codegen_wrapper.h"
#include "codegen/base_codegen.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

class ExecVariableListCodegen: public BaseCodegen<ExecVariableListFn> {
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
  explicit ExecVariableListCodegen(ExecVariableListFn regular_func_ptr,
                                   ExecVariableListFn* ptr_to_regular_func_ptr,
                                   ProjectionInfo* proj_info,
                                   TupleTableSlot* slot);

  virtual ~ExecVariableListCodegen() = default;

 protected:
  /**
   * @brief Generate code for the code path ExecVariableList > slot_getattr >
   * _slot_getsomeattrs > slot_deform_tuple.
   *
   * @param codegen_utils
   *
   * @return true on successful generation; false otherwise.
   *
   * @note Generate code for code path ExecVariableList > slot_getattr >
   * _slot_getsomeattrs > slot_deform_tuple. The regular implementation of
   * ExecvariableList, for each attribute <i>A</i> in the target list, retrieves
   * the slot that <i>A</> comes from and calls slot_getattr. slot_get_attr() will
   * eventually call slot_deform_tuple (through _slot_getsomeattrs), which fetches
   * all yet unread attributes of the slot until the given attribute.
   *
   * This function generates the code for the case that all the
   * attributes in target list use the same slot (created during a
   * scan i.e, ecxt_scantuple). Moreover, instead of looping over the target
   * list one at a time, this approach uses slot_getattr only once, with the
   * largest attribute index from the target list.
   *
   * If code generation is not possible for any reason (e.g., attributes
   * use different slots), then the function returns false and the codegen manager
   * will manage the clean up.
   *
   * This implementation does not support:
   *  (1) Null attributes
   *  (2) Variable length attributes
   *  (3) Attributes passed by reference
   *
   * If at execution time, we see any of the above types of attributes, we fall backs to
   * the regular function.
   */
  bool GenerateCodeInternal(gpcodegen::CodegenUtils* codegen_utils) final;

 private:
  ProjectionInfo* proj_info_;
  TupleTableSlot* slot_;


  static constexpr char kExecVariableListPrefix[] = "ExecVariableList";

  /**
   * @brief Generates runtime code that implements ExecVariableList.
   *
   * @param codegen_utils Utility to ease the code generation process.
   * @return true on successful generation.
   **/
  bool GenerateExecVariableList(gpcodegen::CodegenUtils* codegen_utils);
};

/** @} */

}  // namespace gpcodegen
#endif  // GPCODEGEN_EXECVARIABLELIST_CODEGEN_H_
