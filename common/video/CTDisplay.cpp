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
	m_wnd_width = 0;
	m_wnd_height = 0;
	m_frame_width = 0;
	m_frame_height = 0;
	m_disp_width = 0;
	m_disp_height = 0;
	memset(m_frame_buffers, 0, sizeof(CTAVFrameBuffer *)*CTDISP_BUFFER_INDEX_NUM);
	m_screen = NULL;
	m_renderer = NULL;
	m_texture = NULL;
	m_keep_running = 0;
	m_is_running = 0;
}


CTDisplay::~CTDisplay()
{
}

int CTDisplay::Init()
{
	m_hwnd = 0;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		CTDISPError("Could not initialize SDL - %s\n", SDL_GetError());
		return CTDISP_EC_FAILURE;
	}

	m_wnd_width = CTDISP_WINDOW_WIDTH_DEFAULT;
	m_wnd_height = CTDISP_WINDOW_HEIGHT_DEFAULT;


	// 必须在主线程创建window，否则鼠标无法操作
	m_screen = SDL_CreateWindow("video player", SDL_WINDOWPOS_UNDEFINED,
		 	SDL_WINDOWPOS_UNDEFINED, m_wnd_width, m_wnd_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (m_screen == NULL) {
		CTDISPError("Could not creat window\n");
		return CTDISP_EC_FAILURE;
	}

	m_renderer = SDL_CreateRenderer(m_screen, -1, SDL_RENDERER_ACCELERATED);
	if (!m_renderer) {
		CTDISPError("Could not create renderer\n");
		return CTDISP_EC_FAILURE;
	}

	return CTDISP_EC_OK;
}

#ifdef _WIN32
int CTDisplay::Init(HWND hwnd)
{
	m_hwnd = hwnd;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		CTDISPError("Could not initialize SDL - %s\n", SDL_GetError());
		return CTDISP_EC_FAILURE;
	}

	RECT rect;
	GetWindowRect(m_hwnd, &rect);
	m_wnd_width = rect.right - rect.left;
	m_wnd_height = rect.bottom - rect.top;
	
	m_screen = SDL_CreateWindowFrom((void *)m_hwnd);
	if (m_screen == NULL) {
		CTDISPError("Could not creat window\n");
		return CTDISP_EC_FAILURE;
	}

	m_renderer = SDL_CreateRenderer(m_screen, -1, SDL_RENDERER_ACCELERATED);
	if (!m_renderer) {
		CTDISPError("Could not create renderer\n");
		return CTDISP_EC_FAILURE;
	}

	return CTDISP_EC_OK;
}
#endif

int CTDisplay::SetInputFrameBuffer(CTAVBufferIndex iBufferIndex, CTAVFrameBuffer *pFrameBuffer)
{
	if (iBufferIndex < 0 || iBufferIndex >= CTDISP_BUFFER_INDEX_NUM || pFrameBuffer == NULL)
		return CTDISP_EC_FAILURE;
	m_frame_buffers[iBufferIndex] = pFrameBuffer;

	return CTDISP_EC_OK;
}

int CTDisplay::Start()
{
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
	SDL_Texture *texture = NULL;

	bool got_gap_in_vertical = false;
	int left_x = 0;
	int top_y = 0;

	m_keep_running = 1;
	m_is_running = 1;
	while (m_keep_running) {
		// fetch a frame from Video_Frame_Buffer
		if (CTAVFrameBufferNumFrames(video_frame_buffer) <= 0) {
			CTSleep(1); // sleep for one millisecond
			continue;
		}

		CTAVFrame *frame = CTAVFrameBufferFirstFrame(video_frame_buffer);
		
		if ((m_frame_width == 0 || m_frame_height == 0)/* ||
			(m_iImgWidth != pFrame->width || m_iImgHeight != pFrame->height)*/) {
			m_frame_width = frame->pFrame->width;
			m_frame_height = frame->pFrame->height;

			if (m_frame_width != m_wnd_width || m_frame_height != m_wnd_height) {
				m_wnd_width = m_frame_width;
				m_wnd_height = m_frame_height;
				SDL_SetWindowSize(m_screen, m_wnd_width, m_wnd_width);
			}
			
			double iRatio = (double)m_frame_width / (double)m_frame_height;
			m_disp_width = iRatio * m_wnd_height;
			if (m_disp_width > m_wnd_width) {
				m_disp_width = m_wnd_width;
				m_disp_height = m_disp_width / iRatio;
				got_gap_in_vertical = true;
				top_y = (m_wnd_height - m_disp_height) / 2;
			} else {
				m_disp_height = m_wnd_height;
				got_gap_in_vertical = false;
				left_x = (m_wnd_width - m_disp_width) / 2;
			}

			if (texture == NULL)
			{
				texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
					m_frame_width, m_frame_height);
				if (texture == NULL)
					return CTDISP_EC_FAILURE;
			}
		}


		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = m_frame_width;
		rect.h = m_frame_height;
		SDL_UpdateYUVTexture(texture, &rect,
			frame->pFrame->data[0], frame->pFrame->linesize[0],
			frame->pFrame->data[1], frame->pFrame->linesize[1],
			frame->pFrame->data[2], frame->pFrame->linesize[2]);

		SDL_RenderClear(m_renderer);

		rect.x = left_x;
		rect.y = top_y;
		if (got_gap_in_vertical)
		{
			rect.w = m_wnd_width;
			rect.h = m_disp_height;
		}
		else
		{
			rect.w = m_disp_width;
			rect.h = m_wnd_height;
		}
		SDL_RenderCopy(m_renderer, texture, NULL, &rect);
		SDL_RenderPresent(m_renderer);
			
		av_frame_unref(frame->pFrame);
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
