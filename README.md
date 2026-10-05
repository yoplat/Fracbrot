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

## Deep zoom

Below scale 1e-3 the 2D view switches to perturbation rendering: the view center is an
arbitrary-precision fixed-point number (`src/math/BigFixed`), a reference orbit is iterated on the
CPU at that precision, and each pixel iterates its f32 difference from it on the GPU (with
rebasing, and rescaled deltas below 2^-64). Zoom is limited only by the double `scale`, ~1e290.

```sh
# c = i at zoom ~1e100, any number of digits works for X,Y
./build/debug/fracbrot --view 0,1,1e-100 --iter 5000
```

"copy location" in the UI copies the current view in the same `X,Y,SCALE` format.

## Tests

```sh
ctest --test-dir build/debug --output-on-failure
```

## Credits

The per-iteration quaternion rotation in the 3D quaternion Mandelbrot/Julia is adapted from the
[Quaternion Mandelbrot Set 3D Viewer](https://icefractal.com/mandelbrot/quat3d.php),
(c) Copyright Ice Fractal 2025, MIT license.
