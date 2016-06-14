#   Copyright 2015-2016 Pivotal Software, Inc.
#
#   CMakeLists.txt
#       Cmake configuration for building GPDB codegen examples.
#


cmake_minimum_required(VERSION 2.8.12)

# Example 1: Materialize Tuple
add_executable(materialize_tuple_ex materialize_tuple_ex.cc)
target_link_libraries(materialize_tuple_ex gpcodegen)
