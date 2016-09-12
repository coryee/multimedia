#ifndef _CTAVBUFFER_H_
#define _CTAVBUFFER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <assert.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include "mutexutil.h"


#define CTAV_BUFFER_EC_OK			0
#define CTAV_BUFFER_EC_FAILURE		-1
#define CTAV_BUFFER_EC_NO_ITEM		-2
#define CTAV_BUFFER_EC_FULL			-3

typedef struct CTAVPacketQueue 
{
	AVPacketList	*pFirstPkt;
	AVPacketList	*pLastPkt;
	int				iQueueSize;
	int				iNumPkts;
	int				iTotalSize;
 	CTMutex			*pMutex;
} CTAVPacketQueue;

typedef struct CTAVFrame
{
	AVFrame	*pFrame;
	int		width;	// source width
	int		height; // source width
	double	pts;
} CTAVFrame;

typedef struct CTAVFrameBuffer 
{
	CTAVFrame	*pFrames;
	int			iSize;
	int			iHead;
	int			iTail;
} CTAVFrameBuffer;

// methods for PacketQueue
extern void CTAVPacketQueueInit(CTAVPacketQueue *q, int iQueueSize);
extern void CTAVPacketQueueDeInit(CTAVPacketQueue *pQueue);
extern int CTAVPacketQueueNumItems(CTAVPacketQueue *q);
extern int CTAVPacketQueuePut(CTAVPacketQueue *q, AVPacket *pkt);

// return CTAV_BUFFER_EC_OK if succeed, and put the result into the object pointed by pkt;
//	CTAV_BUFFER_EC_NO_ITEM if there is no available packet;
//	CTAV_BUFFER_EC_FAILURE if failed
extern int CTAVPacketQueueGet(CTAVPacketQueue *q, AVPacket *pkt);

// methods for FrameBuffer
extern int CTAVFrameBufferInit(CTAVFrameBuffer *pFrameBuffer, int iSize);
extern void CTAVFrameBufferDeInit(CTAVFrameBuffer *pFrameBuffer);
extern int CTAVFrameBufferNumAvailFrame(CTAVFrameBuffer *pFrameBuffer);
extern CTAVFrame* CTAVFrameBufferFirstAvailFrame(CTAVFrameBuffer *pFrameBuffer);
extern void CTAVFrameBufferExtend(CTAVFrameBuffer *pFrameBuffer);
extern int CTAVFrameBufferNumFrames(CTAVFrameBuffer *pFrameBuffer);
extern CTAVFrame* CTAVFrameBufferFirstFrame(CTAVFrameBuffer *pFrameBuffer);
extern CTAVFrame* CTAVFrameBufferPeekFirstFrame(CTAVFrameBuffer *pFrameBuffer);
extern void CTAVFrameBufferAdvance(CTAVFrameBuffer *pFrameBuffer);

#ifdef __cplusplus
}
#endif
#endif