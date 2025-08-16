file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(edn_tests ${TEST_SOURCES})

# Ensure new files picked up even if glob cache lags
target_sources(edn_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/phase4_closures_jit_smoke_test.cpp)

target_link_libraries(edn_tests PRIVATE edn)

target_compile_features(edn_tests PRIVATE cxx_std_20)

# Provide project source dir to tests so they can open example .edn files reliably
target_compile_definitions(edn_tests PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME edn.basic COMMAND edn_tests)

# The test entry functions are declared in individual compilation units and called from main.cpp.
# Add the new closures record test to the build by ensuring the source is compiled.
# (Nothing else required here as tests are self-registered via run_* functions.)
