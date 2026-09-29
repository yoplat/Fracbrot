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

@fragment
fn fs(@builtin(position) frag: vec4f) -> @location(0) vec4f {
    let px = (frag.xy - 0.5 * p.resolution) / p.resolution.y * 2.0;
    let c = p.center + vec2f(px.x, -px.y) * p.scale;

    let n = mandelbrot(c, p.maxIter, (p.flags & FLAG_SMOOTH) != 0u);
    if (n < 0.0) { return vec4f(0.0, 0.0, 0.0, 1.0); }

    let t = paletteCoord(n, p.colorDensity, p.colorOffset);
    let col = textureSampleLevel(paletteTex, paletteSampler, vec2f(t, 0.5), 0.0).rgb;
    return vec4f(col, 1.0);
}
