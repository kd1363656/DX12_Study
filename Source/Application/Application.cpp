#include "Application.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	Application::GetInstance().Execute();

	return 0;
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

void Application::Execute()
{
	{
		ComPtr<ID3D12Device> _dev = nullptr;
		ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
		ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
		ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
		ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
		ComPtr<IDXGISwapChain4> _swapChain = nullptr;

		// メモリリークを知らせる
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

		// COM初期化
		if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
		{
			CoUninitialize();
			return;
		}

		// ウィンドウクラスの生成 & 登録
		WNDCLASSEX w = {};
		w.cbSize = sizeof(w);
		w.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
		w.lpszClassName = _T("DX12Test");	      // アプリケーションクラス名(適当で良い)
		w.hInstance = GetModuleHandle(nullptr);	  // ハンドルの取得
		RegisterClassEx(&w); // アプリケーションクラス(ウィンドウクラスの指定を OS に伝える)

		RECT wrc = { 0, 0, window_width , window_height }; // ウィンドウサイズを決める

		// 関数を使ってウィンドウのサイズを補正する
		AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

		// ウィンドウオブジェクトの生成
		HWND hwnd = CreateWindow(w.lpszClassName, // クラス名指定
			_T("DX12 単純ポリゴンテスト"),		// タイトルバーの文字]
			WS_OVERLAPPEDWINDOW,		// タイトルバーと境界線があるウィンドウ
			CW_USEDEFAULT,			// 表示X座標は OS にお任せ
			CW_USEDEFAULT,			// 表示y座標は OS にお任せ
			wrc.right - wrc.left,	// ウィンドウ幅
			wrc.bottom - wrc.top,	// ウィンドウ高
			nullptr,				// 親ウィンドウハンドル
			nullptr,				// メニューハンドル
			w.hInstance,			// 呼び出しアプリケーションハンドル
		    nullptr);				// 追加パラメータ

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

		auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));

		if (FAILED(result))
		{
			assert(false && "DXGIFactory の初期化失敗");
			return;
		}

		// アダプターの列挙用
		std::vector<ComPtr<IDXGIAdapter>> adapters;

		// ここに特定の名前を持つアダプターオブジェクトが入る
		ComPtr<IDXGIAdapter> tmpAdapter = nullptr;

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
			if (D3D12CreateDevice(tmpAdapter.Get(), lv, IID_PPV_ARGS(&_dev)) == S_OK)
			{
				featureLevel = lv;
				break;	// 生成可能なバージョンが見つかったらループを打ち切り
			}
		}

		if (!_dev)
		{
			assert(false && "D3D12Device の生成失敗");
			return;
		}

		result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));

		if (FAILED(result))
		{
			assert(false && "D3D12CommandAllocator の生成失敗");
			return;
		}

		result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&_cmdList));

		if (FAILED(result))
		{
			assert(false && "D3D12CommandList の生成失敗");
			return;
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
			return;
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
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // バックバッファーは伸び縮み可能
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後は速やかに破棄
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // 特に指定なし
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // ウィンドウ<=>フルスクリーン切り替え可能

		ComPtr<IDXGISwapChain1> swapChain1;
		result = _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1);	// 本来は QueryInterface などを用いて
																																			// IDXGISwapChain4* への返還チェックをするが、
																																			// ここではわかりやすさ重視のためキャストで対応
		swapChain1.As(&_swapChain);

		if (FAILED(result))
		{
			assert(false && "IDXGISwapChain4 の生成失敗");
			return;
		}

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューなので RTV
		heapDesc.NodeMask = 0;
		heapDesc.NumDescriptors = 2; // 表裏の二つ
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // 特に指定なし

		ComPtr<ID3D12DescriptorHeap> rtvHeaps = nullptr;

		result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

		if (FAILED(result))
		{
			assert(false && "レンダーターゲット用ディスクリプタヒープの生成失敗");
			return;
		}

		DXGI_SWAP_CHAIN_DESC swcDesc = {};

		result = _swapChain->GetDesc(&swcDesc);

		if (FAILED(result))
		{
			assert(false && "スワップチェーンパラメータ取得失敗");
			return;
		}

		std::vector<ComPtr<ID3D12Resource>> _backBuffers(swcDesc.BufferCount);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
		{
			result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));

			if (FAILED(result)) 
			{
				assert(false && "レンダーターゲット用ディスクリプタヒープの作製が出来ません");
				return;
			}

			_dev->CreateRenderTargetView(_backBuffers[idx].Get(), nullptr, handle);

			handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		ComPtr<ID3D12Fence> _fence = nullptr;
		UINT64 _fenceVal = 0U;
		result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

		if (FAILED(result))
		{
			assert(false && "フェンスの生成失敗");
			return;
		}

		ShowWindow(hwnd , SW_SHOW); // ウィンドウ表示
		
		Vertex vertices[] = 
		{
			{ { -0.4F, -0.7F, 0.0F }, { 0.0F, 1.0F} },	// 左下
			{ { -0.4F,  0.7F, 0.0F }, { 0.0F, 0.0F} },  // 左上
			{ {  0.4F, -0.7F, 0.0F }, { 1.0F, 1.0F} },	// 右下
			{ {  0.4F,  0.7F, 0.0F }, { 1.0F, 0.0F} }	// 右上
		};

		D3D12_HEAP_PROPERTIES heapProp = {};

		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resDesc = {};

		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resDesc.Width = sizeof(vertices); // 頂点情報が入るだけのサイズ
		resDesc.Height = 1;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Format = DXGI_FORMAT_UNKNOWN;
		resDesc.SampleDesc.Count = 1;
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		// UPLOAD ( CPU GPU どちらからもアクセス可能)
		ComPtr<ID3D12Resource> vertBuff = nullptr;
		result = _dev->CreateCommittedResource(
			&heapProp, 
			D3D12_HEAP_FLAG_NONE, 
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, 
			nullptr,
			IID_PPV_ARGS(&vertBuff));

		if (FAILED(result))
		{
			assert(false && "頂点バッファーの生成に失敗");
			return;
		}

		Vertex* vertMap = nullptr;
		result = vertBuff->Map(0, nullptr, (void**)&vertMap);

		if (FAILED(result))
		{
			assert(false && "頂点バッファーの生成に失敗");
			return;
		}

		std::copy(std::begin(vertices), std::end(vertices), vertMap);

		vertBuff->Unmap(0, nullptr);

		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = vertBuff->GetGPUVirtualAddress(); // バッファーの仮想アドレス
		vbView.SizeInBytes    = sizeof(vertices);    // 全バイト数
		vbView.StrideInBytes  = sizeof(vertices[0]); // 1頂点辺りのバイト数

		unsigned short indices[] = { 0, 1, 2, 2, 1, 3 };

		ComPtr<ID3D12Resource> idxBuff = nullptr;

		// 設定は、バッファーのサイズ以外、頂点バッファーの設定を使いまわしてよい
		resDesc.Width = sizeof(indices);

		result = _dev->CreateCommittedResource(
			&heapProp, 
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&idxBuff));

		// 作ったバッファーにインデックスデータをコピー
		unsigned short* mappedIdx = nullptr;
		result = idxBuff->Map(0, nullptr, (void**)&mappedIdx);

		if (FAILED(result))
		{
			assert(false && "インデックスバッファーのマップに失敗");
			return;
		}

		std::copy(std::begin(indices), std::end(indices), mappedIdx);
		idxBuff->Unmap(0, nullptr);

		// インデックスバッファービューの作成
		D3D12_INDEX_BUFFER_VIEW ibView = {};

		ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
		ibView.Format = DXGI_FORMAT_R16_UINT;
		ibView.SizeInBytes = sizeof(indices);

		ComPtr<ID3DBlob> _vsBlob    = nullptr;
		ComPtr<ID3DBlob> _psBlob    = nullptr;
		ComPtr<ID3DBlob> _errorBlob = nullptr;

		result = D3DCompileFromFile(
			L"Asset/Shader/Basic/BasicVertexShader.hlsl", // シェーダー名
			nullptr,				   // "define"はなし		   
			D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
			"BasicVS", "vs_5_0", // 関数は"BasicVS、対象シェーダーはvs_5_0"
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0,
			&_vsBlob, &_errorBlob);	// エラー時は errorBlob にメッセージが入る

		if (FAILED(result))
		{
			if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				assert(false && "頂点シェーダーのファイルが見つかりません");
				return;
			}
			else
			{
				std::string errstr;
				errstr.resize(_errorBlob->GetBufferSize());

				std::copy_n((char*)_errorBlob->GetBufferPointer(),
					_errorBlob->GetBufferSize(),
					errstr.begin());

				errstr += "\n";

				OutputDebugStringA(errstr.c_str());
			}
		}
		result = D3DCompileFromFile(
			L"Asset/Shader/Basic/BasicPixelShader.hlsl", // シェーダー名
			nullptr,				   // "define"はなし		   
			D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
			"BasicPS", "ps_5_0", // 関数は"BasicPS、対象シェーダーはps_5_0"
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0,
			&_psBlob, &_errorBlob);	// エラー時は errorBlob にメッセージが入る

		if (FAILED(result))
		{
			if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				assert(false && "ピクセルシェーダーのファイルが見つかりません");
				return;
			}
			else
			{
				std::string errstr;
				errstr.resize(_errorBlob->GetBufferSize());

				std::copy_n((char*)_errorBlob->GetBufferPointer(),
					_errorBlob->GetBufferSize(),
					errstr.begin());

				errstr += "\n";

				OutputDebugStringA(errstr.c_str());
			}
		}

		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{
				"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				D3D12_APPEND_ALIGNED_ELEMENT,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
			},
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

		gpipeline.pRootSignature = nullptr;	// 後で設定する

		gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
		gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
		gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
		gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();

		/// デフォルトのサンプルマスクを表す定数 (0xffffffff)
		gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		gpipeline.BlendState.AlphaToCoverageEnable  = false;
		gpipeline.BlendState.IndependentBlendEnable = false;

		D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

		// ひとまず加算や乗算やαブレンディングは使用しない
		renderTargetBlendDesc.BlendEnable = false;
		renderTargetBlendDesc.RenderTargetWriteMask =D3D12_COLOR_WRITE_ENABLE_ALL;

		// ひとまず論理演算は使用しない
		renderTargetBlendDesc.LogicOpEnable = false;

		gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

		gpipeline.RasterizerState.MultisampleEnable = false; // まだアンチエイリアスは使わないため false
		gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;	// カリングしない
		gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
		gpipeline.RasterizerState.DepthClipEnable = true; // 深度方向のクリッピングは有効

		// 残り
		gpipeline.RasterizerState.FrontCounterClockwise = false;
		gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		gpipeline.RasterizerState.AntialiasedLineEnable = false;
		gpipeline.RasterizerState.ForcedSampleCount = 0;
		gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		gpipeline.DepthStencilState.DepthEnable = false;
		gpipeline.DepthStencilState.StencilEnable = false;

		gpipeline.InputLayout.pInputElementDescs = inputLayout;    // 入力アセンブラの設定// レイアウト先頭アドレス
		gpipeline.InputLayout.NumElements = _countof(inputLayout); // レイアウト配列の要素数

		gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // カットなし
		gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形で構成

		gpipeline.NumRenderTargets = 1; // 今は一つのみ
		gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0 ~ 1 に正規化されたRGBA

		gpipeline.SampleDesc.Count = 1;	// マルチサンプルしない
		gpipeline.SampleDesc.Quality = 0; // クオリティは最低

		ComPtr<ID3D12RootSignature> rootsignature = nullptr;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> rootSigBlob = nullptr;
		result = D3D12SerializeRootSignature(
			&rootSignatureDesc,	// ルートシグネチャ設定
			D3D_ROOT_SIGNATURE_VERSION_1_0, // ルートシグネチャバージョン
			&rootSigBlob,	// シェーダーを作った時と同じ
			&_errorBlob);	// エラー処理も同じ

		if (FAILED(result))
		{
			assert(false && "ルートシグネチャのシリアライズに失敗");
			return;
		}

		result = _dev->CreateRootSignature(
			0,	// nodeMask 0 でよい
			rootSigBlob->GetBufferPointer(), // シェーダーの時と同様
			rootSigBlob->GetBufferSize(),	 // シェーダーの時と同様
			IID_PPV_ARGS(&rootsignature));

		if (FAILED(result))
		{
			assert(false && "ルートシグネチャの作製に失敗");
			return;
		}

		gpipeline.pRootSignature = rootsignature.Get();

		ComPtr<ID3D12PipelineState> _pipelinestate = nullptr;

		result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

		if (FAILED(result))
		{
			if (result == E_INVALIDARG)
			{
				assert(false && "ルートシグネチャが設定されていないのです");
				return;
			}

			assert(false && "グラフィックスパイプラインステートの作成に失敗");
			return;
		}

		D3D12_VIEWPORT viewport = {};
		viewport.Width = window_width;	 // 出力先の幅(ピクセル数)
		viewport.Height = window_height; // 出力先の高さ(ピクセル数)
		viewport.TopLeftX = 0;    // 出力先の左上座標 X
		viewport.TopLeftY = 0;    // 出力先の左上座標 Y
		viewport.MaxDepth = 1.0F; // 深度最大値
		viewport.MinDepth = 0.0F; // 深度最小値

		D3D12_RECT scissorrect = {};
		scissorrect.top = 0; // 切り抜き上座標
		scissorrect.left = 0; // 切り抜き左座標
		scissorrect.right = scissorrect.left + window_width; // 切り抜き右座標
		scissorrect.bottom = scissorrect.top + window_height; // 切り抜き下座標

		MSG msg = {};
		unsigned int frame = 0;
		while(true)
		{
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			// アプリケーションが終わるときにmessageが WM_QUIT になる
			if (msg.message == WM_QUIT || GetAsyncKeyState(VK_ESCAPE))
			{
				break;
			}

			auto bbIdx = _swapChain->GetCurrentBackBufferIndex();
			D3D12_RESOURCE_BARRIER BarrierDesc = {};
			
			BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;   // 特に指定なし
			BarrierDesc.Transition.pResource = _backBuffers[bbIdx].Get(); // バックバッファーリソース
			BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 直線は PRESENT 状態
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 今からレンダーターゲット状態
			
			_cmdList->ResourceBarrier(1, &BarrierDesc);

			_cmdList->SetPipelineState(_pipelinestate.Get());

			auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
			rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

			// 画面クリア
			float r, g, b;
			r = (float)(0xff & frame >> 16) / 255.0F;
			g = (float)(0xff & frame >> 8) / 255.0F;
			b = (float)(0xff & frame >> 0) / 255.0F;
			float clearColor[] = { r , g , b , 1.0F }; // 黄色
			_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
			++frame;
			_cmdList->RSSetViewports(1, &viewport);
			_cmdList->RSSetScissorRects(1, &scissorrect);
			_cmdList->SetGraphicsRootSignature(rootsignature.Get());

			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->IASetVertexBuffers(0, 1, &vbView);
			_cmdList->IASetIndexBuffer(&ibView);
			
			//_cmdList->DrawInstanced(44, 1, 0, 0);
			_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

			// 前後だけ入れ替える
			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			_cmdList->ResourceBarrier(1, &BarrierDesc);

			// 命令のクローズ
			_cmdList->Close();

			// コマンドリストの実行
			ID3D12CommandList* cmdLists[] = { _cmdList.Get() };
			_cmdQueue->ExecuteCommandLists(1 , cmdLists);

			_cmdQueue->Signal(_fence.Get(), ++_fenceVal);

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

		// 処理をやめる前に"DX12"との同期を待つ
		_cmdQueue->Signal(_fence.Get(), ++_fenceVal);
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
	}

	return;
}

void Application::EnableDebugLayer()
{
	ComPtr<ID3D12Debug> debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));

	debugLayer->EnableDebugLayer(); // デバックレイヤーを有効化する
}