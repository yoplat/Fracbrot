// Shared by every fractal shader; prepended at pipeline creation.

struct Params {
    center: vec2f,
    scale: f32,        // half-height of the view in complex units
    maxIter: u32,
    resolution: vec2f,
    time: f32,
    colorDensity: f32,
    colorOffset: f32,
    flags: u32,
    _pad: vec2u,
};

const FLAG_SMOOTH: u32 = 1u;
const BAILOUT2: f32 = 65536.0; // large radius keeps the smooth estimate accurate

fn inMainBulbs(c: vec2f) -> bool {
    // Main cardioid and period-2 bulb: skip iterating points known to be inside.
    let q = (c.x - 0.25) * (c.x - 0.25) + c.y * c.y;
    if (q * (q + (c.x - 0.25)) <= 0.25 * c.y * c.y) { return true; }
    return (c.x + 1.0) * (c.x + 1.0) + c.y * c.y <= 0.0625;
}

// Escape-time iteration count for c, or -1 if c did not escape (interior).
// Continuous when `smoothed` is set.
fn mandelbrot(c: vec2f, maxIter: u32, smoothed: bool) -> f32 {
    if (inMainBulbs(c)) { return -1.0; }
    var z = vec2f(0.0);
    var i = 0u;
    loop {
        if (i >= maxIter || dot(z, z) > BAILOUT2) { break; }
        z = vec2f(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        i++;
    }
    if (i >= maxIter) { return -1.0; }
    if (!smoothed) { return f32(i); }
    // Normalized iteration count: n + 1 - log2(log|z|).
    return max(f32(i) + 1.0 - log2(0.5 * log(dot(z, z))), 0.0);
}

// Palette coordinate for an iteration count; sqrt spreads colors evenly as counts grow with zoom.
fn paletteCoord(n: f32, density: f32, offset: f32) -> f32 {
    return sqrt(n) * 0.125 * density + offset;
}
