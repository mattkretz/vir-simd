cmake_minimum_required(VERSION 3.0)
project(vir-simd CXX)

# Define main library
add_library(vir-simd INTERFACE)
target_include_directories(
  vir-simd INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                     $<INSTALL_INTERFACE:include>)

# Require at least C++17
target_compile_features(vir-simd INTERFACE cxx_std_17)

set(MSVC_WARNINGS "")
set(CLANG_WARNINGS -Wno-attributes -Wno-unknown-pragmas)
set(GCC_WARNINGS ${CLANG_WARNINGS})

if(MSVC)
  set(PROJECT_WARNINGS_CXX ${MSVC_WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  set(PROJECT_WARNINGS_CXX ${CLANG_WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(PROJECT_WARNINGS_CXX ${GCC_WARNINGS})
endif()

target_compile_options(
  vir-simd INTERFACE $<$<COMPILE_LANGUAGE:CXX>:${PROJECT_WARNINGS_CXX}>)

# Define the install target
install(TARGETS vir-simd EXPORT vir-simd-config)
install(
  EXPORT vir-simd-config
  NAMESPACE vir-simd::
  DESTINATION lib/cmake/vir-simd)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vir
  DESTINATION include
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  PATTERN "*.cpp" EXCLUDE)

# Define test targets
add_library(vir-simd-test-stdlib vir/test.cpp)
target_link_libraries(vir-simd-test-stdlib PRIVATE vir-simd)

add_library(vir-simd-test-fallback vir/test.cpp)
target_link_libraries(vir-simd-test-fallback PRIVATE vir-simd)
target_compile_definitions(vir-simd-test-fallback PRIVATE VIR_DISABLE_STDX_SIMD)
