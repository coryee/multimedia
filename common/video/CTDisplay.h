#pragma once

#include "SDL.h"
#include <SDL_thread.h>
#include "avframebuffer.h"
#undef main

enum {
	CTDISP_BUFFER_INDEX_VIDEO = 0,
	CTDISP_BUFFER_INDEX_AUDIO,
	CTDISP_BUFFER_INDEX_NUM,
};


#define CTDISP_EC_OK		0
#define CTDISP_EC_FAILURE	-1

class CTDisplay
{
public:
	CTDisplay();
	~CTDisplay();

	int init(HWND hwnd);
	int setAVFrameBuffer(int iBufferIndex, ISAVFrameBuffer *pFrameBuffer);
	int start();
	int displayVideo();
private:
	
	int displayAudio();

private:
	HWND m_hwnd;
	int m_iWndWidth;
	int m_iWndHeight;
	int m_iFrameWidth;
	int m_iFrameHeight;
	int m_iDispWidth;
	int m_iDispHeight;
	ISAVFrameBuffer *m_pFrameBuffer[CTDISP_BUFFER_INDEX_NUM];

	SDL_Window      *m_screen;
	SDL_Renderer	*m_renderer;
	SDL_Texture		*m_texture;

	int m_bStop;
};

