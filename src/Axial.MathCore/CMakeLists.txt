cmake_minimum_required(VERSION 3.20)
project(axial_mathcore LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /O2)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

set(CORE_SOURCES
  src/bigint.cpp
  src/rational.cpp
  src/value.cpp
  src/tokenizer.cpp
  src/parser.cpp
  src/vm.cpp
  src/json.cpp
  src/engine.cpp
)

add_library(axial_mathcore STATIC ${CORE_SOURCES})
target_include_directories(axial_mathcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(axial_mathcore PUBLIC AXIAL_STATIC_BUILD)

add_library(axial_mathcore_dll SHARED ${CORE_SOURCES})
target_compile_definitions(axial_mathcore_dll PRIVATE AXIAL_BUILD_DLL)
target_include_directories(axial_mathcore_dll PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(axial_tests
  tests/test_main.cpp
  tests/test_bigint.cpp
  tests/test_rational.cpp
  tests/test_eval.cpp
  tests/test_errors.cpp
)
target_link_libraries(axial_tests PRIVATE axial_mathcore)
target_include_directories(axial_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

option(BUILD_BENCH "Build the benchmark harness" ON)
if(BUILD_BENCH)
  add_executable(axial_bench ${CMAKE_CURRENT_SOURCE_DIR}/../../benchmarks/bench_core.cpp)
  target_link_libraries(axial_bench PRIVATE axial_mathcore)
endif()

enable_testing()
add_test(NAME mathcore COMMAND axial_tests)
