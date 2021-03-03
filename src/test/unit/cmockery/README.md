This is a modified, fork of cmockery orginating from https://github.com/google/cmockery . 

# Cmockery Unit Testing Framework

### Contents

 * [Overview](#Overview)
 * [Building](#Building)
 * [Motivation](#Motivation)
 * [Community](#Community)

Cmockery is a lightweight library that is used to author C unit tests.

## <a name="Overview"></a>Overview

Cmockery tests are compiled into stand-alone executables and linked with
the Cmockery library, the standard C library, and the module being tested.  Any
symbols external to the module being tested should be mocked - replaced with
functions that return values determined by the test - within the test
application.  Even though significant differences may exist between the target
execution environment of a code module and the environment used to test the
code, the unit testing is still valid since its goal is to test the logic of a
code modules at a functional level and not necessarily all of its interactions
with the target execution environment.

It may not be possible to compile a module into a test application without
some modification; therefore, the preprocessor symbol `UNIT_TESTING` should
be defined when Cmockery unit test applications are compiled so code within the
module can be conditionally compiled for tests.

More detailed information about the mechanics of writing tests with Cmockery can
be found in [`docs/user_guide.md`](docs/user_guide.md).

## Building

To compile the Cmockery library and example applications on Linux, run:

~~~
$ ./configure
$ make
~~~

To compile on Windows, run:

~~~
> vsvars.bat
> cd windows
> nmake
~~~

This code has been tested on Linux (Ubuntu) and Windows using VC++7 and VC++8.

## <a name="Motivation"></a>Motivation

There are a variety of C unit testing frameworks available; however, many of
them are fairly complex and require the latest compiler technology.  Some
development requires the use of old compilers which makes it difficult to
use some unit testing frameworks.  In addition, many unit testing frameworks
assume the code being tested is an application or module that is targeted to
the same platform that will ultimately execute the test.  Because of this
assumption, many frameworks require the inclusion of standard C library headers
in the code module being tested, which may collide with the custom or
incomplete implementation of the C library utilized by the code under test.

Cmockery only requires a test application is linked with the standard C
library which minimizes conflicts with standard C library headers.  Also,
Cmockery tries avoid the use of some of the newer features of C compilers.

This results in Cmockery being a relatively small library that can be used
to test a variety of exotic code.  If a developer wishes to simply test an
application with the latest compiler, then other unit testing frameworks may be
preferable.

## Community

If you have questions about Cmockery, use the following resources:

* Stack Overflow: use the
  [`cmockery`](https://stackoverflow.com/questions/tagged/cmockery) tag
* Mailing list: **cmockery (at) googlegroups.com**
  ([archives](https://groups.google.com/group/cmockery))

  To join with a Google account, use the
  [web UI](https://groups.google.com/forum/#!forum/cmockery/join); to
  subscribe/unsubscribe with an arbitrary email address, send an email to:

  * cmockery+subscribe (at) googlegroups.com
  * cmockery+unsubscribe (at) googlegroups.com

## License

Cmockery is licensed under the Apache 2.0 license; please see
[`LICENSE.txt`](LICENSE.txt) for details.
