cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0074 NEW)
project(fluent VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(ICU COMPONENTS uc i18n REQUIRED)

include(FetchContent)
FetchContent_Declare(
    lexy
    URL https://github.com/foonathan/lexy/archive/e9ca531.zip
    URL_HASH MD5=89827b53a39b41fd2336166dfa8d8f29
    SOURCE_DIR lexy
)
FetchContent_MakeAvailable(lexy)

set(FLUENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bundle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/loader.cpp
)
add_library(fluent ${FLUENT_SOURCES})
target_include_directories(fluent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(fluent PUBLIC ${ICU_INCLUDE_DIRS})
target_link_libraries(fluent PRIVATE ${ICU_LIBRARIES} foonathan::lexy)

add_executable(ftlembed ${CMAKE_CURRENT_SOURCE_DIR}/src/embed.cpp)

function(embed_ftl target directory )
    set(options PUBLIC PRIVATE INTERFACE)
    cmake_parse_arguments(EMBED_FTL "${options}" "" "" "" ${ARGN})
    file(GLOB files "${directory}/*/*.ftl")
    get_filename_component(dirname ${directory} NAME)
    foreach(file ${files})
        file(RELATIVE_PATH relative_file ${directory} ${file})
        string(REPLACE ".ftl" ".cpp" file_output
            ${CMAKE_CURRENT_BINARY_DIR}/${dirname}/${relative_file})
        if (EMBED_FTL_INTERFACE)
            target_sources(${target} INTERFACE ${file_output})
        elseif (EMBED_FTL_PUBLIC)
            target_sources(${target} PUBLIC ${file_output})
        else()
            target_sources(${target} PRIVATE ${file_output})
        endif()
        add_custom_command(
             OUTPUT ${file_output}
             DEPENDS ftlembed ${file}
             COMMAND ftlembed ${file} ${file_output}
        )
    endforeach()
endfunction()

add_subdirectory(tests)
