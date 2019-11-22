cmake_minimum_required(VERSION 3.12)
project(gpfdist)

set(GPDB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../)

set (CPPFLAGS "/MP /wd4996 /wd4018 /wd4090 /wd4102 /wd4244 /wd4267 /wd4273 /wd4715")
add_definitions("${CPPFLAGS}")

set(HEADER_DIRS
    ${GPDB_SRC_DIR}/src/include
    ${GPDB_SRC_DIR}/src/include/port
    ${GPDB_SRC_DIR}/src/include/port/win32
    ${GPDB_SRC_DIR}/src/include/port/win32_msvc
    ${GPDB_SRC_DIR}/src/backend
    ${GPDB_SRC_DIR}/src/port
)

set(APR_LIB "${CMAKE_PREFIX_PATH}/lib/libapr-1.lib")
set(APR_DLL "${CMAKE_PREFIX_PATH}/bin/libapr-1.dll")
set(EVENT_LIB "${CMAKE_PREFIX_PATH}/lib/event.lib")

#set include dirs
include_directories("${CMAKE_PREFIX_PATH}/include")
include_directories(${HEADER_DIRS})
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

#set source files
add_executable(gpfdist gpfdist.c gpfdist_helper.c
    ${GPDB_SRC_DIR}/src/backend/utils/misc/fstream/gfile.c
    ${GPDB_SRC_DIR}/src/backend/utils/misc/fstream/fstream.c
    ${GPDB_SRC_DIR}/src/port/glob.c)

SET_SOURCE_FILES_PROPERTIES(gpfdist.c ${GPDB_SRC_DIR}/src/backend/utils/misc/fstream/gfile.c PROPERTIES COMPILE_DEFINITIONS "FRONTEND")

include(FindOpenSSL)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
else ()
    message(FATAL_ERROR "openssl not found")
endif(OPENSSL_FOUND)

find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "zlib not found")
endif(ZLIB_FOUND)

#set lib dirs
target_link_libraries(gpfdist ${ZLIB_LIBRARIES} ${APR_LIB} ${EVENT_LIB} ${OPENSSL_LIBRARIES} libpgport ws2_32 Crypt32)

set_target_properties(gpfdist PROPERTIES FOLDER bin)
install(TARGETS gpfdist DESTINATION bin)
install(FILES ${APR_DLL} DESTINATION bin)
