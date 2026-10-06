include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(TEST_PROJECT_NAME ${PROJECT_NAME}_tests)

add_executable(${TEST_PROJECT_NAME}
    test-format.cpp
    test-sample-type.cpp
    test-color.cpp
)

target_link_libraries(${TEST_PROJECT_NAME} PRIVATE 
    ${target_static} 
    GTest::gtest_main
)
target_compile_features(${TEST_PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_definitions(${TEST_PROJECT_NAME} PRIVATE
    IMAGEIO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

include(GoogleTest)
gtest_discover_tests(${TEST_PROJECT_NAME})