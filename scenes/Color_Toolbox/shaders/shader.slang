struct VSInput {
    float3 pos;
    float3 normal;
    float2 uv;
};

Sampler2D textures[];

struct ShaderData {
    float3 color_filter;
    float shader_interp_value;
    float exposure;
    float temperature;
    float tint;
    float contrast;
    float brightness;
    float saturation;
    float gamma;
    int tone_map_option;
    int is_whitebalance;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv;
    float3 color_filter;
    float shader_interp_value;
    float exposure;
    float temperature;
    float tint;
    float contrast;
    float brightness;
    float saturation;
    float gamma;
    int tone_map_option;
    int is_whitebalance;
};

[shader("vertex")]
VSOutput main(VSInput input, uniform ShaderData* shader_data, uint instance_index : SV_VulkanInstanceID) {
    VSOutput output;
    output.uv = input.uv;
    output.pos = float4(input.pos.xyz, 1.0);
    output.color_filter = shader_data->color_filter;
    output.shader_interp_value = shader_data->shader_interp_value;
    output.exposure = shader_data->exposure;
    output.temperature = shader_data->temperature;
    output.tint = shader_data->tint;
    output.contrast = shader_data->contrast;
    output.brightness = shader_data->brightness;
    output.saturation = shader_data->saturation;
    output.gamma = shader_data->gamma;
    output.tone_map_option = shader_data->tone_map_option;
    output.is_whitebalance = shader_data->is_whitebalance;
    return output;
}

float3 Narkowicz_ACES(float3 hdr) {
    return clamp((hdr * (2.51 * hdr + 0.03)) / (hdr * (2.43 * hdr + 0.59) + 0.14), 0., 1.);
}

float3 Tumblin_Rushmeier(float3 hdr, float3 avg) {
    float Lin = dot(float3(0.2989, 0.589, 0.114), hdr);
    float Lavg = dot(float3(0.2989, 0.589, 0.114), avg);
    float _Ldmax = 1.; //luminance max (usually just 1)
    float _Cmax = 1000.; //contrast max

    float logLrw = log10(Lavg) + 0.84;
    float alphaRw = 0.4 * logLrw + 2.92;
    float betaRw = -0.4 * logLrw * logLrw - 2.584 * logLrw + 2.0208;
    float Lwd = _Ldmax / sqrt(_Cmax);
    float logLd = log10(Lwd) + 0.84;
    float alphaD = 0.4 * logLd + 2.92;
    float betaD = -0.4 * logLd * logLd - 2.584 * logLd + 2.0208;
    float Lout = pow(Lin, alphaRw / alphaD) / _Ldmax * pow(10., (betaRw / betaD) / alphaD) - (1.0 / _Cmax);

    return clamp(hdr / Lin * Lout, 0., 1.);
}

float3 WhiteBalance(float3 col, float te, float ti) {
    float t1 = te * 10.0f / 6.0f;
    float t2 = ti * 10.0f / 6.0f;

    float x = 0.31271f - t1 * (t1 < 0.0f ? 0.1f : 0.05f);
    float standIlluminantY = 2.87f * x - 3.0f * x * x - 0.27509507f;
    float y = standIlluminantY + t2 * 0.05f;

    float3 w1 = float3(0.949237f, 1.03542f, 1.08728f);

    float Y = 1.0f;
    float X = Y * x / y;
    float Z = Y * (1.0f - x - y) / y;
    float L = 0.7328f * X + 0.4296f * Y - 0.1624f * Z;
    float M = -0.7036f * X + 1.6975f * Y + 0.0061f * Z;
    float S = 0.003f * X + 0.0136f * Y + 0.9834f * Z;
    float3 w2 = float3(L, M, S);

    float3 balance = float3(w1.x / w2.x, w1.y / w2.y, w1.z / w2.z);

    float3x3 LIN_2_LMS_MAT;
    LIN_2_LMS_MAT[0] = float3(3.90405e-1, 7.08416e-2, 2.31082e-2);
    LIN_2_LMS_MAT[1] = float3(5.49941e-1, 9.63172e-1, 1.28021e-1);
    LIN_2_LMS_MAT[2] = float3(8.92632e-3, 1.35775e-3, 9.36245e-1);

    float3x3 LMS_2_LIN_MAT;
    LMS_2_LIN_MAT[0] = float3(2.85847e+0, -2.10182e-1, -4.1812e-2);
    LMS_2_LIN_MAT[1] = float3(-1.62879e+0, 1.1582e+0, -1.18169e-1);
    LMS_2_LIN_MAT[2] = float3(-2.4891e-2, 3.24281e-4, 1.06867e+0);

    float3 lms = mul(LIN_2_LMS_MAT, col);
    lms *= balance;
    return mul(LMS_2_LIN_MAT, lms);
}

[shader("fragment")]
float4 main(VSOutput input) {
    float3 tex = textures[NonUniformResourceIndex(0)].Sample(input.uv).rgb;

    float3 color = max(tex * input.exposure, 0.0f);
    if(input.is_whitebalance == 1) {
    	color = WhiteBalance(color, input.temperature, input.tint);
    }

    color = max(input.contrast * (color - 0.5f) + 0.5f + input.brightness, 0.0f);

    color = max(color * input.color_filter, 0.0f);

    color = max(lerp(float3(dot(color, float3(0.2989f, 0.589f, 0.114f))), color, input.saturation), 0.0f);

    // tone mapping
    if(input.tone_map_option == 1) {
    	color = Narkowicz_ACES(color);
    }
    else if(input.tone_map_option == 2) color = Tumblin_Rushmeier(color, textures[NonUniformResourceIndex(0)].SampleLevel(input.uv, 10.0f).rgb);

    color.x = pow(color.x, input.gamma);
    color.y = pow(color.y, input.gamma);
    color.z = pow(color.z, input.gamma);

    return float4(lerp(color, tex, input.shader_interp_value), 1.0f);
}
