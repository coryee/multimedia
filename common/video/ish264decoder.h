#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

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

#include "mbuffer.h"
#include "avframebuffer.h"


#define H264DEC_MAX_ERR_MSG_LEN	128


// error code
#define H264DEC_EC_OK					0
#define H264DEC_EC_FAILURE				-1
#define H264DEC_EC_NEED_MORE_DATA		-2
#define H264DEC_EC_FORMAT_NOT_SUPPORT	-3

class ISH264Decoder
{
public:
	ISH264Decoder()	
	{
		m_pCodec = NULL;
		m_pCodecCtx = NULL;
		m_pCodecParserCtx = NULL;
		m_pFrame = NULL;
		m_iWidth = 0;
		m_iHeight = 0;
		m_pixfmt = AV_PIX_FMT_NONE;

		m_bRunning = 0;
		m_bShouldStop = 0;
	}
	~ISH264Decoder() 
	{
		DeInit();
	}

	int Init();
	void DeInit();

	void SetInputBuffer(MBUFFERSYSBuffer *pSysBuffer);
	ISAVFrameBuffer *GetOutputBuffer();
	
	int StartDecode();
	int Stop();
	void Execute();
	int DecodePacket(AVPacket *pPacket, int *piGotFrame);
	
	int IfHardwareAccelerate();
	int OutputFormat();
private:
	AVCodec *m_pCodec;
	AVCodecContext *m_pCodecCtx;
	AVCodecParserContext *m_pCodecParserCtx;
	AVFrame *m_pFrame;
	AVPacket m_packet;

	MBUFFERSYSBuffer *m_pInputBuffer;
	ISAVFrameBuffer m_frameBuffer;
	int m_bRunning;
	int m_bShouldStop;

#ifdef _WIN32
	HANDLE m_threadHandle;
#endif
public:
	int m_iWidth;
	int m_iHeight;
	enum AVPixelFormat m_pixfmt;

	char m_pcErrMsg[H264DEC_MAX_ERR_MSG_LEN];

	int m_bHWAccel;
	AVPixelFormat m_outputFmt;
};