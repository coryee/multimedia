#pragma once

#include "SDL.h"
#include <SDL_thread.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "CTAVBuffer.h"
#ifdef main
#undef main
#endif

#define CTDISP_WINDOW_WIDTH_DEFAULT		100
#define CTDISP_WINDOW_HEIGHT_DEFAULT	50

#define CTDISP_EC_OK		0
#define CTDISP_EC_FAILURE	-1

class CTSDLDisplay
{
public:
	CTSDLDisplay();
	~CTSDLDisplay();

	int Init();
#ifdef _WIN32
	int Init(HWND hwnd);
#endif
	void SetVideoFrameFormat(AVPixelFormat format);
	int SetFrameResolution(int width, int height);
	int Display(AVFrame *frame);
private:
	int UpdateSettings();
private:
	HWND m_hwnd;
	int m_wnd_width;
	int m_wnd_height;
	int m_frame_width;
	int m_frame_height;
	int m_disp_width;
	int m_disp_height;
	int m_left_x;
	int m_top_y;
	bool m_got_gap_in_vertical;

	AVPixelFormat  m_video_format;
	struct SwsContext	*m_img_convert_ctx;
	AVFrame				*m_converted_frame;
	SDL_Window      *m_screen;
	SDL_Renderer	*m_renderer;
	SDL_Texture		*m_texture;
	

	int m_keep_running;
	int m_is_running;
};

