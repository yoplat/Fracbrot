cmake_minimum_required(VERSION 3.24)
project(Fracbrot LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
  enable_language(OBJC OBJCXX)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(Dependencies)
include(EmbedShaders)

add_executable(fracbrot
  src/app/main.cpp
  src/app/App.cpp
  src/app/Gui.cpp
  src/gfx/Context.cpp
  src/gfx/Readback.cpp
  src/fractal/Bifurcation.cpp
  src/fractal/DistanceEstimator.cpp
  src/fractal/Mandel2D.cpp
  src/fractal/OrbitTracer.cpp
  src/fractal/ReferenceOrbit.cpp
  src/math/BigFixed.cpp
  src/fractal/Raymarcher.cpp
  src/fractal/Palette.cpp
  src/platform/Window.cpp
)

if(EMSCRIPTEN)
  target_sources(fracbrot PRIVATE src/platform/Surface_web.cpp)
elseif(APPLE)
  target_sources(fracbrot PRIVATE src/platform/Surface_macos.mm)
else()
  target_sources(fracbrot PRIVATE src/platform/Surface_glfw.cpp)
endif()

target_include_directories(fracbrot PRIVATE src)
# Reference orbits run millions of bignum ops; keep them fast in debug builds too.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set_source_files_properties(src/math/BigFixed.cpp src/fractal/ReferenceOrbit.cpp PROPERTIES COMPILE_OPTIONS "-O2")
endif()
target_link_libraries(fracbrot PRIVATE fracbrot_deps)
fracbrot_embed_shaders(fracbrot ${CMAKE_SOURCE_DIR}/shaders)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(fracbrot PRIVATE -Wall -Wextra)
endif()

# --- tests ----------------------------------------------------------------------
if(NOT EMSCRIPTEN)
  enable_testing()
  add_executable(fracbrot_tests tests/test_bigfixed.cpp src/math/BigFixed.cpp src/fractal/ReferenceOrbit.cpp)
  target_include_directories(fracbrot_tests PRIVATE src)
  add_test(NAME fracbrot_tests COMMAND fracbrot_tests)

  add_executable(fracbrot_distance_tests tests/test_distance.cpp src/fractal/DistanceEstimator.cpp
                                         src/fractal/Raymarcher.cpp)
  target_include_directories(fracbrot_distance_tests PRIVATE src)
  target_link_libraries(fracbrot_distance_tests PRIVATE fracbrot_deps)
  fracbrot_embed_shaders(fracbrot_distance_tests ${CMAKE_SOURCE_DIR}/shaders)
  add_test(NAME fracbrot_distance_tests COMMAND fracbrot_distance_tests)
endif()
