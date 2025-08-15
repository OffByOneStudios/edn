file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(edn_tests ${TEST_SOURCES})

target_link_libraries(edn_tests PRIVATE edn)

target_compile_features(edn_tests PRIVATE cxx_std_20)

add_test(NAME edn.basic COMMAND edn_tests)
