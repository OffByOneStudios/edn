add_executable(edn_tests main.cpp)

target_link_libraries(edn_tests PRIVATE edn)

target_compile_features(edn_tests PRIVATE cxx_std_17)

add_test(NAME edn.basic COMMAND edn_tests)
