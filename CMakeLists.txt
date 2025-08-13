cmake_minimum_required(VERSION 3.16)
project(edn LANGUAGES CXX VERSION 0.0.1)

# Options
option(EDN_BUILD_TESTS "Build tests" ON)

# Library target (header-only)
add_library(edn INTERFACE)
add_library(edn::edn ALIAS edn)

include(GNUInstallDirs)

target_compile_features(edn INTERFACE cxx_std_17)

target_include_directories(edn
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

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
