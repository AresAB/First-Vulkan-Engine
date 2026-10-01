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
float gaussian2d(float x_dist, float y_dist, float s) {
	return exp(-((x_dist * x_dist)+(y_dist * y_dist))/(2.0*s*s))/(s*s*6.28);
}
// 2.51 ~= sqrt(2pi)
float gaussian(float x_dist, float s) {
	return exp(-(x_dist * x_dist)/(2.0f*s*s))/(s*2.51);
}

[shader("fragment")]
float4 main(VSOutput input) {
    Sampler2D tex = textures[NonUniformResourceIndex(0)];
    float2 tex_dim;
    tex.GetDimensions(tex_dim.x, tex_dim.y);
    float2 tex_size = float2(1.0f/tex_dim.x, 1.0f/tex_dim.y);

    float kernal_strength = 5.0f;
    float min_edge_amplitude = 0.1f;

    float3 sum = float3(0.0f,0.0f,0.0f);
    float norm = 0.0f;

    int kernal_size = kernal_strength;
    int off = kernal_size / 2;
    float gray_center = dot(tex.Sample(input.uv).rgb, float3(0.299, 0.587, 0.114));
    for(int i = -off; i < off; i++) {
    	for(int j = -off; j < off; j++) {
    		float3 color = tex.Sample(float2(input.uv.x + i * tex_size.x, input.uv.y + j * tex_size.y)).rgb;
		float gray = dot(color, float3(0.299, 0.587, 0.114));
		float g = (gaussian2d(i, j, kernal_strength) * gaussian(gray - gray_center, min_edge_amplitude));
		sum += color * g;
		norm += g;
	}
    }
    sum /= norm;
    
    return float4(sum, 1.0f);
    //return tex.Sample(input.uv);
}
