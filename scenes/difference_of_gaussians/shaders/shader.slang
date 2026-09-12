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

static const float3x3 sob_v = float3x3(1, 2, 1, 0, 0, 0, -1, -2, -1);
static const float3x3 sob_h = float3x3(-1, 0, 1,
				       -2, 0, 2,
			               -1, 0, 1);

[shader("fragment")]
float4 main(VSOutput input) {
    Sampler2D tex = textures[NonUniformResourceIndex(0)];
    float2 tex_dim;
    tex.GetDimensions(tex_dim.x, tex_dim.y);
    float2 tex_size = float2(1.0f/tex_dim.x, 1.0f/tex_dim.y);

    float strength = 0.6f; // controls strength of blurs
    float scalar = 1.6f; // generally not changed
    float edge_sharpness = 20.0f; // controls edge sharpness
    float sum_threshold = 0.2f; // controls number of edges
    float contrast = 20.0f; // controls contrast between edges and image

    float sum = 0;
    float[5][5] colors;
    for(int i = 0; i < 5; i++) {
    	for(int j = 0; j < 5; j++) {
    		colors[i][j] = dot(tex.Sample(float2(input.uv.x + (i - 2) * tex_size.x, input.uv.y + (j - 2) * tex_size.y)).rgb, float3(0.299, 0.587, 0.114));
	}
    }

    // Next Step: get the eigen decomposition of the image's smoothened
    // structure tensor. I will be stopping this project for now, since
    // I haven't gotten to eigenvalues yet in LinAlgebra. Currently this
    // gets the sst by sobel-ing each pixel in a 3x3 kernal and gaussian
    // blurring them with parameter sigma_m (needs to be renamed).
    float3 sst = float3(0); // smoothened structure tenser
    float sigma_m = 0.6f;
    for(int i = 0; i < 3; i++) {
    for(int j = 0; j < 3; j++) {
    	float x_g = 0;
	float y_g = 0;
	for(uint ui = 0; ui < 3; ui++) {
	for(uint uj = 0; uj < 3; uj++) {
		float c = colors[i+ui][j+uj];
		x_g += c * sob_v[ui][uj];
		y_g += c * sob_h[ui][uj];
	}
	}
	float g = gaussian(i-1,j-1,sigma_m);
	sst.x += x_g * x_g * g;
	sst.y += x_g * y_g * g;
	sst.z += y_g * y_g * g;
    }
    }
    
    return float4(sst.x,sst.y,sst.z, 1.0f);
}
