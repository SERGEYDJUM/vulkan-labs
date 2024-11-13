const positions = mat3x2(
    0.0, -0.5,
    0.5, 0.5,
    -0.5, 0.5
);

const colors = mat3x3(
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0
);

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec3<f32>,
}

@vertex
fn vertex_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    var result: VertexOutput;
    result.position = vec4f(positions[vertex_index], 0.0, 1.0);
    result.color = colors[vertex_index];

    return result;
}

@fragment
fn fragment_main(@location(0) color: vec3f) -> @location(0) vec4f {
    return vec4f(color, 1.0);
}