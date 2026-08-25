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

static const float3x3 sob_v = float3x3(1, 2, 1, 0, 0, 0, -1, -2, -1);
static const float3x3 sob_h = float3x3(-1, 0, 1,
				       -2, 0, 2,
			               -1, 0, 1);

[shader("fragment")]
float4 main(VSOutput input) {
    Sampler2D tex = textures[NonUniformResourceIndex(0)];
    float3 color = tex.Sample(input.uv).rgb;
    float v_sum = 0;
    float h_sum = 0;
    float2 tex_dim;
    tex.GetDimensions(tex_dim.x, tex_dim.y);
    float2 tex_size = float2(1.0f/tex_dim.x, 1.0f/tex_dim.y);
    for(int i = 0; i < 3; i++) {
    	for(int j = 0; j < 3; j++) {
    		float color = dot(tex.Sample(float2(input.uv.x + (i - 1) * tex_size.x, input.uv.y + (j - 1) * tex_size.y)).rgb, float3(0.299, 0.587, 0.114));
		v_sum += color * sob_v[i][j];
		h_sum += color * sob_h[i][j];
	}
    }
    
    // magnitude approx: float mag = v_sum + h_sum;
    // magnitude accurate: float mag = sqrt(v_sum * v_sum + h_sum * h_sum);
    // angle: atan2(h_sum / x_sum)
    float mag = sqrt(v_sum * v_sum + h_sum * h_sum);
    return float4(mag, mag, mag, 1.0f);

    // might be fun to sobel over each color channel and make something from that
}
