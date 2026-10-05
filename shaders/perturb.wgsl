// Deep-zoom 2D Mandelbrot via perturbation around a high-precision reference
// orbit Z_n (computed on the CPU). Each pixel iterates its difference
//   δ_{n+1} = 2 Z_n δ_n + δ_n² + δc
// in two regimes:
//  - scaled: while |δ| is below f32 range, δ = w · 2^sE with w kept near 1
//    ("rescaled iterations"), so zooms far past 1e38 still work;
//  - plain: ordinary f32 δ with Zhuoran rebasing (restart from Z_0 whenever
//    |Z_n + δ_n| < |δ_n| or the reference ends), which avoids glitches.

struct Perturb {
    offsetPx: vec2f,  // (view center - reference) in pixels, +y up
    pixelMant: f32,   // pixel size = pixelMant * 2^pixelExp
    pixelExp: i32,
    refLen: u32,
    _pad0: u32,
    _pad1: u32,
    _pad2: u32,
};

// value = m * 2^e
struct RefPoint {
    m: vec2f,
    e: i32,
    _pad: i32,
};

@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var paletteTex: texture_2d<f32>;
@group(0) @binding(2) var paletteSampler: sampler;
@group(0) @binding(3) var<uniform> pt: Perturb;
@group(0) @binding(4) var<storage, read> refOrbit: array<RefPoint>;

// Below 2^SCALED_LIMIT deltas are kept in scaled form.
const SCALED_LIMIT: i32 = -64;

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

fn cmul(a: vec2f, b: vec2f) -> vec2f {
    return vec2f(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

// v * 2^e, flushing to zero well below f32 range (ldexp is unspecified there).
fn scaleBy(v: vec2f, e: i32) -> vec2f {
    if (e < -140) { return vec2f(0.0); }
    return ldexp(v, vec2i(min(e, 120)));
}

@fragment
fn fs(@builtin(position) frag: vec4f) -> @location(0) vec4f {
    let px = frag.xy - 0.5 * p.resolution;
    let dcw = (vec2f(px.x, -px.y) + pt.offsetPx) * pt.pixelMant;
    let dcE = pt.pixelExp;
    let dcPlain = scaleBy(dcw, dcE);

    // Start at n = 1: Z_1 = c_ref and δ_1 = δc.
    var w = dcw;
    var sE = dcE;
    var scaled = sE < SCALED_LIMIT;
    var d = dcPlain;
    var n = 1u;
    var i = 1u;
    var z = vec2f(0.0);
    var escaped = false;

    loop {
        if (i >= p.maxIter) { break; }
        let r = refOrbit[n];
        let zRef = scaleBy(r.m, r.e);

        if (scaled) {
            z = zRef + scaleBy(w, sE);
            if (dot(z, z) > BAILOUT2) { escaped = true; break; }
            if (n + 1u >= pt.refLen) {
                // Reference ended; δ is negligible so the pixel is just z. Rebase.
                d = z;
                n = 0u;
                scaled = false;
                continue;
            }
            w = 2.0 * scaleBy(cmul(r.m, w), r.e) + scaleBy(cmul(w, w), sE) + scaleBy(dcw, dcE - sE);
            let mx = max(abs(w.x), abs(w.y));
            if (mx > 256.0 || (mx < 0.00390625 && mx > 0.0)) {
                let e = frexp(mx).exp;
                w = ldexp(w, vec2i(-e));
                sE += e;
            }
            n++;
            i++;
            if (sE > SCALED_LIMIT) {
                scaled = false;
                d = scaleBy(w, sE);
            }
        } else {
            z = zRef + d;
            if (dot(z, z) > BAILOUT2) { escaped = true; break; }
            var zn = zRef;
            if (dot(z, z) < dot(d, d) || n + 1u >= pt.refLen) {
                d = z;
                n = 0u;
                zn = vec2f(0.0);
            }
            d = 2.0 * cmul(zn, d) + cmul(d, d) + dcPlain;
            n++;
            i++;
        }
    }

    if (!escaped) { return vec4f(0.0, 0.0, 0.0, 1.0); }

    var nIter = f32(i);
    if ((p.flags & FLAG_SMOOTH) != 0u) {
        nIter = max(nIter + 1.0 - log2(0.5 * log(dot(z, z))), 0.0);
    }
    let t = paletteCoord(nIter, p.colorDensity, p.colorOffset);
    let col = textureSampleLevel(paletteTex, paletteSampler, vec2f(t, 0.5), 0.0).rgb;
    return vec4f(col, 1.0);
}
