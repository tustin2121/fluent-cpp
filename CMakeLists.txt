cmake_minimum_required(VERSION 3.8)
project(fluent VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(lexy)

include_directories(include)
add_library(fluent src/parser.cpp src/ast.cpp src/bundle.cpp)
target_link_libraries(fluent foonathan::lexy)

add_executable(test EXCLUDE_FROM_ALL test.cpp)
target_link_libraries(test fluent)
