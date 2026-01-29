Texture2D<float4> tex : register(t0); // 0番スロットに設定されたテクスチャ
SamplerState smp : register(s0); // 0番スロットに設定されたサンプラー

struct Output
{
    float4 svpos : SV_POSITION; // システム用頂点座標
    float2 uv : TEXCORRD; // uv 値
};