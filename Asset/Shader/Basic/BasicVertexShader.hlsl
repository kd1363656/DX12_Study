#include "BasicShaderHeader.hlsli"
BasicType BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
    BasicType output; // ピクセルシェーダーに渡す値
    output.svpos = pos;
    output.uv = uv;
	return output;
}