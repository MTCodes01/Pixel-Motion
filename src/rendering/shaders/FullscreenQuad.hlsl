// Fullscreen Quad Vertex Shader

struct VS_INPUT {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PS_INPUT VSMain(VS_INPUT input) {
    PS_INPUT output;
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;
    return output;
}
