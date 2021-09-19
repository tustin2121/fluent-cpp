find_package(Boost REQUIRED COMPONENTS json)
find_package(GTest)

add_definitions(-DTEST)
add_executable(run_tests EXCLUDE_FROM_ALL test.cpp ${FLUENT_SOURCES})
add_executable(test_main EXCLUDE_FROM_ALL main.cpp ${FLUENT_SOURCES})
target_link_libraries(run_tests foonathan::lexy ${ICU_LIBRARIES}
    GTest::gtest_main)
target_link_libraries(test_main foonathan::lexy ${ICU_LIBRARIES})

add_test(
    NAME run_tests
    COMMAND run_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(test
     DEPENDS run_tests
     COMMAND ${CMAKE_CURRENT_BINARY_DIR}/run_tests
     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
