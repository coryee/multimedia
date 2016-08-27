#include <assert.h>
#include "avframebuffer.h"

int ISAVFrameBufferInit(ISAVFrameBuffer *pFrameBuffer, int iSize)
{
	int i;
	assert(pFrameBuffer != NULL);

	memset(pFrameBuffer, 0, sizeof(*pFrameBuffer));
	pFrameBuffer->iSize = iSize;
	pFrameBuffer->iHead = 0;
	pFrameBuffer->iTail = 0;

	pFrameBuffer->frames = malloc(iSize * sizeof(AVFrame*));
	if (!pFrameBuffer->frames)
		return ISAVFB_EC_FAILURE;
	memset(pFrameBuffer->frames, 0, iSize * sizeof(AVFrame*));

	for (i = 0; i < iSize; i++)
	{
		pFrameBuffer->frames[i] = av_frame_alloc();
		if (!pFrameBuffer->frames[i])
			return ISAVFB_EC_FAILURE;
	}

	return ISAVFB_EC_OK;
}

void ISAVFrameBufferDeInit(ISAVFrameBuffer *pFrameBuffer)
{
	int i;

	assert(pFrameBuffer != NULL);

	pFrameBuffer->iSize = 0;
	pFrameBuffer->iHead = 0;
	pFrameBuffer->iTail = 0;
	if (pFrameBuffer->frames)
	{
		for (i = 0; i < pFrameBuffer->iSize; i++)
		{
			if (pFrameBuffer->frames[i])
			{
				av_frame_free(&pFrameBuffer->frames[i]);
				pFrameBuffer->frames[i] = NULL;
			}
		}
		free(pFrameBuffer->frames);
		pFrameBuffer->frames = NULL;
	}
}

// called by producer
int ISAVFrameBufferNumAvailFrame(ISAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	// we subtract by 1 to differentiate the empty and full case
	return ((pFrameBuffer->iHead - pFrameBuffer->iTail + pFrameBuffer->iSize - 1) % pFrameBuffer->iSize) ;
}

// called by producer
AVFrame* ISAVFrameBufferFirstAvailFrame(ISAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	// check if the buffer is full
	if (pFrameBuffer->iHead == ((pFrameBuffer->iTail + 1) % pFrameBuffer->iSize))
		return NULL;
	if (pFrameBuffer->frames[pFrameBuffer->iTail] == NULL)
	{
		int kk = 0;
	}
	return pFrameBuffer->frames[pFrameBuffer->iTail];
}

// called by producer
void ISAVFrameBufferExtend(ISAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	pFrameBuffer->iTail = (pFrameBuffer->iTail + 1) % pFrameBuffer->iSize;
}

int ISAVFrameBufferNumFrame(ISAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	return (pFrameBuffer->iTail - pFrameBuffer->iHead + pFrameBuffer->iSize) % pFrameBuffer->iSize;
}

// called by consumer
AVFrame* ISAVFrameBufferFirstFrame(ISAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);

	AVFrame *pFrame = ISAVFrameBufferPeekFirstFrame(pFrameBuffer);
	if (pFrame != NULL)
		ISAVFrameBufferAdvance(pFrameBuffer);

	return pFrame;
}

// called by consumer
AVFrame* ISAVFrameBufferPeekFirstFrame(ISAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);

	if (pFrameBuffer->iTail == pFrameBuffer->iHead)
		return NULL;
	else
		return pFrameBuffer->frames[pFrameBuffer->iHead];
}

// called by consumer
void ISAVFrameBufferAdvance(ISAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	pFrameBuffer->iHead = (pFrameBuffer->iHead + 1) % pFrameBuffer->iSize;
}