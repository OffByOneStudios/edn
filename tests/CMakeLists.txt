cmake_minimum_required(VERSION 3.20)

# Per-phase test build toggles (default ON for existing behavior)
option(EDN_BUILD_TESTS_CORE "Build core/unit tests" ON)
option(EDN_BUILD_TESTS_PHASE2 "Build Phase 2 tests" ON)
option(EDN_BUILD_TESTS_PHASE3 "Build Phase 3 tests" ON)
option(EDN_BUILD_TESTS_PHASE4 "Build Phase 4 tests (minimal main)" ON)
option(EDN_BUILD_TESTS_PHASE4_FULL "Build Phase 4 full tests (includes optional JIT smokes)" ON)
option(EDN_BUILD_TESTS_PHASE5 "Build Phase 5 tests" ON)
option(EDN_BUILD_GTESTS "Build GoogleTest-based regression tests" ON)

# If we are building any gtest-consuming targets, locate GTest once up front.
if(EDN_BUILD_GTESTS)
	find_package(GTest CONFIG REQUIRED)
endif()

# Core/unit tests runner (non-phase-prefixed files)
set(EDN_TESTS_CORE
	${CMAKE_CURRENT_SOURCE_DIR}/types_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/type_checker_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ir_emitter_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/cast_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/globals_test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_test.cpp
)
if(EDN_BUILD_TESTS_CORE)
add_executable(edn_tests_core ${EDN_TESTS_CORE} ${CMAKE_CURRENT_SOURCE_DIR}/core_main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/test_env.cpp)
# Link only against edn; LLVM libs are already transitively linked
target_link_libraries(edn_tests_core PRIVATE edn)
target_compile_features(edn_tests_core PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_core PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(NOT WIN32)
	# Ensure every TU sees the prototype for our local _putenv on non-Windows
	target_compile_options(edn_tests_core PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/test_env.hpp)
endif()
add_test(NAME edn.core COMMAND edn_tests_core)
endif()

# Phase 2 tests
file(GLOB PHASE2_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/phase2_*.cpp")
if(EDN_BUILD_TESTS_PHASE2)
add_executable(edn_tests_phase2 ${PHASE2_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase2_main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/test_env.cpp)
target_link_libraries(edn_tests_phase2 PRIVATE edn)
target_compile_features(edn_tests_phase2 PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase2 PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(NOT WIN32)
	target_compile_options(edn_tests_phase2 PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/test_env.hpp)
endif()
add_test(NAME edn.phase2 COMMAND edn_tests_phase2)
endif()

# Phase 3 tests
file(GLOB PHASE3_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/phase3_*.cpp")
if(EDN_BUILD_TESTS_PHASE3)
add_executable(edn_tests_phase3 ${PHASE3_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase3_main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/test_env.cpp)
target_link_libraries(edn_tests_phase3 PRIVATE edn)
target_compile_features(edn_tests_phase3 PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase3 PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(NOT WIN32)
	target_compile_options(edn_tests_phase3 PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/test_env.hpp)
endif()
add_test(NAME edn.phase3 COMMAND edn_tests_phase3)
endif()

# Phase 4 tests
file(GLOB PHASE4_ALL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/phase4_*.cpp")
# Minimal Phase 4: exclude both mains from the sources list and add the minimal main explicitly
set(PHASE4_SOURCES ${PHASE4_ALL_SOURCES})
list(FILTER PHASE4_SOURCES EXCLUDE REGEX ".*/phase4_main\\.cpp$")
list(FILTER PHASE4_SOURCES EXCLUDE REGEX ".*/phase4_full_main\\.cpp$")
if(EDN_BUILD_TESTS_PHASE4)
add_executable(edn_tests_phase4 ${PHASE4_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase4_main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/test_env.cpp)
target_link_libraries(edn_tests_phase4 PRIVATE edn)
target_compile_features(edn_tests_phase4 PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase4 PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(NOT WIN32)
	target_compile_options(edn_tests_phase4 PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/test_env.hpp)
endif()
add_test(NAME edn.phase4 COMMAND edn_tests_phase4)
endif()

# Phase 4 full tests (optional, includes JIT smokes when EDN_RUN_JIT=1)
# Use separate list and exclude the minimal main
set(PHASE4_FULL_SOURCES ${PHASE4_ALL_SOURCES})
list(FILTER PHASE4_FULL_SOURCES EXCLUDE REGEX ".*/phase4_main\\.cpp$")
if(EDN_BUILD_TESTS_PHASE4_FULL)
add_executable(edn_tests_phase4_full ${PHASE4_FULL_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase4_full_main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/test_env.cpp)
target_link_libraries(edn_tests_phase4_full PRIVATE edn)
target_compile_features(edn_tests_phase4_full PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase4_full PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(NOT WIN32)
	target_compile_options(edn_tests_phase4_full PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/test_env.hpp)
endif()
add_test(NAME edn.phase4.full COMMAND edn_tests_phase4_full)
endif()

# Phase 5 tests
file(GLOB PHASE5_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/phase5_*.cpp")
# Add explicitly maintained rustlite feature tests (not phase-prefixed) here.
list(APPEND PHASE5_SOURCES
)
if(NOT EDN_BUILD_GTESTS)
	# If gtests are disabled, exclude files that require GoogleTest headers/macros
	list(FILTER PHASE5_SOURCES EXCLUDE REGEX ".*/phase5_complex_lowering_gtest\\.cpp$")
	list(FILTER PHASE5_SOURCES EXCLUDE REGEX ".*/phase5_complex_lowering_regression\\.cpp$")
endif()
if(EDN_BUILD_TESTS_PHASE5)
add_executable(edn_tests_phase5 ${PHASE5_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/phase5_main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/test_env.cpp $<TARGET_OBJECTS:rustlite_parser>)
target_link_libraries(edn_tests_phase5 PRIVATE edn rustlite)
target_include_directories(edn_tests_phase5 PRIVATE ${CMAKE_SOURCE_DIR}/languages/rustlite/include)
if(EDN_BUILD_GTESTS)
	message(STATUS "Phase5: adding GTest::gtest to edn_tests_phase5 (sources=${PHASE5_SOURCES})")
	target_link_libraries(edn_tests_phase5 PRIVATE GTest::gtest)
else()
	message(STATUS "Phase5: EDN_BUILD_GTESTS=OFF, gtest sources filtered out: ${PHASE5_SOURCES}")
endif()
get_target_property(_PHASE5_LINK_LIBS edn_tests_phase5 LINK_LIBRARIES)
message(STATUS "Phase5: initial LINK_LIBRARIES=${_PHASE5_LINK_LIBS}")
target_compile_features(edn_tests_phase5 PRIVATE cxx_std_20)
target_compile_definitions(edn_tests_phase5 PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(NOT WIN32)
	target_compile_options(edn_tests_phase5 PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/test_env.hpp)
endif()
add_test(NAME edn.phase5 COMMAND edn_tests_phase5)
endif()

# GoogleTest-based regression tests (optional, focuses on higher-level JIT behaviors)

if(EDN_BUILD_GTESTS)
	add_executable(edn_gtests
		${CMAKE_CURRENT_SOURCE_DIR}/phase5_complex_lowering_gtest.cpp
	)
	target_link_libraries(edn_gtests PRIVATE GTest::gtest GTest::gtest_main edn)
	target_compile_features(edn_gtests PRIVATE cxx_std_20)
	target_compile_definitions(edn_gtests PRIVATE EDN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
	if(NOT WIN32)
		target_compile_options(edn_gtests PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/test_env.hpp)
	endif()
	add_test(NAME edn.gtests.phase5.complex_lowering COMMAND edn_gtests --gtest_filter=ComplexLoweringGTest.ReturnsSix)
	add_test(NAME edn.gtests.phase5.complex_lowering_fixedadd COMMAND edn_gtests --gtest_filter=ComplexLoweringGTest.ContainsSlotFixedAddOnce)
endif()
