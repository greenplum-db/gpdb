# GPDB codegen utils (src/backend/codegen/utils)
Support library for integrating LLVM code-generation in GPDB.
This augments the core functionality provided by the LLVM C++ libraries for
program construction, optimization, and execution with additional features to
help runtime-generated code integrate smoothly with statically compiled code.
Key features include:

* A `CodegenUtils` class that encapsulates LLVM modules, optimization passes,
  and execution engines, managing the whole lifetime of generated code.
* Fully automatic mapping between C++ types and LLVM IR types (including
  function types of any arity) using templates.
* A type-safe and fully automatic foreign function interface that allows any
  statically compiled function or external function pointer to be called by
  generated code, and vice versa, to allow statically compiled code to get
  pointers to JIT-compiled functions that can be called.
* A family of wrapper function templates that allow instance methods of C++
  classes to be easily used with the foreign-function interface without the need
  to manually write bespoke wrapper code.
* A frontend based on Clang for compiling in-memory C/C++ source code snippets
  for code generation, including foreign-function support that has feature
  parity with the IR version.
* Optional generation of debugging information so that runtime-generated and JIT
  compiled C++ code can be interactively debugged by GDB just like statically
  compiled code.

# Contents

1. [Building Codegen Utils](#building-codegen-utils)
2. [Coding Guidelines](#coding-guidelines)
3. [Debugging Generated Code](#debugging-generated-code)
4. [Learning Resources](#learning-resources)
5. [Codegen GPDB Function](#codegen-gpdb-function)

## Building Codegen Utils

### Prerequisites
To build codegen utils, you will need cmake 2.8 or higher and a recent version of
llvm and clang (including headers and developer libraries) (codegen utils is
currently developed against the LLVM 3.7.X release series). Here's how to
acquire these dependencies for various OSes.

#### Gentoo Linux
Gentoo has the latest stable LLVM in portage. It's easy to install like any
other package:
```
sudo emerge --sync
sudo emerge -n cmake llvm clang
```

#### Debian or Ubuntu Linux

##### Latest OS Versions
Debian 9 "Stretch" and Ubuntu 15.10 "Wily" and above provide packages for the
necessary versions of cmake, LLVM, and dependencies that can be installed as
follows:
```
sudo apt-get update
sudo apt-get install -y build-essential zlib1g-dev libedit-dev cmake llvm-3.7 llvm-3.7-dev clang-3.7 libclang-3.7-dev
```

##### Older Releases
Older versions of Debian and Ubuntu do not package the latest LLVM version
required, but you may be able to install it from the
[APT packages provided by the LLVM project](http://llvm.org/apt/). This should
work with Debian 8 "Jessie" or Ubuntu 14.04 "Trusty" and later.

First, install some necessary prerequisite development pacakges.
```
sudo apt-get update
sudo apt-get install -y build-essential zlib1g-dev libedit-dev cmake wget
```

Next, edit `/etc/apt/sources.list` and add these two lines (example is for Debian
Jessie, [use the appropriate directories for your OS listed here](http://llvm.org/apt/)).
```
deb http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.7 main
deb-src http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.7 main
```

Now, add the GPG key for the LLVM repo to your trusted keys, rerun
`apt-get update`, and install the LLVM packages like so:
```
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-get update
sudo apt-get install -y llvm-3.7 llvm-3.7-dev clang-3.7 libclang-3.7-dev
```

#### FreeBSD
FreeBSD 10.2-STABLE comes with LLVM and Clang 3.4.1 installed by default, but
it also provides the more recent versions that we need in the package manager.
They can be installed like this:
```
sudo pkg update
sudo pkg install -y cmake llvm37 clang37
```

When building codegen, you will need to point cmake at the more recent LLVM
like so:

```
./configure --enable-codegen --with-codegen-prefix=/usr/local/llvm37
```

#### Mac OS X
Installing LLVM and clang libraries is easiest if you use an add-on package
manager like [Homebrew](http://brew.sh/). Do not worry about conflicts with
Apple's development tools and distribution of LLVM, Homebrew will install the
side-by-side and NOT override the defaults.

If you do not already have homebrew, you can install it with this shell snippet:
```
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

As of this writing, the "stable" version of LLVM in Homebrew is 3.6. To install
the newer LLVM 3.7 libraries, do the following:
```
brew tap homebrew/versions
brew install --with-clang llvm37
```

Unfortunately, the cmake configuration module for LLVM that is installed by
brew is broken. Fortunately, we can fix it. Edit
`/usr/local/opt/llvm37/lib/llvm-3.7/share/llvm/cmake/LLVMConfig.cmake` and fix
the path detection logic at the front of the file (before
`set(LLVM_VERSION_MAJOR 3)`) to look like this:
```
# Compute the CMake directory from the LLVMConfig.cmake file location.
get_filename_component(_LLVM_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

# Compute the installation prefix from the LLVMConfig.cmake file location.
get_filename_component(LLVM_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(LLVM_INSTALL_PREFIX "${LLVM_INSTALL_PREFIX}" PATH)
get_filename_component(LLVM_INSTALL_PREFIX "${LLVM_INSTALL_PREFIX}" PATH)
get_filename_component(LLVM_INSTALL_PREFIX "${LLVM_INSTALL_PREFIX}" PATH)
set(_LLVM_LIBRARY_DIR "${LLVM_INSTALL_PREFIX}/lib")
```

Because the version of LLVM installed by brew is not installed in one of the
default locations, you will need to point cmake at it when building codegen
like so:
```
./configure --enable-codegen --with-codegen-prefix=/usr/local/opt/llvm37/lib/llvm-3.7
```

### Testing
You can run unit tests as follows
```
make -C src/backend/codegen unittest-check
```

## Coding Guidelines

This module is written using the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
We have a few additional style rules that aren't covered by the guide:
* Publicly visible classes (and their public methods) should have doxygen
  comments explaining them. Private methods/members and internal helper classes
  don't need full doxygen comments, but some ordinary comments to clarify and
  improve readability are often a good idea.
* Sigils indicating a pointer (`*`) or reference (`&` or `&&` for
  rvalue-reference) type should be written next to the name of the type, not the
  name of a variable or function (e.g. write `int* my_pointer;` not
  `int *my_pointer;`.


## Debugging Generated Code

### Viewing Generated Code

LLVM Modules have a `dump()` method that prints out the complete IR source for
a module to stderr. Similarly, when generating C++ source, an LLVM Twine's
complete text can be printed to stderr by the `dump()` method. Finally, if
errors are encountered during processing of C++ source by
`ClangCompiler::CompileCppSource()` clang error messages will be logged to
stderr.

### Validating Generated Code

LLVM provides some functions in the header `llvm/IR/Verifier.h` that can be used
to verify that modules (`verifyModule()`) and individual functions
(`verifyFunction()`) are well-formed before they are compiled. When compiling
C++ source code, the return value of `ClangCompiler::CompileCppSource()`
indicates whether compilation was successful.

### Interactive Debugging with GDB

GDB can be used to debug generated C++ source code just like statically
compiled C++ source. Calling `ClangCompiler::CompileCppSource()` with the option
`debug = true` causes DWARF debugging information to be included with the
compiled code and dumps the source code to a temporary file, enabling the usual
GDB features. Some caveats apply:

1. Debugging for JITed code only works on platforms that use the
   [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) binary
   format. This includes Linux and all the major BSD flavors, but does NOT
   include Mac OS X (which uses Mach-O).
2. LLVM's MCJIT execution engine uses special hooks to expose symbols to the
   debugger. These hooks are only supported by GDB 7.0+ (and appear to be only
   partially supported by LLDB, so debugging with GDB is recommended).

When running an executable under GDB, you can set a breakpoint on a generated
function that hasn't been compiled yet. GDB will alert you that the function is
not defined and ask if you want to make the breakpoint pending on a future
shared library load (say yes).

Currently full debugging symbols are only generated when compiling from C++
source. Work is in progress to expose debugging information for generated IR.

## Learning Resources
We have collected some documentation and resources to learn more about using
LLVM generally and about some specific programming techniques that are used in
codegen. When we refer to "LLVM IR" below, that means LLVM intermediate
representation, which is the internal language used to build up programs in
LLVM. IR is assembly-like, but it is strongly-typed, uses static single
assignment, and has the illusion of unlimited registers.

### LLVM Resources
* [LLVM Tutorial](http://llvm.org/docs/tutorial/) A guided excercise in
  creating an interpreter for a "toy" language using LLVM. Covers the basics
  of creating values, expressions, control flow, and functions in LLVM IR using
  the C++ IRBuilder interface.
* [LLVM Language Reference Manual](http://llvm.org/docs/LangRef.html) A
  detailed reference about the LLVM IR language. Especially useful are the
  sections labelled "Type System", "Instruction Reference", and "Intrinsic
  Functions".
* [LLVM Programmer's Manual](http://llvm.org/docs/ProgrammersManual.html) A
  guide to working with the LLVM C++ code base. This describes some of the
  general-purpose APIs and data structures that are used internally by LLVM,
  and also provides a useful guide to the most important parts of the C++ API
  for LLVM IR objects in the section labelled "The Core LLVM Class Hierarchy
  Reference".
* [LLVM Doxygen](http://llvm.org/doxygen/) A more or less complete code
  reference for LLVM. Contains automatically-generated documentation for almost
  every publicly visible class and method in the LLVM source.

### C++ Resources
* [Template Specialization](http://www.cprogramming.com/tutorial/template_specialization.html)
  A brief introduction to template specialization and partial specialization,
  which can be used to make a special version of a class template for a
  specific type or category of types.
* [SFINAE](https://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error)
  A wikipedia article on the SFINAE (substitution failure is not an error)
  idiom in C++. SFINAE helps resolve multiple different overloads or partial
  specializations that might apply when instantiating a template down to just
  one by eliminating versions of a template that can't possibly "fit".
* [C++ type_traits library](http://en.cppreference.com/w/cpp/header/type_traits)
  A reference guide to the `type_traits` header introduced to the standard
  library in C++11. `type_traits` provides many useful templates to help with
  template metaprogramming. An especially important one is
  [std::enable_if](http://en.cppreference.com/w/cpp/types/enable_if), which
  is used to selectively enable a partial template specialization depending on
  a compile-time boolean constant.
* [C++ Variadic Templates Guide](http://eli.thegreenplace.net/2014/variadic-templates-in-c/)
  A nice guided tour of variadic templates in C++11 and how to use them to make
  variadic code that is both type-safe and fast.
  
## Codegen GPDB Function
In order to facilitate codegen GPDB function, we introduced the following classes:
* `CodegenInterface` - Interface for all code generators.
* `BaseCodegen`	     - Inherits CodegenInterface and provides common implementation for all
					   generators that derive from it.
* `CodegenManager`   - Manages all CodegenInterface.

More details for above classes are available in doxygen document or in respective source code.

Below are the necessary steps to codegen a target function *F* (e.g. `slot_deform_tuple`) which is
access through an operator.

1. Create a CodegenManager instance in the corresponding operator.
2. Create a new generator class *GC* (E.g. `SlotDeformTupleCodegen`) that derives from BaseCodegen and 
   implements a function that generartes IR instructions / C++ code for *F*. 
3. Store *GC* and a function pointer to *F* in an appropriate struct. For instance the proper struct
   for `slot_deform_tuple` is `TupleTableSlot`.
4. Enroll *GC* with the manager for current operator during `ExecInit`. Enrollment
   process makes sure that the function pointer initally points to the regular version of *F*.
5. Replace the actual function call in GPDB with a call to the above function pointer.

After `ExecInit`, manager uses `CodegenInterface` to generate the runtime code. On sucessful generation,
manager swaps the function pointer (see Step 3) to point to the generated version of *F*. Note that, *GC*
stores the target function *F* along with a reference to a function pointer to *F* (see Step 3). This
mechanism allows manager to fallback on the regular version of *F* when code generation fails.  