cmake_minimum_required(VERSION 3.12)
project(libpq)

add_definitions("/D FRONTEND /D ENABLE_THREAD_SAFETY ${CPPFLAGS}")

set(GPDB_BACKEND_SRC_DIR "${GPDB_SRC_DIR}/src/backend")

set(SOURCE_FILES
    fe-auth.c
    fe-connect.c
    fe-exec.c
    fe-lobj.c
    fe-misc.c
    fe-print.c
    fe-protocol2.c
    fe-protocol3.c
    fe-secure.c
    libpq-events.c
    pqexpbuffer.c
    pthread-win32.c
    win32.c
    ${GPDB_BACKEND_SRC_DIR}/libpq/md5.c
    ${GPDB_BACKEND_SRC_DIR}/libpq/ip.c
    ${GPDB_BACKEND_SRC_DIR}/utils/mb/wchar.c
    ${GPDB_BACKEND_SRC_DIR}/utils/mb/encnames.c
)

set(HEADER_FILES
    fe-auth.h
    libpq-events.h
    libpq-fe.h
    libpq-int.h
    pqexpbuffer.h
    #pthread-win32.h
    win32.h
    ${GPDB_SRC_DIR}/src/include/pg_config.h
)

set(HEADER_DIRS
    ${GPDB_SRC_DIR}/src/include
    ${GPDB_SRC_DIR}/src/include/libpq
    ${GPDB_SRC_DIR}/src/include/port/win32
    ${GPDB_SRC_DIR}/src/include/port/win32_msvc
    ${GPDB_SRC_DIR}/src/port
)

include_directories(${HEADER_DIRS})

add_library(libpq ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(libpq ${OPENSSL_LIBRARIES})