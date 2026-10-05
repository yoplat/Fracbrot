// Logistic-map view: the Mandelbrot set on the ground plane (world x = Re c,
// world z = -Im c) with the real-axis orbit values standing up along world y.

struct Scene {
    viewProj: mat4x4f,
    invViewProj: mat4x4f,
    camPos: vec4f,
    lightDir: vec4f,
    _pad: vec4u,
};

struct Bif {
    cMin: f32,
    cMax: f32,
    columns: u32,
    samples: u32,
    warmup: u32,
    pointSize: f32,     // pixels
    brightness: f32,
    highlightC: f32,
    yScale: f32,
    planeDim: f32,
    resolution: vec2f,
};

@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var<uniform> scene: Scene;
@group(0) @binding(2) var<uniform> b: Bif;
@group(0) @binding(3) var paletteTex: texture_2d<f32>;
@group(0) @binding(4) var paletteSampler: sampler;
@group(0) @binding(5) var<storage, read> points: array<vec2f>;

const PLANE_MIN = vec2f(-2.25, -1.5);
const PLANE_MAX = vec2f(0.75, 1.5);

// ---- Mandelbrot plane ----

struct PlaneOut {
    @builtin(position) pos: vec4f,
    @location(0) xz: vec2f,
};

@vertex
fn vsPlane(@builtin(vertex_index) vi: u32) -> PlaneOut {
    var corners = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
                                  vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let xz = mix(PLANE_MIN, PLANE_MAX, corners[vi]);
    var o: PlaneOut;
    o.pos = scene.viewProj * vec4f(xz.x, 0.0, xz.y, 1.0);
    o.xz = xz;
    return o;
}

@fragment
fn fsPlane(in: PlaneOut) -> @location(0) vec4f {
    let axisWidth = fwidth(in.xz.y) * 1.5;
    let markerWidth = fwidth(in.xz.x) * 1.5;
    let c = vec2f(in.xz.x, -in.xz.y);
    let n = mandelbrot(c, p.maxIter, (p.flags & FLAG_SMOOTH) != 0u);

    var col = vec3f(0.0);
    if (n >= 0.0) {
        let t = paletteCoord(n, p.colorDensity, p.colorOffset);
        col = textureSampleLevel(paletteTex, paletteSampler, vec2f(t, 0.5), 0.0).rgb;
    }
    col *= b.planeDim;

    // Mark the real axis the diagram stands on.
    let onAxis = 1.0 - smoothstep(0.0, axisWidth, abs(in.xz.y));
    col = mix(col, vec3f(0.9), onAxis * 0.6 * step(b.cMin, c.x) * step(c.x, b.cMax));

    // Line across the plane at the highlighted c.
    let onMarker = 1.0 - smoothstep(0.0, markerWidth, abs(c.x - b.highlightC));
    col = mix(col, vec3f(1.0, 0.55, 0.15), onMarker * 0.8);
    return vec4f(col, 1.0);
}

// ---- Bifurcation points (instanced screen-space quads) ----

struct PointOut {
    @builtin(position) pos: vec4f,
    @location(0) corner: vec2f,
    @location(1) highlight: f32,
};

@vertex
fn vsPoint(@builtin(vertex_index) vi: u32, @builtin(instance_index) ii: u32) -> PointOut {
    var corners = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(-1.0, 1.0),
                                  vec2f(-1.0, 1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0));
    let pt = points[ii];
    let corner = corners[vi];
    let columnWidth = (b.cMax - b.cMin) / f32(b.columns);
    let highlight = select(0.0, 1.0, abs(pt.x - b.highlightC) < columnWidth * 0.5);
    let size = b.pointSize * (1.0 + 2.0 * highlight);

    var clip = scene.viewProj * vec4f(pt.x, pt.y * b.yScale, 0.0, 1.0);
    clip = vec4f(clip.xy + corner * size / b.resolution * clip.w, clip.zw);

    var o: PointOut;
    o.pos = clip;
    o.corner = corner;
    o.highlight = highlight;
    return o;
}

@fragment
fn fsPoint(in: PointOut) -> @location(0) vec4f {
    let falloff = max(1.0 - dot(in.corner, in.corner), 0.0);
    let color = mix(vec3f(0.75, 0.88, 1.0), vec3f(1.0, 0.55, 0.15), in.highlight);
    let alpha = b.brightness * falloff * (1.0 + 8.0 * in.highlight);
    return vec4f(color, min(alpha, 1.0));
}
