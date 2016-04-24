//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    code_generator_unittest.cc
//
//  @doc:
//    Unit tests for utils/codegen_utils.cc
//
//  @test:
//
//---------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "codegen/utils/codegen_utils.h"
#include "codegen/utils/annotated_type.h"
#include "codegen/utils/instance_method_wrappers.h"
#include "codegen/utils/utility.h"
#include "gtest/gtest.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Casting.h"

namespace gpcodegen {

namespace {

// Dummy enums with various properties for test purposes.
enum SimpleEnum {
  kSimpleEnumA,
  kSimpleEnumB,
  kSimpleEnumC
};

// The compiler must choose a signed representation if one of the cases is
// explicitly set to a negative value.
enum SignedSimpleEnum {
  kSignedSimpleEnumA = -1,
  kSignedSimpleEnumB,
  kSignedSimpleEnumC
};

// C++11 strongly-typed enum.
enum class StronglyTypedEnum {
  kCaseA,
  kCaseB,
  kCaseC
};

// C++11 strongly-typed enum that must be signed.
enum class SignedStronglyTypedEnum {
  kCaseA = -1,
  kCaseB,
  kCaseC
};

// C++11 strongly-typed enum with explicit underlying type.
enum class StronglyTypedEnumUint64 : std::uint64_t {
  kCaseA,
  kCaseB,
  kCaseC
};

// Dummy struct.
struct DummyStruct {
  int int_field;
  bool bool_field;
  double double_field;
};

// A dummy struct with several char fields that map to LLVM's i8 type. Used to
// check that CodegenUtils::GetPointerToMember() works correctly when the type
// of a pointer to the field to be accessed is the same as the type used for
// underlying pointer arithmetic.
struct DummyStructWithCharFields {
  char front_char;
  char* char_ptr;
  char back_char;
};

// A struct that nests other structs and includes a pointer to another instance
// of its own type.
struct Matryoshka {
  DummyStructWithCharFields nested_dummy_struct_with_char_fields;
  char non_nested_char;
  int non_nested_int;
  Matryoshka* ptr_to_peer;
  DummyStruct nested_dummy_struct;
};

// Dummy abstract base class.
class DummyAbstractBaseClass {
 public:
  explicit DummyAbstractBaseClass(int payload)
      : payload_(payload) {
  }

  virtual ~DummyAbstractBaseClass() = 0;

  virtual int TransformPayload() const = 0;

 protected:
  int payload_;
};

DummyAbstractBaseClass::~DummyAbstractBaseClass() {
}

// One version of a derived class.
class Negater : public DummyAbstractBaseClass {
 public:
  explicit Negater(int payload)
      : DummyAbstractBaseClass(payload) {
  }

  ~Negater() override {
  }

  int TransformPayload() const override {
    return -payload_;
  }
};

// Another version of a derived class.
class Squarer : public DummyAbstractBaseClass {
 public:
  explicit Squarer(int payload)
      : DummyAbstractBaseClass(payload) {
  }

  ~Squarer() override {
  }

  int TransformPayload() const override {
    return payload_ * payload_;
  }
};

// All-static class that wraps a global int variable and has methods to set and
// get it.
class StaticIntWrapper {
 public:
  static void Set(const int value) {
    wrapped_value_ = value;
  }

  static int Get() {
    return wrapped_value_;
  }

 private:
  // Undefined default constructor. Class is all-static and should not be
  // instantiated.
  StaticIntWrapper();

  static int wrapped_value_;
};

int StaticIntWrapper::wrapped_value_;

// Toy object used to test instance method invocation.
template <typename T>
class Accumulator {
 public:
  explicit Accumulator(const T initial)
      : total_(initial) {
  }

  void Accumulate(const T arg) {
    total_ += arg;
  }

  const T Get() const {
    return total_;
  }

 private:
  T total_;
};

// When we GetAnnotatedType<IntegerType>(), do we expect 'is_long' to be true?
template <typename IntegerType, typename Enable = void>
struct ExpectLong
    : std::integral_constant<
        bool,
        std::is_same<IntegerType, long>::value  // NOLINT(runtime/int)
            || std::is_same<IntegerType,
                            unsigned long>::value> {};  // NOLINT(runtime/int)

// Partial specialization for enums.
template <typename EnumT>
struct ExpectLong<EnumT,
                  typename std::enable_if<std::is_enum<EnumT>::value>::type>
    : ExpectLong<typename std::underlying_type<EnumT>::type> {};

// Similar for 'is_long_long'.
template <typename IntegerType, typename Enable = void>
struct ExpectLongLong
    : std::integral_constant<
        bool,
        std::is_same<IntegerType, long long>::value  // NOLINT(runtime/int)
            || std::is_same<
                IntegerType,
                unsigned long long>::value> {};  // NOLINT(runtime/int)

template <typename EnumT>
struct ExpectLongLong<EnumT,
                      typename std::enable_if<std::is_enum<EnumT>::value>::type>
    : ExpectLongLong<typename std::underlying_type<EnumT>::type> {};

}  // namespace

// Test environment to handle global per-process initialization tasks for all
// tests.
class CodegenUtilsTestEnvironment : public ::testing::Environment {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(CodegenUtils::InitializeGlobal());
  }
};

class CodegenUtilsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    codegen_utils_.reset(new CodegenUtils("test_module"));
  }

  // Helper method for GetScalarTypeTest. Tests CodegenUtils::GetType() and
  // CodegenUtils::GetAnnotatedType() for an 'IntegerType' and its
  // const-qualified version.
  template <typename IntegerType>
  void CheckGetIntegerType() {
    llvm::Type* llvm_type = codegen_utils_->GetType<IntegerType>();
    ASSERT_NE(llvm_type, nullptr);
    EXPECT_TRUE(llvm_type->isIntegerTy(sizeof(IntegerType) << 3));

    // Check extra information from AnnotatedType.
    AnnotatedType annotated_type
        = codegen_utils_->GetAnnotatedType<IntegerType>();
    ASSERT_NE(annotated_type.llvm_type, nullptr);
    EXPECT_FALSE(annotated_type.is_voidptr);
    EXPECT_FALSE(annotated_type.is_reference);
    EXPECT_EQ(std::is_unsigned<IntegerType>::value,
              annotated_type.explicitly_unsigned);
    EXPECT_EQ(ExpectLong<IntegerType>::value, annotated_type.is_long);
    EXPECT_EQ(ExpectLongLong<IntegerType>::value, annotated_type.is_long_long);
    ASSERT_EQ(1u, annotated_type.is_const.size());
    EXPECT_FALSE(annotated_type.is_const.front());
    ASSERT_EQ(1u, annotated_type.is_volatile.size());
    EXPECT_FALSE(annotated_type.is_volatile.front());

    // Also check with const-qualifier.
    llvm_type = codegen_utils_->GetType<const IntegerType>();
    ASSERT_NE(llvm_type, nullptr);
    EXPECT_TRUE(llvm_type->isIntegerTy(sizeof(IntegerType) << 3));

    annotated_type = codegen_utils_->GetAnnotatedType<const IntegerType>();
    ASSERT_NE(annotated_type.llvm_type, nullptr);
    EXPECT_FALSE(annotated_type.is_voidptr);
    EXPECT_FALSE(annotated_type.is_reference);
    EXPECT_EQ(std::is_unsigned<IntegerType>::value,
              annotated_type.explicitly_unsigned);
    EXPECT_EQ(ExpectLong<IntegerType>::value, annotated_type.is_long);
    EXPECT_EQ(ExpectLongLong<IntegerType>::value, annotated_type.is_long_long);
    ASSERT_EQ(1u, annotated_type.is_const.size());
    EXPECT_TRUE(annotated_type.is_const.front());
    ASSERT_EQ(1u, annotated_type.is_volatile.size());
    EXPECT_FALSE(annotated_type.is_volatile.front());
  }

  // Helper method for GetScalarTypeTest. Tests CodegenUtils::GetType() for an
  // 'EnumType' that is expected to map to 'EquivalentIntegerType'.
  template <typename EnumType, typename EquivalentIntegerType>
  void CheckGetEnumType() {
    llvm::Type* llvm_type = codegen_utils_->GetType<EnumType>();
    ASSERT_NE(llvm_type, nullptr);
    EXPECT_TRUE(llvm_type->isIntegerTy(sizeof(EquivalentIntegerType) << 3));

    // Check extra information from AnnotatedType.
    AnnotatedType annotated_type
        = codegen_utils_->GetAnnotatedType<EnumType>();
    ASSERT_NE(annotated_type.llvm_type, nullptr);
    EXPECT_FALSE(annotated_type.is_voidptr);
    EXPECT_FALSE(annotated_type.is_reference);
    EXPECT_EQ(std::is_unsigned<EquivalentIntegerType>::value,
              annotated_type.explicitly_unsigned);
    EXPECT_EQ(ExpectLong<EquivalentIntegerType>::value, annotated_type.is_long);
    EXPECT_EQ(ExpectLongLong<EquivalentIntegerType>::value,
              annotated_type.is_long_long);
    ASSERT_EQ(1u, annotated_type.is_const.size());
    EXPECT_FALSE(annotated_type.is_const.front());
    ASSERT_EQ(1u, annotated_type.is_volatile.size());
    EXPECT_FALSE(annotated_type.is_volatile.front());

    // Also check with const-qualifier.
    llvm_type = codegen_utils_->GetType<const EnumType>();
    ASSERT_NE(llvm_type, nullptr);
    EXPECT_TRUE(llvm_type->isIntegerTy(sizeof(EquivalentIntegerType) << 3));

    annotated_type = codegen_utils_->GetAnnotatedType<const EnumType>();
    ASSERT_NE(annotated_type.llvm_type, nullptr);
    EXPECT_FALSE(annotated_type.is_voidptr);
    EXPECT_FALSE(annotated_type.is_reference);
    EXPECT_EQ(std::is_unsigned<EquivalentIntegerType>::value,
              annotated_type.explicitly_unsigned);
    EXPECT_EQ(ExpectLong<EquivalentIntegerType>::value, annotated_type.is_long);
    EXPECT_EQ(ExpectLongLong<EquivalentIntegerType>::value,
              annotated_type.is_long_long);
    ASSERT_EQ(1u, annotated_type.is_const.size());
    EXPECT_TRUE(annotated_type.is_const.front());
    ASSERT_EQ(1u, annotated_type.is_volatile.size());
    EXPECT_FALSE(annotated_type.is_volatile.front());

    // Check that the Type used for the enum is the exact same as the Type that
    // would be used for the underlying integer.
    llvm::Type* int_llvm_type
        = codegen_utils_->GetType<EquivalentIntegerType>();
    EXPECT_EQ(llvm_type, int_llvm_type);
  }

  // Helper method for GetPointerTypeTest. Checks that the annotations produced
  // by CodegenUtils::GetAnnotatedType() are as expected for a given
  // 'PointerType' (which may also be a reference).
  //
  // NOTE(chasseur): This works with up to 2 levels of indirection (e.g. pointer
  // to pointer) which is as far as we test. CodegenUtils::GetAnnotatedType()
  // should work for arbitrarily deep chains of pointers, but this check method
  // only looks at most 2 pointers deep.
  template <typename PointerType>
  static void CheckAnnotationsForPointer(const AnnotatedType& annotated_type) {
    // Strip pointers and references to get the innermost scalar type.
    typedef typename std::remove_pointer<
        typename std::remove_pointer<
            typename std::remove_reference<PointerType>::type>::type>::type
                ScalarType;

    // Is this a void* or pointer to user-defined class or struct represented as
    // a void*?
    EXPECT_EQ(std::is_void<ScalarType>::value
                  || std::is_class<ScalarType>::value,
              annotated_type.is_voidptr);
    // Is the outermost type a pointer or a reference?
    EXPECT_EQ(std::is_reference<PointerType>::value,
              annotated_type.is_reference);
    // Is the inner scalar type explicitly unsigned (i.e. an unsigned arithmetic
    // type other than bool, OR an enum that is represented as such a type)?
    EXPECT_EQ(annotated_type_detail::IsUnsignedAdapter<ScalarType>::value,
              annotated_type.explicitly_unsigned);
    // Is the inner scalar type long or long long (or an enum that has long or
    // long long as its underlying type)?
    EXPECT_EQ(ExpectLong<typename std::remove_cv<ScalarType>::type>::value,
              annotated_type.is_long);
    EXPECT_EQ(ExpectLongLong<typename std::remove_cv<ScalarType>::type>::value,
              annotated_type.is_long_long);

    // Check CV-qualifiers for chains of pointers and/or references.
    if (std::is_reference<PointerType>::value) {
      if (std::is_pointer<
              typename std::remove_reference<PointerType>::type>::value) {
        // Reference to pointer.
        ASSERT_EQ(3u, annotated_type.is_const.size());

        // Constness of innermost scalar type.
        EXPECT_EQ(std::is_const<ScalarType>::value,
                  annotated_type.is_const[0]);
        // Constness of intermediate pointertype.
        EXPECT_EQ(std::is_const<
                      typename std::remove_reference<PointerType>::type>::value,
                  annotated_type.is_const[1]);
        // Outermost reference type can not have any cv-qualifiers.
        EXPECT_FALSE(annotated_type.is_const[2]);

        // Similar for volatile qualifiers.
        ASSERT_EQ(3u, annotated_type.is_volatile.size());
        EXPECT_EQ(std::is_volatile<ScalarType>::value,
                  annotated_type.is_volatile[0]);
        EXPECT_EQ(std::is_volatile<
                      typename std::remove_reference<PointerType>::type>::value,
                  annotated_type.is_volatile[1]);
        EXPECT_FALSE(annotated_type.is_volatile[2]);
      } else {
        // Reference to scalar.
        ASSERT_EQ(2u, annotated_type.is_const.size());
        // Constness of referent scalar type.
        EXPECT_EQ(std::is_const<ScalarType>::value,
                  annotated_type.is_const[0]);
        // Reference type itself can not have any cv-qualifiers.
        EXPECT_FALSE(annotated_type.is_const[1]);

        // Similar for volatile qualifiers.
        ASSERT_EQ(2u, annotated_type.is_volatile.size());
        EXPECT_EQ(std::is_volatile<ScalarType>::value,
                  annotated_type.is_volatile[0]);
        EXPECT_FALSE(annotated_type.is_volatile[1]);
      }
    } else {
      if (std::is_pointer<
              typename std::remove_pointer<PointerType>::type>::value) {
        // Pointer to pointer.
        ASSERT_EQ(3u, annotated_type.is_const.size());

        // Constness of innermost scalar type.
        EXPECT_EQ(std::is_const<ScalarType>::value,
                  annotated_type.is_const[0]);
        // Constness of intermediate pointer-to-scalar type.
        EXPECT_EQ(std::is_const<
                      typename std::remove_pointer<PointerType>::type>::value,
                  annotated_type.is_const[1]);
        // Constness of outermost pointer-to-pointer-to-scalar type.
        EXPECT_EQ(std::is_const<PointerType>::value,
                  annotated_type.is_const[2]);

        // Similar for volatile qualifiers.
        ASSERT_EQ(3u, annotated_type.is_volatile.size());
        EXPECT_EQ(std::is_volatile<ScalarType>::value,
                  annotated_type.is_volatile[0]);
        EXPECT_EQ(std::is_volatile<
                      typename std::remove_pointer<PointerType>::type>::value,
                  annotated_type.is_volatile[1]);
        EXPECT_EQ(std::is_volatile<PointerType>::value,
                  annotated_type.is_volatile[2]);
      } else {
        // Pointer to scalar.
        ASSERT_EQ(2u, annotated_type.is_const.size());
        // Constness of referent scalar type.
        EXPECT_EQ(std::is_const<ScalarType>::value,
                  annotated_type.is_const[0]);
        // Constness of pointer type itself.
        EXPECT_EQ(std::is_const<PointerType>::value,
                  annotated_type.is_const[1]);

        // Similar for volatile qualifiers.
        ASSERT_EQ(2u, annotated_type.is_volatile.size());
        EXPECT_EQ(std::is_volatile<ScalarType>::value,
                  annotated_type.is_volatile[0]);
        EXPECT_EQ(std::is_volatile<PointerType>::value,
                  annotated_type.is_volatile[1]);
      }
    }
  }

  // Helper method for GetPointerTypeTest. Calls CodegenUtils::GetType() and
  // CodegenUtils::GetAnnotatedType() for 4 versions of a pointer to
  // 'PointedType' with various const-qualifiers (regular mutable pointer,
  // pointer to const, const-pointer, and const-pointer to const) and invokes
  // 'check_functor' on the returned llvm::Type*. check_functor's call operator
  // should do the actual checks to make sure that the llvm::Type is as
  // expected.
  //
  // This method also calls CheckAnnotationsForPointer() to check that the
  // additional annotations for C++ type properties not captured by the LLVM
  // type system are as expected for each checked pointer type.
  template <typename PointedType, typename CheckFunctor>
  void CheckAllPointerFlavors(const CheckFunctor& check_functor) {
    check_functor(codegen_utils_->GetType<PointedType*>());
    check_functor(codegen_utils_->GetType<const PointedType*>());
    check_functor(codegen_utils_->GetType<PointedType* const>());
    check_functor(codegen_utils_->GetType<const PointedType* const>());

    // Also check GetAnnotatedType().
    AnnotatedType annotated_type
        = codegen_utils_->GetAnnotatedType<PointedType*>();
    check_functor(annotated_type.llvm_type);
    CheckAnnotationsForPointer<PointedType*>(annotated_type);

    annotated_type = codegen_utils_->GetAnnotatedType<const PointedType*>();
    check_functor(annotated_type.llvm_type);
    CheckAnnotationsForPointer<const PointedType*>(annotated_type);

    annotated_type = codegen_utils_->GetAnnotatedType<PointedType* const>();
    check_functor(annotated_type.llvm_type);
    CheckAnnotationsForPointer<PointedType* const>(annotated_type);

    annotated_type
        = codegen_utils_->GetAnnotatedType<const PointedType* const>();
    check_functor(annotated_type.llvm_type);
    CheckAnnotationsForPointer<const PointedType* const>(annotated_type);
  }

  // Similar to CheckAllPointerFlavors(), but also checks the lvalue-reference
  // types 'PointedType&' and 'const PointedType&', which should map to the
  // same LLVM pointer type.
  template <typename PointedType, typename CheckFunctor>
  void CheckAllPointerAndReferenceFlavors(const CheckFunctor& check_functor) {
    CheckAllPointerFlavors<PointedType, CheckFunctor>(check_functor);

    check_functor(codegen_utils_->GetType<PointedType&>());
    check_functor(codegen_utils_->GetType<const PointedType&>());

    // Also check GetAnnotatedType() for reference types.
    AnnotatedType annotated_type
        = codegen_utils_->GetAnnotatedType<PointedType&>();
    check_functor(annotated_type.llvm_type);
    CheckAnnotationsForPointer<PointedType&>(annotated_type);

    annotated_type = codegen_utils_->GetAnnotatedType<const PointedType&>();
    check_functor(annotated_type.llvm_type);
    CheckAnnotationsForPointer<const PointedType&>(annotated_type);
  }

  // Helper method for GetPointerTypeTest. Tests the various different flavors
  // of a pointer or reference to 'IntegerType'.
  template <typename IntegerType>
  void CheckGetIntegerPointerType() {
    auto integer_pointer_check_lambda = [](const llvm::Type* llvm_type) {
      ASSERT_NE(llvm_type, nullptr);
      ASSERT_TRUE(llvm_type->isPointerTy());
      EXPECT_TRUE(llvm_type->getPointerElementType()->isIntegerTy(
          sizeof(IntegerType) << 3));
    };

    CheckAllPointerAndReferenceFlavors<
        IntegerType,
        decltype(integer_pointer_check_lambda)>(
            integer_pointer_check_lambda);
  }

  // Helper method for GetPointerTypeTest. Tests various different flavors of
  // a pointer or reference to 'EnumType', which is expected to map to
  // 'EquivalentIntegerType'.
  template <typename EnumType, typename EquivalentIntegerType>
  void CheckGetEnumPointerType() {
    auto enum_pointer_check_lambda = [this](const llvm::Type* llvm_type) {
      ASSERT_NE(llvm_type, nullptr);
      ASSERT_TRUE(llvm_type->isPointerTy());
      EXPECT_TRUE(llvm_type->getPointerElementType()->isIntegerTy(
          sizeof(EquivalentIntegerType) << 3));

      llvm::Type* int_llvm_type
          = codegen_utils_->GetType<EquivalentIntegerType*>();
      EXPECT_EQ(llvm_type, int_llvm_type);
    };

    CheckAllPointerAndReferenceFlavors<
        EnumType,
        decltype(enum_pointer_check_lambda)>(
            enum_pointer_check_lambda);
  }

  // Helper method for GetScalarConstantTest. Tests
  // CodegenUtils::GetConstant() for a single 'integer_constant'.
  template <typename IntegerType>
  void CheckGetSingleIntegerConstant(const IntegerType integer_constant) {
    llvm::Constant* constant = codegen_utils_->GetConstant(integer_constant);

    // Check the type.
    EXPECT_EQ(codegen_utils_->GetType<IntegerType>(), constant->getType());

    // Check the value.
    const llvm::APInt& constant_apint = constant->getUniqueInteger();
    if (std::is_signed<IntegerType>::value) {
      // If signed, compare with the APInt's sign-extended representation.
      EXPECT_TRUE(constant_apint.isSignedIntN(sizeof(IntegerType) << 3));
      EXPECT_EQ(integer_constant,
                static_cast<IntegerType>(constant_apint.getSExtValue()));
    } else {
      // If unsigned, compare with the APInt's zero-extended representation.
      EXPECT_TRUE(constant_apint.isIntN(sizeof(IntegerType) << 3));
      EXPECT_EQ(integer_constant,
                static_cast<IntegerType>(constant_apint.getZExtValue()));
    }
  }

  // Helper method for GetScalarConstantTest. Tests
  // CodegenUtils::GetConstant() for an 'IntegerType' with several values
  // of the specified integer type (0, 1, 123, the maximum, and if signed,
  // -1, -123, and the minimum).
  template <typename IntegerType>
  void CheckGetIntegerConstant() {
    CheckGetSingleIntegerConstant<IntegerType>(0);
    CheckGetSingleIntegerConstant<IntegerType>(1);
    CheckGetSingleIntegerConstant<IntegerType>(123);
    CheckGetSingleIntegerConstant<IntegerType>(
        std::numeric_limits<IntegerType>::max());
    if (std::is_signed<IntegerType>::value) {
      CheckGetSingleIntegerConstant<IntegerType>(-1);
      CheckGetSingleIntegerConstant<IntegerType>(-123);
      CheckGetSingleIntegerConstant<IntegerType>(
          std::numeric_limits<IntegerType>::min());
    }
  }

  // Helper method for GetScalarConstantTest. Tests
  // CodegenUtils::GetConstant() for a single 'fp_constant'.
  template <typename FloatingPointType>
  void CheckGetSingleFloatingPointConstant(
      const FloatingPointType fp_constant) {
    llvm::Constant* constant = codegen_utils_->GetConstant(fp_constant);

    // Check the type.
    EXPECT_EQ(codegen_utils_->GetType<FloatingPointType>(),
              constant->getType());

    // Check the value.
    llvm::ConstantFP* constant_as_fp
        = llvm::dyn_cast<llvm::ConstantFP>(constant);
    ASSERT_NE(constant_as_fp, nullptr);
    if (std::is_same<double, FloatingPointType>::value) {
      EXPECT_EQ(fp_constant,
                constant_as_fp->getValueAPF().convertToDouble());
    } else if (std::is_same<float, FloatingPointType>::value) {
      EXPECT_EQ(fp_constant,
                constant_as_fp->getValueAPF().convertToFloat());
    } else {
      ASSERT_TRUE(false)
          << "Can not check value of floating point constant for a type that "
          << "is not float or double.";
    }
  }

  // Helper method for GetScalarConstantTest. Tests
  // CodegenUtils::GetConstant() for a 'FloatingPointType' with several values
  // of the specified floating point type (positive and negative zero, positive
  // and negative 12.34, the minimum and maximum possible normalized values,
  // the highest-magnitude negative value, the smallest possible nonzero
  // denormalized value, and infinity).
  template <typename FloatingPointType>
  void CheckGetFloatingPointConstant() {
    CheckGetSingleFloatingPointConstant(0.0);
    CheckGetSingleFloatingPointConstant(-0.0);
    CheckGetSingleFloatingPointConstant(12.34);
    CheckGetSingleFloatingPointConstant(-12.34);
    CheckGetSingleFloatingPointConstant(
        std::numeric_limits<FloatingPointType>::min());
    CheckGetSingleFloatingPointConstant(
        std::numeric_limits<FloatingPointType>::max());
    CheckGetSingleFloatingPointConstant(
        std::numeric_limits<FloatingPointType>::lowest());
    CheckGetSingleFloatingPointConstant(
        std::numeric_limits<FloatingPointType>::denorm_min());
    CheckGetSingleFloatingPointConstant(
        std::numeric_limits<FloatingPointType>::infinity());
  }

  // Helper method for GetScalarConstantTest. Tests
  // CodegenUtils::GetConstant() for a single 'enum_constant'.
  template <typename EnumType>
  void CheckGetSingleEnumConstant(const EnumType enum_constant) {
    llvm::Constant* constant = codegen_utils_->GetConstant(enum_constant);

    // Check the type.
    EXPECT_EQ(codegen_utils_->GetType<EnumType>(), constant->getType());

    // Check the value (implicitly converted to the underlying integer type).
    const llvm::APInt& constant_apint = constant->getUniqueInteger();
    if (std::is_signed<typename std::underlying_type<EnumType>::type>::value) {
      // If signed, compare with the APInt's sign-extended representation.
      EXPECT_TRUE(constant_apint.isSignedIntN(
          sizeof(typename std::underlying_type<EnumType>::type) << 3));
      EXPECT_EQ(static_cast<typename std::underlying_type<EnumType>::type>(
                    enum_constant),
                static_cast<typename std::underlying_type<EnumType>::type>(
                    constant_apint.getSExtValue()));
    } else {
      // If unsigned, compare with the APInt's zero-extended representation.
      EXPECT_TRUE(constant_apint.isIntN(
          sizeof(typename std::underlying_type<EnumType>::type) << 3));
      EXPECT_EQ(static_cast<typename std::underlying_type<EnumType>::type>(
                    enum_constant),
                static_cast<typename std::underlying_type<EnumType>::type>(
                    constant_apint.getZExtValue()));
    }
  }

  // Helper method for GetScalarConstantTest. Tests
  // CodegenUtils::GetConstant() for an 'EnumType' by calling
  // CheckGetSingleEnumConstant() for each constant listed in 'enum_constants'.
  template <typename EnumType>
  void CheckGetEnumConstants(std::initializer_list<EnumType> enum_constants) {
    for (const EnumType enum_constant : enum_constants) {
      CheckGetSingleEnumConstant(enum_constant);
    }
  }

  // Helper method for GetPointerConstantTest and
  // GetPointerToMemberConstantTest. Generates a unique function name for a
  // global variable accessor function based on 'idx'.
  static std::string GlobalConstantAccessorName(std::size_t idx) {
    static constexpr char kPrefix[] = "global_accessor_";
    char print_buffer[sizeof(kPrefix) + (sizeof(idx) << 1) + 1];
    int written = std::snprintf(print_buffer,
                                sizeof(print_buffer),
                                "%s%zx",
                                kPrefix,
                                idx);
    assert(static_cast<std::size_t>(written) < sizeof(print_buffer));
    return print_buffer;
  }

  // Helper method for GetPointerConstantTest and
  // GetPointerToMemberConstantTest. Calls all of the constant-accessor
  // functions generated during the test and verifies that they return the
  // expected addresses.
  void FinishCheckingGlobalConstantPointers(
      const std::vector<std::uintptr_t>& pointer_check_addresses) {
    for (std::size_t idx = 0; idx < pointer_check_addresses.size(); ++idx) {
      std::uintptr_t (*check_fn)()
          = codegen_utils_->GetFunctionPointer<decltype(check_fn)>(
              GlobalConstantAccessorName(idx));
      EXPECT_EQ(pointer_check_addresses[idx], (*check_fn)());
    }
  }

  // Helper method for GetPointerConstantTest. Tests
  // CodegenUtils::GetConstant() for 'ptr_constant'. Verifies that the
  // pointer Constant returned is the expected type and generates an accessor
  // function that returns the address of the pointer constant, recording the
  // expected address in '*pointer_check_addresses'. After all of the
  // invocations of this method in a test, CodegenUtils::PrepareForExecution()
  // should be called to compile the accessor functions, then
  // FinishCheckingGlobalConstantPointers() should be called to make sure that
  // the accessor functions return the expected addresses.
  template <typename PointerType>
  void CheckGetSinglePointerConstant(
      PointerType ptr_constant,
      std::vector<std::uintptr_t>* pointer_check_addresses) {
    llvm::Constant* constant = codegen_utils_->GetConstant(ptr_constant);

    // Check type.
    EXPECT_EQ(codegen_utils_->GetType<PointerType>(), constant->getType());

    if (ptr_constant == nullptr) {
      // Expect a NULL literal.
      EXPECT_TRUE(constant->isNullValue());
    } else {
      // Expect a GlobalVariable. This will be mapped to the actual external
      // address when CodegenUtils::PrepareForExecution() is called. For now,
      // we generate a function that returns the (constant) address of the
      // GlobalVariable. Later on, we will compile all such functions and check
      // that they return the expected addresses.
      EXPECT_TRUE(llvm::isa<llvm::GlobalVariable>(constant));
      typedef std::uintptr_t (*GloablAccessorFn) ();
      llvm::Function* global_accessor_fn
          = codegen_utils_->CreateFunction<GloablAccessorFn>(
              GlobalConstantAccessorName(pointer_check_addresses->size()));
      llvm::BasicBlock* global_accessor_fn_body
          = codegen_utils_->CreateBasicBlock("body", global_accessor_fn);
      codegen_utils_->ir_builder()->SetInsertPoint(global_accessor_fn_body);
      llvm::Value* global_addr = codegen_utils_->ir_builder()->CreatePtrToInt(
          constant,
          codegen_utils_->GetType<std::uintptr_t>());
      codegen_utils_->ir_builder()->CreateRet(global_addr);

      // Verify function is well-formed.
      EXPECT_FALSE(llvm::verifyFunction(*global_accessor_fn));

      pointer_check_addresses->emplace_back(
          reinterpret_cast<std::uintptr_t>(ptr_constant));
    }
  }

  // Helper method for GetPointerConstantTest. Tests
  // CodegenUtils::GetConstant() by calling CheckGetSinglePointerConstant()
  // for a 'ScalarType*' pointer pointing to NULL, to stack memory, and to heap
  // memory, and for a 'ScalarType**' pointer pointing to NULL, to stack
  // memory, and to heap memory.
  template <typename ScalarType>
  void CheckGetPointerToScalarConstant(
      std::vector<std::uintptr_t>* pointer_check_addresses) {
    ScalarType* bare_ptr = nullptr;
    ScalarType stack_variable;
    std::unique_ptr<ScalarType> heap_variable(new ScalarType);

    // Note that CheckGetSinglePointerConstant() will create functions that
    // refer to global variables based on pointers that will no longer be valid
    // outside of the scope of this method. This is safe in this particular case
    // only because the generated functions do nothing but take the address of
    // the global variable in question (they do not dereference it).
    CheckGetSinglePointerConstant(bare_ptr, pointer_check_addresses);
    CheckGetSinglePointerConstant(&stack_variable, pointer_check_addresses);
    CheckGetSinglePointerConstant(heap_variable.get(), pointer_check_addresses);

    // Also check pointer-to-pointer.
    ScalarType** bare_ptr_to_ptr = nullptr;
    CheckGetSinglePointerConstant(bare_ptr_to_ptr, pointer_check_addresses);
    bare_ptr_to_ptr = &bare_ptr;
    CheckGetSinglePointerConstant(bare_ptr_to_ptr, pointer_check_addresses);
    std::unique_ptr<ScalarType*> heap_ptr(new ScalarType*);
    CheckGetSinglePointerConstant(heap_ptr.get(), pointer_check_addresses);
  }

  // Helper method for ExternalFunctionTest. Registers 'external_function' in
  // 'codegen_utils_' and creates an LLVM wrapper function for it named
  // 'wrapper_function_name'. The wrapper function has the same type-signature
  // as 'external_function' and simply forwards its arguments as-is to
  // 'external_function' and returns back the same return value.
  template <typename ReturnType, typename... ArgumentTypes>
  void MakeWrapperFunction(
      ReturnType (*external_function)(ArgumentTypes...),
      const std::string& wrapper_function_name) {
    // Register 'external_function' in 'codegen_utils_' and check that it has
    // the expected type-signature.
    llvm::Function* llvm_external_function
        = codegen_utils_->RegisterExternalFunction(external_function);
    ASSERT_NE(llvm_external_function, nullptr);
    EXPECT_EQ(
        (codegen_utils_->GetFunctionType<ReturnType, ArgumentTypes...>()
            ->getPointerTo()),
        llvm_external_function->getType());

    // Create a wrapper function in 'codegen_utils_' with the same
    // type-signature that merely forwards its arguments to the external
    // function as-is.
    typedef ReturnType (*WrapperFn) (ArgumentTypes...);
    llvm::Function* wrapper_function
        = codegen_utils_->CreateFunction<WrapperFn>(
            wrapper_function_name);

    llvm::BasicBlock* wrapper_function_body
        = codegen_utils_->CreateBasicBlock("wrapper_fn_body",
                                            wrapper_function);

    codegen_utils_->ir_builder()->SetInsertPoint(wrapper_function_body);
    std::vector<llvm::Value*> forwarded_args;
    for (llvm::Argument& arg : wrapper_function->args()) {
      forwarded_args.push_back(&arg);
    }
    llvm::CallInst* call = codegen_utils_->ir_builder()->CreateCall(
        llvm_external_function,
        forwarded_args);

    // Return the result of the call, or void if the function returns void.
    if (std::is_same<ReturnType, void>::value) {
      codegen_utils_->ir_builder()->CreateRetVoid();
    } else {
      codegen_utils_->ir_builder()->CreateRet(call);
    }

    // Check that the wrapper function is well-formed. LLVM verification
    // functions return false if no errors are detected.
    EXPECT_FALSE(llvm::verifyFunction(*wrapper_function));
  }

  // Helper method for GetPointerToMemberConstantTest. Verifies that
  // CodegenUtils::GetPointerToMember() functions correctly for
  // 'pointers_to_members' by checking the type of the pointer generated when
  // accessing a member from a constant pointer to StructType and generating an
  // accessor function the returns the address of the member, recording the
  // expected address in '*pointer_check_addresses'. After all of the
  // invocations of this method in a test, CodegenUtils::PrepareForExecution()
  // should be called to compile the accessor functions, then
  // FinishCheckingGlobalConstantPointers() should be called to make sure that
  // the accessor functions return the expected addresses.
  template <typename MemberType,
            typename StructType,
            typename... PointerToMemberTypes>
  void CheckGetPointerToMemberConstant(
      std::vector<std::uintptr_t>* pointer_check_addresses,
      const StructType* external_struct,
      const std::size_t expected_offset,
      PointerToMemberTypes&&... pointers_to_members) {
    llvm::Constant* llvm_ptr_to_struct
        = codegen_utils_->GetConstant(external_struct);

    // Generate a function that returns the (constant) address of the member
    // field inside the struct.
    typedef std::uintptr_t (*GlobalAccessorFn) ();
    llvm::Function* global_member_accessor_fn
        = codegen_utils_->CreateFunction<GlobalAccessorFn>(
            GlobalConstantAccessorName(pointer_check_addresses->size()));
    llvm::BasicBlock* global_member_accessor_fn_body
        = codegen_utils_->CreateBasicBlock("body", global_member_accessor_fn);
    codegen_utils_->ir_builder()->SetInsertPoint(
        global_member_accessor_fn_body);
    llvm::Value* member_ptr = codegen_utils_->GetPointerToMember(
        llvm_ptr_to_struct,
        std::forward<PointerToMemberTypes>(pointers_to_members)...);
    llvm::Value* member_address = codegen_utils_->ir_builder()->CreatePtrToInt(
        member_ptr,
        codegen_utils_->GetType<std::uintptr_t>());
    codegen_utils_->ir_builder()->CreateRet(member_address);

    // Verify accessor function is well-formed.
    EXPECT_FALSE(llvm::verifyFunction(*global_member_accessor_fn));

    pointer_check_addresses->emplace_back(
        reinterpret_cast<std::uintptr_t>(external_struct) + expected_offset);
  }

  // Helper method for GetPointerToMemberTest. Generates an accessor function
  // that takes a StructType* pointer and returns the value of the member
  // variable indicated by 'pointers_to_members'.
  template <typename StructType,
            typename MemberType,
            typename... PointerToMemberTypes>
  void MakeStructMemberAccessorFunction(
      const std::string& function_name,
      PointerToMemberTypes&&... pointers_to_members) {
    typedef MemberType (*AccessorFn) (const StructType*);
    llvm::Function* accessor_function
        = codegen_utils_->CreateFunction<AccessorFn>(
            function_name);
    llvm::BasicBlock* accessor_function_body
        = codegen_utils_->CreateBasicBlock("accessor_fn_body",
                                            accessor_function);
    codegen_utils_->ir_builder()->SetInsertPoint(accessor_function_body);

    // Get pointer to member.
    llvm::Value* member_ptr = codegen_utils_->GetPointerToMember(
        ArgumentByPosition(accessor_function, 0),
        std::forward<PointerToMemberTypes>(pointers_to_members)...);
    // Actually load the value from the pointer.
    llvm::Value* member_value
        = codegen_utils_->ir_builder()->CreateLoad(member_ptr);
    // Return the loaded value.
    codegen_utils_->ir_builder()->CreateRet(member_value);

    // Check that the accessor function is well-formed. LLVM verification
    // functions return false if no errors are detected.
    EXPECT_FALSE(llvm::verifyFunction(*accessor_function));
  }

  // Helper method for ProjectScalarArrayTest. Generate an array with random
  // value for given size and return the pointer to it. Caller is responsible
  // for deleting the memory.
  template <class InputType>
  InputType* RandomInputArrayGenerator(size_t input_size) {
     InputType* input = new InputType[input_size];
     for (size_t idx = 0; idx < input_size; ++idx) {
         unsigned int seed = idx;
         input[idx] = rand_r(&seed) % (2 ^ (sizeof(InputType) * 8) - 1);
     }
     return input;
  }

  // Helper method for ProjectScalarArrayTest. Generate an array with random
  // value for given range and size. Caller is responsible for deleting the
  // memory
  size_t* RandomProjectionIndicesGenerator(size_t how_many, size_t range) {
     size_t* projection_indices = new size_t[how_many];
     for (size_t idx = 0; idx < how_many; ++idx) {
         unsigned int seed = idx;
         projection_indices[idx] = rand_r(&seed) % range;
     }
     return projection_indices;
  }

  // Helper method for ProjectScalarArrayTest. Generate an IR module for
  // projecting element from scalar array for given type and indices / offsets.
  template <class InputType>
  void GenerateScalarArrayProjectionFunction(
      const std::string& project_func_name,
      size_t* projection_indices, size_t projection_count) {
    typedef void (*ProjectScalarFn) (InputType*, InputType*);
    llvm::Function* project_scalar_function
      = codegen_utils_->CreateFunction<ProjectScalarFn>(
          project_func_name);

    // BasicBlocks for function entry.
    llvm::BasicBlock* entry_block = codegen_utils_->CreateBasicBlock(
        "entry", project_scalar_function);
    llvm::Value* input_array = ArgumentByPosition(project_scalar_function, 0);
    llvm::Value* output_array = ArgumentByPosition(project_scalar_function, 1);
    codegen_utils_->ir_builder()->SetInsertPoint(entry_block);

    // Build loop unrolled projection code.
    for (size_t idx = 0; idx < projection_count; ++idx) {
        // The next address of the input array where we need to read.
        llvm::Value* next_address =
          codegen_utils_->ir_builder()->CreateInBoundsGEP(input_array,
            {codegen_utils_->GetConstant(projection_indices[idx])});

        // Load the value from the calculated input address.
        llvm::LoadInst* load_instruction =
            codegen_utils_->ir_builder()->CreateLoad(next_address, "input");

        // Find the output address where we need to write our projected element.
        llvm::Value* next_output_address =
            codegen_utils_->ir_builder()->CreateInBoundsGEP(output_array,
              {codegen_utils_->GetConstant(idx)});

        // Store the projecetd element into the output address.
        codegen_utils_->ir_builder()-> CreateStore(
          load_instruction, next_output_address, "output");
     }
     codegen_utils_->ir_builder()->CreateRetVoid();
  }

  // Helper method for ProjectScalarArrayTest. For given type generate random
  // array, random index and then project using IR module.
  template <class InputType>
  void ProjectScalarArrayTestHelper() {
    // Input and project array size
    const int input_size = 100;
    const int projection_count = 10;

    InputType* input_array = RandomInputArrayGenerator<InputType>(input_size);
    size_t* proj_indices = RandomProjectionIndicesGenerator(projection_count,
        input_size);
    InputType* output_array = new InputType[projection_count];
    memset(output_array, -1, projection_count * sizeof(InputType));
    GenerateScalarArrayProjectionFunction<InputType>("func_project",
        proj_indices, projection_count);

    // Prepare for execution.
    EXPECT_TRUE(codegen_utils_->PrepareForExecution(
        CodegenUtils::OptimizationLevel::kNone, true));

    void (*project_scalar_function_compiled)(InputType*, InputType*)
         = codegen_utils_->GetFunctionPointer<
         decltype(project_scalar_function_compiled)>("func_project");

    // Call the generated projection function
    (*project_scalar_function_compiled)(input_array, output_array);

    // Check if all the projected elements are correctly placed in the output.
    for (size_t idx = 0; idx < projection_count; ++idx) {
        EXPECT_EQ(input_array[proj_indices[idx]], output_array[idx]);
    }

    delete[] input_array;
    delete[] proj_indices;
    delete[] output_array;
  }

  std::unique_ptr<CodegenUtils> codegen_utils_;
};

typedef CodegenUtilsTest CodegenUtilsDeathTest;

TEST_F(CodegenUtilsTest, InitializationTest) {
  EXPECT_NE(codegen_utils_->ir_builder(), nullptr);
  ASSERT_NE(codegen_utils_->module(), nullptr);
  EXPECT_EQ(std::string("test_module"),
            codegen_utils_->module()->getModuleIdentifier());
}

TEST_F(CodegenUtilsTest, GetScalarTypeTest) {
  // Check void.
  llvm::Type* llvm_type = codegen_utils_->GetType<void>();
  ASSERT_NE(llvm_type, nullptr);
  EXPECT_TRUE(llvm_type->isVoidTy());

  AnnotatedType annotated_type = codegen_utils_->GetAnnotatedType<void>();
  ASSERT_NE(annotated_type.llvm_type, nullptr);
  EXPECT_TRUE(annotated_type.llvm_type->isVoidTy());
  EXPECT_FALSE(annotated_type.is_voidptr);
  EXPECT_FALSE(annotated_type.is_reference);
  EXPECT_FALSE(annotated_type.explicitly_unsigned);
  EXPECT_FALSE(annotated_type.is_long);
  EXPECT_FALSE(annotated_type.is_long_long);
  ASSERT_EQ(1u, annotated_type.is_const.size());
  EXPECT_FALSE(annotated_type.is_const.front());
  ASSERT_EQ(1u, annotated_type.is_volatile.size());
  EXPECT_FALSE(annotated_type.is_volatile.front());

  // Check bool (represented as i1 in LLVM IR).
  llvm_type = codegen_utils_->GetType<bool>();
  ASSERT_NE(llvm_type, nullptr);
  EXPECT_TRUE(llvm_type->isIntegerTy(1));

  annotated_type = codegen_utils_->GetAnnotatedType<bool>();
  ASSERT_NE(annotated_type.llvm_type, nullptr);
  EXPECT_TRUE(annotated_type.llvm_type->isIntegerTy(1));
  EXPECT_FALSE(annotated_type.is_voidptr);
  EXPECT_FALSE(annotated_type.is_reference);
  EXPECT_FALSE(annotated_type.explicitly_unsigned);
  EXPECT_FALSE(annotated_type.is_long);
  EXPECT_FALSE(annotated_type.is_long_long);
  ASSERT_EQ(1u, annotated_type.is_const.size());
  EXPECT_FALSE(annotated_type.is_const.front());
  ASSERT_EQ(1u, annotated_type.is_volatile.size());
  EXPECT_FALSE(annotated_type.is_volatile.front());

  llvm_type = codegen_utils_->GetType<const bool>();
  ASSERT_NE(llvm_type, nullptr);
  EXPECT_TRUE(llvm_type->isIntegerTy(1));

  annotated_type = codegen_utils_->GetAnnotatedType<const bool>();
  ASSERT_NE(annotated_type.llvm_type, nullptr);
  EXPECT_TRUE(annotated_type.llvm_type->isIntegerTy(1));
  EXPECT_FALSE(annotated_type.is_voidptr);
  EXPECT_FALSE(annotated_type.is_reference);
  EXPECT_FALSE(annotated_type.explicitly_unsigned);
  EXPECT_FALSE(annotated_type.is_long);
  EXPECT_FALSE(annotated_type.is_long_long);
  ASSERT_EQ(1u, annotated_type.is_const.size());
  EXPECT_TRUE(annotated_type.is_const.front());
  ASSERT_EQ(1u, annotated_type.is_volatile.size());
  EXPECT_FALSE(annotated_type.is_volatile.front());

  // Check 32-bit float.
  llvm_type = codegen_utils_->GetType<float>();
  ASSERT_NE(llvm_type, nullptr);
  EXPECT_TRUE(llvm_type->isFloatTy());

  annotated_type = codegen_utils_->GetAnnotatedType<float>();
  ASSERT_NE(annotated_type.llvm_type, nullptr);
  EXPECT_TRUE(annotated_type.llvm_type->isFloatTy());
  EXPECT_FALSE(annotated_type.is_voidptr);
  EXPECT_FALSE(annotated_type.is_reference);
  EXPECT_FALSE(annotated_type.explicitly_unsigned);
  EXPECT_FALSE(annotated_type.is_long);
  EXPECT_FALSE(annotated_type.is_long_long);
  ASSERT_EQ(1u, annotated_type.is_const.size());
  EXPECT_FALSE(annotated_type.is_const.front());
  ASSERT_EQ(1u, annotated_type.is_volatile.size());
  EXPECT_FALSE(annotated_type.is_volatile.front());

  llvm_type = codegen_utils_->GetType<const float>();
  ASSERT_NE(llvm_type, nullptr);
  EXPECT_TRUE(llvm_type->isFloatTy());

  annotated_type = codegen_utils_->GetAnnotatedType<const float>();
  ASSERT_NE(annotated_type.llvm_type, nullptr);
  EXPECT_TRUE(annotated_type.llvm_type->isFloatTy());
  EXPECT_FALSE(annotated_type.is_voidptr);
  EXPECT_FALSE(annotated_type.is_reference);
  EXPECT_FALSE(annotated_type.explicitly_unsigned);
  EXPECT_FALSE(annotated_type.is_long);
  EXPECT_FALSE(annotated_type.is_long_long);
  ASSERT_EQ(1u, annotated_type.is_const.size());
  EXPECT_TRUE(annotated_type.is_const.front());
  ASSERT_EQ(1u, annotated_type.is_volatile.size());
  EXPECT_FALSE(annotated_type.is_volatile.front());

  // Check 64-bit double.
  llvm_type = codegen_utils_->GetType<double>();
  ASSERT_NE(llvm_type, nullptr);
  EXPECT_TRUE(llvm_type->isDoubleTy());

  annotated_type = codegen_utils_->GetAnnotatedType<double>();
  ASSERT_NE(annotated_type.llvm_type, nullptr);
  EXPECT_TRUE(annotated_type.llvm_type->isDoubleTy());
  EXPECT_FALSE(annotated_type.is_voidptr);
  EXPECT_FALSE(annotated_type.is_reference);
  EXPECT_FALSE(annotated_type.explicitly_unsigned);
  EXPECT_FALSE(annotated_type.is_long);
  EXPECT_FALSE(annotated_type.is_long_long);
  ASSERT_EQ(1u, annotated_type.is_const.size());
  EXPECT_FALSE(annotated_type.is_const.front());
  ASSERT_EQ(1u, annotated_type.is_volatile.size());
  EXPECT_FALSE(annotated_type.is_volatile.front());

  llvm_type = codegen_utils_->GetType<const double>();
  ASSERT_NE(llvm_type, nullptr);
  EXPECT_TRUE(llvm_type->isDoubleTy());

  annotated_type = codegen_utils_->GetAnnotatedType<const double>();
  ASSERT_NE(annotated_type.llvm_type, nullptr);
  EXPECT_TRUE(annotated_type.llvm_type->isDoubleTy());
  EXPECT_FALSE(annotated_type.is_voidptr);
  EXPECT_FALSE(annotated_type.is_reference);
  EXPECT_FALSE(annotated_type.explicitly_unsigned);
  EXPECT_FALSE(annotated_type.is_long);
  EXPECT_FALSE(annotated_type.is_long_long);
  ASSERT_EQ(1u, annotated_type.is_const.size());
  EXPECT_TRUE(annotated_type.is_const.front());
  ASSERT_EQ(1u, annotated_type.is_volatile.size());
  EXPECT_FALSE(annotated_type.is_volatile.front());

  // Check C built-in integral types.
  CheckGetIntegerType<char>();
  CheckGetIntegerType<short>();      // NOLINT(runtime/int)
  CheckGetIntegerType<int>();
  CheckGetIntegerType<long>();       // NOLINT(runtime/int)
  CheckGetIntegerType<long long>();  // NOLINT(runtime/int)

  // Check explicitly signed versions of C built-in integral types. Note that
  // integer types in LLVM do not have a signedness property, so
  // signed/unsigned versions of a type in C/C++ have the same representation
  // in LLVM IR.
  CheckGetIntegerType<signed char>();
  CheckGetIntegerType<signed short>();      // NOLINT(runtime/int)
  CheckGetIntegerType<signed int>();
  CheckGetIntegerType<signed long>();       // NOLINT(runtime/int)
  CheckGetIntegerType<signed long long>();  // NOLINT(runtime/int)

  // Check explicitly unsigned versions of C built-in integral types.
  CheckGetIntegerType<unsigned char>();
  CheckGetIntegerType<unsigned short>();      // NOLINT(runtime/int)
  CheckGetIntegerType<unsigned int>();
  CheckGetIntegerType<unsigned long>();       // NOLINT(runtime/int)
  CheckGetIntegerType<unsigned long long>();  // NOLINT(runtime/int)

  // Check cstdint typedefs.
  CheckGetIntegerType<std::int8_t>();
  CheckGetIntegerType<std::int16_t>();
  CheckGetIntegerType<std::int32_t>();
  CheckGetIntegerType<std::int64_t>();
  CheckGetIntegerType<std::uint8_t>();
  CheckGetIntegerType<std::uint16_t>();
  CheckGetIntegerType<std::uint32_t>();
  CheckGetIntegerType<std::uint64_t>();
  CheckGetIntegerType<std::int_fast8_t>();
  CheckGetIntegerType<std::int_fast16_t>();
  CheckGetIntegerType<std::int_fast32_t>();
  CheckGetIntegerType<std::int_fast64_t>();
  CheckGetIntegerType<std::uint_fast8_t>();
  CheckGetIntegerType<std::uint_fast16_t>();
  CheckGetIntegerType<std::uint_fast32_t>();
  CheckGetIntegerType<std::uint_fast64_t>();
  CheckGetIntegerType<std::int_least8_t>();
  CheckGetIntegerType<std::int_least16_t>();
  CheckGetIntegerType<std::int_least32_t>();
  CheckGetIntegerType<std::int_least64_t>();
  CheckGetIntegerType<std::uint_least8_t>();
  CheckGetIntegerType<std::uint_least16_t>();
  CheckGetIntegerType<std::uint_least32_t>();
  CheckGetIntegerType<std::uint_least64_t>();
  CheckGetIntegerType<std::intmax_t>();
  CheckGetIntegerType<std::uintmax_t>();
  CheckGetIntegerType<std::uintptr_t>();

  // Check cstddef typedefs.
  CheckGetIntegerType<std::size_t>();
  CheckGetIntegerType<std::ptrdiff_t>();

  // Check enums.
  CheckGetEnumType<SimpleEnum, std::underlying_type<SimpleEnum>::type>();
  CheckGetEnumType<SignedSimpleEnum,
                   std::underlying_type<SignedSimpleEnum>::type>();
  CheckGetEnumType<StronglyTypedEnum,
                   std::underlying_type<StronglyTypedEnum>::type>();
  CheckGetEnumType<SignedStronglyTypedEnum,
                   std::underlying_type<SignedStronglyTypedEnum>::type>();
  CheckGetEnumType<StronglyTypedEnumUint64, std::uint64_t>();
}

TEST_F(CodegenUtilsTest, GetPointerTypeTest) {
  // Check void*. Void pointers are a special case, because convention in the
  // LLVM type system is to use i8* (equivalent to char* in C) for all
  // "untyped" pointers.
  auto void_pointer_check_lambda = [](const llvm::Type* llvm_type) {
    ASSERT_NE(llvm_type, nullptr);
    ASSERT_TRUE(llvm_type->isPointerTy());
    EXPECT_TRUE(llvm_type->getPointerElementType()->isIntegerTy(8));
  };
  // Unlike other types, we check only pointers, not references, because there
  // is no such thing as void&.
  CheckAllPointerFlavors<void, decltype(void_pointer_check_lambda)>(
      void_pointer_check_lambda);

  // Check bool* (bool is represented as i1 in LLVM IR).
  auto bool_pointer_check_lambda = [](const llvm::Type* llvm_type) {
    ASSERT_NE(llvm_type, nullptr);
    ASSERT_TRUE(llvm_type->isPointerTy());
    EXPECT_TRUE(llvm_type->getPointerElementType()->isIntegerTy(1));
  };
  CheckAllPointerAndReferenceFlavors<
      bool,
      decltype(bool_pointer_check_lambda)>(
          bool_pointer_check_lambda);

  // Check float*.
  auto float_pointer_check_lambda = [](const llvm::Type* llvm_type) {
    ASSERT_NE(llvm_type, nullptr);
    ASSERT_TRUE(llvm_type->isPointerTy());
    EXPECT_TRUE(llvm_type->getPointerElementType()->isFloatTy());
  };
  CheckAllPointerAndReferenceFlavors<
      float,
      decltype(float_pointer_check_lambda)>(
          float_pointer_check_lambda);

  // Check double*.
  auto double_pointer_check_lambda = [](const llvm::Type* llvm_type) {
    ASSERT_NE(llvm_type, nullptr);
    ASSERT_TRUE(llvm_type->isPointerTy());
    EXPECT_TRUE(llvm_type->getPointerElementType()->isDoubleTy());
  };
  CheckAllPointerAndReferenceFlavors<
      double,
      decltype(double_pointer_check_lambda)>(
          double_pointer_check_lambda);

  // Check pointers to C built-in integral types.
  CheckGetIntegerPointerType<char>();
  CheckGetIntegerPointerType<short>();      // NOLINT(runtime/int)
  CheckGetIntegerPointerType<int>();
  CheckGetIntegerPointerType<long>();       // NOLINT(runtime/int)
  CheckGetIntegerPointerType<long long>();  // NOLINT(runtime/int)

  // Check pointers to explicitly signed versions of C built-in integral types.
  CheckGetIntegerPointerType<signed char>();
  CheckGetIntegerPointerType<signed short>();      // NOLINT(runtime/int)
  CheckGetIntegerPointerType<signed int>();
  CheckGetIntegerPointerType<signed long>();       // NOLINT(runtime/int)
  CheckGetIntegerPointerType<signed long long>();  // NOLINT(runtime/int)

  // Check pointers to unsigned versions of C built-in integral types.
  CheckGetIntegerPointerType<unsigned char>();
  CheckGetIntegerPointerType<unsigned short>();      // NOLINT(runtime/int)
  CheckGetIntegerPointerType<unsigned int>();
  CheckGetIntegerPointerType<unsigned long>();       // NOLINT(runtime/int)
  CheckGetIntegerPointerType<unsigned long long>();  // NOLINT(runtime/int)

  // Check pointers to cstdint typedefs.
  CheckGetIntegerPointerType<std::int8_t>();
  CheckGetIntegerPointerType<std::int16_t>();
  CheckGetIntegerPointerType<std::int32_t>();
  CheckGetIntegerPointerType<std::int64_t>();
  CheckGetIntegerPointerType<std::uint8_t>();
  CheckGetIntegerPointerType<std::uint16_t>();
  CheckGetIntegerPointerType<std::uint32_t>();
  CheckGetIntegerPointerType<std::uint64_t>();
  CheckGetIntegerPointerType<std::int_fast8_t>();
  CheckGetIntegerPointerType<std::int_fast16_t>();
  CheckGetIntegerPointerType<std::int_fast32_t>();
  CheckGetIntegerPointerType<std::int_fast64_t>();
  CheckGetIntegerPointerType<std::uint_fast8_t>();
  CheckGetIntegerPointerType<std::uint_fast16_t>();
  CheckGetIntegerPointerType<std::uint_fast32_t>();
  CheckGetIntegerPointerType<std::uint_fast64_t>();
  CheckGetIntegerPointerType<std::int_least8_t>();
  CheckGetIntegerPointerType<std::int_least16_t>();
  CheckGetIntegerPointerType<std::int_least32_t>();
  CheckGetIntegerPointerType<std::int_least64_t>();
  CheckGetIntegerPointerType<std::uint_least8_t>();
  CheckGetIntegerPointerType<std::uint_least16_t>();
  CheckGetIntegerPointerType<std::uint_least32_t>();
  CheckGetIntegerPointerType<std::uint_least64_t>();
  CheckGetIntegerPointerType<std::intmax_t>();
  CheckGetIntegerPointerType<std::uintmax_t>();
  CheckGetIntegerPointerType<std::uintptr_t>();

  // Check pointers to cstddef typedefs.
  CheckGetIntegerPointerType<std::size_t>();
  CheckGetIntegerPointerType<std::ptrdiff_t>();

  // Check pointers to enums.
  CheckGetEnumPointerType<
      SimpleEnum,
      std::underlying_type<SimpleEnum>::type>();
  CheckGetEnumPointerType<
      SignedSimpleEnum,
      std::underlying_type<SignedSimpleEnum>::type>();
  CheckGetEnumPointerType<
      StronglyTypedEnum,
      std::underlying_type<StronglyTypedEnum>::type>();
  CheckGetEnumPointerType<
      SignedStronglyTypedEnum,
      std::underlying_type<SignedStronglyTypedEnum>::type>();
  CheckGetEnumPointerType<StronglyTypedEnumUint64, std::uint64_t>();

  // Pointers and references to structs and classes get transformed to untyped
  // pointers (i8* in LLVM, equivalent to void* in C++). We can reuse
  // 'void_pointer_check_lambda' here.
  CheckAllPointerAndReferenceFlavors<
      DummyStruct,
      decltype(void_pointer_check_lambda)>(
          void_pointer_check_lambda);
  CheckAllPointerAndReferenceFlavors<
      DummyAbstractBaseClass,
      decltype(void_pointer_check_lambda)>(
          void_pointer_check_lambda);
  CheckAllPointerAndReferenceFlavors<
      Negater,
      decltype(void_pointer_check_lambda)>(
          void_pointer_check_lambda);
  CheckAllPointerAndReferenceFlavors<
      Squarer,
      decltype(void_pointer_check_lambda)>(
          void_pointer_check_lambda);

  // Pointer to pointer and reference to pointer.
  auto pointer_to_pointer_to_int_check_lambda
      = [](const llvm::Type* llvm_type) {
    ASSERT_NE(llvm_type, nullptr);
    ASSERT_TRUE(llvm_type->isPointerTy());
    ASSERT_TRUE(llvm_type->getPointerElementType()->isPointerTy());
    EXPECT_TRUE(llvm_type->getPointerElementType()->getPointerElementType()
                    ->isIntegerTy(sizeof(int) << 3));
  };
  CheckAllPointerAndReferenceFlavors<
      int*,
      decltype(pointer_to_pointer_to_int_check_lambda)>(
          pointer_to_pointer_to_int_check_lambda);

  // Also check void** and the like.
  auto pointer_to_pointer_to_void_check_lambda
      = [](const llvm::Type* llvm_type) {
    ASSERT_NE(llvm_type, nullptr);
    ASSERT_TRUE(llvm_type->isPointerTy());
    ASSERT_TRUE(llvm_type->getPointerElementType()->isPointerTy());
    EXPECT_TRUE(llvm_type->getPointerElementType()->getPointerElementType()
                    ->isIntegerTy(8));
  };
  CheckAllPointerAndReferenceFlavors<
      void*,
      decltype(pointer_to_pointer_to_void_check_lambda)>(
          pointer_to_pointer_to_void_check_lambda);

  // Check pointer-to-pointer and reference-to-pointer for struct and class
  // types as well. As above, the last-level pointer becomes a generic untyped
  // pointer (i8* in LLVM, equivalent to void* in C++).
  CheckAllPointerAndReferenceFlavors<
      DummyStruct*,
      decltype(pointer_to_pointer_to_void_check_lambda)>(
          pointer_to_pointer_to_void_check_lambda);
  CheckAllPointerAndReferenceFlavors<
      DummyAbstractBaseClass*,
      decltype(pointer_to_pointer_to_void_check_lambda)>(
          pointer_to_pointer_to_void_check_lambda);
  CheckAllPointerAndReferenceFlavors<
      Negater*,
      decltype(pointer_to_pointer_to_void_check_lambda)>(
          pointer_to_pointer_to_void_check_lambda);
  CheckAllPointerAndReferenceFlavors<
      Squarer*,
      decltype(pointer_to_pointer_to_void_check_lambda)>(
          pointer_to_pointer_to_void_check_lambda);
}

TEST_F(CodegenUtilsTest, GetScalarConstantTest) {
  // Check bool constants.
  llvm::Constant* constant = codegen_utils_->GetConstant(false);
  // Verify the constant's type.
  EXPECT_EQ(codegen_utils_->GetType<bool>(), constant->getType());
  // Verify the constant's value.
  EXPECT_TRUE(constant->isZeroValue());

  constant = codegen_utils_->GetConstant(true);
  EXPECT_EQ(codegen_utils_->GetType<bool>(), constant->getType());
  EXPECT_TRUE(constant->isOneValue());

  // Check the C built-in integer types, and their explicitly signed and
  // unsigned versions.
  CheckGetIntegerConstant<char>();
  CheckGetIntegerConstant<short>();      // NOLINT(runtime/int)
  CheckGetIntegerConstant<int>();
  CheckGetIntegerConstant<long>();       // NOLINT(runtime/int)
  CheckGetIntegerConstant<long long>();  // NOLINT(runtime/int)

  CheckGetIntegerConstant<signed char>();
  CheckGetIntegerConstant<signed short>();      // NOLINT(runtime/int)
  CheckGetIntegerConstant<signed int>();
  CheckGetIntegerConstant<signed long>();       // NOLINT(runtime/int)
  CheckGetIntegerConstant<signed long long>();  // NOLINT(runtime/int)

  CheckGetIntegerConstant<unsigned char>();
  CheckGetIntegerConstant<unsigned short>();      // NOLINT(runtime/int)
  CheckGetIntegerConstant<unsigned int>();
  CheckGetIntegerConstant<unsigned long>();       // NOLINT(runtime/int)
  CheckGetIntegerConstant<unsigned long long>();  // NOLINT(runtime/int)

  // Check cstdint typedefs.
  CheckGetIntegerConstant<std::int8_t>();
  CheckGetIntegerConstant<std::int16_t>();
  CheckGetIntegerConstant<std::int32_t>();
  CheckGetIntegerConstant<std::int64_t>();
  CheckGetIntegerConstant<std::uint8_t>();
  CheckGetIntegerConstant<std::uint16_t>();
  CheckGetIntegerConstant<std::uint32_t>();
  CheckGetIntegerConstant<std::uint64_t>();
  CheckGetIntegerConstant<std::int_fast8_t>();
  CheckGetIntegerConstant<std::int_fast16_t>();
  CheckGetIntegerConstant<std::int_fast32_t>();
  CheckGetIntegerConstant<std::int_fast64_t>();
  CheckGetIntegerConstant<std::uint_fast8_t>();
  CheckGetIntegerConstant<std::uint_fast16_t>();
  CheckGetIntegerConstant<std::uint_fast32_t>();
  CheckGetIntegerConstant<std::uint_fast64_t>();
  CheckGetIntegerConstant<std::int_least8_t>();
  CheckGetIntegerConstant<std::int_least16_t>();
  CheckGetIntegerConstant<std::int_least32_t>();
  CheckGetIntegerConstant<std::int_least64_t>();
  CheckGetIntegerConstant<std::uint_least8_t>();
  CheckGetIntegerConstant<std::uint_least16_t>();
  CheckGetIntegerConstant<std::uint_least32_t>();
  CheckGetIntegerConstant<std::uint_least64_t>();
  CheckGetIntegerConstant<std::intmax_t>();
  CheckGetIntegerConstant<std::uintmax_t>();
  CheckGetIntegerConstant<std::uintptr_t>();

  // Check cstddef typedefs.
  CheckGetIntegerConstant<std::size_t>();
  CheckGetIntegerConstant<std::ptrdiff_t>();

  // Check floating-point types.
  CheckGetFloatingPointConstant<float>();
  CheckGetFloatingPointConstant<double>();

  // Check enums.
  CheckGetEnumConstants<SimpleEnum>(
      {kSimpleEnumA, kSimpleEnumB, kSimpleEnumC});
  CheckGetEnumConstants<SignedSimpleEnum>(
      {kSignedSimpleEnumA, kSignedSimpleEnumB, kSignedSimpleEnumC});
  CheckGetEnumConstants<StronglyTypedEnum>(
      {StronglyTypedEnum::kCaseA, StronglyTypedEnum::kCaseB,
       StronglyTypedEnum::kCaseC});
  CheckGetEnumConstants<SignedStronglyTypedEnum>(
      {SignedStronglyTypedEnum::kCaseA, SignedStronglyTypedEnum::kCaseB,
       SignedStronglyTypedEnum::kCaseC});
  CheckGetEnumConstants<StronglyTypedEnumUint64>(
      {StronglyTypedEnumUint64::kCaseA, StronglyTypedEnumUint64::kCaseB,
       StronglyTypedEnumUint64::kCaseC});
}

TEST_F(CodegenUtilsTest, GetPointerConstantTest) {
  // Remember the addresses of pointer constants, in order, that we expect check
  // functions to return.
  std::vector<std::uintptr_t> pointer_check_addresses;

  // Check void* pointers.
  const void* voidptr = nullptr;
  CheckGetSinglePointerConstant(voidptr, &pointer_check_addresses);
  voidptr = this;
  CheckGetSinglePointerConstant(voidptr, &pointer_check_addresses);
  const void** voidptrptr = nullptr;
  CheckGetSinglePointerConstant(voidptrptr, &pointer_check_addresses);
  voidptrptr = &voidptr;
  CheckGetSinglePointerConstant(voidptrptr, &pointer_check_addresses);

  // Check pointers to C++ built-in scalar types.
  CheckGetPointerToScalarConstant<bool>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<float>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<double>(&pointer_check_addresses);

  CheckGetPointerToScalarConstant<char>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<short>(      // NOLINT(runtime/int)
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<int>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<long>(       // NOLINT(runtime/int)
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<long long>(  // NOLINT(runtime/int)
      &pointer_check_addresses);

  CheckGetPointerToScalarConstant<signed char>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<signed short>(      // NOLINT(runtime/int)
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<signed int>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<signed long>(       // NOLINT(runtime/int)
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<signed long long>(  // NOLINT(runtime/int)
      &pointer_check_addresses);

  CheckGetPointerToScalarConstant<unsigned char>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<unsigned short>(      // NOLINT(runtime/int)
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<unsigned int>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<unsigned long>(       // NOLINT(runtime/int)
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<unsigned long long>(  // NOLINT(runtime/int)
      &pointer_check_addresses);

  // Check pointers to cstdint typedefs.
  CheckGetPointerToScalarConstant<std::int8_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int16_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int32_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int64_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint8_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint16_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint32_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint64_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int_fast8_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int_fast16_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int_fast32_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int_fast64_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint_fast8_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint_fast16_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint_fast32_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint_fast64_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int_least8_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int_least16_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int_least32_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::int_least64_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint_least8_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint_least16_t>(
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint_least32_t>(
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uint_least64_t>(
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::intmax_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uintmax_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::uintptr_t>(&pointer_check_addresses);

  // Check pointers to cstddef typedefs.
  CheckGetPointerToScalarConstant<std::size_t>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<std::ptrdiff_t>(&pointer_check_addresses);

  // Check pointers to enums.
  CheckGetPointerToScalarConstant<SimpleEnum>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<SignedSimpleEnum>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<StronglyTypedEnum>(&pointer_check_addresses);
  CheckGetPointerToScalarConstant<SignedStronglyTypedEnum>(
      &pointer_check_addresses);
  CheckGetPointerToScalarConstant<StronglyTypedEnumUint64>(
      &pointer_check_addresses);

  // Check pointers to struct.
  DummyStruct dummy_struct;
  const DummyStruct* dummy_struct_ptr = nullptr;
  CheckGetSinglePointerConstant(dummy_struct_ptr, &pointer_check_addresses);
  dummy_struct_ptr = &dummy_struct;
  CheckGetSinglePointerConstant(dummy_struct_ptr, &pointer_check_addresses);

  // Check pointers to an abstract base class and a concrete class.
  const DummyAbstractBaseClass* dummy_abstract_ptr = nullptr;
  CheckGetSinglePointerConstant(dummy_abstract_ptr, &pointer_check_addresses);

  Negater dummy_concrete_object(42);
  const Negater* dummy_concrete_ptr = nullptr;
  CheckGetSinglePointerConstant(dummy_concrete_ptr, &pointer_check_addresses);
  dummy_abstract_ptr = &dummy_concrete_object;
  CheckGetSinglePointerConstant(dummy_abstract_ptr, &pointer_check_addresses);
  dummy_concrete_ptr = &dummy_concrete_object;
  CheckGetSinglePointerConstant(dummy_concrete_ptr, &pointer_check_addresses);

  // The various invocations of CheckGetSinglePointerConstant() above created a
  // bunch of accessor functions that we will now compile and use to check that
  // the addresses of global variables are as expected.
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));
  ASSERT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  FinishCheckingGlobalConstantPointers(pointer_check_addresses);
}

TEST_F(CodegenUtilsTest, GetFunctionTypeTest) {
  // Simple function with no parameters that returns void.
  llvm::FunctionType* fn_type = codegen_utils_->GetFunctionType<void>();
  ASSERT_NE(fn_type, nullptr);
  EXPECT_EQ(codegen_utils_->GetType<void>(), fn_type->getReturnType());
  EXPECT_EQ(0u, fn_type->getNumParams());

  // Function that takes a few different scalar parameters and returns double.
  fn_type = codegen_utils_->GetFunctionType<double,
                                             int,
                                             float,
                                             std::size_t,
                                             SignedStronglyTypedEnum>();
  ASSERT_NE(fn_type, nullptr);
  EXPECT_EQ(codegen_utils_->GetType<double>(), fn_type->getReturnType());
  ASSERT_EQ(4u, fn_type->getNumParams());
  EXPECT_EQ(codegen_utils_->GetType<int>(), fn_type->getParamType(0));
  EXPECT_EQ(codegen_utils_->GetType<float>(), fn_type->getParamType(1));
  EXPECT_EQ(codegen_utils_->GetType<std::size_t>(), fn_type->getParamType(2));
  EXPECT_EQ(codegen_utils_->GetType<SignedStronglyTypedEnum>(),
            fn_type->getParamType(3));

  // A mix of pointer and reference parameters.
  fn_type = codegen_utils_->GetFunctionType<void*,
                                             const int&,
                                             float&,
                                             const std::size_t*,
                                             SignedStronglyTypedEnum*>();
  ASSERT_NE(fn_type, nullptr);
  EXPECT_EQ(codegen_utils_->GetType<void*>(), fn_type->getReturnType());
  ASSERT_EQ(4u, fn_type->getNumParams());
  EXPECT_EQ(codegen_utils_->GetType<const int&>(), fn_type->getParamType(0));
  EXPECT_EQ(codegen_utils_->GetType<float&>(), fn_type->getParamType(1));
  EXPECT_EQ(codegen_utils_->GetType<const std::size_t*>(),
            fn_type->getParamType(2));
  EXPECT_EQ(codegen_utils_->GetType<SignedStronglyTypedEnum*>(),
            fn_type->getParamType(3));

  // Pointers and references to user-defined structs and classes.
  fn_type = codegen_utils_->GetFunctionType<DummyAbstractBaseClass*,
                                             const Squarer&,
                                             Negater&,
                                             DummyStruct*>();
  ASSERT_NE(fn_type, nullptr);
  EXPECT_EQ(codegen_utils_->GetType<DummyAbstractBaseClass*>(),
            fn_type->getReturnType());
  ASSERT_EQ(3u, fn_type->getNumParams());
  EXPECT_EQ(codegen_utils_->GetType<const Squarer&>(),
            fn_type->getParamType(0));
  EXPECT_EQ(codegen_utils_->GetType<Negater&>(),
            fn_type->getParamType(1));
  EXPECT_EQ(codegen_utils_->GetType<DummyStruct*>(),
            fn_type->getParamType(2));
}

TEST_F(CodegenUtilsTest, TrivialCompilationTest) {
  // Create an IR function that takes no arguments and returns int.
  typedef int (*SimpleFn) ();
  llvm::Function* simple_fn
      = codegen_utils_->CreateFunction<SimpleFn>("simple_fn");

  // Construct a single BasicBlock for the function's body.
  llvm::BasicBlock* simple_fn_body
      = codegen_utils_->CreateBasicBlock("simple_fn_body", simple_fn);

  // Create a return instruction that returns the constant value '42'.
  codegen_utils_->ir_builder()->SetInsertPoint(simple_fn_body);
  codegen_utils_->ir_builder()->CreateRet(
      codegen_utils_->GetConstant<int>(42));

  // Check that the function and the module are both well-formed (note that the
  // LLVM verification functions return false to indicate success).
  EXPECT_FALSE(llvm::verifyFunction(*simple_fn));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

  // Prepare generated code for execution.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  EXPECT_EQ(nullptr, codegen_utils_->module());
  typedef void (*VoidFn)();
  typedef int (*IntFn)();
  // Try looking up function names that don't exist.
  EXPECT_EQ(nullptr, codegen_utils_->GetFunctionPointer<VoidFn>("foo"));
  EXPECT_EQ(nullptr,
            codegen_utils_->GetFunctionPointer<VoidFn>("simple_fn_body"));

  // Cast to the actual function type and call the generated function.
  int (*function_ptr)()
      = codegen_utils_->GetFunctionPointer<IntFn>("simple_fn");
  ASSERT_NE(function_ptr, nullptr);

  EXPECT_EQ(42, (*function_ptr)());
}

TEST_F(CodegenUtilsTest, ExternalFunctionTest) {
  // Test a function with overloads for different types (we will explicitly use
  // the version of std::fabs() for doubles).
  MakeWrapperFunction<double, double>(&std::fabs,
                                      "fabs_double_wrapper");

  // Test a function that takes a pointer to a struct as an argument.
  MakeWrapperFunction(&std::mktime, "mktime_wrapper");

  // Test a static method of a class that returns void.
  MakeWrapperFunction(&StaticIntWrapper::Set, "StaticIntWrapper::Set_wrapper");

  // Check that the module is well-formed and prepare the generated wrapper
  // functions for execution.
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));

  // Try calling std::fabs() through the generated wrapper.
  double (*fabs_double_wrapper)(double)  // NOLINT(readability/casting)
      = codegen_utils_->GetFunctionPointer<decltype(fabs_double_wrapper)>(
          "fabs_double_wrapper");
  ASSERT_NE(fabs_double_wrapper, nullptr);
  EXPECT_EQ(12.34, (*fabs_double_wrapper)(12.34));
  EXPECT_EQ(56.78, (*fabs_double_wrapper)(-56.78));

  // Try calling std::mktime() through the generated wrapper.
  std::time_t (*mktime_wrapper)(std::tm*)
      = codegen_utils_->GetFunctionPointer<decltype(mktime_wrapper)>(
          "mktime_wrapper");
  ASSERT_NE(mktime_wrapper, nullptr);
  std::tm broken_time = {};
  broken_time.tm_year = 1900 - 1871;
  broken_time.tm_mon = 2;
  broken_time.tm_mday = 18;
  EXPECT_EQ(std::mktime(&broken_time), (*mktime_wrapper)(&broken_time));

  StaticIntWrapper::Set(0);
  void (*static_int_set_wrapper)(const int)
      = codegen_utils_->GetFunctionPointer<decltype(static_int_set_wrapper)>(
          "StaticIntWrapper::Set_wrapper");
  ASSERT_NE(static_int_set_wrapper, nullptr);
  (*static_int_set_wrapper)(42);
  EXPECT_EQ(42, StaticIntWrapper::Get());
}

TEST_F(CodegenUtilsTest, VariadicExternalFunctionTest) {
  // Test a function with overloads an external function that contains
  // variable length arguments : printf and sprintf

  // Register printf with takes variable arguments past char*
  llvm::Function* llvm_printf_function =
      codegen_utils_->RegisterExternalFunction(printf);
  ASSERT_NE(llvm_printf_function, nullptr);
  ASSERT_EQ(
      (codegen_utils_->GetFunctionType<int, char*>(true)->getPointerTo()),
      llvm_printf_function->getType());

  // Register sprintf with takes variable arguments past char*, char*
  llvm::Function* llvm_sprintf_function =
      codegen_utils_->RegisterExternalFunction(sprintf);
  ASSERT_NE(llvm_sprintf_function, nullptr);
  ASSERT_EQ(
      (codegen_utils_->GetFunctionType<int, char*, char*>(true)->getPointerTo()),
      llvm_sprintf_function->getType());

  char sprintf_with_three_args_buffer[16], sprintf_with_four_args_buffer[16];

  // Create a simple function that calls sprintf with 3 and 4 arguments and uses
  // the buffers allocated above
  typedef void (*SprintfTestType)();
  llvm::Function* sprintf_test =
      codegen_utils_->CreateFunction<SprintfTestType>("sprintf_test");
  llvm::BasicBlock* main_block =
      codegen_utils_->CreateBasicBlock( "main", sprintf_test);

  codegen_utils_->ir_builder()->SetInsertPoint(main_block);
  codegen_utils_->ir_builder()->CreateCall(
      llvm_printf_function, {
          codegen_utils_->GetConstant("%s %d"),
          codegen_utils_->GetConstant("Zero is another way of saying "),
          codegen_utils_->GetConstant(0)
      });
  codegen_utils_->ir_builder()->CreateCall(
      llvm_sprintf_function, {
          codegen_utils_->GetConstant(sprintf_with_three_args_buffer),
          codegen_utils_->GetConstant("%d"),
          codegen_utils_->GetConstant(42)
      });
  codegen_utils_->ir_builder()->CreateCall(
      llvm_sprintf_function, {
          codegen_utils_->GetConstant(sprintf_with_four_args_buffer),
          codegen_utils_->GetConstant("%d %s"),
          codegen_utils_->GetConstant(51),
          codegen_utils_->GetConstant("fifty-one")
      });
  codegen_utils_->ir_builder()->CreateRetVoid();

  // Check that the module is well-formed and prepare the generated wrapper
  // functions for execution.
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));

  SprintfTestType llvm_sprintf_test =
      codegen_utils_->GetFunctionPointer<SprintfTestType>("sprintf_test");
  llvm_sprintf_test();

  ASSERT_EQ(strcmp(sprintf_with_three_args_buffer, "42"), 0);
  ASSERT_EQ(strcmp(sprintf_with_four_args_buffer, "51 fifty-one"), 0);
}

TEST_F(CodegenUtilsTest, RecursionTest) {
  // Test a version of the factorial function that works by recursion.
  typedef unsigned (*FactorialRecursiveFn) (unsigned);
  llvm::Function* factorial_recursive
      = codegen_utils_->CreateFunction<FactorialRecursiveFn>(
          "factorial_recursive");

  // Create a BasicBlock for the function's entry point that will branch to a
  // base case and a recursive case.
  llvm::BasicBlock* entry = codegen_utils_->CreateBasicBlock(
      "entry",
      factorial_recursive);
  llvm::BasicBlock* base_case = codegen_utils_->CreateBasicBlock(
      "base_case",
      factorial_recursive);
  llvm::BasicBlock* recursive_case = codegen_utils_->CreateBasicBlock(
      "recursive_case",
      factorial_recursive);

  ASSERT_EQ(1u, factorial_recursive->arg_size());
  llvm::Value* argument = ArgumentByPosition(factorial_recursive, 0);

  // Check if we have reached the base-case (argument == 0) and conditionally
  // branch.
  codegen_utils_->ir_builder()->SetInsertPoint(entry);
  llvm::Value* arg_is_zero = codegen_utils_->ir_builder()->CreateICmpEQ(
      argument,
      codegen_utils_->GetConstant(0u));
  codegen_utils_->ir_builder()->CreateCondBr(arg_is_zero,
                                              base_case,
                                              recursive_case);

  // Base case: 0! = 1.
  codegen_utils_->ir_builder()->SetInsertPoint(base_case);
  codegen_utils_->ir_builder()->CreateRet(codegen_utils_->GetConstant(1u));

  // Recursive case: N! = N * (N - 1)!
  codegen_utils_->ir_builder()->SetInsertPoint(recursive_case);

  std::vector<llvm::Value*> recursive_call_args;
  recursive_call_args.push_back(codegen_utils_->ir_builder()->CreateSub(
      argument,
      codegen_utils_->GetConstant(1u)));
  llvm::Value* child_result = codegen_utils_->ir_builder()->CreateCall(
      factorial_recursive,
      recursive_call_args);

  llvm::Value* product = codegen_utils_->ir_builder()->CreateMul(argument,
                                                                  child_result);
  codegen_utils_->ir_builder()->CreateRet(product);

  // Verify function and module.
  EXPECT_FALSE(llvm::verifyFunction(*factorial_recursive));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

  // Prepare for execution.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  unsigned (*factorial_recursive_compiled)(unsigned)
      = codegen_utils_->GetFunctionPointer<
      decltype(factorial_recursive_compiled)>("factorial_recursive");

  // Test out the compiled function.
  EXPECT_EQ(1u, (*factorial_recursive_compiled)(0u));
  EXPECT_EQ(1u, (*factorial_recursive_compiled)(0u));
  EXPECT_EQ(1u * 2u * 3u * 4u * 5u * 6u * 7u,
            (*factorial_recursive_compiled)(7u));
}

TEST_F(CodegenUtilsTest, SwitchTest) {
  // Test that generates IR code with a SWITCH statement.
  // It takes a char as input and returns 1 if input = 'A',
  // 2 if input is 'B'; -1 otherwise.
  typedef int (*SwitchFn) (char);
  llvm::Function* switch_function
      = codegen_utils_->CreateFunction<SwitchFn>(
          "switch_function");

  // BasicBlocks for function entry, for each case of switch instruction,
  // for the default case, and for function termination
  // where an integer is returned.
  llvm::BasicBlock* entry_block = codegen_utils_->CreateBasicBlock(
      "entry",
      switch_function);

  llvm::BasicBlock* A_block = codegen_utils_->CreateBasicBlock(
      "A_block",
      switch_function);

  llvm::BasicBlock* B_block = codegen_utils_->CreateBasicBlock(
      "B_block",
      switch_function);

  llvm::BasicBlock* default_block = codegen_utils_->CreateBasicBlock(
      "default",
      switch_function);

  llvm::BasicBlock* return_block = codegen_utils_->CreateBasicBlock(
      "return",
      switch_function);

  llvm::Value* argument = ArgumentByPosition(switch_function, 0);

  // Switch instruction is located in the entry point.
  codegen_utils_->ir_builder()->SetInsertPoint(entry_block);
  llvm::SwitchInst* switch_instruction
    = codegen_utils_->ir_builder()->CreateSwitch(argument, default_block, 3);

  // Add switch cases.
  llvm::ConstantInt* val_a = static_cast<llvm::ConstantInt*>(
      codegen_utils_->GetConstant('A'));
  ASSERT_TRUE(llvm::isa<llvm::ConstantInt>(val_a));
  switch_instruction->addCase(val_a, A_block);

  llvm::ConstantInt* val_b = static_cast<llvm::ConstantInt*>(
      codegen_utils_->GetConstant('B'));
  ASSERT_TRUE(llvm::isa<llvm::ConstantInt>(val_b));
  switch_instruction->addCase(val_b, B_block);

  // All switch cases jump to return block.
  codegen_utils_->ir_builder()->SetInsertPoint(default_block);
  codegen_utils_->ir_builder()->CreateBr(return_block);

  codegen_utils_->ir_builder()->SetInsertPoint(A_block);
  codegen_utils_->ir_builder()->CreateBr(return_block);

  codegen_utils_->ir_builder()->SetInsertPoint(B_block);
  codegen_utils_->ir_builder()->CreateBr(return_block);

  // Add incoming edges from switch cases to return block,
  // where each case sends to return block the proper value.
  codegen_utils_->ir_builder()->SetInsertPoint(return_block);
  llvm::PHINode* return_node = codegen_utils_->ir_builder()->CreatePHI(
      codegen_utils_->GetType<int>(), 3);
  return_node->addIncoming(codegen_utils_->GetConstant(-1), default_block);
  return_node->addIncoming(codegen_utils_->GetConstant(1), A_block);
  return_node->addIncoming(codegen_utils_->GetConstant(2), B_block);
  codegen_utils_->ir_builder()->CreateRet(return_node);

  // Verify function and module.
  EXPECT_FALSE(llvm::verifyFunction(*switch_function));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

  // Prepare for execution.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));

  int (*switch_function_compiled)(char)  // NOLINT(readability/casting)
      = codegen_utils_->GetFunctionPointer<decltype(switch_function_compiled)>(
          "switch_function");

  // Test out the compiled function.
  EXPECT_EQ(1, (*switch_function_compiled)('A'));
  EXPECT_EQ(2, (*switch_function_compiled)('B'));
  EXPECT_EQ(-1, (*switch_function_compiled)('C'));
}

TEST_F(CodegenUtilsTest, ProjectScalarIntArrayTest) {
  ProjectScalarArrayTestHelper<int>();
}

TEST_F(CodegenUtilsTest, ProjectScalarInt16ArrayTest) {
  ProjectScalarArrayTestHelper<int16_t>();
}

TEST_F(CodegenUtilsTest, ProjectScalarInt64ArrayTest) {
  ProjectScalarArrayTestHelper<int64_t>();
}

TEST_F(CodegenUtilsTest, ProjectScalarCharArrayTest) {
  ProjectScalarArrayTestHelper<char>();
}

TEST_F(CodegenUtilsTest, IterationTest) {
  // Test a version of the factorial function works with an iterative loop.
  typedef unsigned (*FactorialIterFn) (unsigned);
  llvm::Function* factorial_iterative
      = codegen_utils_->CreateFunction<FactorialIterFn>(
          "factorial_iterative");

  // BasicBlocks for function entry, for the start of the loop where the
  // termination condition is checked, for the loop body where running variables
  // are updated, and for function termination where the computed product is
  // returned.
  llvm::BasicBlock* entry = codegen_utils_->CreateBasicBlock(
      "entry",
      factorial_iterative);
  llvm::BasicBlock* loop_start = codegen_utils_->CreateBasicBlock(
      "loop_start",
      factorial_iterative);
  llvm::BasicBlock* loop_computation = codegen_utils_->CreateBasicBlock(
      "loop_computation",
      factorial_iterative);
  llvm::BasicBlock* terminus = codegen_utils_->CreateBasicBlock(
      "terminus",
      factorial_iterative);

  ASSERT_EQ(1u, factorial_iterative->arg_size());
  llvm::Value* argument = ArgumentByPosition(factorial_iterative, 0);

  // Entry point unconditionally enters the loop. Note that we can't just make
  // "loop_start" the entry point for the function, because it has PHI-nodes
  // that need to be assigned based on predecessor BasicBlocks.
  codegen_utils_->ir_builder()->SetInsertPoint(entry);
  codegen_utils_->ir_builder()->CreateBr(loop_start);

  // Create PHI nodes to represent the current factor (starting at the
  // argument's value and counting down to zero) and the current product
  // (starting at one and getting multiplied for each iteration of the loop).
  codegen_utils_->ir_builder()->SetInsertPoint(loop_start);

  llvm::PHINode* current_factor = codegen_utils_->ir_builder()->CreatePHI(
      codegen_utils_->GetType<unsigned>(), 2);
  current_factor->addIncoming(argument, entry);

  llvm::PHINode* current_product = codegen_utils_->ir_builder()->CreatePHI(
      codegen_utils_->GetType<unsigned>(), 2);
  current_product->addIncoming(codegen_utils_->GetConstant(1u), entry);

  // If 'current_factor' has reached zero, break out of the loop. Otherwise
  // proceed to "loop_computation" to compute the factor and the product for the
  // next iteration.
  llvm::Value* current_factor_is_zero
      = codegen_utils_->ir_builder()->CreateICmpEQ(
          current_factor,
          codegen_utils_->GetConstant(0u));
  codegen_utils_->ir_builder()->CreateCondBr(current_factor_is_zero,
                                              terminus,
                                              loop_computation);

  // Compute values for the next iteration of the loop and go back to
  // "loop_start".
  codegen_utils_->ir_builder()->SetInsertPoint(loop_computation);
  llvm::Value* next_factor = codegen_utils_->ir_builder()->CreateSub(
      current_factor,
      codegen_utils_->GetConstant(1u));
  llvm::Value* next_product = codegen_utils_->ir_builder()->CreateMul(
      current_factor,
      current_product);
  codegen_utils_->ir_builder()->CreateBr(loop_start);

  // Add incoming edges to the PHI nodes in "loop_start" for the newly-computed
  // values.
  current_factor->addIncoming(next_factor, loop_computation);
  current_product->addIncoming(next_product, loop_computation);

  // Terminus just returns the computed product.
  codegen_utils_->ir_builder()->SetInsertPoint(terminus);
  codegen_utils_->ir_builder()->CreateRet(current_product);

  // Verify function and module.
  EXPECT_FALSE(llvm::verifyFunction(*factorial_iterative));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

  // Prepare for execution.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  unsigned (*factorial_iterative_compiled)(unsigned)
      = codegen_utils_->GetFunctionPointer<
      decltype(factorial_iterative_compiled)>("factorial_iterative");

  // Test out the compiled function.
  EXPECT_EQ(1u, (*factorial_iterative_compiled)(0u));
  EXPECT_EQ(1u, (*factorial_iterative_compiled)(0u));
  EXPECT_EQ(1u * 2u * 3u * 4u * 5u * 6u * 7u,
            (*factorial_iterative_compiled)(7u));
}

// Macro that provides some syntactic sugar for a call to
// CheckGetPointerToMemberConstant(). Automates deduction of the expected
// member type and calculation of the expected offset within the struct.
#define GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(struct_ptr, element_name) \
  CheckGetPointerToMemberConstant<                                             \
      std::remove_reference<decltype((struct_ptr)->element_name)>::type>(      \
          &pointer_check_addresses,                                            \
          struct_ptr,                                                          \
          offsetof(std::remove_pointer<decltype(struct_ptr)>::type,            \
                   element_name),                                              \
          &std::remove_pointer<decltype(struct_ptr)>::type::element_name)

// Similar to above, but tests accesing a field nested inside a struct member of
// the top-level struct.
#define GPCODEGEN_TEST_GET_POINTER_TO_NESTED_STRUCT_ELEMENT(                   \
    struct_ptr,                                                                \
    top_element_name,                                                          \
    nested_element_name)                                                       \
  CheckGetPointerToMemberConstant<                                             \
          std::remove_reference<decltype((struct_ptr)                          \
              ->top_element_name.nested_element_name)>::type>(                 \
      &pointer_check_addresses,                                                \
      struct_ptr,                                                              \
      offsetof(std::remove_pointer<decltype(struct_ptr)>::type,                \
               top_element_name)                                               \
          + offsetof(std::remove_reference<                                    \
                         decltype((struct_ptr)->top_element_name)>::type,      \
                     nested_element_name),                                     \
      &std::remove_pointer<decltype(struct_ptr)>::type::top_element_name,      \
      &std::remove_reference<decltype((struct_ptr)->top_element_name)>::type   \
          ::nested_element_name)

// Test for CodegenUtils::GetPointerToMember() with constant pointers to
// external structs.
TEST_F(CodegenUtilsTest, GetPointerToMemberConstantTest) {
  // Remember the addresses of pointer constants, in order, that we expect check
  // functions to return.
  std::vector<std::uintptr_t> pointer_check_addresses;

  // Test a struct on the stack.
  DummyStruct stack_struct;
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(&stack_struct, int_field);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(&stack_struct, bool_field);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(&stack_struct, double_field);

  // Also works without specifying any members at all. This trivially gives a
  // pointer to the original struct.
  CheckGetPointerToMemberConstant<DummyStruct>(&pointer_check_addresses,
                                               &stack_struct,
                                               0);

  // And on the heap.
  std::unique_ptr<DummyStruct> heap_struct(new DummyStruct);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(heap_struct.get(), int_field);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(heap_struct.get(), bool_field);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(heap_struct.get(), double_field);
  CheckGetPointerToMemberConstant<DummyStruct>(&pointer_check_addresses,
                                               heap_struct.get(),
                                               0);

  // A NULL pointer also works, since CodegenUtils::GetPointerToMember() only
  // does address computation and doesn't dereference anything.
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(
      static_cast<DummyStruct*>(nullptr),
      int_field);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(
      static_cast<DummyStruct*>(nullptr),
      bool_field);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(
      static_cast<DummyStruct*>(nullptr),
      double_field);
  CheckGetPointerToMemberConstant<DummyStruct>(
      &pointer_check_addresses,
      static_cast<DummyStruct*>(nullptr),
      0);

  // Also test a struct with char and char* fields to make sure that there is no
  // confusion when the pointer-to-member type is the same as the pointer type
  // used for the underlying address computation.
  DummyStructWithCharFields stack_struct_with_char_fields;
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(&stack_struct_with_char_fields,
                                              front_char);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(&stack_struct_with_char_fields,
                                              char_ptr);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(&stack_struct_with_char_fields,
                                              back_char);

  // Also test a struct that nests other structs.
  Matryoshka stack_matryoshka;
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(
      &stack_matryoshka,
      nested_dummy_struct_with_char_fields);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(
      &stack_matryoshka,
      non_nested_char);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(
      &stack_matryoshka,
      non_nested_int);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(
      &stack_matryoshka,
      ptr_to_peer);
  GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT(
      &stack_matryoshka,
      nested_dummy_struct);

  // Test accessing fields inside nested structs with a single call to
  // GetPointerToMember().
  GPCODEGEN_TEST_GET_POINTER_TO_NESTED_STRUCT_ELEMENT(
      &stack_matryoshka,
      nested_dummy_struct_with_char_fields,
      front_char);
  GPCODEGEN_TEST_GET_POINTER_TO_NESTED_STRUCT_ELEMENT(
      &stack_matryoshka,
      nested_dummy_struct_with_char_fields,
      char_ptr);
  GPCODEGEN_TEST_GET_POINTER_TO_NESTED_STRUCT_ELEMENT(
      &stack_matryoshka,
      nested_dummy_struct_with_char_fields,
      back_char);
  GPCODEGEN_TEST_GET_POINTER_TO_NESTED_STRUCT_ELEMENT(
      &stack_matryoshka,
      nested_dummy_struct,
      int_field);
  GPCODEGEN_TEST_GET_POINTER_TO_NESTED_STRUCT_ELEMENT(
      &stack_matryoshka,
      nested_dummy_struct,
      bool_field);
  GPCODEGEN_TEST_GET_POINTER_TO_NESTED_STRUCT_ELEMENT(
      &stack_matryoshka,
      nested_dummy_struct,
      double_field);

  // Now we compile and call the various constant-accessor functions that were
  // generated in the course of this test, checking that they return the
  // expected addresses of member fields.
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));
  ASSERT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  FinishCheckingGlobalConstantPointers(pointer_check_addresses);
}

#undef GPCODEGEN_TEST_GET_POINTER_TO_NESTED_STRUCT_ELEMENT
#undef GPCODEGEN_TEST_GET_POINTER_TO_STRUCT_ELEMENT

TEST_F(CodegenUtilsTest, GetPointerToMemberTest) {
  // Create some accessor functions that load the value of fields in a struct
  // passed in as a pointer.
  MakeStructMemberAccessorFunction<DummyStruct, int>(
      "Get_DummyStruct::int_field",
      &DummyStruct::int_field);
  MakeStructMemberAccessorFunction<DummyStruct, bool>(
      "Get_DummyStruct::bool_field",
      &DummyStruct::bool_field);
  MakeStructMemberAccessorFunction<DummyStruct, double>(
      "Get_DummyStruct::double_field",
      &DummyStruct::double_field);

  // Check that module is well-formed, then compile.
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));

  int (*Get_DummyStruct_int_field)(const DummyStruct*)
      = codegen_utils_->GetFunctionPointer<
      decltype(Get_DummyStruct_int_field)>("Get_DummyStruct::int_field");
  ASSERT_NE(Get_DummyStruct_int_field, nullptr);

  bool (*Get_DummyStruct_bool_field)(const DummyStruct*)
      = codegen_utils_->GetFunctionPointer<
      decltype(Get_DummyStruct_bool_field)>("Get_DummyStruct::bool_field");
  ASSERT_NE(Get_DummyStruct_bool_field, nullptr);

  double (*Get_DummyStruct_double_field)(const DummyStruct*)
      = codegen_utils_->GetFunctionPointer<
      decltype(Get_DummyStruct_double_field)>("Get_DummyStruct::double_field");
  ASSERT_NE(Get_DummyStruct_double_field, nullptr);

  // Call generated accessor function and make sure they read values from the
  // passed-in struct pointer properly.
  DummyStruct test_struct{42, true, -12.34};

  EXPECT_EQ(42, (*Get_DummyStruct_int_field)(&test_struct));
  EXPECT_EQ(true, (*Get_DummyStruct_bool_field)(&test_struct));
  EXPECT_EQ(-12.34, (*Get_DummyStruct_double_field)(&test_struct));

  // Modify and read again.
  test_struct.int_field = -123;
  test_struct.bool_field = false;
  test_struct.double_field = 1e100;

  EXPECT_EQ(-123, (*Get_DummyStruct_int_field)(&test_struct));
  EXPECT_FALSE((*Get_DummyStruct_bool_field)(&test_struct));
  EXPECT_EQ(1e100, (*Get_DummyStruct_double_field)(&test_struct));
}

TEST_F(CodegenUtilsTest, OptimizationTest) {
  // Create an ultra-simple function that just adds 2 ints. We expect this to be
  // automatically inlined at call sites during optimization.
  typedef int (*Add2Fn) (int, int);
  llvm::Function* add2_func
      = codegen_utils_->CreateFunction<Add2Fn>("add2");
  llvm::BasicBlock* add2_body = codegen_utils_->CreateBasicBlock("body",
                                                                  add2_func);
  codegen_utils_->ir_builder()->SetInsertPoint(add2_body);
  llvm::Value* add2_sum = codegen_utils_->ir_builder()->CreateAdd(
      ArgumentByPosition(add2_func, 0),
      ArgumentByPosition(add2_func, 1));
  codegen_utils_->ir_builder()->CreateRet(add2_sum);

  // Create another function that adds 3 ints by making 2 calls to add2.
  typedef int (*Add3Fn) (int, int, int);
  llvm::Function* add3_func
      = codegen_utils_->CreateFunction<Add3Fn>("add3");
  llvm::BasicBlock* add3_body = codegen_utils_->CreateBasicBlock("body",
                                                                  add3_func);
  codegen_utils_->ir_builder()->SetInsertPoint(add3_body);
  llvm::Value* add3_sum1 = codegen_utils_->ir_builder()->CreateCall(
      add2_func,
      {ArgumentByPosition(add3_func, 0), ArgumentByPosition(add3_func, 1)});
  llvm::Value* add3_sum2 = codegen_utils_->ir_builder()->CreateCall(
      add2_func,
      {add3_sum1, ArgumentByPosition(add3_func, 2)});
  codegen_utils_->ir_builder()->CreateRet(add3_sum2);

  // Before optimization, function memory-access characteristics are not known.
  EXPECT_FALSE(add2_func->doesNotAccessMemory());
  EXPECT_FALSE(add3_func->doesNotAccessMemory());

  // Apply basic optimizations.
  EXPECT_TRUE(codegen_utils_->Optimize(CodegenUtils::OptimizationLevel::kLess,
                                        CodegenUtils::SizeLevel::kNormal,
                                        false));

  // Analysis passes should have marked both functions "readnone" since they do
  // not access any external memory.
  EXPECT_TRUE(add2_func->doesNotAccessMemory());
  EXPECT_TRUE(add3_func->doesNotAccessMemory());

  // We expect the tiny add2 function to be inlined into add3. We iterate
  // through the instructions in add3's body and check that none are calls.
  for (const llvm::Instruction& instruction : *add3_body) {
    EXPECT_NE(instruction.getOpcode(), llvm::Instruction::Call);
  }

  // Now, actually compile machine code from the optimized IR and call it.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kLess,
      false));
  int (*add3_compiled)(int, int, int)
      = codegen_utils_->GetFunctionPointer<decltype(add3_compiled)>("add3");
  EXPECT_EQ(758, (*add3_compiled)(12, -67, 813));
}

// Test code-generation used with instance methods of a statically compiled C++
// class.
TEST_F(CodegenUtilsTest, CppClassObjectTest) {
  // Register method wrappers for Accumulator<double>
  llvm::Function* new_accumulator_double
      = codegen_utils_->RegisterExternalFunction(
          &WrapNew<Accumulator<double>, double>);
  llvm::Function* delete_accumulator_double
      = codegen_utils_->RegisterExternalFunction(
          &WrapDelete<Accumulator<double>>);
  llvm::Function* accumulator_double_accumulate
      = codegen_utils_->RegisterExternalFunction(
          &GPCODEGEN_WRAP_METHOD(&Accumulator<double>::Accumulate));
  llvm::Function* accumulator_double_get
      = codegen_utils_->RegisterExternalFunction(
          &GPCODEGEN_WRAP_METHOD(&Accumulator<double>::Get));

  typedef double (*AccumulateTestFn) (double);
  llvm::Function* accumulate_test_fn
      = codegen_utils_->CreateFunction<AccumulateTestFn>("accumulate_test_fn");
  llvm::BasicBlock* body
      = codegen_utils_->CreateBasicBlock("body", accumulate_test_fn);
  codegen_utils_->ir_builder()->SetInsertPoint(body);

  // Make a new accumulator object, forwarding the function's argument to the
  // constructor.
  llvm::Value* accumulator_ptr = codegen_utils_->ir_builder()->CreateCall(
      new_accumulator_double,
      {ArgumentByPosition(accumulate_test_fn, 0)});

  // Add a few constants to the accumulator via the wrapped instance method.
  codegen_utils_->ir_builder()->CreateCall(
      accumulator_double_accumulate,
      {accumulator_ptr, codegen_utils_->GetConstant(1.0)});
  codegen_utils_->ir_builder()->CreateCall(
      accumulator_double_accumulate,
      {accumulator_ptr, codegen_utils_->GetConstant(2.0)});
  codegen_utils_->ir_builder()->CreateCall(
      accumulator_double_accumulate,
      {accumulator_ptr, codegen_utils_->GetConstant(3.0)});
  codegen_utils_->ir_builder()->CreateCall(
      accumulator_double_accumulate,
      {accumulator_ptr, codegen_utils_->GetConstant(4.0)});

  // Read out the accumulated value.
  llvm::Value* retval = codegen_utils_->ir_builder()->CreateCall(
      accumulator_double_get,
      {accumulator_ptr});

  // Delete the accumulator object.
  codegen_utils_->ir_builder()->CreateCall(
      delete_accumulator_double,
      {accumulator_ptr});

  // Return the accumulated value.
  codegen_utils_->ir_builder()->CreateRet(retval);

  // Check that function and module are well-formed, then compile.
  EXPECT_FALSE(llvm::verifyFunction(*accumulate_test_fn));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));

  double (*accumulate_test_fn_compiled)(double)  // NOLINT(readability/casting)
      = codegen_utils_->GetFunctionPointer<
      decltype(accumulate_test_fn_compiled)>("accumulate_test_fn");

  // Actually invoke the function and make sure that the wrapped behavior of
  // Accumulator is as expected.
  EXPECT_EQ(42.0, (*accumulate_test_fn_compiled)(32.0));
  EXPECT_EQ(-12.75, (*accumulate_test_fn_compiled)(-22.75));
}


#ifdef GPCODEGEN_DEBUG

TEST_F(CodegenUtilsDeathTest, WrongFunctionTypeTest) {
  // Create a function identical to the one in TrivialCompilationTest, but try
  // GetFunctionPointer() with the wrong type-signature.
  typedef int (*SimpleFn) ();
  llvm::Function* simple_fn
      = codegen_utils_->CreateFunction<SimpleFn>("simple_fn");
  llvm::BasicBlock* simple_fn_body
      = codegen_utils_->CreateBasicBlock("simple_fn_body", simple_fn);
  codegen_utils_->ir_builder()->SetInsertPoint(simple_fn_body);
  codegen_utils_->ir_builder()->CreateRet(
      codegen_utils_->GetConstant<int>(42));
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  typedef float (*FloatFn) ();
  EXPECT_DEATH(codegen_utils_->GetFunctionPointer<FloatFn>("simple_fn"), "");
}

TEST_F(CodegenUtilsDeathTest, ModifyExternalFunctionTest) {
  // Register an external function, then try to add a BasicBlock to it.
  llvm::Function* external_function
      = codegen_utils_->RegisterExternalFunction(&std::mktime);

  EXPECT_DEATH(codegen_utils_->CreateBasicBlock("body", external_function),
               "");
}

TEST_F(CodegenUtilsDeathTest, GetPointerToMemberFromNullBasePointerTest) {
  // Set up a dummy function and BasicBlock to hold instructions.
  typedef void (*DummFn) ();
  llvm::Function* dummy_fn
      = codegen_utils_->CreateFunction<DummFn>("dummy_fn");
  llvm::BasicBlock* dummy_fn_body
      = codegen_utils_->CreateBasicBlock("dummy_fn_body", dummy_fn);
  codegen_utils_->ir_builder()->SetInsertPoint(dummy_fn_body);

  EXPECT_DEATH(codegen_utils_->GetPointerToMember(nullptr,
                                                   &DummyStruct::int_field),
               "");
}

TEST_F(CodegenUtilsDeathTest, GetPointerToMemberFromWrongTypeBasePointerTest) {
  // Set up a dummy function and BasicBlock to hold instructions.
  typedef void (*DummyFn) ();
  llvm::Function* dummy_fn
      = codegen_utils_->CreateFunction<DummyFn>("dummy_fn");
  llvm::BasicBlock* dummy_fn_body
      = codegen_utils_->CreateBasicBlock("dummy_fn_body", dummy_fn);
  codegen_utils_->ir_builder()->SetInsertPoint(dummy_fn_body);

  const int external_int = 42;
  llvm::Value* external_int_ptr = codegen_utils_->GetConstant(&external_int);

  // Pointers to structs are expected to be represented as i8*, but here we are
  // passing an i32* pointer.
  EXPECT_DEATH(codegen_utils_->GetPointerToMember(external_int_ptr,
                                                   &DummyStruct::int_field),
               "");
}

#endif  // GPCODEGEN_DEBUG

}  // namespace gpcodegen

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  AddGlobalTestEnvironment(new gpcodegen::CodegenUtilsTestEnvironment);
  return RUN_ALL_TESTS();
}

// EOF
