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
  src/gfx/Context.cpp
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
target_link_libraries(fracbrot PRIVATE fracbrot_deps)
fracbrot_embed_shaders(fracbrot ${CMAKE_SOURCE_DIR}/shaders)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(fracbrot PRIVATE -Wall -Wextra)
endif()
