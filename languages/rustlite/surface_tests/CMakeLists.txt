cmake_minimum_required(VERSION 3.20)

add_executable(rustlite_surface_norm
  surface_norm.cpp
  $<TARGET_OBJECTS:rustlite_parser>)
target_link_libraries(rustlite_surface_norm PRIVATE rustlite edn)
target_include_directories(rustlite_surface_norm PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(rustlite_surface_norm PRIVATE cxx_std_20)

set(RUSTLITE_SURFACE_SAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/samples)
file(GLOB RUSTLITE_SURFACE_SAMPLES CONFIGURE_DEPENDS ${RUSTLITE_SURFACE_SAMPLE_DIR}/*.rl.rs)

foreach(SRC ${RUSTLITE_SURFACE_SAMPLES})
  get_filename_component(NAME_WE ${SRC} NAME_WE) # e.g. tuple_basic.rl
  string(REPLACE ".rl" "" TEST_BASE ${NAME_WE}) # strip trailing .rl -> tuple_basic
  set(GOLD ${RUSTLITE_SURFACE_SAMPLE_DIR}/${TEST_BASE}.gold.edn)
  add_test(NAME rustlite.surface.${TEST_BASE}
    COMMAND rustlite_surface_norm test ${SRC} ${GOLD})
endforeach()
