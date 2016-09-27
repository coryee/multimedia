#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // 从 Windows 头中排除极少使用的资料
#endif
#include "CTDisplay.h"
#include "commutil.h"
#include "threadutil.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL.h>
#ifdef __cplusplus
};
#endif

//#pragma comment(lib, "SDL2.lib")
//#pragma comment(lib, "SDL2main.lib")

#define CTDISPInfo		printf
#define CTDISPError		printf

static int CTDisplayExecute(void *arg);

CTDisplay::CTDisplay()
{
	m_hwnd = 0;
	memset(m_frame_buffers, 0, sizeof(CTAVFrameBuffer *)*CTDISP_BUFFER_INDEX_NUM);
	m_video_format = AV_PIX_FMT_YUV420P;
	m_is_hardware_accelerated = 0;
	m_device_manager = NULL;
	m_reset_token = 0;
	m_keep_running = 0;
	m_is_running = 0;
}


CTDisplay::~CTDisplay()
{
}

int CTDisplay::Init(int is_hardware_accelerated)
{
	return Init(0, is_hardware_accelerated);
}

#ifdef _WIN32
int CTDisplay::Init(HWND hwnd, int is_hardware_accelerated)
{
	m_hwnd = hwnd;
	m_is_hardware_accelerated = is_hardware_accelerated;

	m_is_hardware_accelerated = 1;
	if (m_is_hardware_accelerated) {
		return m_dxva_display.Init(m_hwnd);
	}
	else {
		m_sdl_display.SetVideoFormat(m_video_format);
		return m_sdl_display.Init(m_hwnd);
	}

	return CTDISP_EC_OK;
}
#endif

int CTDisplay::SetDeviceManager(void *device_manager, unsigned int reset_token)
{
	m_device_manager = (IDirect3DDeviceManager9 *)device_manager;
	m_reset_token = reset_token;
	return CTDISP_EC_OK;
}

void* CTDisplay::DeviceManager()
{
	return (void *)m_dxva_display.DeviceManager();
}

int CTDisplay::SetFrameBuffer(CTAVBufferIndex iBufferIndex, CTAVFrameBuffer *pFrameBuffer)
{
	if (iBufferIndex < 0 || iBufferIndex >= CTDISP_BUFFER_INDEX_NUM || pFrameBuffer == NULL)
		return CTDISP_EC_FAILURE;
	m_frame_buffers[iBufferIndex] = pFrameBuffer;

	return CTDISP_EC_OK;
}

void CTDisplay::SetVideoFormat(AVPixelFormat format)
{
	m_video_format = format;
}

int CTDisplay::Start()
{
	int ret;
	if (m_is_hardware_accelerated) {
		// ret = m_dxva_display.Init(m_hwnd);
	}
	else {
		m_sdl_display.SetVideoFormat(m_video_format);
		// ret = m_sdl_display.Init(m_hwnd);
	}
// 	if (ret != CTDISP_EC_OK)
// 		return ret;

	CTThreadHandle handle;
	if (CTCreateThread(&handle, (CTThreadFunc)CTDisplayExecute, this) != 0)
		return CTDISP_EC_FAILURE;
	CTCloseThreadHandle(handle);
	return CTDISP_EC_OK;
}

int CTDisplay::Stop()
{
	m_keep_running = 0;
	while (m_is_running) {
		CTSleep(1);
	}

	return CTDISP_EC_OK;
}

int CTDisplay::Execute()
{
	CTAVFrameBuffer *video_frame_buffer = m_frame_buffers[CTDISP_BUFFER_INDEX_VIDEO];
	
	m_keep_running = 1;
	m_is_running = 1;
	while (m_keep_running) {
		// fetch a frame from Video_Frame_Buffer
		if (CTAVFrameBufferNumFrames(video_frame_buffer) <= 0) {
			CTSleep(1); // sleep for one millisecond
			continue;
		}

		CTAVFrame *ctframe = CTAVFrameBufferFirstFrame(video_frame_buffer);
	
		if (m_is_hardware_accelerated) {
			m_dxva_display.SetFrameResolution(ctframe->pFrame->width, ctframe->pFrame->height);
			m_dxva_display.Display((IDirect3DSurface9 *)ctframe->pFrame->data[3]);
		}
		else {
			m_sdl_display.Display(ctframe->pFrame);
		}
		
		av_frame_unref(ctframe->pFrame);
		// CTSleep(30);
	}
	m_is_running = 0;

	return CTDISP_EC_OK;
}

int CTDisplay::DisplayAudio()
{
	return CTDISP_EC_OK;
}

static int CTDisplayExecute(void *arg)
{
	CTDisplay *disp = (CTDisplay *)arg;
	disp->Execute();
	return(0);
}
