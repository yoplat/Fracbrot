// Shared by the Buddhabrot shaders; prepended after common.wgsl.

struct Buddha {
    center: vec2f,        // view-space center (see toView)
    scale: f32,           // half-height of the view in complex units
    rotate: u32,          // 1: real axis vertical, negative reals up (the classic upright Buddha)
    histSize: vec2u,      // histogram width, height (pixels)
    screenSize: vec2f,    // output target size (pixels)
    limits: vec4u,        // iteration limits for R, G, B; w = the largest
    regionMin: vec2f,     // sampled c range (upper half plane; orbits are mirrored)
    regionMax: vec2f,
    frame: u32,           // accumulation batch index, seeds the RNG
    samplesPerThread: u32,
    minIter: u32,         // orbits escaping sooner are skipped
    channels: u32,        // 3 = nebulabrot (R, G, B limits), 1 = single channel (R limit)
    exposure: f32,
    gamma: f32,
    _pad: vec2u,
};

// Complex plane -> view space. Rotated: right = +imaginary, up = -real.
fn toView(z: vec2f, rotate: u32) -> vec2f {
    if (rotate != 0u) { return vec2f(z.y, -z.x); }
    return z;
}
