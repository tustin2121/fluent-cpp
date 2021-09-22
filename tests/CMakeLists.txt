find_package(Boost)
find_package(GTest)

if (Boost_FOUND)
    add_definitions(-DTEST)
    if (GTest_FOUND)
        add_executable(run_tests EXCLUDE_FROM_ALL parser.cpp static.cpp ${FLUENT_SOURCES})
        target_include_directories(run_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
        target_link_libraries(run_tests foonathan::lexy ${ICU_LIBRARIES}
            GTest::gtest_main Boost::boost)

        embed_ftl(${CMAKE_CURRENT_SOURCE_DIR}/l10n)
        target_sources(run_tests PRIVATE ${FTL_EMBED_OUTPUT})
    endif()

    add_executable(test_main EXCLUDE_FROM_ALL main.cpp ${FLUENT_SOURCES})
    target_include_directories(test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    target_link_libraries(test_main foonathan::lexy ${ICU_LIBRARIES} Boost::boost)

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
endif()
