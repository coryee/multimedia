#ifndef _CTH264DECODER_H_
#define _CTH264DECODER_H_

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
#ifdef __cplusplus
};
#endif

#include "CTAVBuffer.h"
#include "threadutil.h"

#define H264DEC_MAX_ERROR_MSG		128

#define	H264DEC_MODE_PACKETQUEUE	0
#define H264DEC_MODE_PACKETSINGLE	1

#define H264DEC_EC_OK					0
#define H264DEC_EC_FAILURE				-1
#define H264DEC_EC_NEED_MORE_DATA		-2

class CTH264Decoder
{
public:
	CTH264Decoder();
	virtual ~CTH264Decoder();
	
	// use h264 software decoder
	int Init(int iMode);

	// use h264 hardware decoder if possible
	int Init(AVCodecContext *pCodeCtx, int iMode);
	void DeInit();
	int Decode(AVPacket *pPacket, AVFrame *pFrame);
	// the following three methods are used only in H264DEC_MODE_PACKETQUEUE mode
	void SetInputPacketQueue(CTAVPacketQueue *pPacketQueue);
	CTAVFrameBuffer *OutputFrameBuffer();
	int Start();
	int Stop();
	void Execute();
	int OutputPixelFormat();
	int IsHardwareAccelerated();

	

private:
	int m_iMode;
	AVCodec *m_pCodec;
	AVCodecContext *m_pCodecCtx;
	AVCodecParserContext *m_pCodecParserCtx;
	AVFrame *m_pFrame;
	AVPacket m_packet;

	CTAVPacketQueue *m_pPacketQueue;
	CTAVFrameBuffer m_frameBuffer;
	int m_bRunning;
	int m_bShouldStop;

	CTThreadHandle m_threadHandle;

public:
	int m_iWidth;
	int m_iHeight;
	enum AVPixelFormat m_pixfmt;

	char m_pcErrMsg[H264DEC_MAX_ERROR_MSG];

	int m_bHWAccel;
	AVPixelFormat m_outputFmt;
};

#endif