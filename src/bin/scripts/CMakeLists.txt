cmake_minimum_required(VERSION 3.12)
project(scripts)

add_definitions("/D FRONTEND /D ENABLE_THREAD_SAFETY")
add_definitions("${CPPFLAGS}")

set(HEADER_FILES
    common.h
    ${GPDB_SRC_DIR}/src/include/pg_config.h
)

set(HEADER_DIRS
    ${GPDB_SRC_DIR}/src/include
    ${GPDB_SRC_DIR}/src/bin/psql
    ${GPDB_SRC_DIR}/src/bin/pg_dump
    ${GPDB_SRC_DIR}/src/interfaces/libpq
    ${GPDB_SRC_DIR}/src/include/port/win32
    ${GPDB_SRC_DIR}/src/include/port/win32_msvc
)

include_directories(${HEADER_DIRS})

set(CREATEDB_FILES
    createdb.c
    common.c
)

add_executable(createdb ${HEADER_FILES} ${CREATEDB_FILES})
target_link_libraries(createdb port libpq common ws2_32 secur32)

set(CREATELANG_FILES
    createlang.c
    common.c
)

add_executable(createlang ${HEADER_FILES} ${CREATELANG_FILES})
target_link_libraries(createlang port libpq common ws2_32 secur32)

set(CREATEUSER_FILES
    createuser.c
    common.c
)

add_executable(createuser ${HEADER_FILES} ${CREATEUSER_FILES})
target_link_libraries(createuser port libpq common ws2_32 secur32)

set(DROPDB_FILES
    dropdb.c
    common.c
)

add_executable(dropdb ${HEADER_FILES} ${DROPDB_FILES})
target_link_libraries(dropdb port libpq common ws2_32 secur32)

set(DROPLANG_FILES
    droplang.c
    common.c
)

add_executable(droplang ${HEADER_FILES} ${DROPLANG_FILES})
target_link_libraries(droplang port libpq common ws2_32 secur32)

set(DROPUSER_FILES
    dropuser.c
    common.c
)

add_executable(dropuser ${HEADER_FILES} ${DROPUSER_FILES})
target_link_libraries(dropuser port libpq common ws2_32 secur32)

set(CLUSTERDB_FILES
    clusterdb.c
    common.c
)

set_target_properties(createdb createlang createuser dropdb droplang dropuser PROPERTIES FOLDER bin)
install(TARGETS createdb createlang createuser dropdb droplang dropuser DESTINATION bin)