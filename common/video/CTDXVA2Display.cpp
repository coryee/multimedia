#include "CTDXVA2Display.h"
#include <stdio.h>
#include <mmsyscom.h>

#define CTDXVA2DISPInfo		printf
#define CTDXVA2DISPError	printf
#define DBGMSG(x)  // {DbgPrint(TEXT("%s(%u) : "), TEXT(__FILE__), __LINE__); DbgPrint x;}

// TODO: delete color definitions
//
// Studio RGB [16...235] colors.
//

// 100%
const D3DCOLOR RGB_WHITE = D3DCOLOR_XRGB(0xEB, 0xEB, 0xEB);
const D3DCOLOR RGB_RED = D3DCOLOR_XRGB(0xEB, 0x10, 0x10);
const D3DCOLOR RGB_YELLOW = D3DCOLOR_XRGB(0xEB, 0xEB, 0x10);
const D3DCOLOR RGB_GREEN = D3DCOLOR_XRGB(0x10, 0xEB, 0x10);
const D3DCOLOR RGB_CYAN = D3DCOLOR_XRGB(0x10, 0xEB, 0xEB);
const D3DCOLOR RGB_BLUE = D3DCOLOR_XRGB(0x10, 0x10, 0xEB);
const D3DCOLOR RGB_MAGENTA = D3DCOLOR_XRGB(0xEB, 0x10, 0xEB);
const D3DCOLOR RGB_BLACK = D3DCOLOR_XRGB(0x10, 0x10, 0x10);
const D3DCOLOR RGB_ORANGE = D3DCOLOR_XRGB(0xEB, 0x7E, 0x10);

// 75%
const D3DCOLOR RGB_WHITE_75pc = D3DCOLOR_XRGB(0xB4, 0xB4, 0xB4);
const D3DCOLOR RGB_YELLOW_75pc = D3DCOLOR_XRGB(0xB4, 0xB4, 0x10);
const D3DCOLOR RGB_CYAN_75pc = D3DCOLOR_XRGB(0x10, 0xB4, 0xB4);
const D3DCOLOR RGB_GREEN_75pc = D3DCOLOR_XRGB(0x10, 0xB4, 0x10);
const D3DCOLOR RGB_MAGENTA_75pc = D3DCOLOR_XRGB(0xB4, 0x10, 0xB4);
const D3DCOLOR RGB_RED_75pc = D3DCOLOR_XRGB(0xB4, 0x10, 0x10);
const D3DCOLOR RGB_BLUE_75pc = D3DCOLOR_XRGB(0x10, 0x10, 0xB4);

// -4% / +4%
const D3DCOLOR RGB_BLACK_n4pc = D3DCOLOR_XRGB(0x07, 0x07, 0x07);
const D3DCOLOR RGB_BLACK_p4pc = D3DCOLOR_XRGB(0x18, 0x18, 0x18);

// -Inphase / +Quadrature
const D3DCOLOR RGB_I = D3DCOLOR_XRGB(0x00, 0x1D, 0x42);
const D3DCOLOR RGB_Q = D3DCOLOR_XRGB(0x2C, 0x00, 0x5C);

const D3DCOLOR BACKGROUND_COLORS[] =
{
	RGB_WHITE, RGB_RED,  RGB_YELLOW,  RGB_GREEN,
	RGB_CYAN,  RGB_BLUE, RGB_MAGENTA, RGB_BLACK
};
// end delete

INT g_BackgroundColor = 0;

const BYTE DEFAULT_PLANAR_ALPHA_VALUE = 0xFF;
WORD g_PlanarAlphaValue = DEFAULT_PLANAR_ALPHA_VALUE;

const UINT VIDEO_REQUIED_OP = DXVA2_VideoProcess_YUV2RGB |
	DXVA2_VideoProcess_StretchX |
	DXVA2_VideoProcess_StretchY |
	DXVA2_VideoProcess_SubRects |
	DXVA2_VideoProcess_SubStreams;

const D3DFORMAT VIDEO_RENDER_TARGET_FORMAT = D3DFMT_X8R8G8B8;
// const D3DFORMAT VIDEO_MAIN_FORMAT = D3DFMT_YUY2; 
const DWORD VIDEO_MAIN_FORMAT = MAKEFOURCC('N', 'V', '1', '2');

const UINT BACK_BUFFER_COUNT = 1;
const UINT SUB_STREAM_COUNT = 1;
const UINT DWM_BUFFER_COUNT = 4;

const UINT VIDEO_MAIN_WIDTH = 640;
const UINT VIDEO_MAIN_HEIGHT = 480;
const RECT VIDEO_MAIN_RECT = { 0, 0, VIDEO_MAIN_WIDTH, VIDEO_MAIN_HEIGHT };


const UINT VIDEO_FPS = 60;

const UINT EX_COLOR_INFO[][2] =
{
	// SDTV ITU-R BT.601 YCbCr to driver's optimal RGB range
	{ DXVA2_VideoTransferMatrix_BT601, DXVA2_NominalRange_Unknown },
	// SDTV ITU-R BT.601 YCbCr to studio RGB [16...235]
	{ DXVA2_VideoTransferMatrix_BT601, DXVA2_NominalRange_16_235 },
	// SDTV ITU-R BT.601 YCbCr to computer RGB [0...255]
	{ DXVA2_VideoTransferMatrix_BT601, DXVA2_NominalRange_0_255 },
	// HDTV ITU-R BT.709 YCbCr to driver's optimal RGB range
	{ DXVA2_VideoTransferMatrix_BT709, DXVA2_NominalRange_Unknown },
	// HDTV ITU-R BT.709 YCbCr to studio RGB [16...235]
	{ DXVA2_VideoTransferMatrix_BT709, DXVA2_NominalRange_16_235 },
	// HDTV ITU-R BT.709 YCbCr to computer RGB [0...255]
	{ DXVA2_VideoTransferMatrix_BT709, DXVA2_NominalRange_0_255 }
};


//
// Type definitions.
//

typedef HRESULT(WINAPI * PFNDWMISCOMPOSITIONENABLED)(
	__out BOOL* pfEnabled
	);

typedef HRESULT(WINAPI * PFNDWMGETCOMPOSITIONTIMINGINFO)(
	__in HWND hwnd,
	__out DWM_TIMING_INFO* pTimingInfo
	);

typedef HRESULT(WINAPI * PFNDWMSETPRESENTPARAMETERS)(
	__in HWND hwnd,
	__inout DWM_PRESENT_PARAMETERS* pPresentParams
	);

static INT ComputeLongSteps(DXVA2_ValueRange &range)
{
	float f_step = DXVA2FixedToFloat(range.StepSize);

	if (f_step == 0.0)
	{
		return 0;
	}

	float f_max = DXVA2FixedToFloat(range.MaxValue);
	float f_min = DXVA2FixedToFloat(range.MinValue);
	INT steps = INT((f_max - f_min) / f_step / 32);

	return max(steps, 1);
}

DWORD RGBtoYUV(const D3DCOLOR rgb)
{
	const INT A = HIBYTE(HIWORD(rgb));
	const INT R = LOBYTE(HIWORD(rgb)) - 16;
	const INT G = HIBYTE(LOWORD(rgb)) - 16;
	const INT B = LOBYTE(LOWORD(rgb)) - 16;

	//
	// studio RGB [16...235] to SDTV ITU-R BT.601 YCbCr
	//
	INT Y = (77 * R + 150 * G + 29 * B + 128) / 256 + 16;
	INT U = (-44 * R - 87 * G + 131 * B + 128) / 256 + 128;
	INT V = (131 * R - 110 * G - 21 * B + 128) / 256 + 128;

	return D3DCOLOR_AYUV(A, Y, U, V);
}

static DWORD RGBtoYUY2(const D3DCOLOR rgb)
{
	const D3DCOLOR yuv = RGBtoYUV(rgb);

	const BYTE Y = LOBYTE(HIWORD(yuv));
	const BYTE U = HIBYTE(LOWORD(yuv));
	const BYTE V = LOBYTE(LOWORD(yuv));

	return MAKELONG(MAKEWORD(Y, U), MAKEWORD(Y, V));
}

static VOID RGBtoYUY(const D3DCOLOR rgb, BYTE *Y, BYTE *U, BYTE *V)
{
	const D3DCOLOR yuv = RGBtoYUV(rgb);

	*Y = LOBYTE(HIWORD(yuv));
	*U = HIBYTE(LOWORD(yuv));
	*V = LOBYTE(LOWORD(yuv));

}

CTDXVA2Display::CTDXVA2Display()
{
	g_Hwnd = NULL;
	g_bWindowed = 1;
	g_bInModeChange = FALSE;
	g_RectWindow = { 0 };
	g_TargetWidthPercent = 100;
	g_TargetHeightPercent = 100;
	g_bDwmQueuing = FALSE;

	g_bD3D9HW = TRUE;
	g_bDXVA2HW = TRUE;

	g_pD3D9 = NULL;
	g_pD3DD9 = NULL;
	g_pD3DRT = NULL;
	memset(&g_D3DPP, 0, sizeof(g_D3DPP));
	
	g_pDXVAVPS = NULL;
	g_pDXVAVPD = NULL;

	g_pMainStream = NULL;
	g_SrcRect = VIDEO_MAIN_RECT;
	g_DstRect = VIDEO_MAIN_RECT;


	g_GuidVP = { 0 };
	g_VideoDesc = { 0 };
	g_VPCaps = { 0 };

	g_ExColorInfo = 0;
	memset(g_ProcAmpRanges, 0, sizeof(g_ProcAmpRanges));
	memset(g_ProcAmpValues, 0, sizeof(g_ProcAmpValues));
	memset(g_ProcAmpSteps, 0, sizeof(g_ProcAmpSteps));
	
}


CTDXVA2Display::~CTDXVA2Display()
{
}

int CTDXVA2Display::Init(HWND hwnd)
{
	g_Hwnd = hwnd;
	if (InitializeModule() &&
		InitializeD3D9() &&
		InitializeDXVA2())
	{
		return CTDXVADISP_EC_OK;
	}

	return CTDXVADISP_EC_FAILURE;
}

int CTDXVA2Display::Display(IDirect3DSurface9 *surface)
{

	ProcessVideo();
	return CTDXVADISP_EC_OK;
}

BOOL CTDXVA2Display::InitializeModule()
{
	//
	// Load these DLLs dynamically because these may not be available prior to Vista.
	//
	m_rgb9rast_dll = LoadLibrary(TEXT("rgb9rast.dll"));
	if (!m_rgb9rast_dll) {
		CTDXVA2DISPError("LoadLibrary(rgb9rast.dll) failed with error %d.\n", GetLastError());
	}

	g_hDwmApiDLL = LoadLibrary(TEXT("dwmapi.dll"));
	if (!g_hDwmApiDLL) {
		CTDXVA2DISPError("LoadLibrary(dwmapi.dll) failed with error %d.\n", GetLastError());
		goto SKIP_DWMAPI;
	}

	g_pfnDwmIsCompositionEnabled = GetProcAddress(g_hDwmApiDLL, "DwmIsCompositionEnabled");
	if (!g_pfnDwmIsCompositionEnabled) {
		CTDXVA2DISPError("GetProcAddress(DwmIsCompositionEnabled) failed with error %d.\n", GetLastError());
		return FALSE;
	}

	g_pfnDwmGetCompositionTimingInfo = GetProcAddress(g_hDwmApiDLL, "DwmGetCompositionTimingInfo");
	if (!g_pfnDwmGetCompositionTimingInfo) {
		CTDXVA2DISPError("GetProcAddress(DwmGetCompositionTimingInfo) failed with error %d.\n", GetLastError());
		return FALSE;
	}

	g_pfnDwmSetPresentParameters = GetProcAddress(g_hDwmApiDLL, "DwmSetPresentParameters");
	if (!g_pfnDwmSetPresentParameters) {
		CTDXVA2DISPError("GetProcAddress(DwmSetPresentParameters) failed with error %d.\n", GetLastError());
		return FALSE;
	}

SKIP_DWMAPI:

	return TRUE;
}

BOOL CTDXVA2Display::InitializeD3D9()
{
	HRESULT hr;

	g_pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);

	if (!g_pD3D9)
	{
		DBGMSG((TEXT("Direct3DCreate9 failed.\n")));
		return FALSE;
	}

	if (g_bWindowed)
	{
		g_D3DPP.BackBufferWidth = 0;
		g_D3DPP.BackBufferHeight = 0;
	}
	else
	{
		g_D3DPP.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
		g_D3DPP.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
	}

	g_D3DPP.BackBufferFormat = VIDEO_RENDER_TARGET_FORMAT;
	g_D3DPP.BackBufferCount = BACK_BUFFER_COUNT;
	g_D3DPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_D3DPP.hDeviceWindow = g_Hwnd;
	g_D3DPP.Windowed = g_bWindowed;
	g_D3DPP.Flags = D3DPRESENTFLAG_VIDEO;
	g_D3DPP.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	g_D3DPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;


	//
	// First try to create a hardware D3D9 device.
	//
	if (g_bD3D9HW)
	{
		hr = g_pD3D9->CreateDevice(D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			g_Hwnd,
			D3DCREATE_FPU_PRESERVE |
			D3DCREATE_MULTITHREADED |
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&g_D3DPP,
			&g_pD3DD9);

		if (FAILED(hr))
		{
			DBGMSG((TEXT("CreateDevice(HAL) failed with error 0x%x.\n"), hr));
		}
	}

	if (!g_pD3DD9)
	{
		return FALSE;
	}

	return TRUE;	
}

BOOL CTDXVA2Display::InitializeDXVA2()
{
	HRESULT hr;

	//
	// Retrieve a back buffer as the video render target.
	//
	hr = g_pD3DD9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_pD3DRT);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("GetBackBuffer failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	//
	// Create DXVA2 Video Processor Service.
	//
	hr = DXVA2CreateVideoService(g_pD3DD9,
		IID_IDirectXVideoProcessorService,
		(VOID**)&g_pDXVAVPS);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("DXVA2CreateVideoService failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	//
	// Initialize the video descriptor.
	//
	g_VideoDesc.SampleWidth = VIDEO_MAIN_WIDTH;
	g_VideoDesc.SampleHeight = VIDEO_MAIN_HEIGHT;
	g_VideoDesc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
	g_VideoDesc.SampleFormat.NominalRange = DXVA2_NominalRange_16_235;
	g_VideoDesc.SampleFormat.VideoTransferMatrix = EX_COLOR_INFO[g_ExColorInfo][0];
	g_VideoDesc.SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
	g_VideoDesc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
	g_VideoDesc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
	g_VideoDesc.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
	g_VideoDesc.Format = (D3DFORMAT)VIDEO_MAIN_FORMAT;
	g_VideoDesc.InputSampleFreq.Numerator = VIDEO_FPS;
	g_VideoDesc.InputSampleFreq.Denominator = 1;
	g_VideoDesc.OutputFrameFreq.Numerator = VIDEO_FPS;
	g_VideoDesc.OutputFrameFreq.Denominator = 1;

	//
	// Query the video processor GUID.
	//
	UINT count;
	GUID* guids = NULL;

	hr = g_pDXVAVPS->GetVideoProcessorDeviceGuids(&g_VideoDesc, &count, &guids);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("GetVideoProcessorDeviceGuids failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	//
	// Create a DXVA2 device.
	//
	for (UINT i = 0; i < count; i++)
	{
		if (CreateDXVA2VPDevice(guids[i]))
		{
			break;
		}
	}

	CoTaskMemFree(guids);

	if (!g_pDXVAVPD)
	{
		DBGMSG((TEXT("Failed to create a DXVA2 device.\n")));
		return FALSE;
	}

	if (!InitializeVideoNV12())
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CTDXVA2Display::CreateDXVA2VPDevice(REFGUID guid)
{
	HRESULT hr;

	//
	// Query the supported render target format.
	//
	UINT i, count;
	D3DFORMAT* formats = NULL;

	hr = g_pDXVAVPS->GetVideoProcessorRenderTargets(guid,
		&g_VideoDesc,
		&count,
		&formats);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("GetVideoProcessorRenderTargets failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	for (i = 0; i < count; i++)
	{
		if (formats[i] == VIDEO_RENDER_TARGET_FORMAT)
		{
			break;
		}
	}

	CoTaskMemFree(formats);

	if (i >= count)
	{
		DBGMSG((TEXT("GetVideoProcessorRenderTargets doesn't support that format.\n")));
		return FALSE;
	}

	//
	// Query video processor capabilities.
	//
	hr = g_pDXVAVPS->GetVideoProcessorCaps(guid,
		&g_VideoDesc,
		VIDEO_RENDER_TARGET_FORMAT,
		&g_VPCaps);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("GetVideoProcessorCaps failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	//
	// Check to see if the device is hardware device.
	//
	if (!(g_VPCaps.DeviceCaps & DXVA2_VPDev_HardwareDevice))
	{
		DBGMSG((TEXT("The DXVA2 device isn't a hardware device.\n")));
		return FALSE;
	}

	//
	// This is a progressive device and we cannot provide any reference sample.
	//
	if (g_VPCaps.NumForwardRefSamples > 0 || g_VPCaps.NumBackwardRefSamples > 0)
	{
		DBGMSG((TEXT("NumForwardRefSamples or NumBackwardRefSamples is greater than 0.\n")));
		return FALSE;
	}

	//
	// Check to see if the device supports all the VP operations we want.
	//
	if ((g_VPCaps.VideoProcessorOperations & VIDEO_REQUIED_OP) != VIDEO_REQUIED_OP)
	{
		DBGMSG((TEXT("The DXVA2 device doesn't support the VP operations.\n")));
		return FALSE;
	}

	//
	// Create a main stream surface.
	//
	hr = g_pDXVAVPS->CreateSurface(VIDEO_MAIN_WIDTH,
		VIDEO_MAIN_HEIGHT,
		0,
		(D3DFORMAT)VIDEO_MAIN_FORMAT,
		g_VPCaps.InputPool,
		0,
		DXVA2_VideoSoftwareRenderTarget,
		&g_pMainStream,
		NULL);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("CreateSurface(MainStream) failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	//
	// Query ProcAmp ranges.
	//
	DXVA2_ValueRange range;
	for (i = 0; i < ARRAYSIZE(g_ProcAmpRanges); i++)
	{
		if (g_VPCaps.ProcAmpControlCaps & (1 << i))
		{
			hr = g_pDXVAVPS->GetProcAmpRange(guid,
				&g_VideoDesc,
				VIDEO_RENDER_TARGET_FORMAT,
				1 << i,
				&range);

			if (FAILED(hr))
			{
				DBGMSG((TEXT("GetProcAmpRange failed with error 0x%x.\n"), hr));
				return FALSE;
			}

			//
			// Reset to default value if the range is changed.
			//
			if (memcmp(&range, &(g_ProcAmpRanges[i]), sizeof(DXVA2_ValueRange)))
			// if (range != (g_ProcAmpRanges[i]))
			{
				g_ProcAmpRanges[i] = range;
				g_ProcAmpValues[i] = range.DefaultValue;
				g_ProcAmpSteps[i] = ComputeLongSteps(range);
			}
		}
	}

	//
	// Finally create a video processor device.
	//
	hr = g_pDXVAVPS->CreateVideoProcessor(guid,
		&g_VideoDesc,
		VIDEO_RENDER_TARGET_FORMAT,
		SUB_STREAM_COUNT,
		&g_pDXVAVPD);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("CreateVideoProcessor failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	g_GuidVP = guid;

	return TRUE;
}

static VOID FillRectangle(
	D3DLOCKED_RECT& lr,
	const UINT sx,
	const UINT sy,
	const UINT ex,
	const UINT ey,
	const DWORD color)
{
	BYTE* p = (BYTE*)lr.pBits;

	p += lr.Pitch * sy;

	for (UINT y = sy; y < ey; y++, p += lr.Pitch)
	{
		for (UINT x = sx; x < ex; x++)
		{
			((DWORD*)p)[x] = color;
		}
	}
}

BOOL CTDXVA2Display::InitializeVideo()
{
	HRESULT hr;

	//
	// Draw the main stream (SMPTE color bars).
	//
	D3DLOCKED_RECT lr;

	hr = g_pMainStream->LockRect(&lr, NULL, D3DLOCK_NOSYSLOCK);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("LockRect failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	// YUY2 is two pixels per DWORD.
	const UINT dx = VIDEO_MAIN_WIDTH / 2;

	// First row stripes.
	const UINT y1 = VIDEO_MAIN_HEIGHT * 2 / 3;

	FillRectangle(lr, dx * 0 / 7, 0, dx * 1 / 7, y1, RGBtoYUY2(RGB_WHITE_75pc));
	FillRectangle(lr, dx * 1 / 7, 0, dx * 2 / 7, y1, RGBtoYUY2(RGB_YELLOW_75pc));
	FillRectangle(lr, dx * 2 / 7, 0, dx * 3 / 7, y1, RGBtoYUY2(RGB_CYAN_75pc));
	FillRectangle(lr, dx * 3 / 7, 0, dx * 4 / 7, y1, RGBtoYUY2(RGB_GREEN_75pc));
	FillRectangle(lr, dx * 4 / 7, 0, dx * 5 / 7, y1, RGBtoYUY2(RGB_MAGENTA_75pc));
	FillRectangle(lr, dx * 5 / 7, 0, dx * 6 / 7, y1, RGBtoYUY2(RGB_RED_75pc));
	FillRectangle(lr, dx * 6 / 7, 0, dx * 7 / 7, y1, RGBtoYUY2(RGB_BLUE_75pc));

	// Second row stripes.
	const UINT y2 = VIDEO_MAIN_HEIGHT * 3 / 4;

	FillRectangle(lr, dx * 0 / 7, y1, dx * 1 / 7, y2, RGBtoYUY2(RGB_BLUE_75pc));
	FillRectangle(lr, dx * 1 / 7, y1, dx * 2 / 7, y2, RGBtoYUY2(RGB_BLACK));
	FillRectangle(lr, dx * 2 / 7, y1, dx * 3 / 7, y2, RGBtoYUY2(RGB_MAGENTA_75pc));
	FillRectangle(lr, dx * 3 / 7, y1, dx * 4 / 7, y2, RGBtoYUY2(RGB_BLACK));
	FillRectangle(lr, dx * 4 / 7, y1, dx * 5 / 7, y2, RGBtoYUY2(RGB_CYAN_75pc));
	FillRectangle(lr, dx * 5 / 7, y1, dx * 6 / 7, y2, RGBtoYUY2(RGB_BLACK));
	FillRectangle(lr, dx * 6 / 7, y1, dx * 7 / 7, y2, RGBtoYUY2(RGB_WHITE_75pc));

	// Third row stripes.
	const UINT y3 = VIDEO_MAIN_HEIGHT;

	FillRectangle(lr, dx * 0 / 28, y2, dx * 5 / 28, y3, RGBtoYUY2(RGB_I));
	FillRectangle(lr, dx * 5 / 28, y2, dx * 10 / 28, y3, RGBtoYUY2(RGB_WHITE));
	FillRectangle(lr, dx * 10 / 28, y2, dx * 15 / 28, y3, RGBtoYUY2(RGB_Q));
	FillRectangle(lr, dx * 15 / 28, y2, dx * 20 / 28, y3, RGBtoYUY2(RGB_BLACK));
	FillRectangle(lr, dx * 20 / 28, y2, dx * 16 / 21, y3, RGBtoYUY2(RGB_BLACK_n4pc));
	FillRectangle(lr, dx * 16 / 21, y2, dx * 17 / 21, y3, RGBtoYUY2(RGB_BLACK));
	FillRectangle(lr, dx * 17 / 21, y2, dx * 6 / 7, y3, RGBtoYUY2(RGB_BLACK_p4pc));
	FillRectangle(lr, dx * 6 / 7, y2, dx * 7 / 7, y3, RGBtoYUY2(RGB_BLACK));

	hr = g_pMainStream->UnlockRect();

	if (FAILED(hr))
	{
		DBGMSG((TEXT("UnlockRect failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	return TRUE;
}

BOOL CTDXVA2Display::InitializeVideoNV12()
{
	HRESULT hr;

	//
	// Draw the main stream (SMPTE color bars).
	//
	D3DLOCKED_RECT lr;

	hr = g_pMainStream->LockRect(&lr, NULL, D3DLOCK_NOSYSLOCK);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("LockRect failed with error 0x%x.\n"), hr));
		return FALSE;
	}


	BYTE* p = (BYTE*)lr.pBits;

	BYTE Y, U, V;
	RGBtoYUY(RGB_YELLOW_75pc, &Y, &U, &V);

	// Y
	for (UINT i = 0; i < VIDEO_MAIN_HEIGHT; i++)
	{
		memset(p + (i * lr.Pitch), Y, VIDEO_MAIN_WIDTH);
	}

	UINT uvHeight = VIDEO_MAIN_HEIGHT / 2;
	UINT uvWidth = VIDEO_MAIN_WIDTH / 2;
	for (UINT i = VIDEO_MAIN_HEIGHT; i < uvHeight; i++)
	{
		BYTE *uv = p + (i * lr.Pitch);
		for (UINT j = 0; j < uvWidth; j++)
		{
			uv[0] = U;
			uv[1] = V;
			uv += 2;
		}
	}


	hr = g_pMainStream->UnlockRect();

	if (FAILED(hr))
	{
		DBGMSG((TEXT("UnlockRect failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	return TRUE;
}

static DXVA2_AYUVSample16 GetBackgroundColor()
{
	const D3DCOLOR yuv = RGBtoYUV(BACKGROUND_COLORS[g_BackgroundColor]);

	const BYTE Y = LOBYTE(HIWORD(yuv));
	const BYTE U = HIBYTE(LOWORD(yuv));
	const BYTE V = LOBYTE(LOWORD(yuv));

	DXVA2_AYUVSample16 color;

	color.Cr = V * 0x100;
	color.Cb = U * 0x100;
	color.Y = Y * 0x100;
	color.Alpha = 0xFFFF;

	return color;
}

static RECT ScaleRectangle(const RECT& input, const RECT& src, const RECT& dst)
{
	RECT rect;

	UINT src_dx = src.right - src.left;
	UINT src_dy = src.bottom - src.top;

	UINT dst_dx = dst.right - dst.left;
	UINT dst_dy = dst.bottom - dst.top;

	//
	// Scale input rectangle within src rectangle to dst rectangle.
	//
	rect.left = input.left   * dst_dx / src_dx;
	rect.right = input.right  * dst_dx / src_dx;
	rect.top = input.top    * dst_dy / src_dy;
	rect.bottom = input.bottom * dst_dy / src_dy;

	return rect;
}

BOOL CTDXVA2Display::ProcessVideo()
{
	HRESULT hr;

	if (!g_pD3DD9)
	{
		return FALSE;
	}

	RECT rect;
	GetClientRect(g_Hwnd, &rect);
	if (IsRectEmpty(&rect))
	{
		return TRUE;
	}

	// Check the current status of D3D9 device.
	hr = g_pD3DD9->TestCooperativeLevel();
	switch (hr)
	{
	case D3D_OK:
		break;
	case D3DERR_DEVICELOST:
		DBGMSG((TEXT("TestCooperativeLevel returned D3DERR_DEVICELOST.\n")));
		return TRUE;
	case D3DERR_DEVICENOTRESET:
		DBGMSG((TEXT("TestCooperativeLevel returned D3DERR_DEVICENOTRESET.\n")));
		if (!ResetDevice())
		{
			return FALSE;
		}
		break;
	default:
		DBGMSG((TEXT("TestCooperativeLevel failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	DXVA2_VideoProcessBltParams blt = { 0 };
	DXVA2_VideoSample samples[2] = { 0 };


	RECT client;
	GetClientRect(g_Hwnd, &client);

	RECT target;
	target.left = client.left + (client.right - client.left) / 2 * (100 - g_TargetWidthPercent) / 100;
	target.right = client.right - (client.right - client.left) / 2 * (100 - g_TargetWidthPercent) / 100;
	target.top = client.top + (client.bottom - client.top) / 2 * (100 - g_TargetHeightPercent) / 100;
	target.bottom = client.bottom - (client.bottom - client.top) / 2 * (100 - g_TargetHeightPercent) / 100;

	//
	// Initialize VPBlt parameters.
	//
	blt.TargetFrame = 0;
	blt.TargetRect = target;

	// DXVA2_VideoProcess_Constriction
	blt.ConstrictionSize.cx = target.right - target.left;
	blt.ConstrictionSize.cy = target.bottom - target.top;

	blt.BackgroundColor = GetBackgroundColor();

	// DXVA2_VideoProcess_YUV2RGBExtended
	blt.DestFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
	blt.DestFormat.NominalRange = EX_COLOR_INFO[g_ExColorInfo][1];
	blt.DestFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
	blt.DestFormat.VideoLighting = DXVA2_VideoLighting_dim;
	blt.DestFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
	blt.DestFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;

	blt.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

	// DXVA2_ProcAmp_Brightness
	blt.ProcAmpValues.Brightness = g_ProcAmpValues[0];

	// DXVA2_ProcAmp_Contrast
	blt.ProcAmpValues.Contrast = g_ProcAmpValues[1];

	// DXVA2_ProcAmp_Hue
	blt.ProcAmpValues.Hue = g_ProcAmpValues[2];

	// DXVA2_ProcAmp_Saturation
	blt.ProcAmpValues.Saturation = g_ProcAmpValues[3];

	// DXVA2_VideoProcess_AlphaBlend
	blt.Alpha = DXVA2_Fixed32OpaqueAlpha();

	//
	// Initialize main stream video sample.
	//
	samples[0].Start = 0;
	samples[0].End = 100000000;

	// DXVA2_VideoProcess_YUV2RGBExtended
	samples[0].SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
	samples[0].SampleFormat.NominalRange = DXVA2_NominalRange_16_235;
	samples[0].SampleFormat.VideoTransferMatrix = EX_COLOR_INFO[g_ExColorInfo][0];
	samples[0].SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
	samples[0].SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
	samples[0].SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
	samples[0].SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

	samples[0].SrcSurface = g_pMainStream;

	// DXVA2_VideoProcess_SubRects
	samples[0].SrcRect = g_SrcRect;

	// DXVA2_VideoProcess_StretchX, Y
	samples[0].DstRect = ScaleRectangle(g_DstRect, VIDEO_MAIN_RECT, client);

	// DXVA2_VideoProcess_PlanarAlpha
	samples[0].PlanarAlpha = DXVA2FloatToFixed(float(g_PlanarAlphaValue) / 0xFF);

	if (g_TargetWidthPercent < 100 || g_TargetHeightPercent < 100)
	{
		hr = g_pD3DD9->ColorFill(g_pD3DRT, NULL, D3DCOLOR_XRGB(0, 0, 0));

		if (FAILED(hr))
		{
			DBGMSG((TEXT("ColorFill failed with error 0x%x.\n"), hr));
		}
	}

	hr = g_pDXVAVPD->VideoProcessBlt(g_pD3DRT,
		&blt,
		samples,
		1,
		NULL);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("VideoProcessBlt failed with error 0x%x.\n"), hr));
	}

	//
	// Re-enable DWM queuing if it is not enabled.
	//
	EnableDwmQueuing();

	hr = g_pD3DD9->Present(NULL, NULL, NULL, NULL);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("Present failed with error 0x%x.\n"), hr));
	}

	return TRUE;
}

BOOL CTDXVA2Display::ResetDevice(BOOL bChangeWindowMode)
{
	HRESULT hr;

	if (bChangeWindowMode)
	{
		g_bWindowed = !g_bWindowed;

		if (!ChangeFullscreenMode(!g_bWindowed))
		{
			return FALSE;
		}
	}

	if (g_pD3DD9)
	{
		//
		// Destroy DXVA2 device because it may be holding any D3D9 resources.
		//
		DestroyDXVA2();

		if (g_bWindowed)
		{
			g_D3DPP.BackBufferWidth = 0;
			g_D3DPP.BackBufferHeight = 0;
		}
		else
		{
			g_D3DPP.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
			g_D3DPP.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
		}

		g_D3DPP.Windowed = g_bWindowed;

		//
		// Reset will change the parameters, so use a copy instead.
		//
		D3DPRESENT_PARAMETERS d3dpp = g_D3DPP;

		hr = g_pD3DD9->Reset(&d3dpp);

		if (FAILED(hr))
		{
			DBGMSG((TEXT("Reset failed with error 0x%x.\n"), hr));
		}

		if (SUCCEEDED(hr) && InitializeDXVA2())
		{
			return TRUE;
		}

		//
		// If either Reset didn't work or failed to initialize DXVA2 device,
		// try to recover by recreating the devices from the scratch.
		//
		DestroyDXVA2();
		DestroyD3D9();
	}

	if (InitializeD3D9() && InitializeDXVA2())
	{
		return TRUE;
	}

	//
	// Fallback to Window mode, if failed to initialize Fullscreen mode.
	//
	if (g_bWindowed)
	{
		return FALSE;
	}

	DestroyDXVA2();
	DestroyD3D9();

	if (!ChangeFullscreenMode(FALSE))
	{
		return FALSE;
	}

	g_bWindowed = TRUE;

	if (InitializeD3D9() && InitializeDXVA2())
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CTDXVA2Display::EnableDwmQueuing()
{
	HRESULT hr;

	//
	// DWM is not available.
	//
	if (!g_hDwmApiDLL)
	{
		return TRUE;
	}

	//
	// Check to see if DWM is currently enabled.
	//
	BOOL bDWM = FALSE;

	hr = ((PFNDWMISCOMPOSITIONENABLED)g_pfnDwmIsCompositionEnabled)(&bDWM);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("DwmIsCompositionEnabled failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	//
	// DWM queuing is disabled when DWM is disabled.
	//
	if (!bDWM)
	{
		g_bDwmQueuing = FALSE;
		return TRUE;
	}

	//
	// DWM queuing is enabled already.
	//
	if (g_bDwmQueuing)
	{
		return TRUE;
	}

	//
	// Retrieve DWM refresh count of the last vsync.
	//
	DWM_TIMING_INFO dwmti = { 0 };

	dwmti.cbSize = sizeof(dwmti);

	hr = ((PFNDWMGETCOMPOSITIONTIMINGINFO)g_pfnDwmGetCompositionTimingInfo)(NULL, &dwmti);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("DwmGetCompositionTimingInfo failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	//
	// Enable DWM queuing from the next refresh.
	//
	DWM_PRESENT_PARAMETERS dwmpp = { 0 };

	dwmpp.cbSize = sizeof(dwmpp);
	dwmpp.fQueue = TRUE;
	dwmpp.cRefreshStart = dwmti.cRefresh + 1;
	dwmpp.cBuffer = DWM_BUFFER_COUNT;
	dwmpp.fUseSourceRate = FALSE;
	dwmpp.cRefreshesPerFrame = 1;
	dwmpp.eSampling = DWM_SOURCE_FRAME_SAMPLING_POINT;

	hr = ((PFNDWMSETPRESENTPARAMETERS)g_pfnDwmSetPresentParameters)(g_Hwnd, &dwmpp);

	if (FAILED(hr))
	{
		DBGMSG((TEXT("DwmSetPresentParameters failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	//
	// DWM queuing is enabled.
	//
	g_bDwmQueuing = TRUE;

	return TRUE;
}

BOOL CTDXVA2Display::ChangeFullscreenMode(BOOL bFullscreen)
{
	//
	// Mark the mode change in progress to prevent the device is being reset in OnSize.
	// This is because these API calls below will generate WM_SIZE messages.
	//
	g_bInModeChange = TRUE;

	if (bFullscreen)
	{
		//
		// Save the window position.
		//
		if (!GetWindowRect(g_Hwnd, &g_RectWindow))
		{
			DBGMSG((TEXT("GetWindowRect failed with error %d.\n"), GetLastError()));
			return FALSE;
		}

		if (!SetWindowLongPtr(g_Hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE))
		{
			DBGMSG((TEXT("SetWindowLongPtr failed with error %d.\n"), GetLastError()));
			return FALSE;
		}

		if (!SetWindowPos(g_Hwnd,
			HWND_NOTOPMOST,
			0,
			0,
			GetSystemMetrics(SM_CXSCREEN),
			GetSystemMetrics(SM_CYSCREEN),
			SWP_FRAMECHANGED))
		{
			DBGMSG((TEXT("SetWindowPos failed with error %d.\n"), GetLastError()));
			return FALSE;
		}
	}
	else
	{
		if (!SetWindowLongPtr(g_Hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE))
		{
			DBGMSG((TEXT("SetWindowLongPtr failed with error %d.\n"), GetLastError()));
			return FALSE;
		}

		//
		// Restore the window position.
		//
		if (!SetWindowPos(g_Hwnd,
			HWND_NOTOPMOST,
			g_RectWindow.left,
			g_RectWindow.top,
			g_RectWindow.right - g_RectWindow.left,
			g_RectWindow.bottom - g_RectWindow.top,
			SWP_FRAMECHANGED))
		{
			DBGMSG((TEXT("SetWindowPos failed with error %d.\n"), GetLastError()));
			return FALSE;
		}
	}

	g_bInModeChange = FALSE;

	return TRUE;
}

VOID CTDXVA2Display::DestroyDXVA2()
{
	if (g_pMainStream)
	{
		g_pMainStream->Release();
		g_pMainStream = NULL;
	}

	if (g_pDXVAVPD)
	{
		g_pDXVAVPD->Release();
		g_pDXVAVPD = NULL;
	}

	if (g_pDXVAVPS)
	{
		g_pDXVAVPS->Release();
		g_pDXVAVPS = NULL;
	}

	if (g_pD3DRT)
	{
		g_pD3DRT->Release();
		g_pD3DRT = NULL;
	}
}

VOID CTDXVA2Display::DestroyD3D9()
{
	if (g_pD3DD9)
	{
		g_pD3DD9->Release();
		g_pD3DD9 = NULL;
	}

	if (g_pD3D9)
	{
		g_pD3D9->Release();
		g_pD3D9 = NULL;
	}
}