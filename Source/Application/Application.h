#pragma once

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