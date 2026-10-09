// Buddhabrot accumulation: each invocation draws random c, finds whether its orbit escapes,
// and if so adds every orbit point to a per-pixel histogram. Nebulabrot coloring keeps one
// histogram channel per iteration limit (an orbit escaping at n counts in every channel
// whose limit is >= n).

@group(0) @binding(0) var<uniform> b: Buddha;
@group(0) @binding(1) var<storage, read_write> hist: array<atomic<u32>>;

fn pcg(v: u32) -> u32 {
    let state = v * 747796405u + 2891336453u;
    let word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

fn rand01(seed: ptr<function, u32>) -> f32 {
    *seed = pcg(*seed);
    return f32(*seed >> 8u) * (1.0 / 16777216.0);
}

fn sq(z: vec2f) -> vec2f {
    return vec2f(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y);
}

// Iteration at which the orbit of c leaves |z| <= 2, or 0 if it stays bounded for
// `maxIter` steps. Interior orbits are cut short by the strided Brent cycle check.
fn escapeIteration(c: vec2f, maxIter: u32) -> u32 {
    var z = vec2f(0.0);
    var i = 0u;
    var saved = vec2f(0.0);
    var sinceSave = 0u;
    var interval = 2u * PERIOD_STRIDE;
    loop {
        for (var k = 0u; k < PERIOD_STRIDE; k++) {
            if (i >= maxIter || dot(z, z) > 4.0) { break; }
            z = sq(z) + c;
            i++;
        }
        if (dot(z, z) > 4.0) { return i; }
        if (i >= maxIter) { return 0u; }
        let d = z - saved;
        if (dot(d, d) <= PERIOD_EPS2 * dot(z, z)) { return 0u; }
        sinceSave += PERIOD_STRIDE;
        if (sinceSave >= interval) {
            saved = z;
            sinceSave = 0u;
            interval *= 2u;
        }
    }
}

fn deposit(z: vec2f, n: u32) {
    let h = f32(b.histSize.y);
    let p = (toView(z, b.rotate) - b.center) / b.scale * (0.5 * h);
    let x = i32(floor(p.x + 0.5 * f32(b.histSize.x)));
    let y = i32(floor(0.5 * h - p.y));
    if (x < 0 || y < 0 || x >= i32(b.histSize.x) || y >= i32(b.histSize.y)) { return; }
    let idx = (u32(y) * b.histSize.x + u32(x)) * 3u;
    if (n <= b.limits.x) { atomicAdd(&hist[idx], 1u); }
    if (b.channels == 3u) {
        if (n <= b.limits.y) { atomicAdd(&hist[idx + 1u], 1u); }
        if (n <= b.limits.z) { atomicAdd(&hist[idx + 2u], 1u); }
    }
}

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3u) {
    var seed = pcg(id.x ^ pcg(b.frame * 0x9E3779B9u + 0x85EBCA6Bu));
    for (var s = 0u; s < b.samplesPerThread; s++) {
        let c = mix(b.regionMin, b.regionMax, vec2f(rand01(&seed), rand01(&seed)));
        if (inMainBulbs(c)) { continue; }
        let n = escapeIteration(c, b.limits.w);
        if (n == 0u || n < b.minIter) { continue; }

        // The set is symmetric about the real axis, so only the upper half is sampled and
        // each orbit is also deposited mirrored.
        var z = vec2f(0.0);
        for (var k = 0u; k < n; k++) {
            z = sq(z) + c;
            deposit(z, n);
            deposit(vec2f(z.x, -z.y), n);
        }
    }
}
