#include "CTDemuxer.h"
#include "commutil.h"
#include "threadutil.h"

#define CTDemuxInfo		printf
#define CTDLog	printf

static int CTDemuxerExecute(void *arg)
{
	CTDemuxer *disp = (CTDemuxer *)arg;
	disp->Execute();
	return(0);
}

CTDemuxer::CTDemuxer()
{
	m_uri[0] = 0;
	m_ifmt_ctx = NULL;
	m_h264bsfc = NULL;
	m_packet_queues[CTDEMUXER_QUEUE_NUM_STREAMS] = { 0 };

	m_video_idx = -1;
	m_audio_idx = -1;
	m_keep_running = 0;;
	m_is_running = 0;
}


CTDemuxer::~CTDemuxer()
{
	av_packet_unref(&m_packet);
	if (m_ifmt_ctx)
		avformat_close_input(&m_ifmt_ctx);
	if (m_h264bsfc)
		av_bitstream_filter_close(m_h264bsfc);
}

int CTDemuxer::Init(char *uri)
{
	CTStrncpy(m_uri, uri, sizeof(m_uri));
	
	return 0;
}

int CTDemuxer::SetPacketQueue(int stream_idx, CTAVPacketQueue *queue)
{
	if (stream_idx < 0 || stream_idx >= CTDEMUXER_QUEUE_NUM_STREAMS ||
		queue == NULL) {
		CTDLog("failed to set packet queue\n");
		return -1;
	}

	m_packet_queues[stream_idx] = queue;
	return 0;
}

int CTDemuxer::Start()
{
	CTThreadHandle handle;
	if (CTCreateThread(&handle, (CTThreadFunc)CTDemuxerExecute, this) != 0)
		return -1;
	CTCloseThreadHandle(handle);
	m_is_running = 1;
	return 0;
}

int CTDemuxer::Stop()
{
	m_keep_running = 0;
	while (m_is_running) {
		CTSleep(1);
	}

	return 0;
}

int CTDemuxer::Execute()
{
	int ret;
	if (InitInternal() != 0)
		return -1;
	
	AVPacket pkt;
	m_keep_running = 1;

	int num_video_packet = 0;
	int num_audio_packet = 0;
	char error[64];

	// test
#if 1
	FILE *fp = fopen("output.h264", "wb+");
#endif


	while (m_keep_running) {
		ret = 0;
		while ((ret = av_read_frame(m_ifmt_ctx, &pkt)) >= 0) {
			if (pkt.stream_index == m_video_idx) {
				if (m_h264bsfc) {
					av_bitstream_filter_filter(m_h264bsfc, m_ifmt_ctx->streams[m_video_idx]->codec,
						NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
				}
				fwrite(pkt.data, 1, pkt.size, fp);
				printf("got a video packet: %d\n", ++num_video_packet);
				while (CTAV_BUFFER_EC_FULL == CTAVPacketQueuePut(m_packet_queues[CTDEMUXER_QUEUE_VIDEO_STREAM], &pkt)) {
					Sleep(1);
				}
			}
			else if (pkt.stream_index == m_audio_idx){
				/*
				AAC in some container format (FLV, MP4, MKV etc.) need to add 7 Bytes
				ADTS Header in front of AVPacket data manually.
				Other Audio Codec (MP3...) works well.
				*/
				
				while (CTAV_BUFFER_EC_FULL == CTAVPacketQueuePut(m_packet_queues[CTDEMUXER_QUEUE_AUDIO_STREAM], &pkt)) {
					Sleep(1);
				}
				printf("got a audio packet: %d\n", ++num_audio_packet);
			}
			av_packet_unref(&pkt);
		}

		if (ret == AVERROR_EOF)
			m_keep_running = 0;
	}
	fclose(fp);
	m_is_running = 0;

	return 0;
}

int CTDemuxer::IsFinished()
{
	return m_is_running ? 0 : 1;
}

int CTDemuxer::InitInternal()
{
	int ret;
	av_register_all();
	//Input
	if ((ret = avformat_open_input(&m_ifmt_ctx, m_uri, 0, 0)) < 0) {
		CTDLog("Could not open input file.\n");
		return -1;
	}
	if ((ret = avformat_find_stream_info(m_ifmt_ctx, 0)) < 0) {
		CTDLog("Failed to retrieve input stream information\n");
		return -1;
	}

	for (int i = 0; i<m_ifmt_ctx->nb_streams; i++) {
		if (m_ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			m_video_idx = i;
		}
		else if (m_ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			m_audio_idx = i;
		}
	}
	printf("\nInput Video===========================\n");
	av_dump_format(m_ifmt_ctx, 0, m_uri, 0);
	printf("\n======================================\n");

	/*
	FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
	"h264_mp4toannexb" bitstream filter (BSF)
	*Add SPS,PPS in front of IDR frame
	*Add start code ("0,0,0,1") in front of NALU
	H.264 in some container (MPEG2TS) don't need this BSF.
	*/
	if (strcmp(m_ifmt_ctx->iformat->name, "mpegts")) {
		m_h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
	}

	return 0;
}


