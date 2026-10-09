// Tone-maps the Buddhabrot histogram to the screen: bilinear upsampling from the histogram
// resolution, counts normalized by each channel's maximum, scaled by exposure (so the hottest
// spots clip), then a gamma curve (2 = square root, the classic Buddhabrot look).

@group(0) @binding(0) var<uniform> b: Buddha;
@group(0) @binding(1) var<storage, read> hist: array<u32>;
@group(0) @binding(2) var<storage, read> maxv: array<u32, 3>;
@group(0) @binding(3) var paletteTex: texture_2d<f32>;
@group(0) @binding(4) var paletteSampler: sampler;

struct VsOut {
    @builtin(position) pos: vec4f,
};

@vertex
fn vs(@builtin(vertex_index) i: u32) -> VsOut {
    let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
    var o: VsOut;
    o.pos = vec4f(uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

fn texel(p: vec2i) -> vec3f {
    let q = clamp(p, vec2i(0), vec2i(b.histSize) - 1);
    let i = (u32(q.y) * b.histSize.x + u32(q.x)) * 3u;
    return vec3f(f32(hist[i]), f32(hist[i + 1u]), f32(hist[i + 2u]));
}

fn tone(v: f32, maxCount: u32) -> f32 {
    if (maxCount == 0u) { return 0.0; }
    return pow(clamp(v / f32(maxCount) * b.exposure, 0.0, 1.0), 1.0 / b.gamma);
}

@fragment
fn fs(@builtin(position) frag: vec4f) -> @location(0) vec4f {
    let hc = frag.xy / b.screenSize * vec2f(b.histSize) - 0.5;
    let p0 = vec2i(floor(hc));
    let f = fract(hc);
    let v = mix(mix(texel(p0), texel(p0 + vec2i(1, 0)), f.x),
                mix(texel(p0 + vec2i(0, 1)), texel(p0 + vec2i(1, 1)), f.x), f.y);

    if (b.channels == 3u) {
        return vec4f(tone(v.x, maxv[0]), tone(v.y, maxv[1]), tone(v.z, maxv[2]), 1.0);
    }
    // Single channel: brightness picks a color from the first part of the palette (the
    // presets run dark -> bright before cycling back), faded to black where counts are low.
    let t = tone(v.x, maxv[0]);
    let col = textureSampleLevel(paletteTex, paletteSampler, vec2f(0.05 + 0.55 * t, 0.5), 0.0).rgb;
    return vec4f(col * smoothstep(0.0, 0.35, t), 1.0);
}
