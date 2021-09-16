find_package(Boost REQUIRED COMPONENTS json)
find_package(ICU COMPONENTS data uc REQUIRED)

add_definitions(-DTEST)
add_executable(test EXCLUDE_FROM_ALL test.cpp ${FLUENT_SOURCES})
target_link_libraries(test foonathan::lexy ${ICU_LIBRARIES})
