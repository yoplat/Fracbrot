// Heightfield terrain: a gridSize² mesh generated from vertex_index, displaced by
// the field computed in heightmap.wgsl. World xz spans [-1, 1].

struct Scene {
    viewProj: mat4x4f,
    invViewProj: mat4x4f,
    camPos: vec4f,
    lightDir: vec4f,   // towards the light
    heightScale: f32,
    gridSize: u32,
    interiorHeight: f32,
    heightCurve: f32,
};

@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var<uniform> scene: Scene;
@group(0) @binding(2) var heightTex: texture_2d<f32>;
@group(0) @binding(3) var paletteTex: texture_2d<f32>;
@group(0) @binding(4) var paletteSampler: sampler;

const INTERIOR_COLOR = vec3f(0.10, 0.10, 0.13);
const FOG_COLOR = vec3f(0.04, 0.045, 0.07);

fn sampleHeight(ij: vec2i) -> f32 {
    let last = i32(scene.gridSize) - 1;
    return textureLoad(heightTex, clamp(ij, vec2i(0), vec2i(last)), 0).r * scene.heightScale;
}

struct VsOut {
    @builtin(position) pos: vec4f,
    @location(0) world: vec3f,
    @location(1) normal: vec3f,
    @location(2) n: f32,
};

@vertex
fn vs(@builtin(vertex_index) vi: u32) -> VsOut {
    let ij = vec2i(i32(vi % scene.gridSize), i32(vi / scene.gridSize));
    let step = 2.0 / f32(scene.gridSize - 1u);
    let xz = vec2f(ij) * step - 1.0;

    let h = sampleHeight(ij);
    let dx = sampleHeight(ij + vec2i(1, 0)) - sampleHeight(ij - vec2i(1, 0));
    let dz = sampleHeight(ij + vec2i(0, 1)) - sampleHeight(ij - vec2i(0, 1));

    var o: VsOut;
    o.world = vec3f(xz.x, h, xz.y);
    o.normal = normalize(vec3f(-dx, 2.0 * step, -dz));
    o.n = textureLoad(heightTex, ij, 0).g;
    o.pos = scene.viewProj * vec4f(o.world, 1.0);
    return o;
}

@fragment
fn fs(in: VsOut) -> @location(0) vec4f {
    // n is interpolated, so blend towards the interior color across boundary triangles.
    var albedo = INTERIOR_COLOR;
    if (in.n >= 0.0) {
        let t = paletteCoord(in.n, p.colorDensity, p.colorOffset);
        albedo = textureSampleLevel(paletteTex, paletteSampler, vec2f(t, 0.5), 0.0).rgb;
    }

    let nrm = normalize(in.normal);
    let l = normalize(scene.lightDir.xyz);
    let v = normalize(scene.camPos.xyz - in.world);
    let diffuse = max(dot(nrm, l), 0.0);
    let spec = pow(max(dot(nrm, normalize(l + v)), 0.0), 48.0) * 0.25;
    let sky = 0.25 + 0.15 * nrm.y;
    var col = albedo * (sky + 0.85 * diffuse) + spec;

    let fog = 1.0 - exp(-0.08 * max(length(scene.camPos.xyz - in.world) - 1.5, 0.0));
    col = mix(col, FOG_COLOR, fog);
    return vec4f(col, 1.0);
}
