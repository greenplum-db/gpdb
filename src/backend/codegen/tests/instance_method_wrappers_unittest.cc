//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    instance_method_wrappers_unittest.cc
//
//  @doc:
//    Unittests for instance method wrapper feature of codegen.
//
//  @test:
//
//---------------------------------------------------------------------------

#include <cstddef>
#include <cstdlib>

#include "codegen/utils/instance_method_wrappers.h"
#include "gtest/gtest.h"

namespace gpcodegen {

namespace {

class AbstractBase {
 public:
  static std::size_t constructor_call_count_;
  static std::size_t destructor_call_count_;

  explicit AbstractBase(const int int_payload)
      : int_payload_(int_payload) {
    ++constructor_call_count_;
  }

  virtual ~AbstractBase() = 0;

  // A non-overloaded method of the base class.
  int AddPayload(const int arg) const {
    return arg + int_payload_;
  }

  // Overloaded method of base class.
  int SubPayload(const int arg) const {
    return arg - int_payload_;
  }

  double SubPayload(const double arg) const {
    return arg - int_payload_;
  }

  double SubPayload(const double arg_a, const double arg_b) const {
    return arg_a + arg_b - int_payload_;
  }

  // Non-overloaded virtual method of base class.
  virtual int MulPayload(const int arg) const = 0;

  // Overloaded virtual method of base class.
  virtual int DivPayload(const int arg) const = 0;
  virtual double DivPayload(const double arg) const = 0;

  // Zero-argument method.
  int GetPayload() const {
    return int_payload_;
  }

  // Overloaded zero-argument method (overloaded on the const-ness of the
  // implicit 'this' pointer).
  bool ThisConst() {
    return false;
  }

  bool ThisConst() const {
    return true;
  }

  // Overloaded prefix-increment operator.
  AbstractBase& operator++() {
    ++int_payload_;
    return *this;
  }

  // Operator with multiple overloads.
  int operator+(const int arg) const {
    return int_payload_ + arg;
  }

  double operator+(const double arg) const {
    return int_payload_ + arg;
  }

 protected:
  int int_payload_;
};

std::size_t AbstractBase::constructor_call_count_ = 0;
std::size_t AbstractBase::destructor_call_count_ = 0;

AbstractBase::~AbstractBase() {
  ++destructor_call_count_;
}

// Template for derived classes.
template <bool flip_sign>
class Derived : public AbstractBase {
 public:
  static std::size_t constructor_call_count_;
  static std::size_t destructor_call_count_;

  explicit Derived(const int int_payload)
      : AbstractBase(int_payload) {
    ++constructor_call_count_;
  }

  // Copy constructor.
  Derived(const Derived& original) = default;

  // Move constructor. Although not strictly necessary, we zero out the original
  // so that we can check that it was indeed moved-from.
  Derived(Derived&& original)  // NOLINT(build/c++11)
      : Derived(original.int_payload_) {
    original.int_payload_ = 0;
  }

  ~Derived() override {
    ++destructor_call_count_;
  }

  int MulPayload(const int arg) const override {
    return flip_sign ? -arg * int_payload_
                     : arg * int_payload_;
  }

  int DivPayload(const int arg) const override {
    return flip_sign ? -arg / int_payload_
                     : arg / int_payload_;
  }

  double DivPayload(const double arg) const override {
    return flip_sign ? -arg / int_payload_
                     : arg / int_payload_;
  }
};

template <bool flip_sign>
std::size_t Derived<flip_sign>::constructor_call_count_ = 0;

template <bool flip_sign>
std::size_t Derived<flip_sign>::destructor_call_count_ = 0;

typedef Derived<false> DerivedA;
typedef Derived<true> DerivedB;

}  // namespace

class InstanceMethodWrappersTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // Zero out global counters before each test.
    AbstractBase::constructor_call_count_ = 0;
    AbstractBase::destructor_call_count_ = 0;
    DerivedA::constructor_call_count_ = 0;
    DerivedA::destructor_call_count_ = 0;
    DerivedB::constructor_call_count_ = 0;
    DerivedB::destructor_call_count_ = 0;
  }
};

// Simple test for wrapped new and delete.
TEST_F(InstanceMethodWrappersTest, NewDeleteTest) {
  DerivedA* instance_a = WrapNew<DerivedA>(42);
  ASSERT_NE(instance_a, nullptr);
  EXPECT_EQ(42, instance_a->GetPayload());
  EXPECT_EQ(1u, AbstractBase::constructor_call_count_);
  EXPECT_EQ(1u, DerivedA::constructor_call_count_);
  EXPECT_EQ(0u, DerivedB::constructor_call_count_);

  DerivedB* instance_b = WrapNew<DerivedB>(123);
  ASSERT_NE(instance_b, nullptr);
  EXPECT_EQ(123, instance_b->GetPayload());
  EXPECT_EQ(2u, AbstractBase::constructor_call_count_);
  EXPECT_EQ(1u, DerivedA::constructor_call_count_);
  EXPECT_EQ(1u, DerivedB::constructor_call_count_);

  WrapDelete(instance_a);
  EXPECT_EQ(1u, AbstractBase::destructor_call_count_);
  EXPECT_EQ(1u, DerivedA::destructor_call_count_);
  EXPECT_EQ(0u, DerivedB::destructor_call_count_);

  WrapDelete(instance_b);
  EXPECT_EQ(2u, AbstractBase::destructor_call_count_);
  EXPECT_EQ(1u, DerivedA::destructor_call_count_);
  EXPECT_EQ(1u, DerivedB::destructor_call_count_);
}

// Similar to NewDeleteTest, but tests placement-new and invocation of a
// destructor without the delete operator.
TEST_F(InstanceMethodWrappersTest, PlacementNewDeleteTest) {
  void* instance_a_mem = std::malloc(sizeof(DerivedA));
  void* instance_b_mem = std::malloc(sizeof(DerivedB));

  DerivedA* instance_a = WrapPlacementNew<DerivedA>(instance_a_mem, 42);
  ASSERT_NE(instance_a, nullptr);
  EXPECT_EQ(instance_a_mem, instance_a);
  EXPECT_EQ(42, instance_a->GetPayload());
  EXPECT_EQ(1u, AbstractBase::constructor_call_count_);
  EXPECT_EQ(1u, DerivedA::constructor_call_count_);
  EXPECT_EQ(0u, DerivedB::constructor_call_count_);

  DerivedB* instance_b = WrapPlacementNew<DerivedB>(instance_b_mem, 123);
  ASSERT_NE(instance_b, nullptr);
  EXPECT_EQ(instance_b_mem, instance_b);
  EXPECT_EQ(123, instance_b->GetPayload());
  EXPECT_EQ(2u, AbstractBase::constructor_call_count_);
  EXPECT_EQ(1u, DerivedA::constructor_call_count_);
  EXPECT_EQ(1u, DerivedB::constructor_call_count_);

  WrapDestructor(instance_a);
  EXPECT_EQ(1u, AbstractBase::destructor_call_count_);
  EXPECT_EQ(1u, DerivedA::destructor_call_count_);
  EXPECT_EQ(0u, DerivedB::destructor_call_count_);

  WrapDestructor(instance_b);
  EXPECT_EQ(2u, AbstractBase::destructor_call_count_);
  EXPECT_EQ(1u, DerivedA::destructor_call_count_);
  EXPECT_EQ(1u, DerivedB::destructor_call_count_);

  std::free(instance_a_mem);
  std::free(instance_b_mem);
}

// Test copy-construction.
TEST_F(InstanceMethodWrappersTest, CopyTest) {
  DerivedA original(42);

  DerivedA* copy = WrapNew<DerivedA>(original);
  ASSERT_NE(copy, nullptr);
  EXPECT_NE(&original, copy);
  EXPECT_EQ(42, copy->GetPayload());

  // Also try copy constructor with placement-new.
  void* placement_copy_mem = std::malloc(sizeof(DerivedA));
  DerivedA* placement_copy = WrapPlacementNew<DerivedA>(placement_copy_mem,
                                                        original);
  ASSERT_NE(placement_copy, nullptr);
  EXPECT_EQ(placement_copy_mem, placement_copy);
  EXPECT_EQ(42, placement_copy->GetPayload());

  WrapDelete(copy);
  WrapDestructor(placement_copy);
  std::free(placement_copy_mem);
}

// Test move-construction.
TEST_F(InstanceMethodWrappersTest, MoveTest) {
  DerivedA original(42);

  DerivedA* moved = WrapNewMove<DerivedA>(&original);
  ASSERT_NE(moved, nullptr);
  EXPECT_EQ(42, moved->GetPayload());

  // Verify that the original was moved-from and not copied.
  EXPECT_EQ(0, original.GetPayload());

  // Also check placement-new with move semantics.
  void* moved_2_mem = std::malloc(sizeof(DerivedA));
  DerivedA* moved_2 = WrapPlacementNewMove<DerivedA>(moved_2_mem, moved);
  ASSERT_NE(moved_2, nullptr);
  EXPECT_EQ(moved_2_mem, moved_2);
  EXPECT_EQ(42, moved_2->GetPayload());

  EXPECT_EQ(0, moved->GetPayload());

  WrapDelete(moved);
  WrapDestructor(moved_2);
  std::free(moved_2_mem);
}

TEST_F(InstanceMethodWrappersTest, SimpleMethodTest) {
  DerivedA instance_a1(42);
  DerivedA instance_a2(123);
  DerivedB instance_b(-12);

  // Invoke a simple non-overloaded method via a wrapper.
  EXPECT_EQ(45,
            (GPCODEGEN_WRAP_METHOD(&AbstractBase::AddPayload)
                (&instance_a1, 3)));
  EXPECT_EQ(126,
            (GPCODEGEN_WRAP_METHOD(&AbstractBase::AddPayload)
                (&instance_a2, 3)));
  EXPECT_EQ(-9,
            (GPCODEGEN_WRAP_METHOD(&AbstractBase::AddPayload)
                (&instance_b, 3)));

  // Should also work fine if we refer to the derived classes rather than the
  // base.
  EXPECT_EQ(45,
            (GPCODEGEN_WRAP_METHOD(&DerivedA::AddPayload)
                (&instance_a1, 3)));
  EXPECT_EQ(126,
            (GPCODEGEN_WRAP_METHOD(&DerivedA::AddPayload)
                (&instance_a2, 3)));
  EXPECT_EQ(-9,
            (GPCODEGEN_WRAP_METHOD(&DerivedB::AddPayload)
                (&instance_b, 3)));
}

TEST_F(InstanceMethodWrappersTest, OverloadedMethodTest) {
  DerivedA instance_a1(42);
  DerivedA instance_a2(123);
  DerivedB instance_b(-12);

  // Overloaded methods distinguished only by their const-qualifier.
  EXPECT_FALSE((GPCODEGEN_WRAP_OVERLOADED_METHOD_ZERO_ARGS(
                    bool,
                    AbstractBase,
                    ThisConst,)(  // NOLINT(whitespace/comma)
                        &instance_a1)));
  EXPECT_TRUE((GPCODEGEN_WRAP_OVERLOADED_METHOD_ZERO_ARGS(
                   bool,
                   AbstractBase,
                   ThisConst,
                   const)(
                       &instance_a1)));

  // Overloads which take arguments.
  EXPECT_EQ(58,
            (GPCODEGEN_WRAP_OVERLOADED_METHOD(
                int,
                AbstractBase,
                SubPayload,
                const,
                int)(
                    &instance_a1,
                    100)));

  EXPECT_EQ(2.125,
            (GPCODEGEN_WRAP_OVERLOADED_METHOD(
                double,
                AbstractBase,
                SubPayload,
                const,
                double)(
                    &instance_a2,
                    125.125)));

  EXPECT_EQ(124.75,
            (GPCODEGEN_WRAP_OVERLOADED_METHOD(
                double,
                AbstractBase,
                SubPayload,
                const,
                double,
                double)(
                    &instance_b,
                    12.5,
                    100.25)));
}

TEST_F(InstanceMethodWrappersTest, VirtualMethodTest) {
  DerivedA instance_a1(42);
  DerivedA instance_a2(123);
  DerivedB instance_b(-12);

  // Invoke virtual method of base class on instances of derived classes.
  EXPECT_EQ(84,
            (GPCODEGEN_WRAP_METHOD(&AbstractBase::MulPayload)
                (&instance_a1, 2)));
  EXPECT_EQ(369,
            (GPCODEGEN_WRAP_METHOD(&AbstractBase::MulPayload)
                (&instance_a2, 3)));
  EXPECT_EQ(48,
            (GPCODEGEN_WRAP_METHOD(&AbstractBase::MulPayload)
                (&instance_b, 4)));

  // Should also work if we refer to the derived classes than the base.
  EXPECT_EQ(84,
            (GPCODEGEN_WRAP_METHOD(&DerivedA::MulPayload)(&instance_a1, 2)));
  EXPECT_EQ(369,
            (GPCODEGEN_WRAP_METHOD(&DerivedA::MulPayload)(&instance_a2, 3)));
  EXPECT_EQ(48,
            (GPCODEGEN_WRAP_METHOD(&DerivedB::MulPayload)(&instance_b, 4)));
}

TEST_F(InstanceMethodWrappersTest, OverloadedVirtualMethodTest) {
  DerivedA instance_a1(42);
  DerivedA instance_a2(123);
  DerivedB instance_b(-12);

  // Invoke virtual method with overloads.
  EXPECT_EQ(1,
            (GPCODEGEN_WRAP_OVERLOADED_METHOD(
                int,
                AbstractBase,
                DivPayload,
                const,
                int)(
                    &instance_a1,
                    63)));

  EXPECT_EQ(1.5,
            (GPCODEGEN_WRAP_OVERLOADED_METHOD(
                double,
                AbstractBase,
                DivPayload,
                const,
                double)(
                    &instance_a1,
                    63.0)));

  EXPECT_EQ(-1.75,
            (GPCODEGEN_WRAP_OVERLOADED_METHOD(
                double,
                AbstractBase,
                DivPayload,
                const,
                double)(
                    &instance_b,
                    -21.0)));
}

TEST_F(InstanceMethodWrappersTest, OperatorOverloadTest) {
  DerivedA instance(42);

  // Simple overloaded operator.
  AbstractBase& incremented
      = GPCODEGEN_WRAP_METHOD(&AbstractBase::operator++)(&instance);
  EXPECT_EQ(&instance, &incremented);
  EXPECT_EQ(43, instance.GetPayload());

  // Operator with multiple differently-typed overloads.
  EXPECT_EQ(45,
            (GPCODEGEN_WRAP_OVERLOADED_METHOD(
                int,
                AbstractBase,
                operator+,
                const,
                int)(
                    &instance,
                    2)));
  EXPECT_EQ(0.5,
            (GPCODEGEN_WRAP_OVERLOADED_METHOD(
                double,
                AbstractBase,
                operator+,
                const,
                double)(
                    &instance,
                    -42.5)));
}

}  // namespace gpcodegen

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// EOF
