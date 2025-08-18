# Micro-benchmarks for EDN IR generation and LLVM optimization presets

add_executable(edn_bench
    bench_main.cpp
)

target_link_libraries(edn_bench PRIVATE edn)

target_compile_features(edn_bench PRIVATE cxx_std_20)

add_test(NAME edn.bench.basic COMMAND edn_bench)
set_tests_properties(edn.bench.basic PROPERTIES LABELS "bench")
