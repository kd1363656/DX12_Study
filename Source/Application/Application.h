#pragma once

// 頂点データ構造体
struct Vertex
{
	DirectX::XMFLOAT3 pos; // xyz座標
	DirectX::XMFLOAT2 uv;  // uv座標
};

struct TexRGBA
{
	unsigned char R, G, B, A;
};

class Application
{
public:

	static Application& GetInstance()
	{
		static Application instance = {};
		return instance;
	}

	static constexpr std::uint32_t window_width = 1280U;
	static constexpr std::uint32_t window_height = 720U;

	void Execute();

	void EnableDebugLayer();

private:

	Application () = default;
	~Application() = default;
};