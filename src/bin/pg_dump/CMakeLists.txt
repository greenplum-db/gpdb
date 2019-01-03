cmake_minimum_required(VERSION 3.12)
project(pg_dump)

add_definitions("/D FRONTEND /D ENABLE_THREAD_SAFETY")
add_definitions("${CPPFLAGS}")

set(SOURCE_FILES
    pg_backup_archiver.c
    pg_backup_db.c
    pg_backup_custom.c
	pg_backup_null.c
    pg_backup_tar.c
    pg_backup_directory.c
	pg_backup_utils.c
    parallel.c
    compress_io.c
)

set(HEADER_FILES
    compress_io.h
    dumputils.h
    parallel.h
    pg_backup.h
    pg_backup_archiver.h
    pg_backup_db.h
    pg_backup_tar.h
    pg_backup_utils.h
    pg_dump.h
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

set(PG_DUMP_FILES
    ${SOURCE_FILES}
    pg_dump.c
    common.c
    pg_dump_sort.c
)
add_executable(pg_dump ${HEADER_FILES} ${PG_DUMP_FILES})
target_link_libraries(pg_dump port libpq common ws2_32 secur32 ${ZLIB_LIBRARIES})

set(PG_DUMPALL_FILES
    ${KEYWORD_FILES}
    pg_dumpall.c
)
add_executable(pg_dumpall ${HEADER_FILES} ${PG_DUMPALL_FILES})
target_link_libraries(pg_dumpall port libpq common ws2_32 secur32)

set_target_properties(pg_dump pg_dumpall PROPERTIES FOLDER bin)
install(TARGETS pg_dump pg_dumpall DESTINATION bin)