// Orbit samples of z <- z² + c for real c, one column per invocation. After `warmup`
// iterations the next `samples` values are the attractor — the bifurcation diagram,
// which is the logistic map x <- r x (1 - x) under c = r(2 - r)/4, z = r(1/2 - x).

struct Bif {
    cMin: f32,
    cMax: f32,
    columns: u32,
    samples: u32,
    warmup: u32,
    pointSize: f32,
    brightness: f32,
    highlightC: f32,
    yScale: f32,
    planeDim: f32,
    resolution: vec2f,
};

@group(0) @binding(0) var<uniform> b: Bif;
@group(0) @binding(1) var<storage, read_write> points: array<vec2f>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3u) {
    let col = id.x;
    if (col >= b.columns) { return; }

    let c = mix(b.cMin, b.cMax, (f32(col) + 0.5) / f32(b.columns));
    var z = 0.0;
    for (var i = 0u; i < b.warmup; i++) {
        z = z * z + c;
    }
    let base = col * b.samples;
    for (var i = 0u; i < b.samples; i++) {
        z = z * z + c;
        points[base + i] = vec2f(c, z);
    }
}
