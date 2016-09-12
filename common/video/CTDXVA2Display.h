#pragma once

#include <windows.h>
#include <dwmapi.h>
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "d3d9.lib")

#define CTDXVADISP_EC_OK		0
#define CTDXVADISP_EC_FAILURE	-1

class CTDXVA2Display
{
public:
	CTDXVA2Display();
	virtual ~CTDXVA2Display();
	int Init(HWND hwnd);
	int Display(IDirect3DSurface9 *surface);
private:
	BOOL InitializeModule();
	BOOL InitializeD3D9();
	BOOL InitializeDXVA2();
	BOOL CreateDXVA2VPDevice(REFGUID guid);
	BOOL InitializeVideo();
	BOOL InitializeVideoNV12();
	BOOL ProcessVideo();
	BOOL ResetDevice(BOOL bChangeWindowMode = FALSE);
	BOOL EnableDwmQueuing();
	BOOL ChangeFullscreenMode(BOOL bFullscreen);
	VOID DestroyDXVA2();
	VOID DestroyD3D9();


private:
	//HMODULE m_rgb9rast_dll;
	//HMODULE m_dwmapi_dll;
	//PVOID m_fn_dwm_is_composition_enabled;
	//PVOID m_fn_dwm_get_composition_timing_info;
	//PVOID m_fn_dwm_set_present_parameters;

	HMODULE m_rgb9rast_dll;
	HMODULE g_hDwmApiDLL;
	PVOID g_pfnDwmIsCompositionEnabled;
	PVOID g_pfnDwmGetCompositionTimingInfo;
	PVOID g_pfnDwmSetPresentParameters;

	HWND    g_Hwnd;
	BOOL	g_bWindowed;
	BOOL    g_bInModeChange;
	RECT	g_RectWindow;
	UINT	g_TargetWidthPercent;
	UINT	g_TargetHeightPercent;
	BOOL	g_bDwmQueuing;

	HANDLE  g_hTimer;
	BOOL	g_bD3D9HW;
	BOOL	g_bDXVA2HW;

	IDirect3D9*        g_pD3D9;
	IDirect3DDevice9*  g_pD3DD9;
	IDirect3DSurface9* g_pD3DRT;

	D3DPRESENT_PARAMETERS g_D3DPP = { 0 };

	IDirectXVideoProcessorService* g_pDXVAVPS;
	IDirectXVideoProcessor*        g_pDXVAVPD;

	IDirect3DSurface9* g_pMainStream;
	RECT g_SrcRect;
	RECT g_DstRect;

	GUID                     g_GuidVP;
	DXVA2_VideoDesc          g_VideoDesc;
	DXVA2_VideoProcessorCaps g_VPCaps;

	INT g_ExColorInfo;

	DXVA2_ValueRange g_ProcAmpRanges[4];
	DXVA2_Fixed32	g_ProcAmpValues[4];
	INT				g_ProcAmpSteps[4];
};

