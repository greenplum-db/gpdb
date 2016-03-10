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

#ifndef GPCODEGEN_ANNOTATED_TYPE_DETAIL_H_
#define GPCODEGEN_ANNOTATED_TYPE_DETAIL_H_

#include <type_traits>

namespace gpcodegen {

// Additional type_traits-style templates used to implement AnnotatedType are
// contained in this nested detail namespace. They are not considered part of
// the public API.
namespace annotated_type_detail {

// Adapter for std::is_unsigned with some additional specializations. Detects
// the signedness of the underlying type for enums, and does NOT consider bool
// to be unsigned (although bool is an unsigned type, it is illegal to use it
// with the 'unsigned' qualifier keyword).
template <typename T, typename Enable = void>
struct IsUnsignedAdapter
    : std::integral_constant<bool, std::is_unsigned<T>::value> {};

template <typename EnumT>
struct IsUnsignedAdapter<
    EnumT,
    typename std::enable_if<std::is_enum<EnumT>::value>::type>
    : IsUnsignedAdapter<typename std::underlying_type<EnumT>::type> {};

template <typename BoolT>
struct IsUnsignedAdapter<
    BoolT,
    typename std::enable_if<
        std::is_same<bool,
                     typename std::remove_cv<BoolT>::type>::value>::type>
    : std::integral_constant<bool, false> {};


// Traits template that strips away "const", "volatile", and "unsigned"
// qualifiers from a scalar type. Also has a partial specialization for enums
// that converts the enum to its underlying integral type and does the same
// transformations.
template <typename T, typename Enable = void>
struct BaseScalar {
  typedef typename std::remove_cv<T>::type type;
};

// Explicit specializations for bool with different CV qualifiers. We must
// explicitly specialize for bool because std::make_signed<bool> (used in the
// general integer case) is an incomplete type.
template <>
struct BaseScalar<bool> {
  typedef bool type;
};

template <>
struct BaseScalar<const bool> {
  typedef bool type;
};

template <>
struct BaseScalar<volatile bool> {
  typedef bool type;
};

template <>
struct BaseScalar<const volatile bool> {
  typedef bool type;
};

// Partial specialization for integer types.
template <typename IntegralT>
struct BaseScalar<
    IntegralT,
    typename std::enable_if<std::is_integral<IntegralT>::value>::type> {
  typedef typename std::make_signed<
      typename std::remove_cv<IntegralT>::type>::type type;
};

// Partial specialization for enums, which maps to the underlying integer type.
template <typename EnumT>
struct BaseScalar<EnumT,
                  typename std::enable_if<std::is_enum<EnumT>::value>::type>
    : BaseScalar<typename std::underlying_type<EnumT>::type> {};

// Detect if T (or the underlying type of T, if T is an enum), stripped of CV
// and unsigned qualifiers, is the same as the built-in type long.
template <typename T>
using IsLong = std::is_same<long,  // NOLINT(runtime/int)
                            typename BaseScalar<T>::type>;

// Detect if T (or the underlying type of T, if T is an enum), stripped of CV
// and unsigned qualifiers, is the same as the built-in type long long.
template <typename T>
using IsLongLong = std::is_same<long long,  // NOLINT(runtime/int)
                                typename BaseScalar<T>::type>;

}  // namespace annotated_type_detail
}  // namespace gpcodegen

#endif  // GPCODEGEN_ANNOTATED_TYPE_DETAIL_H_
// EOF
