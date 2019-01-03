cmake_minimum_required(VERSION 3.12)
project(pygresql)
add_definitions("/D FRONTEND")
add_definitions("${CPPFLAGS}")
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/__init__.py" "")
set(HEADER_DIRS
    ${GPDB_SRC_DIR}/src/include/
    ${GPDB_SRC_DIR}/src/interfaces/libpq
    ${GPDB_SRC_DIR}/src/include/port
    ${GPDB_SRC_DIR}/src/include/port/win32
    ${GPDB_SRC_DIR}/src/include/port/win32_msvc
    ${GPDB_SRC_DIR}/src/port
    ${Python2_INCLUDE_DIRS})
include_directories(${HEADER_DIRS})
add_library (pygresql SHARED pgmodule.c)
target_link_libraries(pygresql libpq port common ws2_32 secur32 ${Python2_LIBRARIES})
set_target_properties(pygresql PROPERTIES OUTPUT_NAME "_pg")
set_target_properties(pygresql PROPERTIES SUFFIX ".pyd")
install(TARGETS pygresql DESTINATION lib/python/pygresql)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pg.py DESTINATION lib/python/pygresql)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pgdb.py DESTINATION lib/python/pygresql)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION lib/python/pygresql)

