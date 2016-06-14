//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_interface.h
//
//  @doc:
//    Interface for all code generator
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_CODEGEN_INTERFACE_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_CODEGEN_INTERFACE_H_

#include <string>
#include <vector>


namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

// Forward declaration
class CodegenUtils;

/**
 * @brief Interface for all code generators.
 **/
class CodegenInterface {
 public:
  virtual ~CodegenInterface() = default;

  /**
   * @brief Generates specialized code at run time.
   *
   *
   * @param codegen_utils Utility to ease the code generation process.
   * @return true on successful generation.
   **/
  virtual bool GenerateCode(gpcodegen::CodegenUtils* codegen_utils) = 0;

  /**
   * @brief Sets up the caller to use the corresponding regular version of the
   *        target function.
   *
   *
   * @return true on setting to regular version.
   **/
  virtual bool SetToRegular() = 0;

  /**
   * @brief Sets up the caller to use the generated function instead of the
   *        regular version.
   *
   * @param codegen_utils Facilitates in obtaining the function pointer from
   *        the compiled module.
   * @return true on successfully setting to generated functions
   **/
  virtual bool SetToGenerated(gpcodegen::CodegenUtils* codegen_utils) = 0;

  /**
   * @brief Resets the state of the generator, including reverting back to
   *        the regular version of the function.
   *
   **/
  virtual void Reset() = 0;

  /**
   *
   * @return Original function name.
   *
   **/
  virtual const std::string& GetOrigFuncName() const = 0;

  /**
   * @return Unique function name of the generated function to avoid name collision.
   *
   **/
  virtual const std::string& GetUniqueFuncName() const = 0;

  /**
   * @return true if the code generation is successful.
   *
   **/
  virtual bool IsGenerated() const = 0;

 protected:
  /**
   * @brief	Utility function to construct a unique function name from the
   * 			original function name by appending a numeric suffix.
   *
   * @param orig_func_name	Function name that needs to be made unique.
   * @return 	Unique string for given input string.
   *
   **/
  static std::string GenerateUniqueName(const std::string& orig_func_name);

 private:
  // Unique counter for all instances of Codegen Interface.
  static unsigned unique_counter_;
};

/** @} */

}  // namespace gpcodegen

#endif  // GPCODEGEN_CODEGEN_INTERFACE_H_
