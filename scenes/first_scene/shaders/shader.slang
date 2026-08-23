struct VSInput {
    float3 pos;
    float3 normal;
    float2 uv;
};

Sampler2D textures[];

struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4x4 model[3];
    float4 light_pos;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float3 normal;
    float2 uv;
    float3 factor;
    float3 light_vec;
    float3 view_vec;
    uint32_t instance_index;
};

[shader("vertex")]
VSOutput main(VSInput input, uniform ShaderData *shader_data, uint instance_index : SV_VulkanInstanceID) {
    VSOutput output;
    float4x4 model_mat = shader_data->model[instance_index];
    output.normal = mul((float3x3)mul(shader_data->view, model_mat), input.normal);
    output.uv = input.uv;
    output.pos = mul(shader_data->projection, mul(shader_data->view, mul(model_mat, float4(input.pos.xyz, 1.0))));
    output.factor = 3.0f;
    output.instance_index = instance_index;
    // Calculate view vectors required for lighting
    float4 frag_pos = mul(mul(shader_data->view, model_mat), float4(input.pos.xyz, 1.0));
    output.light_vec = shader_data->light_pos.xyz - frag_pos.xyz;
    output.view_vec = -frag_pos.xyz;
    return output;
}

[shader("fragment")]
float4 main(VSOutput input) {
    // Phong lighting
    float3 N = normalize(input.normal);
    float3 L = normalize(input.light_vec);
    float3 V = normalize(input.view_vec);
    float3 R = reflect(-L, N);
    float3 diffuse = max(dot(N, L), 0.0025);
    float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;
    // Sample from texture
    float3 color = textures[NonUniformResourceIndex(input.instance_index)].Sample(input.uv).rgb * input.factor;
    return float4(diffuse * color.rgb + specular, 1.0);
}
