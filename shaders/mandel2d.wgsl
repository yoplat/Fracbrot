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

@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var paletteTex: texture_2d<f32>;
@group(0) @binding(2) var paletteSampler: sampler;

struct VsOut {
    @builtin(position) pos: vec4f,
};

// Fullscreen triangle, no vertex buffer.
@vertex
fn vs(@builtin(vertex_index) i: u32) -> VsOut {
    let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
    var o: VsOut;
    o.pos = vec4f(uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

fn inMainBulbs(c: vec2f) -> bool {
    // Main cardioid and period-2 bulb: skip iterating points known to be inside.
    let q = (c.x - 0.25) * (c.x - 0.25) + c.y * c.y;
    if (q * (q + (c.x - 0.25)) <= 0.25 * c.y * c.y) { return true; }
    return (c.x + 1.0) * (c.x + 1.0) + c.y * c.y <= 0.0625;
}

@fragment
fn fs(@builtin(position) frag: vec4f) -> @location(0) vec4f {
    let px = (frag.xy - 0.5 * p.resolution) / p.resolution.y * 2.0;
    let c = p.center + vec2f(px.x, -px.y) * p.scale;

    if (inMainBulbs(c)) { return vec4f(0.0, 0.0, 0.0, 1.0); }

    var z = vec2f(0.0);
    var i = 0u;
    loop {
        if (i >= p.maxIter || dot(z, z) > BAILOUT2) { break; }
        z = vec2f(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        i++;
    }
    if (i >= p.maxIter) { return vec4f(0.0, 0.0, 0.0, 1.0); }

    var n = f32(i);
    if ((p.flags & FLAG_SMOOTH) != 0u) {
        // Normalized iteration count: n + 1 - log2(log|z|).
        n = n + 1.0 - log2(0.5 * log(dot(z, z)));
    }
    // sqrt spreads the palette evenly as iteration counts grow with zoom.
    let t = sqrt(max(n, 0.0)) * 0.125 * p.colorDensity + p.colorOffset;
    let col = textureSampleLevel(paletteTex, paletteSampler, vec2f(t, 0.5), 0.0).rgb;
    return vec4f(col, 1.0);
}
