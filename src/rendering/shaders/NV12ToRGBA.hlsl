// NV12 to RGBA Pixel Shader
// Converts NV12 (YUV420) format to RGBA for display

Texture2D<float> texY : register(t0);  // Y plane (luminance)
Texture2D<float2> texUV : register(t1); // UV plane (chrominance, interleaved)
SamplerState samplerState : register(s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

// YUV to RGB conversion matrix (BT.709)
static const float3x3 YUVtoRGB = float3x3(
    1.164f,  0.000f,  1.793f,
    1.164f, -0.213f, -0.533f,
    1.164f,  2.112f,  0.000f
);

float4 PSMain(PS_INPUT input) : SV_TARGET {
    // Sample Y and UV values
    float y = texY.Sample(samplerState, input.texCoord).r;
    float2 uv = texUV.Sample(samplerState, input.texCoord).rg;
    
    // Convert from [0,1] to YUV range
    float3 yuv = float3(
        (y - 0.0625f) * 1.164f,
        (uv.r - 0.5f),
        (uv.g - 0.5f)
    );
    
    // Convert YUV to RGB
    float3 rgb = mul(YUVtoRGB, yuv);
    
    // Clamp to [0,1] and return with full alpha
    return float4(saturate(rgb), 1.0f);
}
