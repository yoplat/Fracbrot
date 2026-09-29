# Fracbrot

2D/3D Mandelbrot visualizer in C++20 on WebGPU (Dawn natively → Metal / Vulkan / D3D12; browser WebGPU via Emscripten).

## Build

```sh
cmake --preset release
cmake --build --preset release
./build/release/fracbrot
```

Dependencies (Dawn prebuilt, GLFW, glm) are fetched on first configure into `.cache/deps`.
Set `-DDAWN_DIR=/path/to/dawn` to use a local Dawn install instead.

See [PLAN.md](PLAN.md) for the roadmap.
