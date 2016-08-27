#ifndef _AVFRAMEBUFFER_H_
#define _AVFRAMEBUFFER_H_

#if defined(__cplusplus)
extern "C"
{
#endif

#include <libavutil/frame.h>
#include <windows.h>

#define ISAVFB_EC_OK					0
#define ISAVFB_EC_FAILURE				-1
#define ISAVFB_EC_NULL_POINTER		-2


	typedef struct ISAVFrameBuffer
	{
		AVFrame **frames;
		int iSize;
		int iHead;
		int iTail;
	} ISAVFrameBuffer;

	int ISAVFrameBufferInit(ISAVFrameBuffer *pFrameBuffer, int iSize);
	void ISAVFrameBufferDeInit(ISAVFrameBuffer *pFrameBuffer);

	int ISAVFrameBufferNumAvailFrame(ISAVFrameBuffer *pFrameBuffer);
	AVFrame* ISAVFrameBufferFirstAvailFrame(ISAVFrameBuffer *pFrameBuffer);
	void ISAVFrameBufferExtend(ISAVFrameBuffer *pFrameBuffer);

	int ISAVFrameBufferNumFrame(ISAVFrameBuffer *pFrameBuffer);
	AVFrame* ISAVFrameBufferFirstFrame(ISAVFrameBuffer *pFrameBuffer);
	AVFrame* ISAVFrameBufferPeekFirstFrame(ISAVFrameBuffer *pFrameBuffer);
	void ISAVFrameBufferAdvance(ISAVFrameBuffer *pFrameBuffer);

#if defined(__cplusplus)
}
#endif

#endif