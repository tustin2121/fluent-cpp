cmake_minimum_required(VERSION 3.11)
project(fluent VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(ICU COMPONENTS data uc REQUIRED)

include(FetchContent)
FetchContent_Declare(
    lexy
    URL https://github.com/foonathan/lexy/archive/edc6bd4.zip
    URL_HASH MD5=0d18332b379abea925fb61d73b6347b0
    SOURCE_DIR lexy
)
FetchContent_MakeAvailable(lexy)

include_directories(include)

set(FLUENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bundle.cpp
)
add_library(fluent ${FLUENT_SOURCES})
target_link_libraries(fluent foonathan::lexy ${ICU_LIBRARIES})

add_subdirectory(tests)
