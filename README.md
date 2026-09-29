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

## Editor / LSP (clangd)

Configuring writes `build/<preset>/compile_commands.json`; the repo root has a
`compile_commands.json` symlink pointing at the debug one, which clangd (e.g. nvim-lspconfig)
picks up automatically. Re-link it if you use another preset:

```sh
ln -sf build/release/compile_commands.json compile_commands.json
```

Generated shader headers live in `build/<preset>/generated`, so configure/build once before opening the editor.
