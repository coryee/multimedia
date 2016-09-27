#pragma once

#include "SDL.h"
#include <SDL_thread.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "CTAVBuffer.h"
#include "CTDisplayDefines.h"
#include "CTSDLDisplay.h"
#include "CTDXVA2Display.h"

#ifdef main
#undef main
#endif

typedef enum CTAVBufferIndex {
	CTDISP_BUFFER_INDEX_VIDEO = 0,
	CTDISP_BUFFER_INDEX_AUDIO,
	CTDISP_BUFFER_INDEX_NUM,
} CTAVBufferIndex;

class CTDisplay
{
public:
	CTDisplay();
	~CTDisplay();

	int Init(int is_hardware_accelerated = 0);
#ifdef _WIN32
	int Init(HWND hwnd, int is_hardware_accelerated = 0);
#endif
	int SetDeviceManager(void *device_manager, unsigned int reset_token);
	void *DeviceManager();
	int SetFrameBuffer(CTAVBufferIndex buffer_index, CTAVFrameBuffer *frame_buffer);
	void SetVideoFormat(AVPixelFormat format);
	int Start();
	int Stop();
	int Execute();
private:
	
	int DisplayAudio();

private:
	HWND m_hwnd;
	CTAVFrameBuffer *m_frame_buffers[CTDISP_BUFFER_INDEX_NUM];
	AVPixelFormat  m_video_format;
	IDirect3DDeviceManager9 *m_device_manager;
	unsigned int m_reset_token;
	int				m_is_hardware_accelerated;

	CTSDLDisplay	m_sdl_display;
	CTDXVA2Display	m_dxva_display;

	int m_keep_running;
	int m_is_running;
};

