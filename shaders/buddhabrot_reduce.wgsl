// Per-channel maximum of the Buddhabrot histogram, used to normalize the display.
// Each workgroup reduces a strided slice in shared memory, then one atomicMax per channel.

@group(0) @binding(0) var<uniform> b: Buddha;
@group(0) @binding(1) var<storage, read> hist: array<u32>;
@group(0) @binding(2) var<storage, read_write> maxOut: array<atomic<u32>, 3>;

var<workgroup> partial: array<vec3u, 256>;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid: vec3u, @builtin(local_invocation_index) li: u32,
        @builtin(num_workgroups) groups: vec3u) {
    let count = b.histSize.x * b.histSize.y;
    let stride = groups.x * 256u;
    var m = vec3u(0u);
    for (var p = gid.x; p < count; p += stride) {
        let i = p * 3u;
        m = max(m, vec3u(hist[i], hist[i + 1u], hist[i + 2u]));
    }
    partial[li] = m;
    workgroupBarrier();
    for (var s = 128u; s > 0u; s >>= 1u) {
        if (li < s) { partial[li] = max(partial[li], partial[li + s]); }
        workgroupBarrier();
    }
    if (li == 0u) {
        atomicMax(&maxOut[0], partial[0].x);
        atomicMax(&maxOut[1], partial[0].y);
        atomicMax(&maxOut[2], partial[0].z);
    }
}
