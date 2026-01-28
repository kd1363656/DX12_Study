#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <cassert>
#ifdef _DEBUG
#include <iostream>
#endif

#include "wrl/client.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

using Microsoft::WRL::ComPtr;

// @brief コンソール画面にフォーマット付き文字列を表示
// @param format フォーマット(%d とか %f とかの)
// @remarks この関数はデバック用です。デバック時にしか動作しません
void DebugOutputFormatString(const char* format , ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format , valist);
	va_end(valist);
#endif
}

// 面倒だけど書かなければいけない関数
LRESULT WindowProcedure(HWND hwnd , UINT msg , WPARAM wparam , LPARAM lparam)
{
	// ウィンドウが破棄されたら呼ばれる
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);	// OSに対して「もうこのアプリは終わる」と伝える
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);	 // 既定の処理を行う
}

static constexpr uint32_t window_width  = 1280U;
static constexpr uint32_t window_height = 720U;

ComPtr<ID3D12Device> _dev = nullptr;
ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
ComPtr<IDXGISwapChain4> _swapChain = nullptr;

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));

	debugLayer->EnableDebugLayer(); // デバックレイヤーを有効化する
	debugLayer->Release         ();	// 有効化したらインターフェースを開放する
}

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif

	// メモリリークを知らせる
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// COM初期化
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		CoUninitialize();
		return 0;
	}

	DebugOutputFormatString("Show window test.");
	
	// ウィンドウクラスの生成 & 登録
	WNDCLASSEX w = {};
	w.cbSize = sizeof(w);
	w.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	w.lpszClassName = _T("DX12Sample");	      // アプリケーションクラス名(適当で良い)
	w.hInstance = GetModuleHandle(nullptr);	  // ハンドルの取得

	RegisterClassEx(&w); // アプリケーションクラス(ウィンドウクラスの指定を OS に伝える)

	RECT wrc = { 0, 0, window_width , window_height }; // ウィンドウサイズを決める

	// 関数を使ってウィンドウのサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName, // クラス名指定
		_T("DX12テスト"),		// タイトルバーの文字]
		WS_OVERLAPPEDWINDOW,		// タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT,			// 表示X座標は OS にお任せ
		CW_USEDEFAULT,			// 表示y座標は OS にお任せ
		wrc.right - wrc.left,	// ウィンドウ幅
		wrc.bottom - wrc.top,	// ウィンドウ高
		nullptr,				// 親ウィンドウハンドル
		nullptr,				// メニューハンドル
		w.hInstance,			// 呼び出しアプリケーションハンドル
	    nullptr);				// 追加パラメータ

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
	//デバッグレイヤーをオンに
	EnableDebugLayer();
#endif
	//DirectX12まわり初期化
	//フィーチャレベル列挙
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	HRESULT result = S_OK;

#ifdef _DEBUG
	result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG , IID_PPV_ARGS(&_dxgiFactory));
#else
	resutl = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif

	if (FAILED(result))
	{
		assert(false && "DXGIFactory の初期化失敗");
		return -1;
	}

	// アダプターの列挙用
	std::vector<IDXGIAdapter*> adapters;

	// ここに特定の名前を持つアダプターオブジェクトが入る
	IDXGIAdapter* tmpAdapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i , &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);	// アダプターの説明オブジェクト取得

		std::wstring strDesc = adesc.Description;

		// 探したいアダプターの名前を確認
		if(strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}

	// Direct3D デバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;

	for (auto lv : levels)
	{
		if (D3D12CreateDevice(tmpAdapter , lv, IID_PPV_ARGS(&_dev)) == S_OK)
		{
			featureLevel = lv;
			break;	// 生成可能なバージョンが見つかったらループを打ち切り
		}
	}

	if (!_dev)
	{
		assert(false && "D3D12Device の生成失敗");
		return -1;
	}

	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));

	if (FAILED(result))
	{
		assert(false && "D3D12CommandAllocator の生成失敗");
		return -1;
	}

	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&_cmdList));

	if (FAILED(result))
	{
		assert(false && "D3D12CommandList の生成失敗");
		return -1;
	}

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	
	// タイムアウトなし
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	// アダプターを一つしか使わない時は 0 でよい
	cmdQueueDesc.NodeMask = 0;

	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // プライオリティは特に指定なし

	// コマンドリストと合わせる
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	
	// キュー作成
	result = _dev->CreateCommandQueue(&cmdQueueDesc , IID_PPV_ARGS(&_cmdQueue));

	if (FAILED(result))
	{
		assert(false && "D3D12CommandQueue の生成失敗");
		return -1;
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = window_width;
	swapChainDesc.Height = window_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;

	// バックバッファーは伸び縮み可能
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

	// フリップ後は速やかに破棄
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// 特に指定なし
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// ウィンドウ<=>フルスクリーン切り替え可能
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGISwapChain1> swapChain1;
	result = _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1);	// 本来は QueryInterface などを用いて
																																		// IDXGISwapChain4* への返還チェックをするが、
																																		// ここではわかりやすさ重視のためキャストで対応
	swapChain1.As(&_swapChain);

	if (FAILED(result))
	{
		assert(false && "IDXGISwapChain4 の生成失敗");
		return -1;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューなので RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // 表裏の二つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // 特に指定なし

	ID3D12DescriptorHeap* rtvHeaps = nullptr;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	if (FAILED(result))
	{
		assert(false && "レンダーターゲット用ディスクリプタヒープの生成失敗");
		return -1;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = {};

	result = _swapChain->GetDesc(&swcDesc);

	if (FAILED(result))
	{
		assert(false && "スワップチェーンパラメータ取得失敗");
		return -1;
	}

	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));

		if (FAILED(result)) 
		{
			assert(false && "レンダーターゲット用ディスクリプタヒープの作製が出来ません");
			return -1;
		}

		_dev->CreateRenderTargetView(_backBuffers[idx] , nullptr , handle);

		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0U;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	ShowWindow(hwnd , SW_SHOW); // ウィンドウ表示

	if (FAILED(result))
	{
		assert(false && "フェンスの生成失敗");
		return -1;
	}

	MSG msg = {};
	while(true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// アプリケーションが終わるときにmessageが WM_QUIT になる
		if (msg.message == WM_QUIT) 
		{
			break;
		}

		auto bbIdx = _swapChain->GetCurrentBackBufferIndex();
		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;   // 特に指定なし
		BarrierDesc.Transition.pResource = _backBuffers[bbIdx]; // バックバッファーリソース
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 直線は PRESENT 状態
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 今からレンダーターゲット状態
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

		// 画面クリア
		float clearColor[] = { 1.0F , 1.0F , 0.0F , 1.0F }; // 黄色
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		// 前後だけ入れ替える
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		// 命令のクローズ
		_cmdList->Close();

		// コマンドリストの実行
		ID3D12CommandList* cmdLists[] = { _cmdList.Get() };
		_cmdQueue->ExecuteCommandLists(1 , cmdLists);

		_cmdQueue->Signal(_fence, ++_fenceVal);

		if (_fence->GetCompletedValue() != _fenceVal)
		{
			// イベントハンドルの取得
			auto event = CreateEvent(nullptr, false, false, nullptr);

			_fence->SetEventOnCompletion(_fenceVal, event);

			// イベントが発生するまで待ち続ける("INFINITE")
			WaitForSingleObject(event, INFINITE);

			// イベントハンドルを閉じる
			CloseHandle(event);
		}

		_cmdAllocator->Reset();	// キューをクリア
		_cmdList->Reset(_cmdAllocator.Get(), nullptr); // 再びコマンドリストをためる準備

		// フリップ
		_swapChain->Present(1 , 0);
	}

	_cmdQueue->Signal(_fence, ++_fenceVal);

	if (_fence->GetCompletedValue() != _fenceVal)
	{
		// イベントハンドルの取得
		auto event = CreateEvent(nullptr, false, false, nullptr);

		_fence->SetEventOnCompletion(_fenceVal, event);

		// イベントが発生するまで待ち続ける("INFINITE")
		WaitForSingleObject(event, INFINITE);

		// イベントハンドルを閉じる
		CloseHandle(event);
	}

	// もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);

	// COM解放
	CoUninitialize();

	return 0;
}