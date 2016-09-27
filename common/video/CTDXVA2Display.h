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
	void SetVideoFormat(DWORD format);
	void SetFrameResolution(int width, int height);
	IDirect3DDeviceManager9 *DeviceManager();
	void OnWindowSizeChanged();
	int Display(IDirect3DSurface9 *surface);
private:
	BOOL InitializeModule();
	BOOL InitializeD3D9();
	BOOL InitializeDXVA2();
	BOOL CreateDXVA2VPDevice(REFGUID guid);
	BOOL ProcessVideo();
	BOOL LockDevice(HANDLE *handle, IDirect3DDevice9 **d3dd9);
	BOOL UnlockDevice(HANDLE handle);
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
	HMODULE m_dwmapi_dll;
	PVOID m_fn_dwm_is_composition_enabled;
	PVOID m_fn_dwm_get_composition_timing_info;
	PVOID m_fn_dwm_set_present_parameters;

	HWND    m_wnd;
	BOOL	m_windowed;
	BOOL    m_in_mode_change;
	RECT	m_rect_window;
	UINT	g_target_width_percent;
	UINT	m_target_height_percent;
	BOOL	g_dwm_queuing;

	DWORD	m_video_format;
	UINT	m_frame_width;
	UINT	m_frame_height;

	IDirect3DDeviceManager9	*m_device_manager;
	unsigned int			m_reset_token;
	IDirect3D9*				m_d3d9;
	IDirect3DDevice9*		m_d3dd9;
	IDirect3DSurface9*		m_d3drt;
	IDirect3DSurface9*		m_main_stream;

	D3DPRESENT_PARAMETERS m_d3dpp;

	IDirectXVideoProcessorService* m_dxvaps;
	IDirectXVideoProcessor*        m_dxvapd;

	

	GUID                     m_vp_guid;
	DXVA2_VideoDesc          m_video_desc;
	DXVA2_VideoProcessorCaps m_vp_caps;

	INT m_ex_color_info;

	DXVA2_ValueRange m_proc_amp_ranges[4];
	DXVA2_Fixed32	m_proc_amp_values[4];
	INT				m_proc_amp_steps[4];
};

