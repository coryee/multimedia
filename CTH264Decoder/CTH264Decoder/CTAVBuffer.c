#include "CTAVBuffer.h"

void CTAVPacketQueueInit(CTAVPacketQueue *pQueue, int iQueueSize)
{
	memset(pQueue, 0, sizeof(CTAVPacketQueue));
	pQueue->iQueueSize = iQueueSize;
	pQueue->pMutex = CTMutexCreate();
}

int CTAVPacketQueueNumItems(CTAVPacketQueue *q)
{
	return q->iNumPkts;
}

int CTAVPacketQueuePut(CTAVPacketQueue *pQueue, AVPacket *pPacket) {

	AVPacketList *pPacket1;

	if (pQueue->iNumPkts >= pQueue->iQueueSize)
		return CTAV_BUFFER_EC_FULL;

	pPacket1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
	if (!pPacket1)
		return CTAV_BUFFER_EC_FAILURE;
	pPacket1->pkt = *pPacket;
	if (av_packet_ref(&(pPacket1->pkt), pPacket) < 0) {
		return CTAV_BUFFER_EC_FAILURE;
	}
	pPacket1->next = NULL;

	CTMutexLock(pQueue->pMutex);
	if (!pQueue->pLastPkt)
		pQueue->pFirstPkt = pPacket1;
	else
		pQueue->pLastPkt->next = pPacket1;
	pQueue->pLastPkt = pPacket1;
	pQueue->iNumPkts++;
	pQueue->iTotalSize += pPacket1->pkt.size;

	CTMutexUnlock(pQueue->pMutex);
	return CTAV_BUFFER_EC_OK;
}

int CTAVPacketQueueGet(CTAVPacketQueue *pQueue, AVPacket *pPacket)
{
	AVPacketList *pPacket1;
	int ret = CTAV_BUFFER_EC_FAILURE;

	if (CTAVPacketQueueNumItems(pQueue) <= 0)
		return CTAV_BUFFER_EC_NO_ITEM;

	CTMutexLock(pQueue->pMutex);

	pPacket1 = pQueue->pFirstPkt;
	if (pPacket1) {
		pQueue->pFirstPkt = pPacket1->next;
		if (!pQueue->pFirstPkt)
			pQueue->pLastPkt = NULL;
		pQueue->iNumPkts--;
		pQueue->iTotalSize -= pPacket1->pkt.size;
		*pPacket = pPacket1->pkt;
		av_free(pPacket1);
		ret = CTAV_BUFFER_EC_OK;
	}

	CTMutexUnlock(pQueue->pMutex);
	return ret;
}


// methods for FrameBuffer
int CTAVFrameBufferInit(CTAVFrameBuffer *pFrameBuffer, int iSize)
{
	int i;
	assert(pFrameBuffer != NULL);

	memset(pFrameBuffer, 0, sizeof(*pFrameBuffer));
	pFrameBuffer->iSize = iSize;
	pFrameBuffer->iHead = 0;
	pFrameBuffer->iTail = 0;

	pFrameBuffer->pFrames = malloc(iSize * sizeof(CTAVFrame));
	assert(pFrameBuffer->pFrames != NULL);
	memset(pFrameBuffer->pFrames, 0, iSize * sizeof(CTAVFrame));

	for (i = 0; i < iSize; i++)
	{
		pFrameBuffer->pFrames[i].pFrame = av_frame_alloc();
		assert(pFrameBuffer->pFrames[i].pFrame != NULL);
	}

	return CTAV_BUFFER_EC_OK;
}

void CTAVFrameBufferDeInit(CTAVFrameBuffer *pFrameBuffer)
{
	int i;

	assert(pFrameBuffer != NULL);

	pFrameBuffer->iSize = 0;
	pFrameBuffer->iHead = 0;
	pFrameBuffer->iTail = 0;
	if (pFrameBuffer->pFrames)
	{
		for (i = 0; i < pFrameBuffer->iSize; i++)
		{
			if (pFrameBuffer->pFrames[i].pFrame)
			{
				av_frame_free(&pFrameBuffer->pFrames[i]);
				pFrameBuffer->pFrames[i].pFrame = NULL;
			}
		}
		free(pFrameBuffer->pFrames);
		pFrameBuffer->pFrames = NULL;
	}
}

// called by producer
int CTAVFrameBufferNumAvailFrame(CTAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	// we subtract by 1 to differentiate the empty and full case
	return ((pFrameBuffer->iHead - pFrameBuffer->iTail + pFrameBuffer->iSize - 1) % pFrameBuffer->iSize);
}

// called by producer
CTAVFrame* CTAVFrameBufferFirstAvailFrame(CTAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	// check if the buffer is full
	if (pFrameBuffer->iHead == ((pFrameBuffer->iTail + 1) % pFrameBuffer->iSize))
		return NULL;
	return &(pFrameBuffer->pFrames[pFrameBuffer->iTail]);
}

// called by producer
void CTAVFrameBufferExtend(CTAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	pFrameBuffer->iTail = (pFrameBuffer->iTail + 1) % pFrameBuffer->iSize;
}

int CTAVFrameBufferNumFrames(CTAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	return (pFrameBuffer->iTail - pFrameBuffer->iHead + pFrameBuffer->iSize) % pFrameBuffer->iSize;
}

// called by consumer
CTAVFrame* CTAVFrameBufferFirstFrame(CTAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);

	AVFrame *pFrame = CTAVFrameBufferPeekFirstFrame(pFrameBuffer);
	if (pFrame != NULL)
		CTAVFrameBufferAdvance(pFrameBuffer);

	return pFrame;
}

// called by consumer
CTAVFrame* CTAVFrameBufferPeekFirstFrame(CTAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);

	if (pFrameBuffer->iTail == pFrameBuffer->iHead)
		return NULL;
	else
		return &(pFrameBuffer->pFrames[pFrameBuffer->iHead]);
}

// called by consumer
void CTAVFrameBufferAdvance(CTAVFrameBuffer *pFrameBuffer)
{
	assert(pFrameBuffer != NULL);
	pFrameBuffer->iHead = (pFrameBuffer->iHead + 1) % pFrameBuffer->iSize;
}
