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

## Controls

The panel's Controls section lists the current mode's bindings. In every mode: 1-4 / Tab switch
mode, R resets the current view, P saves a screenshot, - / + (the - and = keys) step the
iterations (held keys repeat), Z / X zoom in / out and W A S D move. 2D and Buddhabrot: drag
pans, scroll zooms at the cursor, O traces the orbit under the cursor (2D). Logistic map: drag
orbits, scroll zooms, Q / E move down / up.

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

## 3D fractals

Formulas (3D mode, `--formula N`): quaternion Mandelbrot / Julia, Mandelbulb, bicomplex
(Tetrabrot), the hypercomplex Mandelbrot (default) and the quaternion with Syntopia's flip.
The hypercomplex squaring (x² − y² − z² − w², 2(xy + zw), 2(xz − yw), 2(xw + yz)) is the
commuting-units system behind Yann Le Bihan's "quaternion" video.

Navigation: scroll zooms toward the surface under the cursor (Z / X toward the screen center),
drag orbits the grabbed point (half a turn per screen-height drag), shift-drag pans so the grabbed
point follows the cursor, W A S D fly and Q / E move down / up, with speeds scaled to the distance
to the surface. Over background, zooming scales the focus distance instead, so it converges and
zooming back out retraces it. The camera never moves inside the set.
Below 0.01 from the surface the quaternion family and hypercomplex formulas switch to
perturbation (offsets from a double-precision reference orbit), so close-ups stay sharp down
to ~1e-8. When the view stops moving it refines with up to 16 jittered samples
(progressive anti-aliasing).

## Buddhabrot

Mode 4 (`--mode 3`): GPU-accumulated Buddhabrot with Nebulabrot coloring (one histogram channel
per iteration limit), upright by default; pan/zoom restart the accumulation.

## Tests

```sh
ctest --test-dir build/debug --output-on-failure
```

## Credits

The per-iteration quaternion rotation in the 3D quaternion Mandelbrot/Julia is adapted from the
[Quaternion Mandelbrot Set 3D Viewer](https://icefractal.com/mandelbrot/quat3d.php),
(c) Copyright Ice Fractal 2025, MIT license.

## Benchmarks

```sh
./build/release/fracbrot --bench bench/new.json     # times fixed 2D/3D/logistic scenes
python3 bench/compare.py bench/baseline.json bench/new.json
```

Each frame is timed from swapchain acquire (vsync excluded) until the GPU finishes, at the
default 1280x800 window (2560x1600 on Retina). `bench/baseline.json` is the pre-optimization run. For low-noise comparisons use
`bench/run.sh NAME 3` (best median of 3 runs) and `bench/compare.py bench/out/A bench/out/B`.
GPU speed drifts with machine state, so to judge a change copy the old binary and run
`bench/ab.sh OLD_BIN NEW_BIN NAME 3`, which interleaves A and B runs.
