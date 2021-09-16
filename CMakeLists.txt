cmake_minimum_required(VERSION 3.8)
project(fluent VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(ICU COMPONENTS data uc REQUIRED)

add_subdirectory(lexy)

include_directories(include)

set(FLUENT_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bundle.cpp
)
add_library(fluent ${FLUENT_SOURCES})
target_link_libraries(fluent foonathan::lexy ${ICU_LIBRARIES})

add_subdirectory(tests)
