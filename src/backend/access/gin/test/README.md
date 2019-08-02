# `src/backend/access/gin` unit tests
The unit tests for `src/backend/access/gin` run differently from most other 
unit tests of backend modules.

## Most used behaviour
Usually, backend modules' unit tests use `src/backend/mock.mk` file to provide 
test executables with mocks of most backend objects (e.g. `palloc`).

However, the named objects are quite dependant on each other. In order to 
provide a unified build script for all unit test cases, `src/backend/mock.mk` 
includes almost (see details in the mentioned file) all backend objects.

This behaviour makes every unit test case quite dependant on the rest of the 
backend. On the other hand, it provides a convenient backend unit testing 
framework.


## Behaviour of `src/backend/access/gin` unit tests
The unit test mocks the functions of backend it uses (see 
`ginpostinglist_fakes.c`) and is not dependant on other parts of the backend. 
For this reason, it uses a custom test Makefile.

In order to expand the test suite, the Makefile may be expanded. To add a new 
test, one can use an existing target `ginpostinglist.t` as an example. Its 
definition is:
```
ginpostinglist.t: $(CMOCKERY_OBJS) $(top_srcdir)/$(subdir)/ginpostinglist.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) \
		$(CMOCKERY_OBJS) \
		$(top_srcdir)/$(subdir)/ginpostinglist.o \
		ginpostinglist_fakes.c \
		ginpostinglist_test.c \
		-o ginpostinglist.t
```
In this definition:
* `$(CMOCKERY_OBJS)` is a set of cmockery object files. The test target both 
depends on them, and links them to build an executable;
* `$(top_srcdir)/$(subdir)/ginpostinglist.o` is an object file whose functions 
are under test. The test target both depends on this file and links it to build 
an executable;
* `ginpostinglist_fakes.c` is a file with mocks specific for this particular 
unit tests;
* `ginpostinglist_test.c` is a test case file.

The `check` target should list all unit test executables both as dependencies 
and as files to be executed.
