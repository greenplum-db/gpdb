cmake_minimum_required(VERSION 3.12)

set(CATALOGDIR ${GPDB_SRC_DIR}/src/backend/catalog)

# utils/errcodes.h
execute_process(COMMAND ${PERL_EXECUTABLE} "generate-errcodes.pl" "errcodes.txt"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/utils
    OUTPUT_FILE "${GPDB_SRC_DIR}/src/include/utils/errcodes.h")