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

typedef enum CTAVBufferIndex {
	CTDISP_BUFFER_INDEX_VIDEO = 0,
	CTDISP_BUFFER_INDEX_AUDIO,
	CTDISP_BUFFER_INDEX_NUM,
} CTAVBufferIndex;

#define CTDISP_WINDOW_WIDTH_DEFAULT		100
#define CTDISP_WINDOW_HEIGHT_DEFAULT	50


#define CTDISP_EC_OK		0
#define CTDISP_EC_FAILURE	-1

class CTDisplay
{
public:
	CTDisplay();
	~CTDisplay();

	int Init();
#ifdef _WIN32
	int Init(HWND hwnd);
#endif
	int SetInputFrameBuffer(CTAVBufferIndex iBufferIndex, CTAVFrameBuffer *pFrameBuffer);
	int Start();
	int Stop();
	int Execute();
private:
	
	int DisplayAudio();

private:
	HWND m_hwnd;
	int m_wnd_width;
	int m_wnd_height;
	int m_frame_width;
	int m_frame_height;
	int m_disp_width;
	int m_disp_height;
	CTAVFrameBuffer *m_frame_buffers[CTDISP_BUFFER_INDEX_NUM];

	SDL_Window      *m_screen;
	SDL_Renderer	*m_renderer;
	SDL_Texture		*m_texture;

	int m_keep_running;
	int m_is_running;
};

