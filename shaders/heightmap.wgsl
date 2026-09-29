// Samples the escape-time field over the square region center ± scale into a
// gridSize² texture. Row 0 is the top of the region (+imag), matching world -z.
// Output: r = normalized height in [0, 1] (2x2 supersampled), g = iteration count at the
// grid point (-1 inside the set) for coloring.

struct Scene {
    viewProj: mat4x4f,
    invViewProj: mat4x4f,
    camPos: vec4f,
    lightDir: vec4f,
    heightScale: f32,
    gridSize: u32,
    interiorHeight: f32,
    heightCurve: f32,
};

@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var<uniform> scene: Scene;
@group(0) @binding(2) var heightOut: texture_storage_2d<rg32float, write>;

fn heightOf(n: f32) -> f32 {
    if (n < 0.0) { return scene.interiorHeight; }
    let h = log2(1.0 + n) / log2(1.0 + f32(p.maxIter));
    return pow(clamp(h, 0.0, 1.0), scene.heightCurve);
}

fn coordOf(uv: vec2f) -> vec2f {
    return p.center + vec2f(uv.x, -uv.y) * p.scale;
}

@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id: vec3u) {
    let size = textureDimensions(heightOut);
    if (id.x >= size.x || id.y >= size.y) { return; }

    let cell = 2.0 / f32(size.x - 1u);
    let uv = vec2f(id.xy) * cell - 1.0;
    let n = mandelbrot(coordOf(uv), p.maxIter, true);

    // Average 4 sub-samples around the grid point to tame aliasing near the boundary.
    var h = 0.0;
    for (var k = 0u; k < 4u; k++) {
        let o = (vec2f(f32(k & 1u), f32(k >> 1u)) - 0.5) * 0.5 * cell;
        h += heightOf(mandelbrot(coordOf(uv + o), p.maxIter, true));
    }
    textureStore(heightOut, id.xy, vec4f(h * 0.25, n, 0.0, 0.0));
}
