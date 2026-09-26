struct VSInput {
    float3 pos;
    float3 normal;
    float2 uv;
};

Sampler2D textures[];

struct ShaderData {
    float3 color;
    float shader_interp_value;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float3 color;
    float2 uv;
    float shader_interp_value;
};

[shader("vertex")]
VSOutput main(VSInput input, uniform ShaderData* shader_data, uint instance_index : SV_VulkanInstanceID) {
    VSOutput output;
    output.uv = input.uv;
    output.pos = float4(input.pos.xyz, 1.0);
    output.color = shader_data->color;
    output.shader_interp_value = shader_data->shader_interp_value;
    return output;
}

[shader("fragment")]
float4 main(VSOutput input) {
    float3 color = textures[NonUniformResourceIndex(0)].Sample(input.uv).rgb;
    return float4(lerp(color * input.color, color, input.shader_interp_value), 1.0);
}
