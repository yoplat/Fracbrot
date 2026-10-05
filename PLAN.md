# Fracbrot plan

2D/3D Mandelbrot visualizer in C++20. One renderer written against the WebGPU C API:
Dawn natively (Metal on macOS, Vulkan on Linux, D3D12 on Windows), Emscripten + browser WebGPU on the web.

## Architecture

```
src/
  app/       main.cpp, App (per-frame callback, input, ImGui UI)
  platform/  window + surface (GLFW native, canvas on web)
  gfx/       thin Device/Pipeline/Buffer/Texture layer over webgpu.h
  fractal/   Mandel2D, Raymarcher (quaternion/Julia/bulb), Bifurcation, perturbation
  camera/    Camera2D (pan/zoom), OrbitCamera3D
shaders/     *.wgsl, embedded at build time
web/         shell.html
external/    dawn, glfw, glm, imgui (FetchContent)
```

Web-friendly rules:
- no blocking main loop — `App::frame()` driven by GLFW loop or `emscripten_set_main_loop`
- no runtime file IO — shaders/palettes embedded
- threads optional (job system with single-thread fallback)
- one POD `FractalParams` uniform struct shared by every mode

## Phases

- [x] 0 `chore:` CMake + presets, Dawn/GLFW via FetchContent, window + clear color (Metal)
- [x] 1 `feat:` 2D Mandelbrot fullscreen pass, pan/zoom, iteration control (ImGui)
- [x] 2 `feat:` smooth coloring, palette texture, screenshot export
- [x] 3 `feat:` mode switch 2D/3D, orbit camera (heightfield terrain later removed)
- [x] 4 `feat:` 3D ray-marcher: quaternion Mandelbrot (4D slice/tilt, default), quaternion Julia, Mandelbulb; DE, soft shadows, AO, orbit traps
- [x] 4b `feat:` logistic map mode: bifurcation diagram standing on the Mandelbrot real axis in 3D (c = r(2-r)/4), 2D orbit tracer
- [ ] 5 `feat:` web build (Emscripten preset, canvas surface, shell.html)
- [x] 6 `feat:` deep zoom via perturbation (high-precision CPU reference orbit)
- [x] 7 `chore:` CI (macOS; Linux/web later)
- [ ] 8 optional: hand-written Metal/Vulkan backends behind gfx interface

## Conventions

Commits: short conventional messages (`feat: ...`, `fix: ...`, `chore: ...`), no co-author trailers.
