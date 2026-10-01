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

// e (epsilon) controls the treshold of the pass itself (low or high)
// p (phi) controls the contrast of the low-pass colors
float threshold(float x, float e, float p) {
	return (x >= e) ? 1.0f : 1.0f+tanh(p*(x-e));
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

    float strength = 0.6f; // controls strength of blurs
    float scalar = 1.6f; // generally not changed
    float edge_sharpness = 5.0f; // controls edge sharpness
    float sum_threshold = 0.2f; // controls number of edges
    float contrast = 20.0f; // controls contrast between edges and image

    float sum = 0;
    float norm = 0;
    for(int i = 0; i < 5; i++) {
    	for(int j = 0; j < 5; j++) {
    		float color = dot(tex.Sample(float2(input.uv.x + (i - 2) * tex_size.x, input.uv.y + (j - 2) * tex_size.y)).rgb, float3(0.299, 0.587, 0.114));
		float g1 = gaussian(i-2,j-2, strength);
		float g2 = gaussian(i-2, j-2, strength*scalar);
		float dg = ((1.0f + edge_sharpness) * g1 - edge_sharpness * g2);
		sum += color * dg;
		norm += dg;
	}
    }
    sum /= norm;
    sum = threshold(sum, sum_threshold, contrast);
    
    return float4(sum, sum, sum, 1.0f);
}
