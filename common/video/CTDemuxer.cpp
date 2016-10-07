#include "CTDemuxer.h"
#include "commutil.h"
#include "threadutil.h"
#include "adtsutil.h"

#define CTDemuxInfo		printf
#define CTDLog	printf

#define ADTS_HEADER_LENGTH	7

static int CTDemuxerExecute(void *arg)
{
	CTDemuxer *disp = (CTDemuxer *)arg;
	disp->Execute1();
	return(0);
}

CTDemuxer::CTDemuxer()
{
	m_uri[0] = 0;
	m_video_output_file[0] = 0;
	m_audio_output_file[0] = 0;
	m_ifmt_ctx = NULL;
	m_h264bsfc = NULL;
	m_packet_queues[CTDEMUXER_QUEUE_NUM_STREAMS] = { 0 };

	m_video_idx = -1;
	m_audio_idx = -1;
	m_is_aac_codec = 0;
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

int CTDemuxer::SetOutputFile(char *video_output_file, char *audio_output_file)
{
	CTStrncpy(m_video_output_file, video_output_file, sizeof(m_video_output_file));
	CTStrncpy(m_audio_output_file, audio_output_file, sizeof(m_audio_output_file));

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

	int output_to_file = 0;
	FILE *fp_video = NULL;
	FILE *fp_audio = NULL;
	if (m_video_output_file[0]) {
		fp_video = fopen(m_video_output_file, "wb+");
		if (fp_video == NULL)
			return -1;
		output_to_file = 1;
	}

	if (m_audio_output_file[0]) {
		fp_audio = fopen(m_audio_output_file, "wb+");
		if (fp_audio == NULL)
			return -1;
		output_to_file = 1;
	}

	av_init_packet(&pkt);
	while (m_keep_running) {
		ret = 0;
		while ((ret = av_read_frame(m_ifmt_ctx, &pkt)) >= 0) {
			if (pkt.stream_index == m_video_idx) {
				if (output_to_file != 0) {
					uint8_t *data = pkt.data;
					int size = pkt.size;
					if (m_h264bsfc) {
						av_bitstream_filter_filter(m_h264bsfc, m_ifmt_ctx->streams[m_video_idx]->codec,
							NULL, &data, &size, pkt.data, pkt.size, 0);
					}

					ret = fwrite(data, 1, size, fp_video);
					if (m_h264bsfc)
						av_free(data);
					if (size != ret) {
						CTDLog("failed to write data to file!!!\n");
						m_keep_running = 0;
						break;
					}
				} else {
					while (CTAV_BUFFER_EC_FULL ==
						CTAVPacketQueuePut(m_packet_queues[CTDEMUXER_QUEUE_VIDEO_STREAM], &pkt)) {
						Sleep(1);
					}
				}
				printf("got a video packet: %d\n", ++num_video_packet);
				av_packet_unref(&pkt);
			}
			else if (pkt.stream_index == m_audio_idx){
				if (output_to_file != 0) {
					/*
					AAC in some container format (FLV, MP4, MKV etc.) need to add 7 Bytes
					ADTS Header in front of AVPacket data manually.
					Other Audio Codec (MP3...) works well.
					*/
					if (pkt.size != fwrite(pkt.data, 1, pkt.size, fp_audio)) {
						CTDLog("failed to write data to file!!!\n");
						m_keep_running = 0;
						break;
					}
				} else {
					while (CTAV_BUFFER_EC_FULL == CTAVPacketQueuePut(m_packet_queues[CTDEMUXER_QUEUE_AUDIO_STREAM], &pkt)) {
						Sleep(1);
					}
				}
				printf("got a audio packet: %d\n", ++num_audio_packet);
				av_packet_unref(&pkt);
			}
			Sleep(10);
		}

		if (ret == AVERROR_EOF)
			m_keep_running = 0;
	}
	if (fp_video)
		fclose(fp_video);
	if (fp_audio)
		fclose(fp_audio);
	m_is_running = 0;

	return 0;
}


int CTDemuxer::Execute1()
{
	int ret;
	if (InitInternal() != 0)
		return -1;

	AVPacket pkt;
	m_keep_running = 1;

	int num_video_packet = 0;
	int num_audio_packet = 0;

	int output_to_file = 0;
	FILE *fp_video = NULL;
	FILE *fp_audio = NULL;
	if (m_video_output_file[0]) {
		fp_video = fopen(m_video_output_file, "wb+");
		if (fp_video == NULL)
			return -1;
		output_to_file = 1;
	}

	if (m_audio_output_file[0]) {
		fp_audio = fopen(m_audio_output_file, "wb+");
		if (fp_audio == NULL)
			return -1;
		output_to_file = 1;
	}

	av_init_packet(&pkt);
	while (m_keep_running && av_read_frame(m_ifmt_ctx, &pkt) >= 0) {
		ret = 0;
		if (pkt.stream_index == m_video_idx) {
			if (output_to_file != 0) {
				uint8_t *data = pkt.data;
				int size = pkt.size;
				if (m_h264bsfc) {
					av_bitstream_filter_filter(m_h264bsfc, m_ifmt_ctx->streams[m_video_idx]->codec,
						NULL, &data, &size, pkt.data, pkt.size, 0);
				}

				ret = fwrite(data, 1, size, fp_video);
				if (m_h264bsfc)
					av_free(data);
				if (size != ret) {
					CTDLog("failed to write data to file!!!\n");
					break;
				}
			}
			else {
				while (CTAV_BUFFER_EC_FULL ==
					CTAVPacketQueuePut(m_packet_queues[CTDEMUXER_QUEUE_VIDEO_STREAM], &pkt)) {
					Sleep(1);
				}
				//fclose(fp);
				//return 0;
			}
			printf("got a video packet: %d\n", ++num_video_packet);
			av_packet_unref(&pkt);
		}
		else if (pkt.stream_index == m_audio_idx){
			if (output_to_file != 0) {
				/*
				AAC in some container format (FLV, MP4, MKV etc.) need to add 7 Bytes
				ADTS Header in front of AVPacket data manually.
				Other Audio Codec (MP3...) works well.
				*/
				if (m_is_aac_codec) {
					char adts_header[ADTS_HEADER_SIZE];
					GetADTS4AACPacket(&pkt, adts_header, ADTS_HEADER_SIZE);
					if (ADTS_HEADER_SIZE != fwrite(adts_header, 1, ADTS_HEADER_SIZE, fp_audio)) {
						CTDLog("failed to write data to file!!!\n");
						break;
					}
				}
				if (pkt.size != fwrite(pkt.data, 1, pkt.size, fp_audio)) {
					CTDLog("failed to write data to file!!!\n");
					break;
				}
			}
			else {
				while (CTAV_BUFFER_EC_FULL == CTAVPacketQueuePut(m_packet_queues[CTDEMUXER_QUEUE_AUDIO_STREAM], &pkt)) {
					Sleep(1);
				}
			}
			printf("got a audio packet: %d\n", ++num_audio_packet);
			av_packet_unref(&pkt);
		}
	}
	if (fp_video)
		fclose(fp_video);
	if (fp_audio)
		fclose(fp_audio);
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
			if (m_ifmt_ctx->streams[i]->codec->codec_id == AV_CODEC_ID_AAC)
				m_is_aac_codec = 1;
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
	if (strstr(m_ifmt_ctx->iformat->name, "mpegts") == 0) {
		m_h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
	}

	return 0;
}

int CTDemuxer::GetADTS4AACPacket(AVPacket *pkt, char *buffer, int buffer_size)
{
	ADTSContext adts;
	AVCodecContext *audio_ctx = m_ifmt_ctx->streams[m_audio_idx]->codec;

	aac_decode_extradata(&adts, audio_ctx->extradata, audio_ctx->extradata_size);
	aac_set_adts_head(&adts, (unsigned char *)buffer, pkt->size);
	return 0;
}
