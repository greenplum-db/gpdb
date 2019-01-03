cmake_minimum_required(VERSION 3.12)
if (NOT ${CMAKE_GENERATOR} STREQUAL "Visual Studio 15 2017 Win64")
    message(FATAL_ERROR "Only Visual Studio 15 2017 Win64 is supported")
endif()

set (CMAKE_CONFIGURATION_TYPES Release RelWithDebInfo)
project(gpdb C)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set (GPDB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "zlib not found")
endif(ZLIB_FOUND)

include(FindPerl)
if (NOT PERL_FOUND)
    message(FATAL_ERROR "Perl not found")
endif()

include(FindFLEX)
if (NOT FLEX_FOUND)
    message(FATAL_ERROR "Flex not found")
endif()

include(FindBISON)
if (NOT BISON_FOUND)
    message(FATAL_ERROR "bison not found")
endif()

find_package(Python2 COMPONENTS Interpreter Development)
if (Python2_FOUND)
    include_directories(${Python2_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "python2 not found")
endif(Python2_FOUND)

include(FindOpenSSL)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
else ()
    message(FATAL_ERROR "openssl not found")
endif(OPENSSL_FOUND)

set (CPPFLAGS "/MP /wd4996 /wd4018 /wd4090 /wd4102 /wd4244 /wd4267 /wd4273 /wd4715")
configure_file("${GPDB_SRC_DIR}/src/include/pg_config.h.win32" "${GPDB_SRC_DIR}/src/include/pg_config.h" COPYONLY)

# () specify a subgroup to capature, it matches the PG_MAJOR_VERSION from configure.in.
# Extract version number from subgroup, into $CMAKE_MATCH_1
# Append definition to final pg_config.h
set(VERSION_PATTERN "PG_PACKAGE_VERSION=([0-9\.]*)")
set(GPDB_VERSION_PATTERN "\\\[Greenplum Database\\\], \\\[([^\]]*)\\\]")
file(STRINGS "${GPDB_SRC_DIR}/configure.in" CONFIGURE_FILE)
string(REGEX MATCH "${VERSION_PATTERN}" PG_MAJORVERSION_STRING ${CONFIGURE_FILE})
set(PG_MAJORVERSION ${CMAKE_MATCH_1})
file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define PG_MAJORVERSION \"${PG_MAJORVERSION}\"\n" )
message("PG_MAJORVERSION: " "${PG_MAJORVERSION}")

string(REGEX MATCH "${GPDB_VERSION_PATTERN}" GP_VERSION_STRING ${CONFIGURE_FILE})
set(GP_VERSION ${CMAKE_MATCH_1})
file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define GP_VERSION \"${GP_VERSION}\"\n" )
message("GP_VERSION: " "${GP_VERSION}")

string(REGEX MATCH "[^\.]*" GP_MAJORVERSION ${GP_VERSION})
file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define GP_MAJORVERSION \"${GP_MAJORVERSION}\"\n" )
message("GP_MAJORVERSION: " "${GP_MAJORVERSION}")

file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define USE_SSL 1\n")
file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define HAVE_LIBZ 1\n")

if(NOT DEFINED BLOCKSIZE)
    set(BLOCKSIZE 32)
endif()
math(EXPR BLCKSZ "${BLOCKSIZE}*1024")
file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define BLCKSZ ${BLCKSZ}\n")

if(NOT DEFINED SEGSIZE)
    set(SEGSIZE 1)
endif()
math(EXPR RELSEG_SIZE "1024/32*${SEGSIZE}*1024")
file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define RELSEG_SIZE ${RELSEG_SIZE}\n")


if(NOT DEFINED WAL_BLOCKSIZE)
    set(WAL_BLOCKSIZE 32)
endif()
math(EXPR XLOG_BLCKSZ "${WAL_BLOCKSIZE}*1024")
file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define XLOG_BLCKSZ ${XLOG_BLCKSZ}\n")

if(NOT DEFINED WAL_SEGSIZE)
    set(WAL_SEGSIZE 64)
endif()
math(EXPR XLOG_SEG_SIZE "${WAL_SEGSIZE}*1024*1024")
file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define XLOG_SEG_SIZE ${XLOG_SEG_SIZE}\n")

if(NOT DEFINED FLOAT4BYVAL)
    set(FLOAT4BYVAL true)
endif()
if(FLOAT4BYVAL)
    file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define USE_FLOAT4_BYVAL 1\n")
    file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define FLOAT4PASSBYVAL true\n")
else()
    file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define FLOAT4PASSBYVAL false\n")
endif()

if(NOT DEFINED FLOAT8BYVAL)
    set(FLOAT8BYVAL true)
endif()
if(FLOAT8BYVAL)
    file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define USE_FLOAT8_BYVAL 1\n")
    file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define FLOAT8PASSBYVAL true\n")
else()
    file(APPEND "${GPDB_SRC_DIR}/src/include/pg_config.h" "\n#define FLOAT8PASSBYVAL false\n")
endif()

configure_file("${GPDB_SRC_DIR}/src/include/pg_config_ext.h.win32" "${GPDB_SRC_DIR}/src/include/pg_config_ext.h" COPYONLY)
configure_file("${GPDB_SRC_DIR}/src/include/port/win32.h" "${GPDB_SRC_DIR}/src/include/pg_config_os.h" COPYONLY)

# pg_config_paths.h is shared by both port and libpq
file (WRITE "${GPDB_SRC_DIR}/src/port/pg_config_paths.h"
    "#define PGBINDIR \"${CMAKE_INSTALL_PREFIX}/bin\""
    "\n#define PGSHAREDIR \"${CMAKE_INSTALL_PREFIX}/share/postgresql\""
    "\n#define SYSCONFDIR \"${CMAKE_INSTALL_PREFIX}/etc/postgresql\""
    "\n#define INCLUDEDIR \"${CMAKE_INSTALL_PREFIX}/include\""
    "\n#define PKGINCLUDEDIR \"${CMAKE_INSTALL_PREFIX}/include/postgresql\""
    "\n#define INCLUDEDIRSERVER \"${CMAKE_INSTALL_PREFIX}/include/postgresql/server\""
    "\n#define LIBDIR \"${CMAKE_INSTALL_PREFIX}/lib\""
    "\n#define PKGLIBDIR \"${CMAKE_INSTALL_PREFIX}/lib/postgresql\""
    "\n#define LOCALEDIR \"${CMAKE_INSTALL_PREFIX}/share/locale\""
    "\n#define DOCDIR \"${CMAKE_INSTALL_PREFIX}/doc/postgresql\""
    "\n#define HTMLDIR \"${CMAKE_INSTALL_PREFIX}/doc/postgresql\""
    "\n#define MANDIR \"${CMAKE_INSTALL_PREFIX}/share/man\""
)

add_subdirectory(src/common)
add_subdirectory(src/port)
add_subdirectory(src/interfaces/libpq)
add_subdirectory(src/bin)
add_subdirectory(src/backend)
add_subdirectory(gpMgmt/bin/pythonSrc/PyGreSQL-4.0/)