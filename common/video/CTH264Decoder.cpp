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
	m_pHWInputStream = NULL;
	m_dVideoClock = 0;
}

CTH264Decoder::~CTH264Decoder()
{
}


int CTH264Decoder::Init(CTH264DecodeMode iMode)
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

	// 需设置成1， 在解码时avframe结构中的内存由自己控制，即avframe消费完之后需要调用av_frame_unref释放其中内存
	m_pCodecCtx->refcounted_frames = 1;

// 	m_pCodecCtx->coded_width = 4096;
// 	m_pCodecCtx->coded_height = 2048;
	if (UseHardwareDecoder())
	{
		m_pCodecCtx->pix_fmt = m_outputFmt;
	}

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

	av_init_packet(&m_packet);

	return H264DEC_EC_OK;
}

int CTH264Decoder::Init(AVStream *pVideoStream, CTH264DecodeMode iMode)
{
	int iResult;

	if (pVideoStream == NULL)
		return H264DEC_EC_FAILURE;

	m_iMode = iMode;
	m_pVideoStream = pVideoStream;
	
	if (CTAVFrameBufferInit(&m_frameBuffer, 10) != CTAV_BUFFER_EC_OK)
		return H264DEC_EC_FAILURE;

	/* register all formats and codecs */
	av_register_all();
	m_pCodec = avcodec_find_decoder(m_pVideoStream->codec->codec_id);
	if (!m_pCodec) 
	{
		H264DECLog("Unsupported codec!\n");
		return H264DEC_EC_FAILURE;
	}

	m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
	if (avcodec_copy_context(m_pCodecCtx, pVideoStream->codec) != 0) 
	{
		H264DECLog("Couldn't copy codec context");
		return H264DEC_EC_FAILURE;
	}


	m_bHWAccel = 0;
	m_outputFmt = AV_PIX_FMT_YUV420P;
	// 需设置成1， 在解码时avframe结构中的内存由自己控制，即avframe消费完之后需要调用av_frame_unref释放其中内存
	m_pCodecCtx->refcounted_frames = 1;

	
	if (UseHardwareDecoder() != H264DEC_EC_OK)
	{ 
		m_pCodecCtx->pix_fmt = m_outputFmt;
	}

	if ((iResult = avcodec_open2(m_pCodecCtx, m_pCodec, NULL)) < 0)
	{
		av_strerror(iResult, m_pcErrMsg, H264DEC_MAX_ERROR_MSG);
		H264DECLog("avcodec_open2 failed, err = %s\n", m_pcErrMsg);
		return H264DEC_EC_FAILURE;
	}

	av_init_packet(&m_packet);

	return H264DEC_EC_OK;
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

void CTH264Decoder::SetPacketQueue(CTAVPacketQueue *pPacketQueue)
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
	if (m_pHWInputStream)
	{
		delete m_pHWInputStream;
		m_pHWInputStream = NULL;
	}

	return H264DEC_EC_OK;
}

void CTH264Decoder::Execute()
{
	int iFrameCount = 0;
	int iRet = H264DEC_EC_NEED_MORE_DATA;

	if (m_iMode != H264DEC_MODE_PACKETQUEUE)
		return;

	m_bRunning = 1;
	while (!m_bShouldStop)
	{
		while (CTAVPacketQueueNumItems(m_pPacketQueue) <= 0 || 
			CTAVFrameBufferNumAvailFrames(&m_frameBuffer) <= 0)
		{
			if (m_bShouldStop)
				break;
			CTSleep(1); // just sleep for one millisecond to release cpu time slice.
		}

		if (m_bShouldStop)
			continue;

		CTAVPacketQueueGet(m_pPacketQueue, &m_packet);
		CTAVFrame *pFrame = CTAVFrameBufferFirstAvailFrame(&m_frameBuffer);
		if (DecodeEx(&m_packet, pFrame) == H264DEC_EC_OK)
		{
			CTAVFrameBufferExtend(&m_frameBuffer);
			printf("decoded frame:%d\n", ++iFrameCount);
		}
	}

	/* flush cached frames */
	m_packet.data = NULL;
	m_packet.size = 0;
	do {
		if (CTAVFrameBufferNumAvailFrames(&m_frameBuffer) <= 0)
			break;

		CTAVFrame *pFrame = CTAVFrameBufferFirstAvailFrame(&m_frameBuffer);
		if (DecodeEx(&m_packet, pFrame) == H264DEC_EC_OK)
		{
			CTAVFrameBufferExtend(&m_frameBuffer);
		}
		else
		{
			break;
		}

	} while (1);

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
		if (m_bHWAccel)
		{
			if (0 != dxva2_retrieve_data_call(m_pCodecCtx, pFrame))
				return H264DEC_EC_FAILURE;
		}
		if (m_pixfmt == AV_PIX_FMT_NONE)
			m_pixfmt = m_pCodecCtx->pix_fmt;
		return H264DEC_EC_OK;
	}

	return H264DEC_EC_NEED_MORE_DATA;
}

int CTH264Decoder::IsHardwareAccelerated()
{
	return m_bHWAccel;
}

int CTH264Decoder::OutputPixelFormat()
{
	return m_outputFmt;
}

bool CTH264Decoder::UseHardwareDecoder()
{
	return false;
	// try to use hardware decoder firstly
	InputStream *ist = new InputStream();
	assert(ist);

	ist->hwaccel_id = HWACCEL_AUTO;
	ist->hwaccel_device = "dxva2";
	ist->dec = m_pCodec;
	ist->dec_ctx = m_pCodecCtx;
	m_pCodecCtx->opaque = ist;
	if (0 == dxva2_init(m_pCodecCtx)) // success
	{
		m_pCodecCtx->thread_count = 1;
		m_pCodecCtx->get_buffer2 = ist->hwaccel_get_buffer;
		m_pCodecCtx->get_format = GetHwFormat;
		m_pCodecCtx->thread_safe_callbacks = 1;
		m_bHWAccel = 1;
		m_outputFmt = AV_PIX_FMT_NV12;

		m_pHWInputStream = ist;

		return true;
	}
	else
	{
		delete ist;
		return false;
	}

}

int CTH264Decoder::DecodeEx(AVPacket *pPacket, CTAVFrame *pFrame)
{
	int iRet = H264DEC_EC_OK;
	if ((iRet = Decode(pPacket, pFrame->pFrame)) == H264DEC_EC_OK)
	{
		double pts;
		if (m_packet.dts != AV_NOPTS_VALUE) {
			pts = (double)av_frame_get_best_effort_timestamp(pFrame->pFrame);
		}
		else {
			pts = 0;
		}

		pts *= av_q2d(m_pVideoStream->time_base);
		pts = SynchronizeVideo(pFrame->pFrame, pts);

		pFrame->pts = pts;
	}

	return iRet;
}

double CTH264Decoder::SynchronizeVideo(AVFrame *pSrcFrame, double pts) 
{

	double dFrameDelay;

	if (pts != 0) {
		/* if we have pts, set video clock to it */
		m_dVideoClock = pts;
	}
	else {
		/* if we aren't given a pts, set it to the clock */
		pts = m_dVideoClock;
	}
	/* update the video clock */
	dFrameDelay = av_q2d(m_pVideoStream->codec->framerate);
	/* if we are repeating a frame, adjust clock accordingly */
	dFrameDelay += pSrcFrame->repeat_pict * (dFrameDelay * 0.5);
	m_dVideoClock += dFrameDelay;
	return pts;
}