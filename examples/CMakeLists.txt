add_executable(edn_llvm_ir_example llvm_ir_emitter.cpp)
set_target_properties(edn_llvm_ir_example PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
target_link_libraries(edn_llvm_ir_example PRIVATE edn)
if(TARGET fmt::fmt)
	target_link_libraries(edn_llvm_ir_example PRIVATE fmt::fmt)
endif()
## No direct LLVM linkage needed; edn target already links required LLVM components

# Traits example (Phase 4 experimental)
add_executable(edn_traits_example traits_example.cpp)
set_target_properties(edn_traits_example PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
target_link_libraries(edn_traits_example PRIVATE edn)
if(TARGET fmt::fmt)
	target_link_libraries(edn_traits_example PRIVATE fmt::fmt)
endif()
## Inherit LLVM linkage transitively via edn

# Generics example
add_executable(edn_generics_example generics_example.cpp)
set_target_properties(edn_generics_example PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
target_link_libraries(edn_generics_example PRIVATE edn)
if(TARGET fmt::fmt)
	target_link_libraries(edn_generics_example PRIVATE fmt::fmt)
endif()
## Inherit LLVM linkage transitively via edn

# Sum types example
add_executable(edn_sum_example sum_example.cpp)
set_target_properties(edn_sum_example PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
target_link_libraries(edn_sum_example PRIVATE edn)
if(TARGET fmt::fmt)
	target_link_libraries(edn_sum_example PRIVATE fmt::fmt)
endif()
## Inherit LLVM linkage transitively via edn
