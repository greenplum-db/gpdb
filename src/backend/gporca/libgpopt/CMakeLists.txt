# Copyright (c) 2015, VMware, Inc. or its affiliates. All Rights Reserved.

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/libgpcost/include)
include_directories(${PROJECT_SOURCE_DIR}/libnaucrates/include)

# Generate version-number header.
# for the generated config.h file.
include_directories(${PROJECT_BINARY_DIR}/libgpos/include/)

file(GLOB_RECURSE hdrs ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Add headers to make them visible in some IDEs (Clion, VS, Xcode)
list(APPEND srcs ${hdrs})

add_library(gpopt ${srcs})

target_link_libraries(gpopt
                      gpdbcost
                      naucrates
                      ${CMAKE_DL_LIBS}
                      gpos
                      ${XERCES_LIBRARIES})
