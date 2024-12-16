// const positions = mat3x2(
//     0.0, -0.5,
//     0.5, 0.5,
//     -0.5, 0.5
// );

// const colors = mat3x3(
//     1.0, 0.0, 0.0,
//     0.0, 1.0, 0.0,
//     0.0, 0.0, 1.0
// );

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec3<f32>,
}

// @vertex
// fn vertex_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
//     var result: VertexOutput;
//     result.position = vec4f(positions[vertex_index], 0.0, 1.0);
//     result.color = colors[vertex_index];
//     return result;
// }

fn rng(p: vec2f) -> f32 {
    let K1 = vec2f(23.14069263277926, 2.665144142690225);
    return fract(cos(dot(p, K1)) * 12345.6789);
}

fn normalize(x: f32) -> f32 {
    return (x + 1) / 2;
}

@fragment
fn fragment_main(input: VertexOutput) -> @location(0) vec4f {
    var output = vec4f(input.color, 1.0);

    let lightness = dot(output.rgb, vec3f(1, 1, 1)) / 3;
    let noise = normalize(rng(input.position.xy));

    output = vec4f(output.rgb - 0.5 * noise * lightness, output.w);
    output = pow(output, vec4f(0.8));

    if u32(input.position.x) % 5 == 0 {
        output = vec4f(output.rgb / 1.5, 1.0);
    }

    return output;
}