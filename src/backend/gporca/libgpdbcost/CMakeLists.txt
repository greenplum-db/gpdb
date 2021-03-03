# Copyright (c) 2015, VMware, Inc. or its affiliates. All Rights Reserved.

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/libgpopt/include)
include_directories(${PROJECT_SOURCE_DIR}/libnaucrates/include)
# for the generated config.h file.
include_directories(${PROJECT_BINARY_DIR}/libgpos/include/)

file(GLOB_RECURSE hdrs ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Add headers to make them visible in some IDEs (Clion, VS, Xcode)
list(APPEND srcs ${hdrs})

add_library(gpdbcost ${srcs})

target_link_libraries(gpdbcost  
                      ${CMAKE_DL_LIBS}
                      gpos
                      ${XERCES_LIBRARIES})

# Prevent linker errors from undefined symbols on OSX.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set_target_properties(gpdbcost PROPERTIES
                        LINK_FLAGS "-undefined dynamic_lookup")
endif()
