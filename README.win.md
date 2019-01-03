We only support compilation of client tools on Windows. 

## 1. Install toolchain and dependencies

- CMake: https://cmake.org/download/
- Visual Studio 2017 Build Tools: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2017
- git: https://git-scm.com/download/win
- flex, bison via MSYS2: https://www.msys2.org/  
After install msys64, open MSYS2 command line and run  
```pacman -S flex bison```
- perl: https://www.activestate.com/activeperl/downloads
- python2: https://www.python.org/downloads/release/python-2715/
- OpenSSL: https://slproweb.com/products/Win32OpenSSL.html

## 2. Compile external dependecies
Assume you want to download source to `C:\ext-src` and install to `C:\ext`
- zlib: https://zlib.net/zlib-1.2.11.tar.gz  
Extract to "C:\ext-src"  
Open "Developer Command Prompt for VS 2017" and execute
```
cd "C:\ext-src\zlib-1.2.11"  
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=C:\ext -G "Visual Studio 15 2017 Win64" ..
msbuild ALL_BUILD.vcxproj /property:Configuration=Release
msbuild INSTALL.vcxproj /property:Configuration=Release
```
- apr: https://apr.apache.org/download.cgi
Extract to "C:\ext-src"  
Open "Developer Command Prompt for VS 2017" and execute
```
cd  C:\ext-src\apr-1.6.5-win32-src\apr-1.6.5
mkdir build2
cd build2
cmake -DCMAKE_INSTALL_PREFIX:PATH=C:\ext -G "Visual Studio 15 2017 Win64" ..
msbuild ALL_BUILD.vcxproj /property:Configuration=Release
msbuild INSTALL.vcxproj /property:Configuration=Release
```
- libevent:
Open "Developer Command Prompt for VS 2017" and execute
```
cd C:\ext-src
git clone https://github.com/libevent/libevent.git
cd libevent
git checkout release-2.1.8-stable
mkdir build
cd build
cmake -DEVENT__DISABLE_OPENSSL=ON -DCMAKE_INSTALL_PREFIX:PATH=C:\ext -G "Visual Studio 15 2017 Win64" ..
msbuild ALL_BUILD.vcxproj /property:Configuration=Release
msbuild INSTALL.vcxproj /property:Configuration=Release
```

## 3. Compile GPDB client tools
Suppose gpdb source code is at `C:\gpdb` and you want to install to `C:\greenplum-db-devel`  
Open "Developer Command Prompt for VS 2017" and execute
```
cd C:\gpdb
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH:PATH=C:\ext -DCMAKE_INSTALL_PREFIX:PATH=C:\greenplum-db-devel -G "Visual Studio 15 2017 Win64" ..
msbuild ALL_BUILD.vcxproj /property:Configuration=Release
msbuild INSTALL.vcxproj /property:Configuration=Release
```
There's a Visual Studio Solution generated at `C:\gpdb\build\gpdb.sln`  
You can also use Visual Studio to build and debug client tools.