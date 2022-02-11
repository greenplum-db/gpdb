<pre>
======================================================================
                 __________  ____  ____  _________
                / ____/ __ \/ __ \/ __ \/ ____/   |
               / / __/ /_/ / / / / /_/ / /   / /| |
              / /_/ / ____/ /_/ / _, _/ /___/ ___ |
              \____/_/    \____/_/ |_|\____/_/  |_|
                  The Greenplum Query Optimizer
              Copyright (c) 2015, VMware, Inc. or its affiliates.
            Licensed under the Apache License, Version 2.0
======================================================================
</pre>

Welcome to GPORCA, the Greenplum Next Generation Query Optimizer!

To understand the objectives and architecture of GPORCA please refer to the following articles:
* [Orca: A Modular Query Optimizer Architecture for Big Data](https://content.pivotal.io/white-papers/orca-a-modular-query-optimizer-architecture-for-big-data).
* [Profiling Query Compilation Time with GPORCA](http://engineering.pivotal.io/post/orca-profiling/)
* [Improving Constraints In ORCA](http://engineering.pivotal.io/post/making-orca-smarter/)

Want to [Contribute](#contribute)?

GPORCA supports various build types: debug, release with debug info, release.
You'll need CMake 3.1 or higher to build the gporca_test and gpos_test
utilities to test GPORCA. Get it from cmake.org, or your operating system's
package manager.

# First Time Setup

## Build and install GPORCA

ORCA is built automatically with GPDB as long as `--disable-orca` is not used.
<a name="test"></a>

## Test GPORCA
To test GPORCA, first go into the `gporca` directory:

```
cmake -GNinja -H. -Bbuild
ninja -C build
```


To run all GPORCA tests, simply use the `ctest` command from the build directory
after build finishes.

```
cd build
ctest
```

Much like `make`, `ctest` has a -j option that allows running multiple tests in
parallel to save time. Using it is recommended for faster testing.

```
ctest -j8
```

By default, `ctest` does not print the output of failed tests. To print the
output of failed tests, use the `--output-on-failure` flag like so (this is
useful for debugging failed tests):

```
ctest -j8 --output-on-failure
```

To run only the previously failed ctests, use the `--rerun-failed` flag.
```
ctest -j8 --rerun-failed --output-on-failure
```

To run a specific individual test, use the `gporca_test` executable directly.

```
./server/gporca_test -U CAggTest
```

To run a specific minidump, for example for `../data/dxl/minidump/TVFRandom.mdp`:
```
./server/gporca_test -d ../data/dxl/minidump/TVFRandom.mdp
```

Note that some tests use assertions that are only enabled for DEBUG builds, so
DEBUG-mode tests tend to be more rigorous.

<a name="addtest"></a>
## Adding tests

Most of the regression tests come in the form of a "minidump" file.
A minidump is an XML file that contains all the input needed to plan a query,
including information about all tables, datatypes, and functions used, as well
as statistics. It also contains the resulting plan.

A new minidump can be created by running a query on a live GPDB server:

1. Run these in a psql session:

```
set client_min_messages='log';
set optimizer=on;
set optimizer_enumerate_plans=on;
set optimizer_minidump=always;
set optimizer_enable_constant_expression_evaluation=off;
```

2. Run the query in the same psql session. It will create a minidump file
   under the "minidumps" directory, in the master's data directory:

```
$ ls -l $MASTER_DATA_DIRECTORY/minidumps/
total 12
-rw------- 1 heikki heikki 10818 Jun 10 22:02 Minidump_20160610_220222_4_14.mdp
```

3. Run xmllint on the minidump to format it better, and copy it under the
   data/dxl/minidump directory:

```
xmllint --format $MASTER_DATA_DIRECTORY/minidumps/Minidump_20160610_220222_4_14.mdp > data/dxl/minidump/MyTest.mdp
```

4. Add it to the test suite, in server/src/unittest/gpopt/minidump/CICGTest.cpp

```
--- a/server/src/unittest/gpopt/minidump/CICGTest.cpp
+++ b/server/src/unittest/gpopt/minidump/CICGTest.cpp
@@ -217,6 +217,7 @@ const CHAR *rgszFileNames[] =
                "../data/dxl/minidump/EffectsOfJoinFilter.mdp",
                "../data/dxl/minidump/Join-IDF.mdp",
                "../data/dxl/minidump/CoerceToDomain.mdp",
+               "../data/dxl/minidump/Mytest.mdp",
                "../data/dxl/minidump/LeftOuter2InnerUnionAllAntiSemiJoin.mdp",
 #ifndef GPOS_DEBUG
                // TODO:  - Jul 14 2015; disabling it for debug build to reduce testing time
```

Alternatively, it could also be added to the proper test suite in `server/CMakeLists.txt` as follows:
```
--- a/server/CMakeLists.txt
+++ b/server/CMakeLists.txt
@@ -183,7 +183,8 @@ CPartTbl5Test:
 PartTbl-IsNullPredicate PartTbl-IsNotNullPredicate PartTbl-IndexOnDefPartOnly
 PartTbl-SubqueryOuterRef PartTbl-CSQ-PartKey PartTbl-CSQ-NonPartKey
 PartTbl-LeftOuterHashJoin-DPE-IsNull PartTbl-LeftOuterNLJoin-DPE-IsNull
-PartTbl-List-DPE-Varchar-Predicates PartTbl-List-DPE-Int-Predicates;
+PartTbl-List-DPE-Varchar-Predicates PartTbl-List-DPE-Int-Predicates
+Mytest;
```

<a name="updatetest"></a>
## Update tests

In some situations, a failing test does not necessarily imply that the fix is
wrong. Occasionally, existing tests need to be updated. There is now a script
that allows for users to quickly and easily update existing mdps. This script
takes in a logfile that it will use to update the mdps. This logfile can be
obtained from running ctest as shown below.

Existing minidumps can be updated by running the following:


1. Run `ctest -j8`.

2. If there are failing tests, run
```
ctest -j8 --rerun-failed --output-on-failure | tee /tmp/failures.out
```

3. The output file can then be used with the `fix_mdps.py` script.
```
gporca/scripts/fix_mdps.py --logFile /tmp/failures.out
```
Note: This will overwrite existing mdp files. This is best used after
committing existing changes, so you can more easily see the diff.
Alternatively, you can use `gporca/scripts/fix_mdps.py --dryRun` to not change
mdp files

4. Ensure that all changes are valid and as expected.

# Advanced Setup

## How to generate build files with different options

Here are a few build flavors (commands run from the ORCA checkout directory):

```
# debug build
cmake -GNinja -D CMAKE_BUILD_TYPE=Debug -H. -Bbuild.debug
```

```
# release build with debug info
cmake -GNinja -D CMAKE_BUILD_TYPE=RelWithDebInfo -H. -Bbuild.release
```

## Explicitly Specifying Xerces For Build

If you want to build with a custom version of Xerces, is recommended to use the
`--prefix` option to the Xerces-C configure script to install Xerces in a
location other than the default under `/usr/local/`, because you may have other
software that depends on the platform's version of Xerces-C. Installing in a
non-default prefix allows you to have GP-Xerces installed side-by-side with
unpatched Xerces without incompatibilities.

You can point cmake at your custom Xerces installation using the
`XERCES_INCLUDE_DIR` and `XERCES_LIBRARY` options like so:

However, to use the current build scripts in GPDB, Xerces will need to be
placed on the /usr path.

```
cmake -GNinja -D XERCES_INCLUDE_DIR=/opt/gp_xerces/include -D XERCES_LIBRARY=/opt/gp_xerces/lib/libxerces-c.so ..
```

Again, on Mac OS X, the library name will end with `.dylib` instead of `.so`.

## How to debug the build

Show all command lines while building (for debugging purpose)

```
ninja -v -C build
```

<a name="contribute"></a>
# How to Contribute

We accept contributions via [Github Pull requests](https://help.github.com/articles/using-pull-requests) only.


ORCA has a [style guide](StyleGuide.md), try to follow the existing style in your contribution to be consistent.

[clang-format]: https://clang.llvm.org/docs/ClangFormat.html
A set of [clang-format]-based rules are enforced in CI. Your editor or IDE may automatically support it. When in doubt, check formatting locally before submitting your PR:

```
CLANG_FORMAT=clang-format src/tools/fmt chk
```

For more information, head over to the [formatting README](README.format.md).

[clang-tidy]: https://clang.llvm.org/extra/clang-tidy/index.html

A set of [clang-tidy]-based checks are enforced in CI. Your editor or IDE may support displaying ClangTidy diagnostics. When in doubt, check formatting locally before submitting your patches:

```
CLANG_TIDY=clang-tidy-12 src/tools/tidy build.debug
```

See our [Clang-Tidy README](README.tidy.md) for details about how to invoke the tidy script.

We follow GPDB's comprehensive contribution policy. Please refer to it [here](https://github.com/greenplum-db/gpdb#contributing) for details.

