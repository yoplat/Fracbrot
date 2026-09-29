// 2D overlay: the orbit z0, z1, ... of one point, drawn as a line strip.
// Vertices are complex offsets from the view center (computed in double on the CPU).

struct Orbit {
    scale: f32,   // half-height of the view in complex units
    aspect: f32,  // width / height
    _pad: vec2f,
    color: vec4f,
};

@group(0) @binding(0) var<uniform> o: Orbit;

@vertex
fn vs(@location(0) offset: vec2f) -> @builtin(position) vec4f {
    let d = offset / o.scale;
    return vec4f(d.x / o.aspect, d.y, 0.0, 1.0);
}

@fragment
fn fs() -> @location(0) vec4f {
    return o.color;
}
