struct VSInput {
    float3 pos;
    float3 normal;
    float2 uv;
};

Sampler2D textures[];

struct ShaderData {
    float3 color;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float3 color;
    float2 uv;
};

[shader("vertex")]
VSOutput main(VSInput input, uniform ShaderData* shader_data, uint instance_index : SV_VulkanInstanceID) {
    VSOutput output;
    output.uv = input.uv;
    output.pos = float4(input.pos.xyz, 1.0);
    output.color = shader_data->color;
    return output;
}

// 6.28 ~= 2pi
// dist = pos - center_pos, s = strength
// exp = e^(x)
float gaussian(float x_dist, float y_dist, float s) {
	return exp(-((x_dist * x_dist)+(y_dist * y_dist))/(2.0*s*s))/(s*s*6.28);
}

[shader("fragment")]
float4 main(VSOutput input) {
    Sampler2D tex = textures[NonUniformResourceIndex(0)];
    float2 tex_dim;
    tex.GetDimensions(tex_dim.x, tex_dim.y);
    float2 tex_size = float2(1.0f/tex_dim.x, 1.0f/tex_dim.y);
    float sum = 0;
    float strength = 0.6;
    float scalar = 1.5;
    for(int i = 0; i < 5; i++) {
    	for(int j = 0; j < 5; j++) {
    		float color = dot(tex.Sample(float2(input.uv.x + (i - 2) * tex_size.x, input.uv.y + (j - 2) * tex_size.y)).rgb, float3(0.299, 0.587, 0.114));
		sum += color * (gaussian(i-2, j-2, strength*scalar) - gaussian(i-2,j-2, strength));
	}
    }
    sum *= 10;
    
    return float4(sum, sum, sum, 1.0f);
}
