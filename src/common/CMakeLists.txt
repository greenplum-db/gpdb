cmake_minimum_required(VERSION 3.12)
project(common)

add_definitions("/D MAX_PATH=260")
add_definitions("/D FRONTEND /D ENABLE_THREAD_SAFETY /wd4996 /wd4018 /wd4090 /wd4244 /wd4267")

set(SOURCE_FILES
   exec.c
   fe_memutils.c
   psprintf.c
   relpath.c
   username.c
   wait_error.c
   ${GPDB_SRC_DIR}/src/bin/psql/print.c
   ${GPDB_SRC_DIR}/src/bin/psql/mbprint.c
   ${GPDB_SRC_DIR}/src/bin/pg_dump/keywords.c
   ${GPDB_SRC_DIR}/src/bin/pg_dump/dumputils.c
   ${GPDB_SRC_DIR}/src/backend/parser/kwlookup.c
)

set(HEADER_FILES
    ${GPDB_SRC_DIR}/src/include/common/fe_memutils.h
    ${GPDB_SRC_DIR}/src/include/common/relpath.h
)

set(HEADER_DIRS
    ${GPDB_SRC_DIR}/src/include
    ${GPDB_SRC_DIR}/src/interfaces/libpq
    ${GPDB_SRC_DIR}/src/bin/psql
    ${GPDB_SRC_DIR}/src/bin/pg_dump
    ${GPDB_SRC_DIR}/src/include/port
    ${GPDB_SRC_DIR}/src/include/port/win32
    ${GPDB_SRC_DIR}/src/include/port/win32_msvc
)

include_directories(${HEADER_DIRS})

add_library(common ${HEADER_FILES} ${SOURCE_FILES})