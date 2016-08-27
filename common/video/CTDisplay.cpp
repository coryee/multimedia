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

static int CTDisplayExecute(void *arg);

CTDisplay::CTDisplay()
{
	m_bStop = 0;
	m_iFrameWidth = 0;
	m_iFrameHeight = 0;
	m_iDispWidth = 0;
	int m_iDispHeight = 0;
}


CTDisplay::~CTDisplay()
{
}


int CTDisplay::init(HWND hwnd)
{
	m_hwnd = hwnd;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("[CTDisplay] Could not initialize SDL - %s\n", SDL_GetError());
		return CTDISP_EC_FAILURE;
	}

	RECT rect;
	GetWindowRect(m_hwnd, &rect);
	m_iWndWidth = rect.right - rect.left;
	m_iWndHeight = rect.bottom - rect.top;
	

	// 必须在主线程创建window，否则鼠标无法操作
// 	m_screen = SDL_CreateWindow("video player", SDL_WINDOWPOS_UNDEFINED,
// 		SDL_WINDOWPOS_UNDEFINED, m_iWidth, m_iHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	m_screen = SDL_CreateWindowFrom((void *)m_hwnd);
	if (m_screen == NULL) {
		printf("[CTDisplay] SDL: could not creat window - exiting\n");
		return CTDISP_EC_FAILURE;
	}

	m_renderer = SDL_CreateRenderer(m_screen, -1, SDL_RENDERER_SOFTWARE/*SDL_RENDERER_ACCELERATED*/);
	if (!m_renderer) {
		printf("[CTDisplay] SDL: could not create renderer - exiting\n");
		return CTDISP_EC_FAILURE;
	}

	return CTDISP_EC_OK;
}

int CTDisplay::setAVFrameBuffer(int iBufferIndex, ISAVFrameBuffer *pFrameBuffer)
{
	if (iBufferIndex < 0 || iBufferIndex >= CTDISP_BUFFER_INDEX_NUM || pFrameBuffer == NULL)
		return CTDISP_EC_FAILURE;
	m_pFrameBuffer[iBufferIndex] = pFrameBuffer;

	return CTDISP_EC_OK;
}

int CTDisplay::start()
{
	ThreadHandle handle;
	if (CTCreateThread(&handle, (ThreadFunc)CTDisplayExecute, this) != 0)
		return CTDISP_EC_FAILURE;
	return CTDISP_EC_OK;
}

int CTDisplay::displayVideo()
{
	ISAVFrameBuffer *pVideoFrameBuffer = m_pFrameBuffer[CTDISP_BUFFER_INDEX_VIDEO];
	SDL_Texture *pTexture = NULL;

//	TRACE("displayVideo begin\n");

	bool bGotGapInVertical = false;
	int iLeftX = 0;
	int iTopY = 0;

	while (!m_bStop)
	{
		// fetch frame from Video_Frame_Buffer
		if (ISAVFrameBufferNumFrame(pVideoFrameBuffer) <= 0)
		{
			CTSleep(1); // sleep for one millisecond
			continue;
		}
//		continue;

		AVFrame *pFrame = ISAVFrameBufferFirstFrame(pVideoFrameBuffer);
		
		if ((m_iFrameWidth == 0 || m_iFrameHeight == 0)/* ||
			(m_iImgWidth != pFrame->width || m_iImgHeight != pFrame->height)*/)
		{
			m_iFrameWidth = pFrame->width;
			m_iFrameHeight = pFrame->height;
			
			double iRatio = (double)m_iFrameWidth / (double)m_iFrameHeight;
			m_iDispWidth = iRatio * m_iWndHeight;
			if (m_iDispWidth > m_iWndWidth)
			{
				m_iDispWidth = m_iWndWidth;
				m_iDispHeight = m_iDispWidth / iRatio;
				bGotGapInVertical = true;
				iTopY = (m_iWndHeight - m_iDispHeight) / 2;
			}
			else
			{
				m_iDispHeight = m_iWndHeight;
				bGotGapInVertical = false;
				iLeftX = (m_iWndWidth - m_iDispWidth) / 2;
			}

			if (pTexture == NULL)
			{
				pTexture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
					m_iFrameWidth, m_iFrameHeight);
				if (pTexture == NULL)
					return CTDISP_EC_FAILURE;
			}
		}


		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = m_iFrameWidth;
		rect.h = m_iFrameHeight;
		SDL_UpdateYUVTexture(pTexture, &rect,
			pFrame->data[0], pFrame->linesize[0],
			pFrame->data[1], pFrame->linesize[1],
			pFrame->data[2], pFrame->linesize[2]);

		SDL_RenderClear(m_renderer);

		rect.x = iLeftX;
		rect.y = iTopY;
		if (bGotGapInVertical)
		{
			rect.w = m_iWndWidth;
			rect.h = m_iDispHeight;
		}
		else
		{
			rect.w = m_iDispWidth;
			rect.h = m_iWndHeight;
		}
		SDL_RenderCopy(m_renderer, pTexture, NULL, &rect);
		SDL_RenderPresent(m_renderer);
			
		av_frame_unref(pFrame);
		// CTSleep(30);
	}

	return CTDISP_EC_OK;
}

int CTDisplay::displayAudio()
{
	return CTDISP_EC_OK;
}

static int CTDisplayExecute(void *arg)
{
	CTDisplay *disp = (CTDisplay *)arg;
	disp->displayVideo();
	return(0);
}
