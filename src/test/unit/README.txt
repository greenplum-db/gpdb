= guts - GPDB Unit Testing System =

The following documentation should help developing unit tests for GPDB using 
the cmockery framework.

Unit testing is an approach to test a small part of a program isolated from the 
test. This type of testing is very common in Java community. The benefits are 
a) that problems are found very early in the development process, 
b) it simplifies the integration of different modules because unit testing 
established a basic trust in the small units themselves, c) it serves as 
documentation about the expected behavior, d) enables easier code reuse because 
code is already tested via two different code paths [Wikipedia].

The unit test code is stored in src/test/unit. The directory contains the
modified cmockery library, all mock source files currently in use (mock) and a 
directory test, which contains all test code.

The different test executables can be found in the subdirectories of the 
directory test. By the design the used test library cmockery works, a test 
executable can only focus on a set of source files. We call this portion the 
System Under Test (SUT). We will talk more about the System Under Test later. 
For now, it is important to know that the SUT is the port of code tested in a 
test and that each SUT has its own test executable. Usually a SUT consists of a 
single file, e.g. heapam.c, but it can also consist of multiple related files.
 
== Test Development Process ==

The development of a new test consists of multiple steps:

1.	Selecting an appropriate System Under Test. Often the SUT consists of the 
    source file of the function to test. But exceptions from this rule of thumb 
    are possible. There are already tests with the given SUT, the new test can 
    be added to the existing executable. Step 2 can be skipped in this case.
    
2.	Creating the SUT test executable. If the SUT does not exist, a new test 
    executable needs to be created:

	- Create a new test executable directory, usually named after the SUT source 
	  file(s) and add a Makefile. The directory template contains template files
	  for the Makefile and the README file.
	  
	  > TARGETS=SOMETHING_test
	  > include ../Makefile.all
      > include ${CMOCKERY_DIR}/Makefile
      > include ${MOCK_DIR}/Makefile

      > MOCK_OBJS=
	
      > include ../Makefile.all2
	  
	- Change the TARGETS, and <testname>_REAL_OBJS variables in the
	  Makefile. TARGETS is usually straightforward to set.
	  
	  By default, the test program is linked with mock versions of most
	  backend files. The <testname>_REAL_OBJS needs to list any files
	  that should *not* be mocked, for which the real file should linked
	  in instead.
	  
	- Create a new test source file, also usually named after the SUT. An 
	  example is heapam_test.c in the heapam SUT directory. In the beginning 
	  the test source file consists only of the main function.
	  
	  Here is an example:
	  
      > #include <stdarg.h>
      > #include <stddef.h>
      > #include <setjmp.h>
      > #include "cmockery.h"
      > 
      > #include "postgres.h"
      > 
      > void test_foo_bar(void** state)
      > {
      > 	assert_int_equal(0, 1); /* Do not do this. Provide a real test */
      > }
      > 
      > int main(int argc, char* argv[])
      > {
      > 	cmockery_parse_arguments(argc, argv);
      > 
      > 	const UnitTest tests[] = {
      > 			unit_test(test_foo_bar)
      > 	};
      > 	return run_tests(tests);
      > }

	  
3.	Insert new test case functions and register them in the main function. A 
	test function usually consists of three steps:
    
	- Describe the interaction of the called functions from the SUT with the 
	  environment. This is only via expect_- and will_-functions from the 
	  cmockery library. Will_ functions are used to define which mocked 
	  functions are expected to be called by the test code. While the ordering 
	  will_-function calls for different functions names don't need to be 
	  strictly ordered, it is recommended to keep a strict ordering here. 
	  Will-function calls also define the return values, assignments to 
	  out-going parameters and side effects. This way we are able to define the 
	  exact state a tested function can observe.
	  
      In addition to the will_-functions, the expect_-functions are used to 
      describe our expectations about the values of parameters in the mocked 
      function calls. Details and a full list of the functions are showed later 
      in this document. 

    - Call a function or a series of functions from the System Under Test. This 
      function or functions are the unit testing with a specific test case.

    - The return values and the state after each test function call, can be 
      validated by assert_ calls. A list of all available assert_ calls is 
      presented later in this document.

== CMockery Usage ==

A cmockery test executable usually starts similar to this example from 
heapam_test.c:

int main(int argc, char* argv[]) {
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
    		unit_test(test_heap_xlog_newpage)
    };
    return run_tests(tests);
}

The test framework accepts command line arguments:

- --cmockery_abort_at_missing_check
  In certain situations, is helpful to call the test executable with 
  --cmockery_abort_at_missing_check. When this option is activated or if the 
  cmockery_abort_at_missing_check() function is called, cmockery aborts instead 
  of failing a test when a check is missing. 
  
  If used with a debugger attached or when generating core files, this helps to 
  identify where in the SUT code unexpected function calls have been made.
  However, usually checked in code should not contain this 
  abort_at_missing_check call since otherwise all following tests in the test 
  executable are not executed at all.
  
- --cmockery_generate_suppression
  In certain situations, it is helpful to call the test executable with 
  --cmockery_generate_suppression. When this option is activated or if the 
  cmockery_enable_generate_suppression() function is called, cmockery outputs
  example expect_ and will_ lines that are currently missing in the test 
  executable.
  
  While a test is pretty useless if it solely consists of generated expect_ 
  and will_ calls, it is helpful to setup the test environment. This is 
  especially true, if the test only deals with a certain line in a very long 
  function.
  
- --run_disabled_tests
  Certain test cases can be disabled with the disable_unit_test() function. 
  If cmockery is called with --run_disabled_tests, these tests are executed 
  even when they are disabled.

A test function like test_heap_xlog_newpage in the example has the 
signature "void test_heap_xlog_newpage(void** state)". The parameter state is 
used by cmockery internally and should not be used or modified by the test 
developer.

The following functions are provided by cmockery to describe the interaction of 
a SUT with its environment:

=== Assertions ===
- assert_true(c): Assert that the given expression is true. 
  If the expression is not true, the test fails.
  
- assert_false(c): Asserts that the given expression is false. 
  If the expression is true, the test fails.
  
- assert_int_equal(a, b): Asserts that two integral values are equal.

- assert_int_not_equal(a, b): Asserts that two integral values are not equal.

- assert_string_equal(a, b): Asserts that the contents of two strings are equal.

- assert_string_not_equal(a, b): Asserts that the contents of two strings are 
  different.

- assert_memory_equal(a, b, size): Asserts that two memory areas of a given 
  size have the same contents.
  
- assert_memory_not_equal(a, b, size): Asserts that two memory areas of a given 
  size have different contents.
  
- assert_in_range(value, min, max): Asserts that the value of an expression lies 
  in a given range.
  
- assert_not_in_range(value, min, max): Similar to assert_in_range, but negates.

- assert_in_set(value, values, number_of_values): Asserts that a value is in a 
  set of values. In contrast to except_in_set, values can be a pointer to an 
  array here. The parameter number_of_values determines the length of the array.
  
- assert_not_in_set(value, values, number_of_values): Similar to assert_in_set, 
  but negates.

=== Function call expectations ===

- will_return(function_name, value): Adds an expectation that a function with 
  the given name will be called once. If the function is called, the value 
  provided by will_return is returned to the SUT. 
  
- will_be_called(function_name): Adds an expectation that a function 
  with the given name will be called once. This function should be used for 
  functions that do not return values.
  
- will_return_with_sideeffect(function_name, value, sideeffect_function, 
      sideeffect_data): 
  This function is similar to will_return, but also executes a callback 
  function. The callback can be used to perform actions on global variables. 
  
- will_be_called_with_sideeffect(function_name, sideeffect_function, 
      sideeffect_data): 
  This function is similar to will_be_called, but also executes a callback 
  function. The callback can be used to perform actions on global variables.
  
=== Basic function parameter expectations ===

- expect_value(function_name, parameter, value): Adds an expectations that, 
  when a function is called, a parameter has a given value. If the function is 
  called, and the parameter has a different value the test fails.
  
- expect_value_count(function_name, parameter, value, count): Similar to 
  expect_value, but expect that the function is consecutively called count 
  number of times with the parameter having the given value. If count is -1, 
  the function establishes that for all later calls of the function, the 
  parameter has to have the given value. Similar functions having a _count 
  suffix exist for all expect_ functions. 
  We will omit them for the other functions. 
  
- expect_not_value(function_name, parameter, value): Adds an expectation that a 
  parameter has not a given value.
  
- expect_string(function_name, parameter, string): Adds an expectation that a 
  string parameter has a value equal to the given string. 

- expect_not_string(function_name, parameter, string): Adds an expectation that 
  a string parameter has a value not equal to the given string.
  
- expect_any(function_name, parameter): Declares that we have no expectation on 
  a given parameter during a specific function call. This expectation never 
  fails.
  
=== Advanced parameter expectations ===

- expect_memory(function_name, parameter, memory, size): Adds an expectation 
  that the memory a pointer parameter points is equal to the memory parameter 
  for the next size bytes. The data behind the memory parameter is copied and 
  can be safely modified or freed with out changing the expectation.
  
- expect_not_memory(function_name, parameter, memory, size): 
  Similar to expect_memory, but negated. 
  
- expect_check(function_name, parameter, check_function, check_data): 
  Adds an expectation that when a function is called, a parameter fulfills a 
  certain property.
   
  The check_function has the signature int(const LongestIntegralValue value, 
  const LongtestIntegralValue check_data). 
  If the function returns 1 the value fulfills the property. 0 indicates an 
  error. The value is the value of the parameter during the function call 
  casted to the type LongestIntegralValue. Similarly, check_data is the data 
  from the check_data parameter of the expect_check call.
  Usually this function is not used directly. 
  All other expect_ calls are implemented via expect_check. However, it can be 
  helpful for certain custom expectations. 
  
- expect_in_range(function, parameter, min, max): 
  Adds an expectation that a parameter value lies in a range.
  
- expect_not_in_range(function, parameter, min, max): 
  Similar to expect_in_range, but negated. The parameter value should not be in 
  the given range.
  
- expect_in_set(function, parameter, value_array): 
  Adds an expectation that a parameter value is in the set of values specified 
  by the value array. Note that the value array needs to be an actual array. 
  A pointer to values is not working.
  
- expect_not_in_set(function, parameter, value_array): 
  Similar to expect_in_set, but negates. The parameter value should not be in 
  the set.
  
- will_assign_value(function, parameter, value): 
  When the function is called, the given value will be assigned to the 
  dereferenced parameter. This function is used to assign 
  specific parameters to out-going parameters. 
  
- will_assign_string(function, parameter, string): 
  When the function is called, the given string will be assigned to the 
  out-going (aka non-const) string parameter.
  
- will_assign_memory(function, parameter, memory, size): 
  When the function is called, the memory block is copied to the memory 
  the out-going (aka non-const) pointer parameter points to.
  
=== Other cmockery features

- If disable_unit_test() is called after the start of a test case function, the
  execution of the test is skipped. While it is generally discouraged to skip a 
  test case because the test keeps failing, it is handy in certain situations.
  
== Basic Tips ==
- A unit test should be automated. It should not contain manual input. 
  If a test was successful or failed needs to be determined by assert_ and 
  expect_ calls and not be the "correct" output on screen.

== Modification on cmockery ==

The underlying unit testing/mocking library cmockery is open sourced under
the Apache 2.0 license. To better suit our requirements, we have made the 
following modifications

- Add will_be_called for void functions
- Add will_assign_/optional_assignment to support out-going parameters
- Adds the option to abort on an missing expectation
- Adds the option to generate expect_/will_ statements on missing expectations
- Adds a better and colored console output
- Add the parsing of command line parameters

== Further information ==

- cmockery Project Wiki:
  http://code.google.com/p/cmockery/wiki/Cmockery_Unit_Testing_Framework
  
- Wikipedia: 
  http://en.wikipedia.org/wiki/Unit_testing 
