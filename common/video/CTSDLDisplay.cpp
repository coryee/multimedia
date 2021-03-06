#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // 从 Windows 头中排除极少使用的资料
#endif
#include "CTSDLDisplay.h"
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

#define SDLDISPInfo		printf
#define SDLDISPError	printf

static int CTDisplayExecute(void *arg);

CTSDLDisplay::CTSDLDisplay()
{
	m_hwnd = 0;
	m_wnd_width = 0;
	m_wnd_height = 0;
	m_frame_width = 0;
	m_frame_height = 0;
	m_disp_width = 0;
	m_disp_height = 0;
	m_video_format = AV_PIX_FMT_YUV420P;
	m_img_convert_ctx = NULL;
	m_converted_frame = NULL;
	m_screen = NULL;
	m_renderer = NULL;
	m_texture = NULL;
}


CTSDLDisplay::~CTSDLDisplay()
{
	if (m_converted_frame) {
		av_frame_free(&m_converted_frame);
		m_converted_frame = NULL;
	}

	if (m_img_convert_ctx) {
		sws_freeContext(m_img_convert_ctx);
		m_img_convert_ctx = NULL;
	}

	if (m_texture) {
		SDL_DestroyTexture(m_texture);
		m_texture = NULL;
	}

	if (m_screen) {
		SDL_DestroyWindow(m_screen);
		m_screen = NULL;
	}

	if (m_renderer) {
		SDL_DestroyRenderer(m_renderer);
		m_renderer = NULL;
	}
}

int CTSDLDisplay::Init()
{
	return Init(0);
}

#ifdef _WIN32
int CTSDLDisplay::Init(HWND hwnd)
{
	m_hwnd = hwnd;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		SDLDISPError("Could not initialize SDL - %s\n", SDL_GetError());
		return CTDISP_EC_FAILURE;
	}

	if (hwnd) {
		RECT rect;
		GetWindowRect(m_hwnd, &rect);
		m_wnd_width = rect.right - rect.left;
		m_wnd_height = rect.bottom - rect.top;
		m_screen = SDL_CreateWindowFrom((void *)m_hwnd);
	} else {
		m_wnd_width = CTDISP_WINDOW_WIDTH_DEFAULT;
		m_wnd_height = CTDISP_WINDOW_HEIGHT_DEFAULT;

		// 必须在主线程创建window，否则鼠标无法操作
		m_screen = SDL_CreateWindow("video player", SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED, m_wnd_width, m_wnd_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	}
	if (m_screen == NULL) {
		SDLDISPError("Could not creat window\n");
		return CTDISP_EC_FAILURE;
	}

	m_renderer = SDL_CreateRenderer(m_screen, -1, SDL_RENDERER_ACCELERATED);
	if (!m_renderer) {
		SDLDISPError("Could not create renderer\n");
		return CTDISP_EC_FAILURE;
	}

	// set default value;
	SetVideoFormat(AV_PIX_FMT_YUV420P);
	return CTDISP_EC_OK;
}
#endif

void CTSDLDisplay::SetVideoFormat(AVPixelFormat format)
{
	m_video_format = format;
}

void CTSDLDisplay::SetFrameResolution(int width, int height)
{
	if (m_frame_width == width && m_frame_height == height)
		return;
	m_frame_width = width;
	m_frame_height = height;
	UpdateLayout();
	UpdateDisplayContext();
}

void CTSDLDisplay::SetWindowSize(int width, int height)
{
	if (m_wnd_width == width && m_wnd_height == height) {
		return;
	}
	m_wnd_width = width;
	m_wnd_height = height;
	UpdateLayout();
}

void CTSDLDisplay::OnWindowSizeChanged()
{
	RECT rect;
	long width;
	long heigth;

	if (m_hwnd) {
		GetWindowRect(m_hwnd, &rect);
		width = rect.right - rect.left;
		heigth = rect.bottom - rect.top;
		SetWindowSize(width, heigth);
	}
}

int CTSDLDisplay::Display(AVFrame *frame)
{
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = m_frame_width;
	rect.h = m_frame_height;

	SetFrameResolution(frame->width, frame->height);

	if (m_video_format != AV_PIX_FMT_YUV420P) {
		sws_scale(m_img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, m_frame_height, 
			m_converted_frame->data, m_converted_frame->linesize);
		frame = m_converted_frame;
	}
	SDL_UpdateYUVTexture(m_texture, &rect,
		frame->data[0], frame->linesize[0],
		frame->data[1], frame->linesize[1],
		frame->data[2], frame->linesize[2]);

	SDL_RenderClear(m_renderer);

	rect.x = m_left_x;
	rect.y = m_top_y;
	if (m_got_gap_in_vertical)
	{
		rect.w = m_wnd_width;
		rect.h = m_disp_height;
	}
	else
	{
		rect.w = m_disp_width;
		rect.h = m_wnd_height;
	}
	SDL_RenderCopy(m_renderer, m_texture, NULL, &rect);
	SDL_RenderPresent(m_renderer);
		
	return CTDISP_EC_OK;
}

int CTSDLDisplay::UpdateLayout()
{
	m_got_gap_in_vertical = false;
	m_left_x = 0;
	m_top_y = 0;

	if (m_frame_width != m_wnd_width || m_frame_height != m_wnd_height) {
		if (m_hwnd == 0) {
			m_wnd_width = m_frame_width;
			m_wnd_height = m_frame_height;
			SDL_SetWindowSize(m_screen, m_wnd_width, m_wnd_height);
		}
	}

	double iRatio = (double)m_frame_width / (double)m_frame_height;
	m_disp_width = iRatio * m_wnd_height;
	if (m_disp_width > m_wnd_width) {
		m_disp_width = m_wnd_width;
		m_disp_height = m_disp_width / iRatio;
		m_got_gap_in_vertical = true;
		m_top_y = (m_wnd_height - m_disp_height) / 2;
	}
	else {
		m_disp_height = m_wnd_height;
		m_got_gap_in_vertical = false;
		m_left_x = (m_wnd_width - m_disp_width) / 2;
	}
	return CTDISP_EC_FAILURE;
}

int CTSDLDisplay::UpdateDisplayContext()
{
	if (m_video_format != AV_PIX_FMT_YUV420P) {
		if (m_img_convert_ctx) {
			sws_freeContext(m_img_convert_ctx);
			m_img_convert_ctx = NULL;
		}
		m_img_convert_ctx = sws_getContext(m_frame_width, m_frame_height, m_video_format,
			m_frame_width, m_frame_height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
		if (m_img_convert_ctx == NULL) {
			SDLDISPError("sws_getContext failed\n");
			assert(false);
			return CTDISP_EC_FAILURE;
		}


		if (m_converted_frame) {
			av_frame_free(&m_converted_frame);
			m_converted_frame = NULL;
		}
		m_converted_frame = av_frame_alloc();
		if (m_converted_frame == NULL) {
			SDLDISPError("out of memory\n");
			assert(false);
			return CTDISP_EC_FAILURE;
		}

		uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, m_frame_width, m_frame_height));
		if (out_buffer == NULL) {
			SDLDISPError("out of memory\n");
			assert(false);
			return CTDISP_EC_FAILURE;
		}
		avpicture_fill((AVPicture *)m_converted_frame, out_buffer, AV_PIX_FMT_YUV420P, m_frame_width, m_frame_height);
	}

	if (m_texture) {
		SDL_DestroyTexture(m_texture);
		m_texture = NULL;
	}
	m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
		m_frame_width, m_frame_height);
	if (m_texture == NULL) {
		SDLDISPError("SDL_CreateTexture failed\n");
		assert(false);
		return CTDISP_EC_FAILURE;
	}

	return CTDISP_EC_OK;
}