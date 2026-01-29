#pragma once

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

#include "DirectXMath.h"

#include "d3dcompiler.h"

#include "DirectXTex.h"

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dCompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using Microsoft::WRL::ComPtr;