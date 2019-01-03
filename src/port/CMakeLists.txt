cmake_minimum_required(VERSION 3.12)
project(port)

add_definitions("/D MAX_PATH=260")
add_definitions("/D FRONTEND /D ENABLE_THREAD_SAFETY")
add_definitions("${CPPFLAGS}")

set(SOURCE_FILES
    chklocale.c
    crypt.c
    dirent.c
    dirmod.c
    erand48.c
    fls.c
    fseeko.c
    getaddrinfo.c
    getopt.c
    getopt_long.c
    getpeereid.c
    getrusage.c
    gettimeofday.c
    glob.c
    inet_aton.c
    inet_net_ntop.c
    isinf.c
    kill.c
    noblock.c
    open.c
    path.c
    pg_crc.c
    pg_crc32c_choose.c
    pg_crc32c_sb8.c
    pg_crc32c_sse42.c
    pgcheckdir.c
    pgmkdirp.c
    pgsleep.c
    pgstrcasecmp.c
    pqsignal.c
    qsort.c
    qsort_arg.c
    quotes.c
    random.c
    snprintf.c
    sprompt.c
    srandom.c
    strlcat.c
    strlcpy.c
    system.c
    tar.c
    thread.c
    win32env.c
    win32error.c
    win32setlocale.c
)

set(HEADER_FILES
    glob.h
    pg_config_paths.h
    ${GPDB_SRC_DIR}/src/include/pg_config.h
)

set(HEADER_DIRS
    ${GPDB_SRC_DIR}/src/include
    ${GPDB_SRC_DIR}/src/include/port
    ${GPDB_SRC_DIR}/src/include/port/win32
    ${GPDB_SRC_DIR}/src/include/port/win32_msvc
)

include_directories(${HEADER_DIRS})

add_library(port ${HEADER_FILES} ${SOURCE_FILES})