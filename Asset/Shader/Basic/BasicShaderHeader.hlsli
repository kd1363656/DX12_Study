// 頂点シェーダー -> ピクセルシェーダーへのやり取りに使用する構造体
struct BasicType
{
    float4 svpos : SV_POSITION; // システム用頂点座標
    float2 uv : TEXCORRD; // UV値
};