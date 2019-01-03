cmake_minimum_required(VERSION 3.12)
project(gpfdist)
#copy gfile.c and fstream.c files to current directory
file(COPY "${GPDB_SRC_DIR}/src/backend/utils/misc/fstream/gfile.c"
  DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
file(COPY "${GPDB_SRC_DIR}/src/backend/utils/misc/fstream/fstream.c"
  DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_definitions("/D FRONTEND")
add_definitions("${CPPFLAGS}")
set(HEADER_DIRS
    ${GPDB_SRC_DIR}/src/include
    ${GPDB_SRC_DIR}/src/include/port
    ${GPDB_SRC_DIR}/src/include/port/win32
    ${GPDB_SRC_DIR}/src/include/port/win32_msvc
    ${GPDB_SRC_DIR}/src/backend
    ${GPDB_SRC_DIR}/src/port
)

set(APR_LIB "c:/ext/lib/libapr-1.lib")
set(EVENT_LIB "c:/ext/lib/event.lib")

#set include dirs
include_directories("c:/ext/include")
include_directories(${HEADER_DIRS})

#set source files
add_executable(gpfdist gpfdist.c gfile.c fstream.c gpfdist_helper.c)

#set lib dirs
target_link_libraries(gpfdist ${ZLIB_LIBRARIES} ${APR_LIB} ${EVENT_LIB} ${OPENSSL_LIBRARIES} port ws2_32 Crypt32)

set_target_properties(gpfdist PROPERTIES FOLDER bin)
install(TARGETS gpfdist DESTINATION bin)