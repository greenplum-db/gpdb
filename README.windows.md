# Building GPDB client tools on Windows

We only support building 64-bit client tools on Windows using Visual Studio 2017. Building backend is not supported.

If not specified, all commands in this readme should be executed in x64 Native Tools Command Prompt.

# Components of client tools

- Command line client: psql, pg_dump, pg_dumpall
- Script tools: createdb, createuser, createlang, dropdb, dropuser, droplang, clusterdb, vacuumdb
- Loaders: gpfdist, gpload

# Prerequisite

## Tools

Install following tools and add them to PATH, make sure you can invoke them from cmd.

1. Visual Studio build tools

[Download](https://visualstudio.microsoft.com/downloads/) and install "Visual C++ Build tools"

2. CMake

[Download](https://cmake.org/download/) and install latest Windows win64-x64 Installer.

3. Git

[Download](https://git-scm.com/download/win) and install.

4. Flex and Bison via MSYS2

[Download](https://www.msys2.org/) and install latest x86_64 package.

After installation, open MSYS2 command line, run ```pacman -S flex bison```. Add ```<msys2>\usr\bin``` to PATH.

5. Perl

[Download](https://www.activestate.com/activeperl/downloads) 64-bit and install.

6. Python 2.7

[Download](https://www.python.org/downloads/release/python-2715/) Windows x86-64 MSI installer and install.

## Required dependencies of gpfdist

We will install all dependencies to C:\dep. If you want another location,
make sure you've changed C:\dep in the following scripts.

1. zlib
```
git clone --branch v1.2.11 --depth 1 https://github.com/madler/zlib.git
cd zlib

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=C:\dep -G "Visual Studio 15 2017 Win64" ..
cmake --build . --config Release --target ALL_BUILD
cmake --build . --config Release --target INSTALL
copy C:\dep\lib\zlibstatic.lib C:\dep\lib\zdll.lib
```

2. libapr
```
git clone --branch 1.6.5 --depth 1 https://github.com/apache/apr.git
cd apr
mkdir build2
cd build2
cmake -DCMAKE_INSTALL_PREFIX:PATH=C:\dep -G "Visual Studio 15 2017 Win64" ..
cmake --build . --config Release --target ALL_BUILD
cmake --build . --config Release --target INSTALL
```

3. libevent
```
git clone --branch release-2.1.8-stable --depth 1 https://github.com/libevent/libevent.git
cd libevent
mkdir build
cd build
cmake -DEVENT__DISABLE_OPENSSL=ON -DCMAKE_INSTALL_PREFIX:PATH=C:\dep -G "Visual Studio 15 2017 Win64" ..
cmake --build . --config Release --target ALL_BUILD
cmake --build . --config Release --target INSTALL
```

## Optional dependencies

We will install all dependencies to C:\dep. If you want another location,
make sure you've changed C:\dep in the following scripts.

1. OpenSSL 1.0.2

```
git clone --branch OpenSSL_1_0_2r --depth 1 https://github.com/openssl/openssl
cd openssl
perl Configure --prefix=C:\dep VC-WIN64A 
ms\do_win64a
nmake -f ms\ntdll.mak
nmake -f ms\ntdll.mak install
```

2. Kerberos
```
git clone --branch krb5-1.17-final --depth 1 https://github.com/krb5/krb5.git
cd krb5
set NO_LEASH=1
set PATH=%PATH%;"%WindowsSdkVerBinPath%"\x86
set KRB_INSTALL_DIR=C:\dep
cd src
nmake -f Makefile.in prep-windows
nmake NODEBUG=1
nmake install NODEBUG=1
```

# Build steps

Replace <path\to\gpdb> with real location of your gpdb source code. Make sure you have
also cloned the submodule at gpMgmt\bin\pythonSrc\ext.

We will install client package to C:\greenplum-db-devel. If you want another location,
make sure you've replaced C:\greenplum-db-devel in the following scripts.


1. Create config.pl at src/tools/msvc. If you don't build with these supports, it's ok to skip this step.
```
cd <path\to\gpdb>\src\tools\msvc
echo print "our \$config = {gss => 'c:/dep', openssl => 'c:/dep', zlib => 'c:/dep'};" | perl >config.pl
```

2. Build postgres clients and scripts
```
cd <path\to\gpdb>\src\tools\msvc
build client
install C:\greenplum-db-devel client
```

3. Build gpfdist
```
cd <path\to\gpdb>\src\bin\gpfdist
cd build
cmake -DCMAKE_PREFIX_PATH:PATH=C:\ext -DCMAKE_INSTALL_PREFIX:PATH=C:\greenplum-db-devel -G "Visual Studio 15 2017 Win64" ..
cmake --build . --config Release --target ALL_BUILD
cmake --build . --config Release --target INSTALL
```

4. Build pygresql, needed by gpload
```
cd <path\to\gpdb>\gpMgmt\bin\pythonSrc\PyGreSQL
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=C:\greenplum-db-devel -DCMAKE_INSTALL_PREFIX:PATH=C:\greenplum-db-devel -G "Visual Studio 15 2017 Win64" ..
cmake --build . --config Release --target ALL_BUILD
cmake --build . --config Release --target INSTALL
```
