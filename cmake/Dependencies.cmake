include(FetchContent)

add_library(fracbrot_deps INTERFACE)

# --- Dawn (prebuilt release) -------------------------------------------------
set(DAWN_RELEASE v20260928.195327)
set(DAWN_HASH cd061e9948a12df12d69c754765dee91e912f04f)
set(DAWN_BASE_URL https://github.com/google/dawn/releases/download/${DAWN_RELEASE})

if(EMSCRIPTEN)
  # emdawnwebgpu port: same webgpu.h / webgpu_cpp.h on top of browser WebGPU.
  FetchContent_Declare(emdawnwebgpu
    URL ${DAWN_BASE_URL}/emdawnwebgpu_pkg-${DAWN_RELEASE}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(emdawnwebgpu)
  set(_port "--use-port=${emdawnwebgpu_SOURCE_DIR}/emdawnwebgpu.port.py")
  target_compile_options(fracbrot_deps INTERFACE ${_port})
  target_link_options(fracbrot_deps INTERFACE ${_port})
else()
  set(DAWN_DIR "" CACHE PATH "Local Dawn install (skips download)")
  if(NOT DAWN_DIR)
    if(APPLE)
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_dawn_platform macos-latest)
      else()
        set(_dawn_platform macos-15-intel)
      endif()
    elseif(WIN32)
      set(_dawn_platform windows-latest)
    else()
      set(_dawn_platform ubuntu-latest)
    endif()
    FetchContent_Declare(dawn
      URL ${DAWN_BASE_URL}/Dawn-${DAWN_HASH}-${_dawn_platform}-Release.tar.gz
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(dawn)
    set(DAWN_DIR ${dawn_SOURCE_DIR})
  endif()
  find_package(Threads REQUIRED)
  find_package(Dawn CONFIG REQUIRED PATHS ${DAWN_DIR} NO_DEFAULT_PATH)
  target_link_libraries(fracbrot_deps INTERFACE dawn::webgpu_dawn)

  # --- GLFW -------------------------------------------------------------------
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(glfw)
  target_link_libraries(fracbrot_deps INTERFACE glfw)
endif()

# --- glm ----------------------------------------------------------------------
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glm)
target_link_libraries(fracbrot_deps INTERFACE glm::glm)
