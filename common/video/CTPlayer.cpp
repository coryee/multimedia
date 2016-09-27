#include "CTPlayer.h"
#include "commutil.h"

#define CTPLAYER_MAX_PACKETQUEUE_SIZE	20

#define CTPlayerInfo	printf
#define CTPlayerError	printf

static int CTPlayerExecute(void *arg);

static int ReadDataFromFile(void *arg)
{
	MBUFFERSYSBuffer *gSysbuffer = (MBUFFERSYSBuffer *)arg;
	char *pInFile = "../../data/When_You_Believe.h264";
	FILE *fpInFile = fopen(pInFile, "rb");
	if (fpInFile == NULL)
		return -1;
	int iTotalSize = 0;
	while (!feof(fpInFile))
	{
		int iAvaiLen = MBUFFERSYSBufferSpaceAvailableToEnd(gSysbuffer);
		if (iAvaiLen > 0)
		{
			int iReadSize = fread(MBUFFERSYSAppendDataPos(gSysbuffer), 1, iAvaiLen, fpInFile);
			if (iReadSize < 0)
				break;
			MBUFFERSYSExtend(iReadSize, gSysbuffer);
			iTotalSize += iReadSize;
		}
		else
			Sleep(1);
	}

	return 0;
}

CTPlayer::CTPlayer()
{
	m_packet_queue = 0;
	m_status = CTPLAYER_STATUS_DEFAULT;
	m_keep_running = 0;
}

CTPlayer::~CTPlayer()
{
}

int CTPlayer::SetHandle(HWND hwnd)
{
	m_hwnd = hwnd;
	return CTPLAYER_EC_FAILURE;
}

int CTPlayer::Play(const char *url)
{
	CTStrncpy(m_url, url, CTPLAYER_MAX_URL);


	int ret = Init();
	if (CTPLAYER_EC_OK != ret)
		return ret;

	m_status = CTPLAYER_STATUS_INITED;

	CTThreadHandle thread;
	CTCreateThread(&thread, (CTThreadFunc)CTPlayerExecute, this);
	CTCloseThreadHandle(thread);

	return CTPLAYER_EC_OK;
}

int CTPlayer::Pause()
{
	return CTPLAYER_EC_OK;
}

int CTPlayer::Stop()
{
	m_keep_running = 0;
	while (m_status == CTPLAYER_STATUS_RUNNING) {
		CTSleep(1);
	}

	m_decoder.Stop();
	m_display.Stop();
	if (m_format_ctx != NULL) 
		avformat_close_input(&m_format_ctx);
	if (m_packet_queue != NULL) {
		CTAVPacketQueueDeInit(m_packet_queue);
		free(m_packet_queue);
		m_packet_queue = NULL;
	}


	return CTPLAYER_EC_OK;
}

int CTPlayer::Execute()
{
	//int ret = Init();
	//if (CTPLAYER_EC_OK != ret)
	//	return ret;
	

	int end_of_file = 0;
	m_keep_running = 1;
	m_status = CTPLAYER_STATUS_RUNNING;
	while (m_keep_running)
	{
		while (!end_of_file)
		{
			if (av_read_frame(m_format_ctx, &m_packet) < 0) {
				end_of_file = 1;
				break;
			}
			if (m_packet.stream_index == m_video_idx)
				break;
		}

		if (end_of_file) {
			m_keep_running = 0;
			continue;
		}

		while (CTAVPacketQueuePut(m_packet_queue, &m_packet) == CTAV_BUFFER_EC_FULL)
		{
			
			CTSleep(1);
		}

// 		while (CTAVFrameBufferNumFrames(m_frame_buffer) > 0)
// 		{
// 			CTAVFrame *pFrame = CTAVFrameBufferFirstFrame(m_frame_buffer);
// 			printf("width:%d \n", pFrame->pFrame->width);
// 		}
		av_packet_unref(&m_packet);	
	}
	m_status = CTPLAYER_STATUS_STOPPED;

	Stop();

	return CTPLAYER_EC_OK;
}

int CTPlayer::Init()
{
	av_register_all();
	m_format_ctx = avformat_alloc_context();
	if (avformat_open_input(&m_format_ctx, m_url, NULL, NULL) != 0) {
		CTPlayerError("Couldn't open input stream.\n");
		return CTPLAYER_EC_FAILURE;
	}
	if (avformat_find_stream_info(m_format_ctx, NULL) < 0) {
		CTPlayerError("Couldn't find stream information.\n");
		return CTPLAYER_EC_FAILURE;
	}

	m_video_idx = -1;
	for (int i = 0; i < m_format_ctx->nb_streams; i++) {
		if (m_format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			m_video_idx = i;
			break;
		}
	}
	if (m_video_idx == -1) {
		CTPlayerError("Didn't find a video stream.\n");
		return CTPLAYER_EC_FAILURE;
	}

	m_packet_queue = (CTAVPacketQueue *)malloc(sizeof(CTAVPacketQueue));
	if (m_packet_queue == NULL) {
		CTPlayerError("Out of memory\n");
		return CTPLAYER_EC_FAILURE;
	}
	CTAVPacketQueueInit(m_packet_queue, CTPLAYER_MAX_PACKETQUEUE_SIZE);


	m_display.Init(m_hwnd, m_decoder.IsHardwareAccelerated());
	if (H264DEC_EC_FAILURE ==
		m_decoder.Init(m_format_ctx->streams[m_video_idx], H264DEC_MODE_PACKETQUEUE, m_display.DeviceManager())) {
		CTPlayerError("Can't initialize decoder\n");
		return CTPLAYER_EC_FAILURE;
	}
	m_decoder.SetPacketQueue(m_packet_queue);
	m_frame_buffer = m_decoder.OutputFrameBuffer();

	// test
	// 	ThreadHandle thread;
	// 	CTCreateThread(&thread, (ThreadFunc)readDataFromFile, &m_sys_buffer);

	// 	unsigned int reset_token;
	// 	void *hw_ctx = m_decoder.GetHWAccelContext(&reset_token);
	//	m_display.SetDeviceManager(hw_ctx, reset_token);
	m_display.SetFrameBuffer(CTDISP_BUFFER_INDEX_VIDEO, m_frame_buffer);
	m_display.SetVideoFormat(m_decoder.m_outputFmt);

	m_decoder.Start();
	m_display.Start();

	return CTPLAYER_EC_OK;
}



static int CTPlayerExecute(void *arg)
{
	CTPlayer *player = (CTPlayer *)arg;
	player->Execute();

	return 0;
}