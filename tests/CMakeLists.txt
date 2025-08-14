add_executable(edn_tests main.cpp types_test.cpp type_checker_test.cpp ir_emitter_test.cpp cast_test.cpp globals_test.cpp)

target_link_libraries(edn_tests PRIVATE edn)

target_compile_features(edn_tests PRIVATE cxx_std_20)

add_test(NAME edn.basic COMMAND edn_tests)
