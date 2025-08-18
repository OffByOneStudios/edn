cmake_minimum_required(VERSION 3.20)
project(edn LANGUAGES CXX VERSION 0.0.1)

# Options
option(EDN_BUILD_TESTS "Build tests" ON)

# MSVC: ensure exception handling (unwind semantics) is enabled to avoid C4530 warnings
if(MSVC)
    # /EHsc = enable standard C++ exception handling; /Zc:preprocessor for modern preprocessing (optional)
    add_compile_options(/EHsc)
endif()

## Add vcpkg dependencies
find_package(LLVM CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_library(edn STATIC
    src/edn.cpp
    src/type_check.cpp
    src/diagnostics_json.cpp
)
add_library(edn::edn ALIAS edn)

include(GNUInstallDirs)

target_compile_features(edn PUBLIC cxx_std_20)

target_include_directories(edn
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<IF:$<BOOL:LLVM_FOUND>,${LLVM_INCLUDE_DIRS},> )

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
set(LLVM_COMPONENTS Support Core IRReader OrcJit)
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    list(APPEND LLVM_COMPONENTS AArch64)
elseif(APPLE)
    list(APPEND LLVM_COMPONENTS X86)
elseif(WIN32)
    list(APPEND LLVM_COMPONENTS X86)
elseif(UNIX)
    list(APPEND LLVM_COMPONENTS X86)
endif()
llvm_map_components_to_libnames(llvm_libs ${LLVM_COMPONENTS})
target_link_libraries(edn PUBLIC ${llvm_libs})

if(TARGET fmt::fmt)
    target_link_libraries(edn PUBLIC fmt::fmt)
endif()
if(TARGET cxxopts::cxxopts)
    target_link_libraries(edn PUBLIC cxxopts::cxxopts)
endif()

message(STATUS "Found LLVM 18: ${LLVM_PACKAGE_VERSION}")
list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})

# Provide version header generation (simple) into build tree
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/edn)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/edn/version.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/edn/version.hpp @ONLY)

# Install rules
install(DIRECTORY include/edn
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/edn/version.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/edn)

install(TARGETS edn EXPORT ednTargets)
install(EXPORT ednTargets
        NAMESPACE edn::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edn)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ednConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ednConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ednConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edn)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ednConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ednConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edn)

# Tests
if(EDN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples (always build small samples)
add_subdirectory(examples)

# Benchmarks (tiny microbench harness)
add_subdirectory(benchmarks)

# Tools
add_executable(phase1_driver tools/phase1_driver.cpp)
target_link_libraries(phase1_driver PRIVATE edn ${llvm_libs})
target_compile_features(phase1_driver PRIVATE cxx_std_20)

add_executable(phase3_driver tools/phase3_driver.cpp)
target_link_libraries(phase3_driver PRIVATE edn ${llvm_libs})
target_compile_features(phase3_driver PRIVATE cxx_std_20)

# Languages (prototype front-ends)
add_subdirectory(languages)
