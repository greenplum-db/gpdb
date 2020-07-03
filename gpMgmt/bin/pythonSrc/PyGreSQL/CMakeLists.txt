cmake_minimum_required(VERSION 3.12)

set (CMAKE_CONFIGURATION_TYPES Release RelWithDebInfo)
project(pygresql C)

find_package(Python2 COMPONENTS Interpreter Development)
if (Python2_FOUND)
    include_directories(${Python2_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "python2 not found")
endif(Python2_FOUND)

file(GLOB SRC_TARBALL ${CMAKE_CURRENT_SOURCE_DIR}/../ext/PyGreSQL-*.tar.gz)
if (NOT SRC_TARBALL)
    message(FATAL_ERROR "PyGreSQL source tarball not found, run git submodule update --init --recursive")
endif()

execute_process(COMMAND tar -xf ${SRC_TARBALL} --strip-components=1
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

file(READ pgmodule.c PGMODULE_LINES)
file(WRITE pgmodule.c "__declspec(dllexport) void init_pg(void);\n")
file(APPEND pgmodule.c "${PGMODULE_LINES}")

add_definitions("/D FRONTEND")

set (CPPFLAGS "/MP /wd4996 /wd4018 /wd4090 /wd4102 /wd4244 /wd4267 /wd4273 /wd4715")
add_definitions("${CPPFLAGS}")

file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/__init__.py" "")
set(GPDB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../)
include_directories(${GPDB_SRC_DIR}/src/include
    ${GPDB_SRC_DIR}/src/interfaces/libpq
    ${GPDB_SRC_DIR}/src/include/port
    ${GPDB_SRC_DIR}/src/include/port/win32
    ${GPDB_SRC_DIR}/src/include/port/win32_msvc
    ${GPDB_SRC_DIR}/src/port
    ${Python2_INCLUDE_DIRS})
link_directories(${CMAKE_PREFIX_PATH}/lib)
find_library(LIBPQ NAMES libpq HINTS ${CMAKE_INSTALL_PREFIX}/LIB)
find_library(LIBPGPORT NAMES libpgport HINTS ${CMAKE_INSTALL_PREFIX}/LIB)
find_library(LIBPGCOMMON NAMES libpgcommon HINTS ${CMAKE_INSTALL_PREFIX}/LIB)

add_library (pygresql SHARED pgmodule.c)
target_link_libraries(pygresql ${LIBPQ} ${LIBPGPORT} ${LIBPGCOMMON} ws2_32 secur32 ${Python2_LIBRARIES})

set_target_properties(pygresql PROPERTIES OUTPUT_NAME "_pg")
set_target_properties(pygresql PROPERTIES SUFFIX ".pyd")
install(TARGETS pygresql DESTINATION lib/python)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pg.py DESTINATION lib/python)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pgdb.py DESTINATION lib/python)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION lib/python)