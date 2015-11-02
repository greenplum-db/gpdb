//   Copyright 2016 Pivotal Software, Inc.
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#ifndef BALERION_ANNOTATED_TYPE_H_
#define BALERION_ANNOTATED_TYPE_H_

#include <algorithm>
#include <type_traits>
#include <vector>

#include "balerion/annotated_type_detail.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

namespace balerion {

/** \addtogroup Balerion
 *  @{
 */

/**
 * @brief Augments an llvm::Type with additional information about C++ type
 *        properties that are not captured by the LLVM type system.
 **/
struct AnnotatedType {
  /**
   * @brief Create an AnnotatedType to represent the base scalar type T (not
   *        a pointer or reference type), automatically inferring its
   *        properties.
   *
   * @tparam T A C++ scalar type.
   * @param llvm_type The type's representation in LLVM, obtained by calling
   *        CodeGenerator::GetType<T>() or similar.
   * @return An AnnotatedType that includes additional information about T not
   *         captured by the llvm::Type.
   **/
  template <typename T>
  static AnnotatedType CreateScalar(llvm::Type* llvm_type) {
    return AnnotatedType{
        llvm_type,
        false,  // is_voidptr
        false,  // is_reference
        annotated_type_detail::IsUnsignedAdapter<T>::value,
        annotated_type_detail::IsLong<T>::value,
        annotated_type_detail::IsLongLong<T>::value,
        {std::is_const<T>::value},
        {std::is_volatile<T>::value}};
  }

  /**
   * @brief Create an AnnotatedType to represent a void pointer type T
   *        (optionally with const and/or volatile qualifiers).
   *
   * @tparam T A C++ void pointer type with any combination of const and/or
   *         volatile qualifiers.
   * @param llvm_type The type's representation in LLVM, obtained by calling
   *        CodeGenerator::GetType<T>() or similar.
   * @return An AnnotatedType that includes additional information about the
   *         void pointer type T not captured by the llvm::Type.
   **/
  template <typename T>
  static AnnotatedType CreateVoidPtr(llvm::Type* llvm_type) {
    return AnnotatedType{
        llvm_type,
        true,   // is_voidptr
        false,  // is_reference
        false,  // is_unsigned
        false,  // is_long
        false,  // is_long_long
        {std::is_const<typename std::remove_pointer<T>::type>::value,
            std::is_const<T>::value},
        {std::is_volatile<typename std::remove_pointer<T>::type>::value,
            std::is_volatile<T>::value}};
  }

  // Value semantics.
  AnnotatedType(const AnnotatedType&) = default;
  AnnotatedType(AnnotatedType&&) = default;

  AnnotatedType& operator=(const AnnotatedType&) = default;
  AnnotatedType& operator=(AnnotatedType&&) = default;

  bool operator==(const AnnotatedType& other) const {
    return (llvm_type == other.llvm_type)
           && (is_voidptr == other.is_voidptr)
           && (is_reference == other.is_reference)
           && (explicitly_unsigned == other.explicitly_unsigned)
           && (is_long == other.is_long)
           && (is_long_long == other.is_long_long)
           && (is_const.size() == other.is_const.size())
           && (is_volatile.size() == other.is_volatile.size())
           && (std::equal(is_const.begin(),
                          is_const.end(),
                          other.is_const.begin()))
           && (std::equal(is_volatile.begin(),
                          is_volatile.end(),
                          other.is_volatile.begin()));
  }

  /**
   * @brief Modify an AnnotatedType in place, adding a pointer to it. CV
   *        qualifiers of the pointer are autodetected from the template
   *        parameter T.
   **/
  template <typename T>
  AnnotatedType& AddPointer() {
    llvm_type = llvm_type->getPointerTo();
    is_const.push_back(std::is_const<T>::value);
    is_volatile.push_back(std::is_volatile<T>::value);
    return *this;
  }

  /**
   * @brief Convert the outermost pointer in this AnnotatedType to a reference.
   **/
  AnnotatedType& ConvertToReference() {
    is_reference = true;
    return *this;
  }

  // Type's representation in LLVM;
  llvm::Type* llvm_type;

  // Is this an untyped void* instead of char*?
  bool is_voidptr : 1;

  // Is the outermost pointer actually a C++ reference?
  bool is_reference : 1;

  // Does the innermost scalar type have an explicit unsigned qualifier?
  bool explicitly_unsigned : 1;

  // Is the innermost scalar type the builtin type "long" (which may be 32
  // or 64 bits depending on architecture)?
  bool is_long: 1;

  // Is the innermost scalar type the builtin type "long long"?
  bool is_long_long: 1;

  // Capture CV-qualifiers for chains of pointers/references. Element 0 is the
  // base scalar type, element 1 is the pointer to that type, element 2 is
  // pointer-to-pointer, etc.
  std::vector<bool> is_const;
  std::vector<bool> is_volatile;
};

/** @} */

}  // namespace balerion

#endif  // BALERION_ANNOTATED_TYPE_H_
// EOF
