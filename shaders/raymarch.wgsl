// Ray-marched 3D fractals with distance estimators, soft shadows, AO and
// orbit-trap coloring:
//   0: quaternion Mandelbrot  q <- q² + c,  c = (x, y, z, sliceW), z0 = 0
//   1: quaternion Julia       q <- q² + k,  q0 = (x, y, z, sliceW)
//   2: Mandelbulb (power N), formula axis z mapped to world +y
//   3: bicomplex Mandelbrot ("Tetrabrot"): q <- q² + c with commuting units; it splits
//      into two complex Mandelbrot iterations, so the set is M x M seen through a slice
// For the quaternion formulas world x is the real axis.

struct Scene {
    viewProj: mat4x4f,
    invViewProj: mat4x4f,
    camPos: vec4f,
    lightDir: vec4f,   // towards the light
    _pad: vec4u,
};

struct March {
    formula: u32,
    iterations: u32,
    detail: f32,        // hit threshold in pixels
    maxSteps: u32,
    shadowSoftness: f32,
    aoStrength: f32,
    flags: u32,
    trapMix: f32,       // 0 = flat base color, 1 = full orbit-trap palette
    pixelAngle: f32,    // angular size of one pixel
    power: f32,         // Mandelbulb exponent
    sliceW: f32,        // 4th coordinate of the 3D slice
    tilt: f32,          // rotation of the slice between the real axis and w (radians)
    juliaC: vec4f,
    boundCenter: vec3f,
    boundRadius: f32,
    rotation: vec4f,    // per-iteration rotation angles (XZ, YZ, XW), radians
};

const FORMULA_QUAT_MANDEL: u32 = 0u;
const FORMULA_QUAT_JULIA: u32 = 1u;
const FORMULA_BULB: u32 = 2u;
const FORMULA_BICOMPLEX: u32 = 3u;
const FLAG_SHADOWS: u32 = 1u;
const FLAG_AO: u32 = 2u;
const FLAG_REFLECT: u32 = 4u;
const COARSE_DIVISOR: u32 = 4u; // coarse march / shadow iterations = iterations / this
const BASE_COLOR = vec3f(0.85, 0.82, 0.78);

@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var<uniform> scene: Scene;
@group(0) @binding(2) var<uniform> m: March;
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

fn qsq(q: vec4f) -> vec4f {
    return vec4f(q.x * q.x - dot(q.yzw, q.yzw), 2.0 * q.x * q.yzw);
}

// Maps a world point into the 4D slice.
fn toSlice(pos: vec3f) -> vec4f {
    let ct = cos(m.tilt);
    let st = sin(m.tilt);
    return vec4f(pos.x * ct - m.sliceW * st, pos.y, pos.z, pos.x * st + m.sliceW * ct);
}

// Rotates z in three fixed planes after each iteration. Adapted from the Quaternion
// Mandelbrot Set 3D Viewer, (c) Copyright Ice Fractal 2025, MIT license,
// https://icefractal.com/mandelbrot/quat3d.php Quaternions don't commute, so this breaks the rotational symmetry of plain
// z² + c around the real axis and creates genuinely 3D structure. Planes are named by
// imaginary units: XZ = (i, k), YZ = (j, k), XW = (i, real).
fn rotateIter(q: vec4f) -> vec4f {
    var z = q;
    let cs = cos(m.rotation.xyz);
    let sn = sin(m.rotation.xyz);
    z = vec4f(z.x, z.y * cs.x - z.w * sn.x, z.z, z.y * sn.x + z.w * cs.x);
    z = vec4f(z.x, z.y, z.z * cs.y - z.w * sn.y, z.z * sn.y + z.w * cs.y);
    z = vec4f(z.y * sn.z + z.x * cs.z, z.y * cs.z - z.x * sn.z, z.z, z.w);
    return z;
}

// Quaternion iteration; |a b| = |a||b| makes the scalar derivative bound exact, and the
// per-iteration rotation preserves lengths, so it stays valid with rotation too.
fn deQuat(pos: vec3f, julia: bool, iters: u32) -> vec2f {
    let p = toSlice(pos);
    var z = vec4f(0.0);
    var c = p;
    var dr = 0.0;
    if (julia) {
        z = p;
        c = m.juliaC;
        dr = 1.0;
    }
    let dc = select(1.0, 0.0, julia);
    var r2 = dot(z, z);
    var trap = 1e10;
    for (var i = 0u; i < iters; i++) {
        if (r2 > 256.0) { break; }
        dr = 2.0 * sqrt(r2) * dr + dc;
        z = rotateIter(qsq(z) + c);
        r2 = dot(z, z);
        trap = min(trap, r2);
    }
    let r = sqrt(r2);
    return vec2f(0.5 * r * log(max(r, 1e-8)) / max(dr, 1e-8), trap);
}

fn deBulb(worldPos: vec3f, iters: u32) -> vec2f {
    let c = worldPos.xzy;
    var z = c;
    var dr = 1.0;
    var r = length(z);
    var trap = 1e10;
    for (var i = 0u; i < iters; i++) {
        if (r > 2.0) { break; }
        let theta = acos(clamp(z.z / max(r, 1e-8), -1.0, 1.0)) * m.power;
        let phi = atan2(z.y, z.x) * m.power;
        let rp = pow(r, m.power - 1.0);
        dr = rp * m.power * dr + 1.0;
        z = rp * r * vec3f(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)) + c;
        trap = min(trap, dot(z, z));
        r = length(z);
    }
    return vec2f(0.5 * log(max(r, 1e-8)) * r / dr, trap);
}

fn cmul(a: vec2f, b: vec2f) -> vec2f {
    return vec2f(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

// 2D Mandelbrot distance estimate for c: (distance, orbit trap); distance 0 inside.
fn deMandel2(c: vec2f, iters: u32) -> vec2f {
    var z = vec2f(0.0);
    var dz = vec2f(0.0);
    var r2 = 0.0;
    var trap = 1e10;
    for (var i = 0u; i < iters; i++) {
        dz = 2.0 * cmul(z, dz) + vec2f(1.0, 0.0);
        z = cmul(z, z) + c;
        r2 = dot(z, z);
        trap = min(trap, r2);
        if (r2 > 1e4) { break; }
    }
    if (r2 <= 1e4) { return vec2f(0.0, trap); }
    let r = sqrt(r2);
    return vec2f(0.5 * r * log(r) / length(dz), trap);
}

// Bicomplex q = (a, b, c, d) in idempotent form: z1 = (a - d) + i(b + c), z2 = (a + d) + i(b - c).
// q is in the set iff both z1 and z2 are in M. Each projection stretches distances by at most
// sqrt(2), so max(d1, d2) / sqrt(2) bounds the distance to the intersection.
fn deBicomplex(pos: vec3f, iters: u32) -> vec2f {
    let q = toSlice(pos);
    let d1 = deMandel2(vec2f(q.x - q.w, q.y + q.z), iters);
    let d2 = deMandel2(vec2f(q.x + q.w, q.y - q.z), iters);
    return vec2f(max(d1.x, d2.x) * 0.70710678, min(d1.y, d2.y));
}

// (distance estimate, orbit trap) using `iters` iterations.
fn deN(pos: vec3f, iters: u32) -> vec2f {
    switch (m.formula) {
        case FORMULA_BICOMPLEX: { return deBicomplex(pos, iters); }
        case FORMULA_QUAT_JULIA: { return deQuat(pos, true, iters); }
        case FORMULA_BULB: { return deBulb(pos, iters); }
        default: { return deQuat(pos, false, iters); }
    }
}

fn de(pos: vec3f) -> vec2f {
    return deN(pos, m.iterations);
}

// Iterations for the cheap estimate used to march far from the surface and for soft
// shadows / AO. Only the bicomplex set benefits: its interior points run every iteration
// in both projections, and fewer iterations give a containing shape, so the estimate never
// overshoots. The quaternion orbits escape within a few iterations anyway, and with
// per-iteration rotation a coarse estimate can skip thin fins, so they keep full iterations.
fn coarseIterations() -> u32 {
    if (m.formula != FORMULA_BICOMPLEX) { return m.iterations; }
    return min(m.iterations, max(8u, m.iterations / COARSE_DIVISOR));
}

fn intersectSphere(ro: vec3f, rd: vec3f) -> vec2f {
    let oc = ro - m.boundCenter;
    let b = dot(oc, rd);
    let h = b * b - (dot(oc, oc) - m.boundRadius * m.boundRadius);
    if (h < 0.0) { return vec2f(-1.0); }
    let s = sqrt(h);
    return vec2f(-b - s, -b + s);
}

fn calcNormal(pos: vec3f, eps: f32) -> vec3f {
    let k = vec2f(1.0, -1.0);
    return normalize(k.xyy * de(pos + k.xyy * eps).x + k.yyx * de(pos + k.yyx * eps).x +
                     k.yxy * de(pos + k.yxy * eps).x + k.xxx * de(pos + k.xxx * eps).x);
}

fn softShadow(ro: vec3f, rd: vec3f) -> f32 {
    var res = 1.0;
    var t = 0.01;
    for (var i = 0; i < 64; i++) {
        let h = deN(ro + rd * t, coarseIterations()).x;
        res = min(res, m.shadowSoftness * h / t);
        t += clamp(h, 0.005, 0.1);
        if (res < 0.001 || t > 3.0) { break; }
    }
    return clamp(res, 0.0, 1.0);
}

fn ambientOcclusion(pos: vec3f, nrm: vec3f) -> f32 {
    var occ = 0.0;
    var w = 1.0;
    for (var i = 1; i <= 5; i++) {
        let h = 0.02 * f32(i);
        occ += (h - deN(pos + nrm * h, coarseIterations()).x) * w;
        w *= 0.7;
    }
    return clamp(1.0 - m.aoStrength * 6.0 * occ, 0.0, 1.0);
}

fn background(rd: vec3f) -> vec3f {
    let t = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    return mix(vec3f(0.02, 0.03, 0.07), vec3f(0.22, 0.38, 0.62), t * t);
}

@fragment
fn fs(@builtin(position) frag: vec4f) -> @location(0) vec4f {
    let ndc = vec2f(frag.x / p.resolution.x * 2.0 - 1.0, 1.0 - frag.y / p.resolution.y * 2.0);
    let farPoint = scene.invViewProj * vec4f(ndc, 1.0, 1.0);
    let ro = scene.camPos.xyz;
    let rd = normalize(farPoint.xyz / farPoint.w - ro);

    let bg = background(rd);
    let bounds = intersectSphere(ro, rd);
    if (bounds.y < 0.0) { return vec4f(bg, 1.0); }

    var t = max(bounds.x, 0.0);
    var hit = false;
    var trap = 0.0;
    var steps = 0u;
    // March with the cheap estimate until it reports a hit, then refine at the same point
    // with full iterations and continue from there.
    var iters = coarseIterations();
    for (; steps < m.maxSteps; steps++) {
        let d = deN(ro + rd * t, iters);
        let eps = max(t * m.pixelAngle * m.detail, 1e-6);
        if (d.x < eps) {
            if (iters < m.iterations) {
                iters = m.iterations;
                continue;
            }
            hit = true;
            trap = d.y;
            break;
        }
        t += d.x;
        if (t > bounds.y) { break; }
    }

    // Glow from how close the ray passed the surface.
    let glow = f32(steps) / f32(m.maxSteps);
    if (!hit) { return vec4f(bg + vec3f(0.3, 0.45, 0.7) * glow * glow * 0.6, 1.0); }

    let pos = ro + rd * t;
    let nrm = calcNormal(pos, max(t * m.pixelAngle, 1e-5));
    let l = normalize(scene.lightDir.xyz);

    let tc = paletteCoord(sqrt(sqrt(trap)) * 48.0, p.colorDensity, p.colorOffset);
    let trapColor = textureSampleLevel(paletteTex, paletteSampler, vec2f(tc, 0.5), 0.0).rgb;
    let albedo = mix(BASE_COLOR, trapColor, m.trapMix);

    var shadow = 1.0;
    if ((m.flags & FLAG_SHADOWS) != 0u) { shadow = softShadow(pos + nrm * 0.002, l); }
    var ao = 1.0;
    if ((m.flags & FLAG_AO) != 0u) { ao = ambientOcclusion(pos, nrm); }

    let diffuse = max(dot(nrm, l), 0.0) * shadow;
    let spec = pow(max(dot(nrm, normalize(l - rd)), 0.0), 48.0) * shadow * 0.5;
    let sky = (0.5 + 0.5 * nrm.y) * 0.35;
    var col = albedo * (sky * ao + 1.1 * diffuse) + spec;

    if ((m.flags & FLAG_REFLECT) != 0u) {
        let fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(nrm, -rd), 0.0), 5.0);
        col += background(reflect(rd, nrm)) * fresnel * ao * 0.8;
    }

    col = mix(col, bg, 1.0 - exp(-0.015 * t * t));
    return vec4f(pow(col, vec3f(0.9)), 1.0);
}
