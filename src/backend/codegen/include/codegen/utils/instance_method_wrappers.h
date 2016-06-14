//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    instance_method_wrappers.h
//
//  @doc:
//    Templated helper class that provides a static Call() method which
//    wraps an invocation of an instance method of a class.
//
//  @test:
//
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_INSTANCE_METHOD_WRAPPERS_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_INSTANCE_METHOD_WRAPPERS_H_

#include <utility>

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

/**
 * @brief Templated helper class that provides a static Call() method which
 *        wraps an invocation of an instance method of a class.
 *
 * @note Writing out the explicit MethodType parameter can be cumbersome and
 *       error-prone. See the macros GPCODEGEN_WRAP_METHOD(),
 *       GPCODEGEN_WRAP_OVERLOADED_METHOD(), and
 *       GPCODEGEN_WRAP_OVERLOADED_METHOD_ZERO_ARGS() which provide some
 *       additional syntactic sugar to make this easier.
 *
 * @tparam MethodType A pointer-to-method type.
 * @tparam MethodPtr A pointer-to-method.
 **/
template <typename MethodType, MethodType MethodPtr>
class InstanceMethodWrapper;

/**
 * @brief Partial specialization of InstanceMethodWrapper for methods without
 *        a const and/or volatile qualifier.
 **/
template <typename ClassType, typename ReturnType, typename... ArgumentTypes,
          ReturnType (ClassType::*MethodPtr)(ArgumentTypes...)>
class InstanceMethodWrapper<ReturnType (ClassType::*)(ArgumentTypes...),
                            MethodPtr> {
 public:
  static ReturnType Call(ClassType* object,
                         ArgumentTypes... args) {
    return (object->*MethodPtr)(args...);
  }
};

/**
 * @brief Partial specialization of InstanceMethodWrapper for const-qualified
 *        methods.
 **/
template <typename ClassType, typename ReturnType, typename... ArgumentTypes,
          ReturnType (ClassType::*MethodPtr)(ArgumentTypes...) const>
class InstanceMethodWrapper<ReturnType (ClassType::*)(ArgumentTypes...) const,
                            MethodPtr> {
 public:
  static ReturnType Call(const ClassType* object,
                         ArgumentTypes... args) {
    return (object->*MethodPtr)(args...);
  }
};

/**
 * @brief Partial specialization of InstanceMethodWrapper for volatile-qualified
 *        methods.
 **/
template <typename ClassType, typename ReturnType, typename... ArgumentTypes,
          ReturnType (ClassType::*MethodPtr)(ArgumentTypes...) volatile>
class InstanceMethodWrapper<
    ReturnType (ClassType::*)(ArgumentTypes...) volatile,
    MethodPtr> {
 public:
  static ReturnType Call(volatile ClassType* object,
                         ArgumentTypes... args) {
    return (object->*MethodPtr)(args...);
  }
};

/**
 * @brief Partial specialization of InstanceMethodWrapper for methods which are
 *        both const and volatile qualified.
 **/
template <typename ClassType, typename ReturnType, typename... ArgumentTypes,
          ReturnType (ClassType::*MethodPtr)(ArgumentTypes...) const volatile>
class InstanceMethodWrapper<
    ReturnType (ClassType::*)(ArgumentTypes...) const volatile,
    MethodPtr> {
 public:
  static ReturnType Call(const volatile ClassType* object,
                         ArgumentTypes... args) {
    return (object->*MethodPtr)(args...);
  }
};

/**
 * @brief Macro which provides some syntactic sugar for
 *        InstanceMethodWrapper::Call() by using
 *        automatic type deduction for the type of a pointer-to-method.
 *
 * @note This macro doesn't work with overloaded methods. See
 *       GPCODEGEN_WRAP_OVERLOADED_METHOD() instead.
 *
 * @param method_ptr A pointer-to-method.
 **/
#define GPCODEGEN_WRAP_METHOD(method_ptr)  \
    ::gpcodegen::InstanceMethodWrapper<decltype(method_ptr), (method_ptr)>::Call

/**
 * @brief Macro which provides syntactic sugar for InstanceMethodWrapper::Call()
 *        to be used on an overloaded class method. Provides a wrapper for this
 *        specific version of an overloaded method:
 *        `ReturnType ClassName::MethodName(...) Qualifiers`
 *
 * @note This macro is for overloads that take at least one argument. See
 *       GPCODEGEN_WRAP_OVERLOADED_METHOD_ZERO_ARGS() for overloads that take no
 *       argument (other than the implicit 'this' pointer). (Many compilers will
 *       actually allow zero variadic arguments in a variadic macro invocation,
 *       but this is an extension and not an official requirement of the C
 *       preprocessor standard).
 * @note The return type of an overloaded function or method is not normally
 *       used in overload resolution by the C++ compiler, but ReturnType must
 *       nonetheless be specified correctly here because it is a part of the
 *       method's type signature, and there is no way to automatically deduce it
 *       when the method itself is a template parameter.
 *
 * @param ReturnType The type that the desired overloaded version of MethodName
 *        returns.
 * @param ClassName The name of the class that the overloaded method is a member
 *        of.
 * @param MethodName The name of the overloaded method.
 * @param Qualifiers Method-level qualifiers (e.g. 'const', 'volatile', or
 *        'const volatile'. This is OPTIONAL and should be left empty (by
 *        writing two successive commas in the macro argument list) if the
 *        desired version of the method has no cv-qualifiers.
 * @param ... Variadic arguments for the types of parameters passed to the
 *        overloaded method.
 **/
#define GPCODEGEN_WRAP_OVERLOADED_METHOD(ReturnType,         \
                                        ClassName,          \
                                        MethodName,         \
                                        Qualifiers,         \
                                        ...)                \
    ::gpcodegen::InstanceMethodWrapper<                      \
        ReturnType (ClassName::*)(__VA_ARGS__) Qualifiers,  \
        &ClassName::MethodName>                             \
            ::Call

/**
 * @brief Special version of GPCODEGEN_WRAP_OVERLOADED_METHOD() for overloads
 *        that take no argument other than the implicit 'this' pointer.
 *
 * @param ReturnType The type that the desired overloaded version of MethodName
 *        returns.
 * @param ClassName The name of the class that the overloaded method is a member
 *        of.
 * @param MethodName The name of the overloaded method.
 * @param Qualifiers Method-level qualifiers (e.g. 'const', 'volatile', or
 *        'const volatile'. This is OPTIONAL and should be left empty (by
 *        writing a comma and then the closing right-paren immediately
 *        following) if the desired version of the method has no cv-qualifiers.
 **/
#define GPCODEGEN_WRAP_OVERLOADED_METHOD_ZERO_ARGS(ReturnType,  \
                                                  ClassName,   \
                                                  MethodName,  \
                                                  Qualifiers)  \
    ::gpcodegen::InstanceMethodWrapper<                         \
        ReturnType (ClassName::*)() Qualifiers,                \
        &ClassName::MethodName>                                \
            ::Call

/**
 * @brief Templated wrapper function that invokes the 'new' operator to create
 *        a new object on the heap.
 *
 * @tparam ClassType The type of the object to create.
 * @tparam ArgumentTypes... The types of arguments (if any) to ClassType's
 *         constructor.
 * @param args The arguments (if any) to forward to a constructor of ClassType.
 * @return A new heap-allocated instance of ClassType (i.e. the result of
 *         calling `new ClassType(args...)`).
 **/
template <typename ClassType, typename... ArgumentTypes>
ClassType* WrapNew(ArgumentTypes... args) {
  return new ClassType(args...);
}

/**
 * @brief Templated wrapper function that invokes the placement form of the
 *        'new' operator to create a new object a a specific pre-allocated place
 *        in memory.
 *
 * @warning All of the usual caveats about placement new also apply here, e.g.
 *          memory must be allocated and of a large enough size and proper
 *          alignment for ClassType.
 *
 * @tparam ClassType The type of the object to create.
 * @tparam ArgumentTypes... The types of arguments (if any) to ClassType's
 *         constructor.
 * @param place The location in memory to place the new instance of ClassType.
 * @param args The arguments (if any) to forward to a constructor of ClassType.
 * @return A new instance of ClassType placed at place (i.e. the result of
 *         calling `new(place) ClassType(args...)`).
 **/
template <typename ClassType, typename... ArgumentTypes>
ClassType* WrapPlacementNew(void* place, ArgumentTypes... args) {
  return new(place) ClassType(args...);
}

/**
 * @brief Templated wrapper function that invokes the 'new' operator with the
 *        move constructor for a class (if any) to move-from an existing object.
 *
 * @note This template works around the fact that CodegenUtils does not handle
 *       rvalue-references. It takes a pointer which it dereferences and
 *       converts to an rvalue-reference that is moved-from.
 *
 * @tparam ClassType The type of object to create.
 * @param original A pointer to an instance of ClassType that is moved-from.
 * @return A new heap-allocated instance of ClassType that is moved-from
 *         original.
 **/
template <typename ClassType>
ClassType* WrapNewMove(ClassType* original) {
  return new ClassType(std::move(*original));
}

/**
 * @brief Templated wrapper function that invokes the placement form of the
 *        'new' operator with the move constructor for a class (if any) to
 *        move-from an existing object.
 *
 * @note This template works around the fact that CodegenUtils does not handle
 *       rvalue-references. It takes a pointer which it dereferences and
 *       converts to an rvalue-reference that is moved-from.
 * @warning All of the usual caveats about placement new also apply here, e.g.
 *          memory must be allocated and of a large enough size and proper
 *          alignment for ClassType.
 *
 * @tparam ClassType The type of object to create.
 * @param place The location in memory to place the new instance of ClassType.
 * @param original A pointer to an instance of ClassType that is moved-from.
 * @return A new instance of ClassType placed at place and moved-from original.
 **/
template <typename ClassType>
ClassType* WrapPlacementNewMove(void* place, ClassType* original) {
  return new(place) ClassType(std::move(*original));
}

/**
 * @brief Templated wrapper function that invokes the 'delete' operator to
 *        destroy an object.
 *
 * @tparam ClassType The type of object to destroy.
 * @param object A pointer to an object to delete.
 **/
template <typename ClassType>
void WrapDelete(ClassType* object) {
  delete object;
}

/**
 * @brief Templated wrapper function that invokes an object's destructor without
 *        deleting it (i.e. the destructor is run, but memory for the object
 *        itself is not freed).
 *
 * @tparam ClassType The type of object to invoke the destructor on.
 * @param object A pointer to an object to invoke the destructor on.
 **/
template <typename ClassType>
void WrapDestructor(ClassType* object) {
  object->~ClassType();
}

/** @} */

}  // namespace gpcodegen

#endif  // GPCODEGEN_INSTANCE_METHOD_WRAPPERS_H_
