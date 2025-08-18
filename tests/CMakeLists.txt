cmake_minimum_required(VERSION 3.20)

# Core/unit tests runner (non-phase-prefixed files)
set(EDN_TESTS_CORE
	${CMAKE_CURRENT_SOURCE_DIR}/types_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/type_checker_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ir_emitter_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/cast_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/globals_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_test.cpp
)
add_executable(edn_tests_core ${EDN_TESTS_CORE} ${CMAKE_CURRENT_SOURCE_DIR}/core_main.cpp)
target_link_libraries(edn_tests_core PRIVATE edn)
target_compile_features(edn_tests_core PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_core PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME edn.core COMMAND edn_tests_core)

# Phase 2 tests
file(GLOB PHASE2_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/phase2_*.cpp")
add_executable(edn_tests_phase2 ${PHASE2_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase2_main.cpp)
target_link_libraries(edn_tests_phase2 PRIVATE edn)
target_compile_features(edn_tests_phase2 PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase2 PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME edn.phase2 COMMAND edn_tests_phase2)

# Phase 3 tests
file(GLOB PHASE3_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/phase3_*.cpp")
add_executable(edn_tests_phase3 ${PHASE3_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase3_main.cpp)
target_link_libraries(edn_tests_phase3 PRIVATE edn)
target_compile_features(edn_tests_phase3 PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase3 PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME edn.phase3 COMMAND edn_tests_phase3)

# Phase 4 tests
file(GLOB PHASE4_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/phase4_*.cpp")
add_executable(edn_tests_phase4 ${PHASE4_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase4_main.cpp)
target_link_libraries(edn_tests_phase4 PRIVATE edn)
target_compile_features(edn_tests_phase4 PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase4 PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME edn.phase4 COMMAND edn_tests_phase4)

# Phase 4 full tests (optional, includes JIT smokes when EDN_RUN_JIT=1)
# Reuse Phase 4 sources but exclude the minimal phase4_main.cpp to avoid duplicate main
list(FILTER PHASE4_SOURCES EXCLUDE REGEX ".*/phase4_main\\.cpp$")
add_executable(edn_tests_phase4_full ${PHASE4_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase4_full_main.cpp)
target_link_libraries(edn_tests_phase4_full PRIVATE edn)
target_compile_features(edn_tests_phase4_full PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase4_full PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME edn.phase4.full COMMAND edn_tests_phase4_full)

# Phase 5 tests
file(GLOB PHASE5_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/phase5_*.cpp")
add_executable(edn_tests_phase5 ${PHASE5_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase5_main.cpp)
target_link_libraries(edn_tests_phase5 PRIVATE edn)
target_compile_features(edn_tests_phase5 PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase5 PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME edn.phase5 COMMAND edn_tests_phase5)
