

void test__ShmemAlloc_can_allocate_memory(void) {
  char *someMemoryPlace = (char *)ShmemAlloc(10);
  strlcpy(someMemoryPlace, "123456789", 10);
  assert_string_equal(someMemoryPlace, "123456789");
}

void test__add_size__when_two_positive_sizes_are_passed_it_returns_the_sum_of_the_sizes(void) {
    Size resultSize = add_size(99, 100);

    assert_int_equal(resultSize, 199);
}

void test__mul_size__when_first_argument_is_zero_it_returns_zero(void) {
    Size resultSize = mul_size(0, 100);

    assert_int_equal(resultSize, 0);
}

void test__mul_size__when_second_argument_is_zero_it_returns_zero(void) {
    Size resultSize = mul_size(100, 0);

    assert_int_equal(resultSize, 0);
}

void test__mul_size__when_both_arguments_are_zero_it_returns_zero(void) {
    Size resultSize = mul_size(0, 0);

    assert_int_equal(resultSize, 0);
}
void test__mul_size__when_both_arguments_are_not_zero_it_returns_the_mutiplication_of_sizes(void) {
    Size resultSize = mul_size(10, 30);

    assert_int_equal(resultSize, 300);
}