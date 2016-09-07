#include "CTH264Decoder.h"
#include "commutil.h"
#include "ffmpeg_dxva2.h"

#ifdef _WIN32
#include <windows.h>
#endif


#define H264DECLog	printf
#define H264DECDebug printf

#define H264DEC_NUM_BYTES_THRESHOLD	4096

static int CTH264DecoderExecute(void *pH264Decoder)
{
	CTH264Decoder *theH264Decoder = (CTH264Decoder *)pH264Decoder;
	theH264Decoder->Execute();
	return(0);
}

static AVPixelFormat GetHwFormat(AVCodecContext *s, const AVPixelFormat *pix_fmts)
{
	InputStream* ist = (InputStream*)s->opaque;
	ist->active_hwaccel_id = HWACCEL_DXVA2;
	ist->hwaccel_pix_fmt = AV_PIX_FMT_DXVA2_VLD;
	return ist->hwaccel_pix_fmt;
}

CTH264Decoder::CTH264Decoder()
{

}

CTH264Decoder::~CTH264Decoder()
{
}


int CTH264Decoder::Init(int iMode)
{
	int iResult;

	m_iMode = iMode;

	if (CTAVFrameBufferInit(&m_frameBuffer, 10) != CTAV_BUFFER_EC_OK)
		return H264DEC_EC_FAILURE;

	/* register all formats and codecs */
	av_register_all();
	m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!m_pCodec)
	{
		H264DECLog("avcodec_find_decoder failed\n");
		return H264DEC_EC_FAILURE;
	}

	m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
	if (!m_pCodec)
	{
		H264DECLog("avcodec_alloc_context3 failed\n");
		assert(m_pCodec);
		return H264DEC_EC_FAILURE;
	}

	m_bHWAccel = 0;
	m_outputFmt = AV_PIX_FMT_YUV420P;

	m_pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	// 需设置成1， 在解码时avframe结构中的内存由自己控制，即avframe消费完之后需要调用av_frame_unref释放其中内存
	m_pCodecCtx->refcounted_frames = 1;

	m_pCodecCtx->coded_width = 4096;
	m_pCodecCtx->coded_height = 2048;

	// try to use hardware decoder
	m_pCodecCtx->thread_count = 1;
	InputStream *ist = new InputStream();
	ist->hwaccel_id = HWACCEL_AUTO;
	ist->hwaccel_device = "dxva2";
	ist->dec = m_pCodec;
	ist->dec_ctx = m_pCodecCtx;

	// 	m_pCodecCtx->opaque = ist;
	// 	if (0 == dxva2_init(m_pCodecCtx))
	// 	{
	// 		int kk = 0;
	// 		m_pCodecCtx->get_buffer2 = ist->hwaccel_get_buffer;
	// 		m_pCodecCtx->get_format = GetHwFormat;
	// 		m_pCodecCtx->thread_safe_callbacks = 1;
	// 		m_bHWAccel = 1;
	// 		m_outputFmt = AV_PIX_FMT_NV12;
	// 	}

	m_pCodecParserCtx = av_parser_init(AV_CODEC_ID_H264);
	if (!m_pCodec)
	{
		H264DECLog("av_parser_init failed\n");
		return H264DEC_EC_FAILURE;
	}

	if ((iResult = avcodec_open2(m_pCodecCtx, m_pCodec, NULL)) < 0)
	{
		av_strerror(iResult, m_pcErrMsg, H264DEC_MAX_ERROR_MSG);
		H264DECLog("avcodec_open2 failed, err = %s\n", m_pcErrMsg);
		return H264DEC_EC_FAILURE;
	}

	m_pFrame = av_frame_alloc();
	if (!m_pFrame)
	{
		H264DECLog("av_frame_alloc failed\n");
		return H264DEC_EC_FAILURE;
	}

	av_init_packet(&m_packet);

	return H264DEC_EC_OK;
}

int CTH264Decoder::Init(AVCodecContext *pCodeCtx, int iMode)
{

}


void CTH264Decoder::DeInit()
{
	CTAVFrameBufferDeInit(&m_frameBuffer);

	if (!m_pCodecParserCtx)
	{
		av_parser_close(m_pCodecParserCtx);
		m_pCodecParserCtx = NULL;
	}

	if (!m_pCodecCtx)
	{
		avcodec_close(m_pCodecCtx);
		av_free(m_pCodecCtx);
		m_pCodecCtx = NULL;
	}

	if (!m_pFrame)
	{
		av_frame_free(&m_pFrame);
		m_pFrame = NULL;
	}
}

void CTH264Decoder::SetInputPacketQueue(CTAVPacketQueue *pPacketQueue)
{
	m_pPacketQueue = pPacketQueue;
}

CTAVFrameBuffer *CTH264Decoder::OutputFrameBuffer()
{
	return &m_frameBuffer;
}

int CTH264Decoder::Start()
{
	// create a thread for execution
	m_bShouldStop = 0;

	if (0 != CTCreateThread(&m_threadHandle, (CTThreadFunc)CTH264DecoderExecute, this))
		return H264DEC_EC_FAILURE;
	return H264DEC_EC_OK;
}

int CTH264Decoder::Stop()
{
	m_bShouldStop = 1;
	while (m_bRunning)
	{
		CTSleep(1);
	}
	CTCloseThreadHandle(m_threadHandle);
	return H264DEC_EC_OK;
}

void CTH264Decoder::Execute()
{
	int iRet = H264DEC_EC_NEED_MORE_DATA;
	int iCurSize;
	int iGotFrame;

	if (m_iMode != H264DEC_MODE_PACKETQUEUE)
		return;

	m_bRunning = 1;
	while (!m_bShouldStop)
	{
		while (CTAVPacketQueueGet(m_pPacketQueue, &m_packet) == CTAV_BUFFER_EC_NO_ITEM)
		{
			CTSleep(1);
		}

		while (CTAVFrameBufferNumAvailFrame(&m_frameBuffer) <= 0)
		{
			Sleep(1);
		}


		CTAVFrame *pFrame = CTAVFrameBufferFirstAvailFrame(&m_frameBuffer);



		iGotFrame = 0;
		if (DecodePacket(&m_packet, &iGotFrame) == H264DEC_EC_FORMAT_NOT_SUPPORT)
			break;
	}

	/* flush cached frames */
	m_packet.data = NULL;
	m_packet.size = 0;
	do {
		if (ISAVFrameBufferNumAvailFrame(&m_frameBuffer) <= 0)
			break;
		DecodePacket(&m_packet, &iGotFrame);
	} while (iGotFrame);

	m_bRunning = 0;
}

int CTH264Decoder::Decode(AVPacket *pPacket, AVFrame *pFrame)
{
	int iGotFrame;
	int iResult;
	static int iNumFrame = 0;

	iResult = avcodec_decode_video2(m_pCodecCtx, pFrame, &iGotFrame, pPacket);
	if (iResult < 0)
	{
		av_strerror(iResult, m_pcErrMsg, H264DEC_MAX_ERROR_MSG);
		H264DECLog("avcodec_decode_video2 failed, err = %s\n", m_pcErrMsg);
		return H264DEC_EC_FAILURE;
	}

	if (iGotFrame)
	{
		H264DECDebug("Got a new frame! NumFrame:%d\n", ++iNumFrame);
		if (m_pixfmt == AV_PIX_FMT_NONE)
			m_pixfmt = m_pCodecCtx->pix_fmt;

		if (m_pCodecCtx->pix_fmt != AV_PIX_FMT_YUV420P &&
			m_pCodecCtx->pix_fmt != AV_PIX_FMT_NV12)
		{
			return H264DEC_EC_FORMAT_NOT_SUPPORT;
		}
	}

	return H264DEC_EC_OK;
}

int CTH264Decoder::DecodePacket(AVPacket *pPacket, int *piGotFrame)
{
	int iGotFrame;
	int iResult;
	static int iNumFrame = 0;

	*piGotFrame = 0;
	AVFrame *pFrame = ISAVFrameBufferFirstAvailFrame(&m_frameBuffer);
	iResult = avcodec_decode_video2(m_pCodecCtx, pFrame, &iGotFrame, pPacket);
	if (iResult < 0)
	{
		av_strerror(iResult, m_pcErrMsg, H264DEC_MAX_ERR_MSG_LEN);
		H264DECLog("avcodec_decode_video2 failed, err = %s\n", m_pcErrMsg);
		return H264DEC_EC_FAILURE;
	}

	if (iGotFrame)
	{
		*piGotFrame = 1;
		H264DECDebug("Got a new frame! NumFrame:%d\n", ++iNumFrame);
		if (m_pixfmt == AV_PIX_FMT_NONE)
			m_pixfmt = m_pCodecCtx->pix_fmt;

		ISAVFrameBufferExtend(&m_frameBuffer);

		if (m_pCodecCtx->pix_fmt != AV_PIX_FMT_YUV420P &&
			m_pCodecCtx->pix_fmt != AV_PIX_FMT_NV12)
		{
			return H264DEC_EC_FORMAT_NOT_SUPPORT;
		}
	}

	return H264DEC_EC_OK;
}

int CTH264Decoder::IfHardwareAccelerate()
{
	return m_bHWAccel;
}

int CTH264Decoder::OutputFormat()
{
	return m_outputFmt;
}


