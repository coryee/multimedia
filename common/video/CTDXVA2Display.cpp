#include "CTDXVA2Display.h"
#include "CTVideoUtil.h"
#include <stdio.h>
#include <mmsyscom.h>
#include <strsafe.h>

#define CTDXVA2DISPInfo		printf
#define CTDXVA2DISPError	printf
#define DBGMSG(x) {DbgPrint(TEXT("%s(%u) : "), TEXT(__FILE__), __LINE__); DbgPrint x;}
#define DXVA2DISPDEBUG

VOID DbgPrint(PCTSTR format, ...)
{
	va_list args;
	va_start(args, format);

	TCHAR string[MAX_PATH];

	if (SUCCEEDED(StringCbVPrintf(string, sizeof(string), format, args)))
	{
		OutputDebugString(string);
	}
	else
	{
		DebugBreak();
	}
}


const D3DCOLOR BACKGROUND_COLOR = RGB_BLACK; // RGB_BLACK

const BYTE DEFAULT_PLANAR_ALPHA_VALUE = 0xFF;

const UINT VIDEO_REQUIED_OP = DXVA2_VideoProcess_YUV2RGB |
	DXVA2_VideoProcess_StretchX |
	DXVA2_VideoProcess_StretchY |
	DXVA2_VideoProcess_SubRects |
	DXVA2_VideoProcess_SubStreams;

const D3DFORMAT VIDEO_RENDER_TARGET_FORMAT = D3DFMT_X8R8G8B8;
// const D3DFORMAT VIDEO_MAIN_FORMAT = D3DFMT_YUY2; 
const DWORD VIDEO_MAIN_FORMAT = MAKEFOURCC('N', 'V', '1', '2');

const UINT BACK_BUFFER_COUNT = 1;
const UINT DWM_BUFFER_COUNT = 4;

const UINT VIDEO_MAIN_WIDTH = 640;
const UINT VIDEO_MAIN_HEIGHT = 480;
//const UINT VIDEO_MAIN_WIDTH = 4096;
//const UINT VIDEO_MAIN_HEIGHT = 2048;

const UINT VIDEO_FPS = 60;

const UINT EX_COLOR_INFO[][2] = {
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

// Type definitions.
typedef HRESULT(WINAPI * PFNDWMISCOMPOSITIONENABLED)(__out BOOL* pfEnabled);
typedef HRESULT(WINAPI * PFNDWMGETCOMPOSITIONTIMINGINFO)(__in HWND hwnd, __out DWM_TIMING_INFO* pTimingInfo);
typedef HRESULT(WINAPI * PFNDWMSETPRESENTPARAMETERS)(__in HWND hwnd, __inout DWM_PRESENT_PARAMETERS* pPresentParams);

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

static VOID FillRectangle(D3DLOCKED_RECT& lr, const UINT sx, const UINT sy, const UINT ex, const UINT ey, const DWORD color)
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

static DXVA2_AYUVSample16 GetBackgroundColor()
{
	const D3DCOLOR yuv = RGBtoYUV(BACKGROUND_COLOR);

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

	// Scale input rectangle within src rectangle to dst rectangle.
	rect.left = input.left   * dst_dx / src_dx;
	rect.right = input.right  * dst_dx / src_dx;
	rect.top = input.top    * dst_dy / src_dy;
	rect.bottom = input.bottom * dst_dy / src_dy;

	return rect;
}


CTDXVA2Display::CTDXVA2Display()
{
	m_wnd = NULL;
	m_windowed = 1;
	m_in_mode_change = FALSE;
	m_rect_window = { 0 };
	g_target_width_percent = 100;
	m_target_height_percent = 100;
	g_dwm_queuing = FALSE;

	m_video_format = VIDEO_MAIN_FORMAT;
	m_frame_width = VIDEO_MAIN_WIDTH;
	m_frame_height = VIDEO_MAIN_HEIGHT;

	m_d3d9 = NULL;
	m_d3dd9 = NULL;
	m_d3drt = NULL;
	memset(&m_d3dpp, 0, sizeof(m_d3dpp));

	m_device_manager = NULL;
	m_dxvaps = NULL;
	m_dxvapd = NULL;

	m_main_stream = NULL;

	m_vp_guid = { 0 };
	m_video_desc = { 0 };
	m_vp_caps = { 0 };

	m_ex_color_info = 0;
	memset(m_proc_amp_ranges, 0, sizeof(m_proc_amp_ranges));
	memset(m_proc_amp_values, 0, sizeof(m_proc_amp_values));
	memset(m_proc_amp_steps, 0, sizeof(m_proc_amp_steps));
}

CTDXVA2Display::~CTDXVA2Display()
{
	DestroyDXVA2();
	DestroyD3D9();
}

int CTDXVA2Display::Init(HWND hwnd)
{
	m_wnd = hwnd;
	if (!InitializeModule())
		return CTDXVADISP_EC_FAILURE;
	if (!InitializeD3D9())
		return CTDXVADISP_EC_FAILURE;
	if (!InitializeDXVA2())
		return CTDXVADISP_EC_FAILURE;

	return CTDXVADISP_EC_OK;
}

void CTDXVA2Display::SetVideoFormat(DWORD format)
{
	m_video_format = format;
}

void CTDXVA2Display::SetFrameResolution(int width, int height)
{
	if (m_frame_width == width && m_frame_height == height)
		return;

	m_frame_width = width;
	m_frame_height = height;
#if 0
	if (m_main_stream)
	{
		m_main_stream->Release();
		m_main_stream = NULL;
	}

	HRESULT hr;
	// Create a main stream surface.
	hr = m_dxvaps->CreateSurface(m_frame_width,
		m_frame_height,
		0,
		(D3DFORMAT)m_video_format,
		m_vp_caps.InputPool,
		0,
		DXVA2_VideoSoftwareRenderTarget,
		&m_main_stream,
		NULL);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("CreateSurface(MainStream) failed with error 0x%x.\n"), hr));
		return;
	}

	FillNV12SurfaceWithColor(m_main_stream, m_frame_width, m_frame_height, RGB_GREEN_75pc);
#endif // DXVA2DISPDEBUG
}

IDirect3DDeviceManager9 *CTDXVA2Display::DeviceManager()
{
	return m_device_manager;
}

void CTDXVA2Display::OnWindowSizeChanged()
{

}



int CTDXVA2Display::Display(IDirect3DSurface9 *surface)
{
	m_main_stream = surface;

	ProcessVideo();
	return CTDXVADISP_EC_OK;
}

BOOL CTDXVA2Display::InitializeModule()
{
	// Load these DLLs dynamically because these may not be available prior to Vista.
	m_rgb9rast_dll = LoadLibrary(TEXT("rgb9rast.dll"));
	if (!m_rgb9rast_dll) {
		CTDXVA2DISPError("LoadLibrary(rgb9rast.dll) failed with error %d.\n", GetLastError());
	}

	m_dwmapi_dll = LoadLibrary(TEXT("dwmapi.dll"));
	if (!m_dwmapi_dll) {
		CTDXVA2DISPError("LoadLibrary(dwmapi.dll) failed with error %d.\n", GetLastError());
		goto SKIP_DWMAPI;
	}

	m_fn_dwm_is_composition_enabled = GetProcAddress(m_dwmapi_dll, "DwmIsCompositionEnabled");
	if (!m_fn_dwm_is_composition_enabled) {
		CTDXVA2DISPError("GetProcAddress(DwmIsCompositionEnabled) failed with error %d.\n", GetLastError());
		return FALSE;
	}

	m_fn_dwm_get_composition_timing_info = GetProcAddress(m_dwmapi_dll, "DwmGetCompositionTimingInfo");
	if (!m_fn_dwm_get_composition_timing_info) {
		CTDXVA2DISPError("GetProcAddress(DwmGetCompositionTimingInfo) failed with error %d.\n", GetLastError());
		return FALSE;
	}

	m_fn_dwm_set_present_parameters = GetProcAddress(m_dwmapi_dll, "DwmSetPresentParameters");
	if (!m_fn_dwm_set_present_parameters) {
		CTDXVA2DISPError("GetProcAddress(DwmSetPresentParameters) failed with error %d.\n", GetLastError());
		return FALSE;
	}

SKIP_DWMAPI:

	return TRUE;
}

BOOL CTDXVA2Display::InitializeD3D9()
{
	HRESULT hr;

	m_d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (!m_d3d9)
	{
		DBGMSG((TEXT("Direct3DCreate9 failed.\n")));
		return FALSE;
	}

	if (m_windowed)
	{
		m_d3dpp.BackBufferWidth = 0;
		m_d3dpp.BackBufferHeight = 0;
	}
	else
	{
		m_d3dpp.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
		m_d3dpp.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
	}

	m_d3dpp.BackBufferFormat = VIDEO_RENDER_TARGET_FORMAT;
	m_d3dpp.BackBufferCount = BACK_BUFFER_COUNT;
	m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	m_d3dpp.hDeviceWindow = m_wnd;
	m_d3dpp.Windowed = m_windowed;
	m_d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
	m_d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;


	// First try to create a hardware D3D9 device.
	hr = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		m_wnd,
		D3DCREATE_FPU_PRESERVE |
		D3DCREATE_MULTITHREADED |
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&m_d3dpp,
		&m_d3dd9);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("CreateDevice(HAL) failed with error 0x%x.\n"), hr));
	}


	hr = DXVA2CreateDirect3DDeviceManager9(&m_reset_token, &m_device_manager);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("DXVA2CreateDirect3DDeviceManager9 failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	hr = m_device_manager->ResetDevice(m_d3dd9, m_reset_token);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("DeviceManager:ResetDevice failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	return TRUE;	
}

BOOL CTDXVA2Display::InitializeDXVA2()
{
	HRESULT hr;

	// Retrieve a back buffer as the video render target.
	hr = m_d3dd9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_d3drt);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("GetBackBuffer failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	// Create DXVA2 Video Processor Service.
	hr = DXVA2CreateVideoService(m_d3dd9,
		IID_IDirectXVideoProcessorService,
		(VOID**)&m_dxvaps);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("DXVA2CreateVideoService failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	// Initialize the video descriptor.
	m_video_desc.SampleWidth = m_frame_width;
	m_video_desc.SampleHeight = m_frame_height;
	m_video_desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
	m_video_desc.SampleFormat.NominalRange = DXVA2_NominalRange_16_235;
	m_video_desc.SampleFormat.VideoTransferMatrix = EX_COLOR_INFO[m_ex_color_info][0];
	m_video_desc.SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
	m_video_desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
	m_video_desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
	m_video_desc.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
	m_video_desc.Format = (D3DFORMAT)m_video_format;
	m_video_desc.InputSampleFreq.Numerator = VIDEO_FPS;
	m_video_desc.InputSampleFreq.Denominator = 1;
	m_video_desc.OutputFrameFreq.Numerator = VIDEO_FPS;
	m_video_desc.OutputFrameFreq.Denominator = 1;

	// Query the video processor GUID.
	UINT count;
	GUID* guids = NULL;
	hr = m_dxvaps->GetVideoProcessorDeviceGuids(&m_video_desc, &count, &guids);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("GetVideoProcessorDeviceGuids failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	// Create a DXVA2 device.
	for (UINT i = 0; i < count; i++)
	{
		if (CreateDXVA2VPDevice(guids[i]))
		{
			break;
		}
	}
	CoTaskMemFree(guids);

	if (!m_dxvapd)
	{
		DBGMSG((TEXT("Failed to create a DXVA2 device.\n")));
		return FALSE;
	}

	return TRUE;
}

BOOL CTDXVA2Display::CreateDXVA2VPDevice(REFGUID guid)
{
	HRESULT hr;

	// Query the supported render target format.
	UINT i, count;
	D3DFORMAT* formats = NULL;

	hr = m_dxvaps->GetVideoProcessorRenderTargets(guid,
		&m_video_desc,
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

	// Query video processor capabilities.
	hr = m_dxvaps->GetVideoProcessorCaps(guid,
		&m_video_desc,
		VIDEO_RENDER_TARGET_FORMAT,
		&m_vp_caps);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("GetVideoProcessorCaps failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	// Check to see if the device is hardware device.
	if (!(m_vp_caps.DeviceCaps & DXVA2_VPDev_HardwareDevice))
	{
		DBGMSG((TEXT("The DXVA2 device isn't a hardware device.\n")));
		return FALSE;
	}

	// This is a progressive device and we cannot provide any reference sample.
	if (m_vp_caps.NumForwardRefSamples > 0 || m_vp_caps.NumBackwardRefSamples > 0)
	{
		DBGMSG((TEXT("NumForwardRefSamples or NumBackwardRefSamples is greater than 0.\n")));
		return FALSE;
	}

	// Check to see if the device supports all the VP operations we want.
	if ((m_vp_caps.VideoProcessorOperations & VIDEO_REQUIED_OP) != VIDEO_REQUIED_OP)
	{
		DBGMSG((TEXT("The DXVA2 device doesn't support the VP operations.\n")));
		return FALSE;
	}

#if 0
	// Create a main stream surface.
	hr = m_dxvaps->CreateSurface(m_frame_width,
		m_frame_height,
		0,
		(D3DFORMAT)m_video_format,
		m_vp_caps.InputPool,
		0,
		DXVA2_VideoSoftwareRenderTarget,
		&m_main_stream,
		NULL);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("CreateSurface(MainStream) failed with error 0x%x.\n"), hr));
		return FALSE;
	}
	FillNV12SurfaceWithColor(m_main_stream, m_frame_width, m_frame_height, RGB_GREEN_75pc);
#endif

	// Query ProcAmp ranges.
	DXVA2_ValueRange range;
	for (i = 0; i < ARRAYSIZE(m_proc_amp_ranges); i++)
	{
		if (m_vp_caps.ProcAmpControlCaps & (1 << i))
		{
			hr = m_dxvaps->GetProcAmpRange(guid,
				&m_video_desc,
				VIDEO_RENDER_TARGET_FORMAT,
				1 << i,
				&range);

			if (FAILED(hr))
			{
				DBGMSG((TEXT("GetProcAmpRange failed with error 0x%x.\n"), hr));
				return FALSE;
			}

			// Reset to default value if the range is changed.
			if (memcmp(&range, &(m_proc_amp_ranges[i]), sizeof(DXVA2_ValueRange))) // if (range != (g_ProcAmpRanges[i]))
			{
				m_proc_amp_ranges[i] = range;
				m_proc_amp_values[i] = range.DefaultValue;
				m_proc_amp_steps[i] = ComputeLongSteps(range);
			}
		}
	}

	// Finally create a video processor device.
	hr = m_dxvaps->CreateVideoProcessor(guid,
		&m_video_desc,
		VIDEO_RENDER_TARGET_FORMAT,
		0, //SUB_STREAM_COUNT,
		&m_dxvapd);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("CreateVideoProcessor failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	m_vp_guid = guid;

	return TRUE;
}

BOOL CTDXVA2Display::ProcessVideo()
{
	HRESULT hr;
	HANDLE device_hanlde;

	if (!LockDevice(&device_hanlde, &m_d3dd9))
		return FALSE;

	RECT rect;
	GetClientRect(m_wnd, &rect);
	if (IsRectEmpty(&rect))
		return TRUE;

	m_d3drt = NULL;
	hr = m_d3dd9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_d3drt);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("GetBackBuffer failed with error 0x%x.\n"), hr));
		return FALSE;
	}
	// Check the current status of D3D9 device.
	hr = m_d3dd9->TestCooperativeLevel();
	switch (hr)
	{
	case D3D_OK:
		break;
	case D3DERR_DEVICELOST:
		DBGMSG((TEXT("TestCooperativeLevel returned D3DERR_DEVICELOST.\n")));
		return TRUE;
	case D3DERR_DEVICENOTRESET:
		DBGMSG((TEXT("TestCooperativeLevel returned D3DERR_DEVICENOTRESET.\n")));
#if 0
		if (!ResetDevice())
		{
			return FALSE;
		}
#endif
		break;
	default:
		DBGMSG((TEXT("TestCooperativeLevel failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	RECT client;
	GetClientRect(m_wnd, &client);

	RECT target;
	target.left = client.left + (client.right - client.left) / 2 * (100 - g_target_width_percent) / 100;
	target.right = client.right - (client.right - client.left) / 2 * (100 - g_target_width_percent) / 100;
	target.top = client.top + (client.bottom - client.top) / 2 * (100 - m_target_height_percent) / 100;
	target.bottom = client.bottom - (client.bottom - client.top) / 2 * (100 - m_target_height_percent) / 100;

	DXVA2_VideoProcessBltParams blt = { 0 };
	DXVA2_VideoSample samples[1] = { 0 };

	// Initialize VPBlt parameters.
	blt.TargetFrame = 0;
	blt.TargetRect = target;

	// DXVA2_VideoProcess_Constriction
	blt.ConstrictionSize.cx = target.right - target.left;
	blt.ConstrictionSize.cy = target.bottom - target.top;

	blt.BackgroundColor = GetBackgroundColor();

	// DXVA2_VideoProcess_YUV2RGBExtended
	blt.DestFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
	blt.DestFormat.NominalRange = EX_COLOR_INFO[m_ex_color_info][1];
	blt.DestFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
	blt.DestFormat.VideoLighting = DXVA2_VideoLighting_dim;
	blt.DestFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
	blt.DestFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
	blt.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

	// DXVA2_ProcAmp_Brightness
	blt.ProcAmpValues.Brightness = m_proc_amp_values[0];
	// DXVA2_ProcAmp_Contrast
	blt.ProcAmpValues.Contrast = m_proc_amp_values[1];
	// DXVA2_ProcAmp_Hue
	blt.ProcAmpValues.Hue = m_proc_amp_values[2];
	// DXVA2_ProcAmp_Saturation
	blt.ProcAmpValues.Saturation = m_proc_amp_values[3];
	// DXVA2_VideoProcess_AlphaBlend
	blt.Alpha = DXVA2_Fixed32OpaqueAlpha();

	// Initialize main stream video sample.
//  	samples[0].Start = 0;
//  	samples[0].End = 10000;

	// DXVA2_VideoProcess_YUV2RGBExtended
	samples[0].SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
	samples[0].SampleFormat.NominalRange = DXVA2_NominalRange_16_235;
	samples[0].SampleFormat.VideoTransferMatrix = EX_COLOR_INFO[m_ex_color_info][0];
	samples[0].SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
	samples[0].SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
	samples[0].SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
	samples[0].SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

	samples[0].SrcSurface = m_main_stream;

	RECT src_rect = { 0, 0, (LONG)m_frame_width, (LONG)m_frame_height};
	// DXVA2_VideoProcess_SubRects
	samples[0].SrcRect = src_rect;
	// DXVA2_VideoProcess_StretchX, Y
	samples[0].DstRect = ScaleRectangle(src_rect, src_rect, client);
	// samples[0].DstRect = src_rect;

	// DXVA2_VideoProcess_PlanarAlpha
	samples[0].PlanarAlpha = DXVA2FloatToFixed(float(DEFAULT_PLANAR_ALPHA_VALUE) / 0xFF);

	if (g_target_width_percent < 100 || m_target_height_percent < 100)
	{
		hr = m_d3dd9->ColorFill(m_d3drt, NULL, D3DCOLOR_XRGB(0, 0, 0));

		if (FAILED(hr))
		{
			DBGMSG((TEXT("ColorFill failed with error 0x%x.\n"), hr));
		}
	}
	// E_INVALIDARG;
	// D3DERR_INVALIDCALL;
	hr = m_dxvapd->VideoProcessBlt(m_d3drt, &blt, samples, 1, NULL);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("VideoProcessBlt failed with error 0x%x.\n"), hr));
	}
	
	// Re-enable DWM queuing if it is not enabled.
	EnableDwmQueuing();

	hr = m_d3dd9->Present(NULL, NULL, NULL, NULL);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("Present failed with error 0x%x.\n"), hr));
	}

	UnlockDevice(device_hanlde);

	return TRUE;
}

BOOL CTDXVA2Display::LockDevice(HANDLE *handle, IDirect3DDevice9 **d3dd9)
{
	HRESULT hr;

	hr = m_device_manager->OpenDeviceHandle(handle);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("OpenDeviceHandle failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	hr = m_device_manager->LockDevice(*handle, d3dd9, TRUE);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("LockDevice failed with error 0x%x.\n"), hr));
		m_device_manager->CloseDeviceHandle(*handle);
		return FALSE;
	}

	return TRUE;
}

BOOL CTDXVA2Display::UnlockDevice(HANDLE handle)
{
	HRESULT hr;
	BOOL ret = TRUE;

	hr = m_device_manager->UnlockDevice(handle, FALSE);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("OpenDeviceHandle failed with error 0x%x.\n"), hr));
		ret = FALSE;
	}

	hr = m_device_manager->CloseDeviceHandle(handle);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("CloseDeviceHandle failed with error 0x%x.\n"), hr));
		ret = FALSE;
	}

	return TRUE;
}

BOOL CTDXVA2Display::ResetDevice(BOOL bChangeWindowMode)
{
	HRESULT hr;

	if (bChangeWindowMode)
	{
		m_windowed = !m_windowed;

		if (!ChangeFullscreenMode(!m_windowed))
		{
			return FALSE;
		}
	}

	if (m_d3dd9)
	{
		// Destroy DXVA2 device because it may be holding any D3D9 resources.
		DestroyDXVA2();

		if (m_windowed)
		{
			m_d3dpp.BackBufferWidth = 0;
			m_d3dpp.BackBufferHeight = 0;
		}
		else
		{
			m_d3dpp.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
			m_d3dpp.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
		}

		m_d3dpp.Windowed = m_windowed;

		// Reset will change the parameters, so use a copy instead.
		D3DPRESENT_PARAMETERS d3dpp = m_d3dpp;
		hr = m_d3dd9->Reset(&d3dpp);
		if (FAILED(hr))
		{
			DBGMSG((TEXT("Reset failed with error 0x%x.\n"), hr));
		}

		if (SUCCEEDED(hr) && InitializeDXVA2())
		{
			return TRUE;
		}

		// If either Reset didn't work or failed to initialize DXVA2 device,
		// try to recover by recreating the devices from the scratch.
		DestroyDXVA2();
		DestroyD3D9();
	}

	if (InitializeD3D9() && InitializeDXVA2())
	{
		return TRUE;
	}

	// Fallback to Window mode, if failed to initialize Fullscreen mode.
	if (m_windowed)
	{
		return FALSE;
	}

	DestroyDXVA2();
	DestroyD3D9();

	if (!ChangeFullscreenMode(FALSE))
	{
		return FALSE;
	}

	m_windowed = TRUE;

	if (InitializeD3D9() && InitializeDXVA2())
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CTDXVA2Display::EnableDwmQueuing()
{
	HRESULT hr;

	// DWM is not available.
	if (!m_dwmapi_dll)
	{
		return TRUE;
	}

	// Check to see if DWM is currently enabled.
	BOOL bDWM = FALSE;

	hr = ((PFNDWMISCOMPOSITIONENABLED)m_fn_dwm_is_composition_enabled)(&bDWM);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("DwmIsCompositionEnabled failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	// DWM queuing is disabled when DWM is disabled.
	if (!bDWM)
	{
		g_dwm_queuing = FALSE;
		return TRUE;
	}

	// DWM queuing is enabled already.
	if (g_dwm_queuing)
	{
		return TRUE;
	}

	// Retrieve DWM refresh count of the last vsync.
	DWM_TIMING_INFO dwmti = { 0 };
	dwmti.cbSize = sizeof(dwmti);

	hr = ((PFNDWMGETCOMPOSITIONTIMINGINFO)m_fn_dwm_get_composition_timing_info)(NULL, &dwmti);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("DwmGetCompositionTimingInfo failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	// Enable DWM queuing from the next refresh.
	DWM_PRESENT_PARAMETERS dwmpp = { 0 };
	dwmpp.cbSize = sizeof(dwmpp);
	dwmpp.fQueue = TRUE;
	dwmpp.cRefreshStart = dwmti.cRefresh + 1;
	dwmpp.cBuffer = DWM_BUFFER_COUNT;
	dwmpp.fUseSourceRate = FALSE;
	dwmpp.cRefreshesPerFrame = 1;
	dwmpp.eSampling = DWM_SOURCE_FRAME_SAMPLING_POINT;

	hr = ((PFNDWMSETPRESENTPARAMETERS)m_fn_dwm_set_present_parameters)(m_wnd, &dwmpp);
	if (FAILED(hr))
	{
		DBGMSG((TEXT("DwmSetPresentParameters failed with error 0x%x.\n"), hr));
		return FALSE;
	}

	// DWM queuing is enabled.
	g_dwm_queuing = TRUE;

	return TRUE;
}

BOOL CTDXVA2Display::ChangeFullscreenMode(BOOL bFullscreen)
{
	// Mark the mode change in progress to prevent the device is being reset in OnSize.
	// This is because these API calls below will generate WM_SIZE messages.
	m_in_mode_change = TRUE;

	if (bFullscreen)
	{
		// Save the window position.
		if (!GetWindowRect(m_wnd, &m_rect_window))
		{
			DBGMSG((TEXT("GetWindowRect failed with error %d.\n"), GetLastError()));
			return FALSE;
		}

		if (!SetWindowLongPtr(m_wnd, GWL_STYLE, WS_POPUP | WS_VISIBLE))
		{
			DBGMSG((TEXT("SetWindowLongPtr failed with error %d.\n"), GetLastError()));
			return FALSE;
		}

		if (!SetWindowPos(m_wnd,
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
		if (!SetWindowLongPtr(m_wnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE))
		{
			DBGMSG((TEXT("SetWindowLongPtr failed with error %d.\n"), GetLastError()));
			return FALSE;
		}

		// Restore the window position.
		if (!SetWindowPos(m_wnd,
			HWND_NOTOPMOST,
			m_rect_window.left,
			m_rect_window.top,
			m_rect_window.right - m_rect_window.left,
			m_rect_window.bottom - m_rect_window.top,
			SWP_FRAMECHANGED))
		{
			DBGMSG((TEXT("SetWindowPos failed with error %d.\n"), GetLastError()));
			return FALSE;
		}
	}

	m_in_mode_change = FALSE;

	return TRUE;
}

VOID CTDXVA2Display::DestroyDXVA2()
{
	if (m_dxvapd)
	{
		m_dxvapd->Release();
		m_dxvapd = NULL;
	}

	if (m_dxvaps)
	{
		m_dxvaps->Release();
		m_dxvaps = NULL;
	}

	if (m_d3drt)
	{
		m_d3drt->Release();
		m_d3drt = NULL;
	}
}

VOID CTDXVA2Display::DestroyD3D9()
{
	if (m_d3dd9)
	{
		m_d3dd9->Release();
		m_d3dd9 = NULL;
	}

	if (m_d3d9)
	{
		m_d3d9->Release();
		m_d3d9 = NULL;
	}

	if (m_device_manager)
	{
		m_device_manager->Release();
		m_device_manager = NULL;
	}
}