#include <windows.h>
#include <stdio.h>
#include "logging.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma warning(disable: 4838)
#include "XNAMath\xnamath.h"

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib,"D3dcompiler.lib")

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

LRESULT CALLBACK WndCallbackProc(HWND, UINT, WPARAM, LPARAM);

FILE *gpFile = NULL;
const char *logFileName = "log.txt";

HWND gblHwnd = NULL;

DWORD gblDwStyle;
WINDOWPLACEMENT gblWindowPlcaement = { sizeof(WINDOWPLACEMENT) };

bool gblFullScreen = false;
bool gblIsEscPressed = false;
bool gblActiveWindow = false;

float gClearColor[4];
IDXGISwapChain *gpIDXGISwapChain = NULL;
ID3D11Device *gpID3D11Device = NULL;
ID3D11DeviceContext *gpID3D11DeviceContext = NULL;
ID3D11RenderTargetView *gpID3D11RenderTargetView = NULL;

ID3D11VertexShader *gpID3D11VertexShader = NULL;
ID3D11PixelShader *gpID3D11PixelShader = NULL;
ID3D11HullShader *gpID3D11HullShader = NULL;
ID3D11DomainShader *gpID3D11DomainShader = NULL;
ID3D11Buffer *gpID3D11BufferVertexBuffer = NULL;
ID3D11InputLayout *gpID3D11InputLayout = NULL;
ID3D11Buffer *gpID3D11BufferConstantBufferPixel = NULL;
ID3D11Buffer *gpID3D11BufferConstantBufferHull = NULL;
ID3D11Buffer *gpID3D11BufferConstantBufferDomain = NULL;

unsigned int gNumberOfLineSegments;

struct CBUFFER
{
	XMVECTOR LineColor;
};

struct CBUFFER_HULL
{
	XMVECTOR HullConstantFunctionPara;
};

struct CBUFFER_DOMAIN
{
	XMMATRIX WorldViewProjectionMatrix;
};

XMMATRIX gPerspectiveProjectionMatrix;

int WINAPI WinMain(HINSTANCE currentHInstance, HINSTANCE prevHInstance, LPSTR lpszCmdLune, int iCmdShow)
{
	// function prototype
	HRESULT initialize(void);
	void display(void);
	void uninitialize(void);

	// variables declartion
	WNDCLASSEX wndClass;
	HWND hwnd;
	MSG msg;
	int iScreenWidth, iScreenHeight;
	TCHAR szClassName[] = TEXT("Direct3D11");
	bool bDone = false;

	if (fopen_s(&gpFile, logFileName, "w") != 0)
	{
		MessageBox(NULL, TEXT("Log file can't be created \n existing"), TEXT("ERROR"), MB_OK | MB_TOPMOST | MB_ICONSTOP);
		exit(0);
	}
	else 
	{
		fprintf(gpFile, "Log file created succcessfully \n");
		fclose(gpFile);
	}

	//initialize window object
	wndClass.cbSize = sizeof(WNDCLASSEX);
	/*
	CS_OWNDC : Is required to make sure memory allocated is neither movable or discardable.
	*/
	wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = currentHInstance;
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.hIcon = wndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.lpfnWndProc = WndCallbackProc;
	wndClass.lpszClassName = szClassName;
	wndClass.lpszMenuName = NULL;

	// register class
	RegisterClassEx(&wndClass);

	iScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	iScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	// create window
	hwnd = CreateWindowEx(WS_EX_APPWINDOW,
		szClassName,
		TEXT("Direct3D11 : Tessellation"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
		(iScreenWidth / 2) - (WIN_WIDTH / 2),
		(iScreenHeight / 2) - (WIN_HEIGHT / 2),
		WIN_WIDTH,
		WIN_HEIGHT,
		NULL,
		NULL,
		currentHInstance,
		NULL);
	gblHwnd = hwnd;

	// render window
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

	// initialize
	HRESULT hr;
	hr = initialize();
	if (FAILED(hr)) {
		LOG_ERROR(gpFile, logFileName, "Initiliazation failed. Exiting now \n");
		DestroyWindow(hwnd);
		hwnd = NULL;
	}
	else 
	{
		LOG_ERROR(gpFile, logFileName, "Initiliazation succeeded \n");
	}

	// game loop
	while (bDone == false)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				bDone = true;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			if (gblActiveWindow == true)
			{
				display();
				if (gblIsEscPressed == true)
					bDone = true;
			}
		}
	}

	uninitialize();

	return (int)msg.wParam;
}

LRESULT CALLBACK WndCallbackProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	HRESULT resize(int, int);
	void uninitialize(void);
	void toggleFullScreen(void);

	HRESULT hr;

	switch (iMsg)
	{
	case WM_ACTIVATE:
		gblActiveWindow = (HIWORD(wParam) == 0);
		break;

	case WM_ERASEBKGND:
		/*
		telling windows, dont paint window background, this program
		has ability to paint window background by itself.*/
		return(0);

	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
		case 'q':
		case 'Q':
			gblIsEscPressed = true;
			break;

		case VK_UP:
			gNumberOfLineSegments = gNumberOfLineSegments > 50 ? 50 : gNumberOfLineSegments + 1;
			break;

		case VK_DOWN:
			gNumberOfLineSegments = gNumberOfLineSegments > 50 ? 50 : gNumberOfLineSegments - 1;
			break;

		case 0x46: // 'f' or 'F'
			gblFullScreen = !gblFullScreen;
			toggleFullScreen();
			break;

		default:
			break;
		}
		break;

	case WM_SIZE:
		if (gpID3D11DeviceContext)
		{
			hr = resize(LOWORD(lParam), HIWORD(lParam));
			if (FAILED(hr)) {
				LOG_ERROR(gpFile, logFileName, "resize() failed \n");
				return hr;
			}
			else
			{
				LOG_ERROR(gpFile, logFileName, "resize() succeeded \n");
			}
		}
		
		break;

	case WM_LBUTTONDOWN:
		break;

	case WM_CLOSE:
		uninitialize();
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		break;
	}

	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

void toggleFullScreen(void)
{
	MONITORINFO monInfo;
	HMONITOR hMonitor;
	monInfo = { sizeof(MONITORINFO) };
	hMonitor = MonitorFromWindow(gblHwnd, MONITORINFOF_PRIMARY);

	if (gblFullScreen == true) {
		gblDwStyle = GetWindowLong(gblHwnd, GWL_STYLE);
		if (gblDwStyle & WS_OVERLAPPEDWINDOW)
		{
			gblWindowPlcaement = { sizeof(WINDOWPLACEMENT) };
			if (GetWindowPlacement(gblHwnd, &gblWindowPlcaement) && GetMonitorInfo(hMonitor, &monInfo))
			{
				SetWindowLong(gblHwnd, GWL_STYLE, gblDwStyle & ~WS_OVERLAPPEDWINDOW);
				SetWindowPos(gblHwnd, HWND_TOP, monInfo.rcMonitor.left, monInfo.rcMonitor.top, (monInfo.rcMonitor.right - monInfo.rcMonitor.left), (monInfo.rcMonitor.bottom - monInfo.rcMonitor.top), SWP_NOZORDER | SWP_FRAMECHANGED);
			}

			ShowCursor(FALSE);
		}
	}
	else
	{
		SetWindowLong(gblHwnd, GWL_STYLE, gblDwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(gblHwnd, &gblWindowPlcaement);
		SetWindowPos(gblHwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

		ShowCursor(TRUE);
	}
}

HRESULT initialize(void)
{
	HRESULT resize(int, int);
	
	// variable declarations
	HRESULT hr;
	D3D_DRIVER_TYPE d3dDriverType;
	D3D_DRIVER_TYPE d3dDriverTypes[] = {
		D3D_DRIVER_TYPE_HARDWARE, 
		D3D_DRIVER_TYPE_WARP, 
		D3D_DRIVER_TYPE_REFERENCE, 
	};
	D3D_FEATURE_LEVEL d3dFeatureLevelRequired = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL d3dFeatureLevelAcquired = D3D_FEATURE_LEVEL_10_0; // default, lowest
	UINT createDeviceFlags = 0;
	UINT numDriverTypes = 0;
	UINT numFeatureLevels = 1; // based upon d3dFeatureLevelRequired

	numDriverTypes = sizeof(d3dDriverTypes) / sizeof(d3dDriverTypes[0]);

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	ZeroMemory((void *)&dxgiSwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
	dxgiSwapChainDesc.BufferCount = 1;
	dxgiSwapChainDesc.BufferDesc.Width = WIN_WIDTH;
	dxgiSwapChainDesc.BufferDesc.Height = WIN_HEIGHT;
	dxgiSwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.OutputWindow = gblHwnd;
	dxgiSwapChainDesc.SampleDesc.Count = 1;
	dxgiSwapChainDesc.SampleDesc.Quality = 0;
	dxgiSwapChainDesc.Windowed = TRUE;

	for (UINT index = 0; index < numDriverTypes; index++)
	{
		d3dDriverType = d3dDriverTypes[index];
		hr = D3D11CreateDeviceAndSwapChain(
			NULL,                          
			d3dDriverType,                 
			NULL,                          
			createDeviceFlags,             
			&d3dFeatureLevelRequired,     
			numFeatureLevels,              
			D3D11_SDK_VERSION,             
			&dxgiSwapChainDesc,            
			&gpIDXGISwapChain,             
			&gpID3D11Device,               
			&d3dFeatureLevelAcquired,     
			&gpID3D11DeviceContext);       
		
		if (SUCCEEDED(hr))
		{
			break;
		}
	}

	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "CreateSwapChain() failed \n");
		return hr;
	}
	else 
	{
		LOG_ERROR(gpFile, logFileName, "CreateSwapChain() succeeded \n");
		
		LOG_ERROR(gpFile, logFileName, "Choosen driver is of ");
		if (d3dDriverType == D3D_DRIVER_TYPE_HARDWARE)
		{
			LOG_ERROR(gpFile, logFileName, "Hardware type \n");
		}
		else if (d3dDriverType == D3D_DRIVER_TYPE_WARP)
		{
			LOG_ERROR(gpFile, logFileName, "Warp type \n");
		}
		else if (d3dDriverType == D3D_DRIVER_TYPE_REFERENCE)
		{
			LOG_ERROR(gpFile, logFileName, "Reference type \n");
		}
		else
		{
			LOG_ERROR(gpFile, logFileName, "Unknown type \n");
		}

		LOG_ERROR(gpFile, logFileName, "Supported highest level feature is ");
		if (d3dFeatureLevelAcquired == D3D_FEATURE_LEVEL_11_0)
		{
			LOG_ERROR(gpFile, logFileName, "11.0 \n");
		}
		else if (d3dFeatureLevelAcquired == D3D_FEATURE_LEVEL_10_1)
		{
			LOG_ERROR(gpFile, logFileName, "10.1 \n");
		}
		else if (d3dFeatureLevelAcquired == D3D_FEATURE_LEVEL_10_0)
		{
			LOG_ERROR(gpFile, logFileName, "10.0 \n");
		}
		else
		{
			LOG_ERROR(gpFile, logFileName, "Unknown \n");
		}
	}

	// vertex shader
	const char *vertexShaderSourceCode =
		"struct VertexOutput" \
		"{" \
			"float4 position: position;"\
		"};" \

		"VertexOutput main(float2 pos: POSITION)" \
		"{" \
			"VertexOutput output;"\
			"output.position = float4(pos, 0.0, 1.0);" \
			"return output;" \
		"}";

	ID3DBlob *pID3DBlobVertexShaderCode = NULL;
	ID3DBlob *pID3DBlobError= NULL;
		
	hr = D3DCompile(
		vertexShaderSourceCode,
		lstrlenA(vertexShaderSourceCode) + 1,
		"VS",
		NULL,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"vs_5_0",
		0,
		0,
		&pID3DBlobVertexShaderCode,
		&pID3DBlobError
	);
	if (FAILED(hr))
	{
		if (pID3DBlobError != NULL)
		{
			LOG_ERROR(gpFile, logFileName, "D3DCompile() vertex shader compilation failed : %s \n", (char*) pID3DBlobError -> GetBufferPointer());
			pID3DBlobError -> Release();
			pID3DBlobError = NULL;
		}
		else
		{
			LOG_ERROR(gpFile, logFileName, "D3DCompile() vertex shader compilation failed \n");
		}

		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "D3DCompile() vertex shader compilation succeeded \n");
	}
	
	hr = gpID3D11Device -> CreateVertexShader(
		pID3DBlobVertexShaderCode -> GetBufferPointer(),
		pID3DBlobVertexShaderCode -> GetBufferSize(),
		NULL,
		&gpID3D11VertexShader
	);
	
	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11VertexShader:: CreateVertexShader() creation failed \n");
		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11VertexShader:: CreateVertexShader() creation succeeded \n");
	}
	gpID3D11DeviceContext -> VSSetShader(gpID3D11VertexShader, 0, 0);
	
	// hull shader 
	const char *hullShaderSourceCode =
		"cbuffer ConstantBuffer" \
		"{" \
			"float4 hullconstantfunctionpara; " \
		"};" \

		"struct VertexOutput" \
		"{" \
			"float4 position: position;"\
		"};" \

		"struct HullConstantOutput" \
		"{" \
			"float edges[2]: SV_TESSFACTOR;" \
		"};" \

		"HullConstantOutput hull_constant_function(void)" \
		"{" \
			"HullConstantOutput output;" \
			"float numberOfStrips = hullconstantfunctionpara[0];" \
			"float numberOfSegments = hullconstantfunctionpara[1];" \
			"output.edges[0] = numberOfStrips;" \
			"output.edges[1] = numberOfSegments;" \
			"return output;" \
		"}" \

		"struct HullOutput" \
		"{" \
			"float4 position: position;" \
		"};" \

		"[domain(\"isoline\")]" \
		"[partitioning(\"integer\")]" \
		"[outputtopology(\"line\")]" \
		"[outputcontrolpoints(4)]" \
		"[patchconstantfunc(\"hull_constant_function\")]" \

		"HullOutput main(InputPatch<VertexOutput, 4> inputPatch, uint i: SV_OUTPUTCONTROLPOINTID)" \
		"{" \
			"HullOutput output;" \
			"output.position = inputPatch[i].position;" \
			"return output;" \
		"}";

	ID3DBlob *pID3DBlobHullShaderCode = NULL;
	pID3DBlobError = NULL;

	hr = D3DCompile(
		hullShaderSourceCode,
		lstrlenA(hullShaderSourceCode) + 1,
		"HS",
		NULL,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"hs_5_0",
		0,
		0,
		&pID3DBlobHullShaderCode,
		&pID3DBlobError
	);
	if (FAILED(hr))
	{
		if (pID3DBlobError != NULL)
		{
			LOG_ERROR(gpFile, logFileName, "D3DCompile() hull shader compilation failed : %s \n", (char*)pID3DBlobError->GetBufferPointer());
			pID3DBlobError->Release();
			pID3DBlobError = NULL;
		}
		else
		{
			LOG_ERROR(gpFile, logFileName, "D3DCompile() hull shader compilation failed \n");
		}

		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "D3DCompile() hull shader compilation succeeded \n");
	}

	hr = gpID3D11Device -> CreateHullShader(
		pID3DBlobHullShaderCode->GetBufferPointer(),
		pID3DBlobHullShaderCode->GetBufferSize(),
		NULL,
		&gpID3D11HullShader
	);

	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11VertexShader:: CreateHullShader() creation failed \n");
		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11VertexShader:: CreateHullShader() creation succeeded \n");
	}
	gpID3D11DeviceContext->HSSetShader(gpID3D11HullShader, 0, 0);

	// domain shader 
	const char *domainShaderSourceCode =
		"cbuffer ConstantBuffer" \
		"{" \
			"float4x4 WorldViewProjectionMatrix; " \
		"};" \

		"struct DomainOutput" \
		"{" \
			"float4 position: SV_POSITION;" \
		"};" \

		"struct HullConstantOutput" \
		"{" \
			"float edges[2]: SV_TESSFACTOR;" \
		"};" \

		"struct HullOutput" \
		"{" \
			"float4 position: position;" \
		"};" \

		"[domain(\"isoline\")]" \
		"[partitioning(\"integer\")]" \
		"[outputtopology(\"line\")]" \
		"[outputcontrolpoints(4)]" \
		"[patchconstantfunc(\"hull_constant_function\")]" \

		"DomainOutput main(HullConstantOutput input, OutputPatch<HullOutput, 4> outputPatch, float2 tesscoord: SV_DOMAINLOCATION)" \
		"{" \
			"DomainOutput output;" \
			"float u = tesscoord.x;" \
			"float3 p0 = outputPatch[0].position.xyz;" \
			"float3 p1 = outputPatch[1].position.xyz;" \
			"float3 p2 = outputPatch[2].position.xyz;" \
			"float3 p3 = outputPatch[3].position.xyz;" \
			"float u1 = (1.0 - u);" \
			"float u2 = u * u;" \
			"float b3 = u2 * u;" \
			"float b2 = 3.0 * u2 * u1;" \
			"float b1 = 3.0 * u * u1 * u1;" \
			"float b0 = u1 * u1 * u1;" \
			"float3 p = p0 + b0 + p1 * b1 + p2 * b2 + p3 * b3;" \
			"output.position = mul(WorldViewProjectionMatrix, float4(p, 1.0));" \
			"return output;" \
		"}";

	ID3DBlob *pID3DBlobDomainShaderCode = NULL;
	pID3DBlobError = NULL;

	hr = D3DCompile(
		domainShaderSourceCode,
		lstrlenA(domainShaderSourceCode) + 1,
		"DS",
		NULL,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ds_5_0",
		0,
		0,
		&pID3DBlobDomainShaderCode,
		&pID3DBlobError
	);
	if (FAILED(hr))
	{
		if (pID3DBlobError != NULL)
		{
			LOG_ERROR(gpFile, logFileName, "D3DCompile() domain shader compilation failed : %s \n", (char*)pID3DBlobError->GetBufferPointer());
			pID3DBlobError->Release();
			pID3DBlobError = NULL;
		}
		else
		{
			LOG_ERROR(gpFile, logFileName, "D3DCompile() domain shader compilation failed \n");
		}

		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "D3DCompile() domain shader compilation succeeded \n");
	}

	hr = gpID3D11Device->CreateDomainShader(
		pID3DBlobDomainShaderCode->GetBufferPointer(),
		pID3DBlobDomainShaderCode->GetBufferSize(),
		NULL,
		&gpID3D11DomainShader
	);

	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11VertexShader:: CreateDomainShader() creation failed \n");
		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11VertexShader:: CreateDomainShader() creation succeeded \n");
	}
	gpID3D11DeviceContext->DSSetShader(gpID3D11DomainShader, 0, 0);

	// pixel (fragment) shader
	const char *pixelShaderSourceCode =
		"cbuffer ConstantBuffer" \
		"{" \
			"float4 lineColor; " \
		"};" \

		"float4 main(): SV_TARGET" \
		"{" \
			"return lineColor;" \
		"}";

	ID3DBlob *pID3DBlobPixelShaderCode = NULL;
	pID3DBlobError = NULL;

	hr = D3DCompile(
		pixelShaderSourceCode,
		lstrlenA(pixelShaderSourceCode) + 1,
		"PS",
		NULL,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_0",
		0,
		0,
		&pID3DBlobPixelShaderCode,
		&pID3DBlobError
	);
	if (FAILED(hr))
	{
		if (pID3DBlobError != NULL)
		{
			LOG_ERROR(gpFile, logFileName, "D3DCompile() pixel shader compilation failed: %s \n", (char*)pID3DBlobError->GetBufferPointer());
			pID3DBlobError -> Release();
			pID3DBlobError = NULL;
		}
		else
		{
			LOG_ERROR(gpFile, logFileName, "D3DCompile() pixel shader compilation failed \n");
		}

		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "D3DCompile() pixel shader compilation succeeded \n");
	}

	hr = gpID3D11Device -> CreatePixelShader(
		pID3DBlobPixelShaderCode -> GetBufferPointer(),
		pID3DBlobPixelShaderCode -> GetBufferSize(),
		NULL,
		&gpID3D11PixelShader
	);

	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11PixelShader:: CreatePixelShader() creation failed \n");
		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11PixelShader:: CreatePixelShader() compilation succeeded \n");
	}
	gpID3D11DeviceContext -> PSSetShader(gpID3D11PixelShader, 0, 0);
	pID3DBlobPixelShaderCode-> Release();
	pID3DBlobPixelShaderCode = NULL;

	//clockwise winding order
	float vertices[] =
	{
		-1.0f, -1.0f, -0.5f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f
	};

	//create vertex buffer
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(D3D11_BUFFER_DESC));
	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.ByteWidth = sizeof(float) * ARRAYSIZE(vertices);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = gpID3D11Device -> CreateBuffer(&vertexBufferDesc, NULL, &gpID3D11BufferVertexBuffer);
	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateBuffer() failed for vertex buffer \n");
		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateBuffer() succeeded for vertex buffer \n");
	}

	//copy data into buffer
	D3D11_MAPPED_SUBRESOURCE mappedSubSource;
	ZeroMemory(&mappedSubSource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	gpID3D11DeviceContext -> Map(gpID3D11BufferVertexBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &mappedSubSource);
	memcpy(mappedSubSource.pData, vertices, sizeof(vertices));
	gpID3D11DeviceContext -> Unmap(gpID3D11BufferVertexBuffer, NULL);

	//create and set input layout
	D3D11_INPUT_ELEMENT_DESC inputElementDesc;
	inputElementDesc.SemanticName = "POSITION";
	inputElementDesc.SemanticIndex = 0;
	inputElementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDesc.InputSlot = 0;
	inputElementDesc.AlignedByteOffset = 0;
	inputElementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	inputElementDesc.InstanceDataStepRate = 0;

	hr = gpID3D11Device -> CreateInputLayout(
		&inputElementDesc, 
		1, 
		pID3DBlobVertexShaderCode -> GetBufferPointer(),
		pID3DBlobVertexShaderCode ->GetBufferSize(),
		&gpID3D11InputLayout);

	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateInputLayout() failed \n");
		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateInputLayout() succeeded \n");
	}
	gpID3D11DeviceContext -> IASetInputLayout(gpID3D11InputLayout);
	pID3DBlobVertexShaderCode -> Release();
	pID3DBlobVertexShaderCode = NULL;

	//define and set the constant buffer
	D3D11_BUFFER_DESC constantBufferDesc;
	ZeroMemory(&constantBufferDesc, sizeof(D3D11_BUFFER_DESC));
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.ByteWidth = sizeof(CBUFFER_HULL);
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = gpID3D11Device->CreateBuffer(&constantBufferDesc, nullptr, &gpID3D11BufferConstantBufferHull);
	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateBuffer() failed to create constant buffer hull \n")
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateBuffer()  succeeded to create constant buffer hull \n")
	}
	gpID3D11DeviceContext -> HSSetConstantBuffers(0, 1, &gpID3D11BufferConstantBufferHull);
	
	ZeroMemory(&constantBufferDesc, sizeof(D3D11_BUFFER_DESC));
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.ByteWidth = sizeof(CBUFFER_DOMAIN);
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = gpID3D11Device->CreateBuffer(&constantBufferDesc, nullptr, &gpID3D11BufferConstantBufferDomain);
	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateBuffer() failed to create constant buffer domain \n")
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateBuffer()  succeeded to create constant buffer domain \n")
	}
	gpID3D11DeviceContext -> DSSetConstantBuffers(0, 1, &gpID3D11BufferConstantBufferDomain);

	ZeroMemory(&constantBufferDesc, sizeof(D3D11_BUFFER_DESC));
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.ByteWidth = sizeof(CBUFFER);
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = gpID3D11Device->CreateBuffer(&constantBufferDesc, nullptr, &gpID3D11BufferConstantBufferPixel);
	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateBuffer() failed to create constant buffer \n")
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateBuffer()  succeeded to create constant buffer \n")
	}
	gpID3D11DeviceContext->PSSetConstantBuffers(0, 1, &gpID3D11BufferConstantBufferPixel);

	// d3d clear color (black)
	gClearColor[0] = 0.0f;
	gClearColor[1] = 0.0f;
	gClearColor[2] = 0.0f;
	gClearColor[3] = 1.0f;

	//set projection
	gPerspectiveProjectionMatrix = XMMatrixIdentity();

	hr = resize(WIN_WIDTH, WIN_HEIGHT);
	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "resize() failed \n");
		return hr;
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "resize() succeeded \n");
	}

	return S_OK;
}

/*
Very important for Dirext X not for OpenGL
becuase DirextX is not state machine and change in windows resize empose
re-rendering of Direct X (even for Vulcan) scenes.
*/
HRESULT resize(int width, int height)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D *pID3D11Texture2DBackBuffer;

	if (gpID3D11RenderTargetView)
	{
		gpID3D11RenderTargetView->Release();
		gpID3D11RenderTargetView = NULL;
	}

	gpIDXGISwapChain -> ResizeBuffers(1, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);

	// use texture buffer as back buffer from swap chain
	gpIDXGISwapChain -> GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pID3D11Texture2DBackBuffer);

	hr = gpID3D11Device -> CreateRenderTargetView(pID3D11Texture2DBackBuffer, NULL, &gpID3D11RenderTargetView);

	if (FAILED(hr))
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateRenderTargetView()::CreateRenderTargetView failed \n" );
	}
	else
	{
		LOG_ERROR(gpFile, logFileName, "ID3D11Device::CreateRenderTargetView()::CreateRenderTargetView succeeded \n");
	}

	pID3D11Texture2DBackBuffer -> Release();
	pID3D11Texture2DBackBuffer = NULL;

	// set render target view as target
	gpID3D11DeviceContext -> OMSetRenderTargets(1, &gpID3D11RenderTargetView, NULL);

	// set viewport
	D3D11_VIEWPORT d3dViewPort;
	d3dViewPort.TopLeftX = 0;
	d3dViewPort.TopLeftY = 0;
	d3dViewPort.Width = (float) width;
	d3dViewPort.Height = (float) height;
	d3dViewPort.MinDepth = 0.0f;
	d3dViewPort.MaxDepth = 1.0f;
	gpID3D11DeviceContext -> RSSetViewports(1, &d3dViewPort);

	gNumberOfLineSegments = 1;

	gPerspectiveProjectionMatrix = XMMatrixPerspectiveFovLH(45.0f, (float)width / (float)height, 0.1f, 100.0f);
	
	return hr;
}

void display(void)
{
	gpID3D11DeviceContext -> ClearRenderTargetView(gpID3D11RenderTargetView, gClearColor);

	// select buffer to display
	UINT stride = sizeof(float) * 2;
	UINT offset = 0;
	gpID3D11DeviceContext -> IASetVertexBuffers(0, 1, &gpID3D11BufferVertexBuffer, &stride, &offset);

	// select geometry premetive (topology)
	gpID3D11DeviceContext -> IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

	// translation
	XMMATRIX worldMatrix = XMMatrixIdentity();
	XMMATRIX viewMatrix = XMMatrixIdentity();

	worldMatrix = XMMatrixTranslation(0.5f, 0.5f, 2.0f);

	// final worldviewProjection matrix
	XMMATRIX wvpMatrix = worldMatrix * viewMatrix * gPerspectiveProjectionMatrix;

	// load the data into constant buffer
	CBUFFER constantBufferPixel;
	constantBufferPixel.LineColor = XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f);
	gpID3D11DeviceContext -> UpdateSubresource(gpID3D11BufferConstantBufferPixel, 0, NULL, &constantBufferPixel, 0, 0);

	CBUFFER_HULL constantBufferHull;
	constantBufferHull.HullConstantFunctionPara = XMVectorSet(1, gNumberOfLineSegments, 0.0f, 0.0f);
	gpID3D11DeviceContext->UpdateSubresource(gpID3D11BufferConstantBufferHull, 0, NULL, &constantBufferHull, 0, 0);

	CBUFFER_DOMAIN constantBufferDomain;
	constantBufferDomain.WorldViewProjectionMatrix = wvpMatrix;
	gpID3D11DeviceContext->UpdateSubresource(gpID3D11BufferConstantBufferDomain, 0, NULL, &constantBufferDomain, 0, 0);

	// draw vertex buffer to render target
	gpID3D11DeviceContext -> Draw(4, 0);

	// switch between front and back buffer
	gpIDXGISwapChain -> Present(0, 0);
}

void uninitialize(void)
{
	// tear down 

	if (gpID3D11BufferConstantBufferPixel)
	{
		gpID3D11BufferConstantBufferPixel->Release();
		gpID3D11BufferConstantBufferPixel = NULL;
	}
	
	if (gpID3D11BufferConstantBufferDomain)
	{
		gpID3D11BufferConstantBufferDomain->Release();
		gpID3D11BufferConstantBufferDomain = NULL;
	}

	if (gpID3D11BufferConstantBufferHull)
	{
		gpID3D11BufferConstantBufferHull->Release();
		gpID3D11BufferConstantBufferHull = NULL;
	}

	if (gpID3D11InputLayout)
	{
		gpID3D11InputLayout -> Release();
		gpID3D11InputLayout = NULL;
	}

	if (gpID3D11BufferVertexBuffer)
	{
		gpID3D11BufferVertexBuffer -> Release();
		gpID3D11BufferVertexBuffer = NULL;
	}

	if (gpID3D11PixelShader)
	{
		gpID3D11PixelShader -> Release();
		gpID3D11PixelShader = NULL;
	}

	if (gpID3D11HullShader)
	{
		gpID3D11HullShader->Release();
		gpID3D11HullShader = NULL;
	}

	if (gpID3D11DomainShader)
	{
		gpID3D11DomainShader->Release();
		gpID3D11DomainShader = NULL;
	}

	if (gpID3D11VertexShader)
	{
		gpID3D11VertexShader -> Release();
		gpID3D11VertexShader = NULL;
	}

	if (gpID3D11RenderTargetView)
	{
		gpID3D11RenderTargetView -> Release();
		gpID3D11RenderTargetView = NULL;
	}

	if (gpIDXGISwapChain)
	{
		gpIDXGISwapChain -> Release();
		gpIDXGISwapChain = NULL;
	}

	if (gpID3D11DeviceContext)
	{
		gpID3D11DeviceContext -> Release();
		gpID3D11DeviceContext = NULL;
	}

	if (gpID3D11Device)
	{
		gpID3D11Device -> Release();
		gpID3D11Device = NULL;
	}

	if (gpFile)
	{
		LOG_ERROR(gpFile, logFileName, "uninitialize() succeeded \n");
		LOG_ERROR(gpFile, logFileName, "Log file is closed \n");
	}
}