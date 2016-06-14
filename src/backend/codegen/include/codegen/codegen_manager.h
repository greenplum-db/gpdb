//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_manager.h
//
//  @doc:
//    Object that manage all CodegenInterface and CodegenUtils
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_CODEGEN_MANAGER_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_CODEGEN_MANAGER_H_

#include <memory>
#include <vector>
#include <string>

#include "codegen/utils/macros.h"
#include "codegen/codegen_wrapper.h"

namespace gpcodegen {
/** \addtogroup gpcodegen
 *  @{
 */

// Forward declaration of CodegenUtils to manage llvm module
class CodegenUtils;

// Forward declaration of a CodegenInterface that will be managed by manager
class CodegenInterface;

/**
 * @brief Object that manages all code gen.
 **/
class CodegenManager {
 public:
  /**
   * @brief Constructor.
   *
   * @param module_name A human-readable name for the module that this
   *        CodegenManager will manage.
   **/
  explicit CodegenManager(const std::string& module_name);

  ~CodegenManager() = default;

  /**
   * @brief Enroll a code generator with manager
   *
   * @note Manager manages the memory of enrolled generator.
   *
   * @param funcLifespan Life span of the enrolling generator. Based on life span,
   *                     corresponding CodegenUtils will be used for code generation
   * @param generator    Generator that needs to be enrolled with manager.
   * @return true on successful enrollment.
   **/
  bool EnrollCodeGenerator(CodegenFuncLifespan funcLifespan,
                           CodegenInterface* generator);

  /**
   * @brief Request all enrolled generators to generate code.
   *
   * @return The number of enrolled codegen that successfully generated code.
   **/
  unsigned int GenerateCode();

  /**
   * @brief Compile all the generated functions. On success,
   *        a pointer to the generated method becomes available to the caller.
   *
   * @return The number of enrolled codegen that successully generated code
   *         and 0 on failure
   **/
  unsigned int PrepareGeneratedFunctions();

  /**
   * @brief 	Notifies the manager of a parameter change.
   *
   * @note 	This is called during a ReScan or other parameter change process.
   * 			Upon receiving this notification the manager may invalidate all the
   * 			generated code that depend on parameters.
   *
   **/
  void NotifyParameterChange();

  /**
   * @brief Invalidate all generated functions.
   *
   * @return true if successfully invalidated.
   **/
  bool InvalidateGeneratedFunctions();

  /**
   * @return Number of enrolled generators.
   **/
  size_t GetEnrollmentCount() {
    return enrolled_code_generators_.size();
  }

 private:
  // CodegenUtils provides a facade to LLVM subsystem.
  std::unique_ptr<gpcodegen::CodegenUtils> codegen_utils_;

  std::string module_name_;

  // List of all enrolled code generators.
  std::vector<std::unique_ptr<CodegenInterface>> enrolled_code_generators_;

  DISALLOW_COPY_AND_ASSIGN(CodegenManager);
};

/** @} */

}  // namespace gpcodegen
#endif  // GPCODEGEN_CODEGEN_MANAGER_H_
