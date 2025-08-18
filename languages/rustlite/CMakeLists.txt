cmake_minimum_required(VERSION 3.20)
project(rustlite LANGUAGES CXX)

add_library(rustlite STATIC
  src/rustlite.cpp
  src/expand.cpp
)

target_include_directories(rustlite PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(rustlite PUBLIC edn)

target_compile_features(rustlite PUBLIC cxx_std_20)

enable_testing()
add_executable(rustlite_driver tools/rustlite_driver.cpp)

# Link language driver to edn + rustlite
if(TARGET edn)
  target_link_libraries(rustlite_driver PRIVATE rustlite edn)
endif()

target_compile_features(rustlite_driver PRIVATE cxx_std_20)

add_test(NAME rustlite.smoke COMMAND rustlite_driver)
