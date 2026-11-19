// Helper passes for the HW GS renderer (gs_hw.cpp): textured blits between targets and solid fills (partial clears).
// Both draw one quad from the vertex id (triangle strip of 4) inside `rect` (NDC: left, top, right, bottom).

cbuffer Quad : register(b0)
{
    float4 rect;
    float4 uvRect;   // blit: source texture coordinates (0..1) at the left/top and right/bottom edges
    float4 color;    // fill: colour 0..1
    float4 params;   // x: fill depth 0..1, y: blit writes alpha 1 (the displayed picture), z: exposure (gsHwSetExposure;
                     // 0 or 1: none)
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut vs_main(uint id : SV_VertexID)
{
    const float2 c = float2(id & 1, id >> 1);
    VSOut o;
    o.pos = float4(lerp(rect.x, rect.z, c.x), lerp(rect.y, rect.w, c.y), 0.0, 1.0);
    o.uv = lerp(uvRect.xy, uvRect.zw, c);
    return o;
}

Texture2D<float4> source : register(t0);
SamplerState sourceSampler : register(s0);

// [gta-recomp] gsHwSetExposure: exposure e as a gain in linear light (gamma 2.2), the whole gain up to the knee (half the
// light). Above 1 the rest rolls off along a shoulder that takes the gained white (e) to exactly 1, its slope 1 at the
// knee: s u / (1 + (s - 1) u), u from the knee to e, s = (e - knee) / (1 - knee). So white stays white and nothing
// clips. Below 1 a plain gain.
float3 exposed(float3 c, float e)
{
    const float knee = 0.5;
    const float3 x = pow(saturate(c), 2.2) * e;
    float3 y = x;
    if (e > 1.0)
    {
        const float s = (e - knee) / (1.0 - knee);
        const float3 u = saturate((x - knee) / (e - knee));
        y = x > knee ? knee + (1.0 - knee) * (s * u / (1.0 + (s - 1.0) * u)) : x;
    }
    return pow(saturate(y), 1.0 / 2.2);
}

float4 ps_blit(VSOut i) : SV_Target0
{
    float4 t = source.Sample(sourceSampler, i.uv);
    if (params.z > 0.0 && params.z != 1.0) // the displayed picture only (gs_hw.cpp composeDisplay)
        t.rgb = exposed(t.rgb, params.z);
    return float4(t.rgb, params.y > 0.5 ? 1.0 : t.a);
}

float4 ps_fill_color(VSOut i) : SV_Target0
{
    return color;
}

float ps_fill_depth(VSOut i) : SV_Depth
{
    return params.x;
}
