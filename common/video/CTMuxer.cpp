#include "CTMuxer.h"


CTMuxer::CTMuxer()
{
	memset(m_input_stream_infos, 0, sizeof(m_input_stream_infos));
	memset(m_input_stream_time_bases, 0, sizeof(m_input_stream_time_bases));
	
	m_aacbsfc = NULL;
	m_h264bsfc = NULL;
	m_inited = 0;
}


CTMuxer::~CTMuxer()
{
	vector<CTMuxerOutput *>::iterator iter;
	for (iter = m_outputs.begin(); iter != m_outputs.end(); iter++) {
        CTMuxerOutput *output = *iter;
		//Write file trailer
		if (m_inited) {
			av_write_trailer(output->ofmt_ctx);
		}
		if(output->ofmt_ctx && !(output->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
			avio_close(output->ofmt_ctx->pb);
		avformat_free_context(output->ofmt_ctx);
	}
    for (iter = m_outputs.begin(); iter != m_outputs.end(); iter++) {
        free(*iter);
    }
	if (m_h264bsfc) {
		av_bitstream_filter_close(m_h264bsfc);
	}
	if (m_aacbsfc) {
		av_bitstream_filter_close(m_aacbsfc);
	}
}

int CTMuxer::AddOutputFileName(const char *output_filename)
{
	if (!output_filename|| output_filename[0] == 0) {
		return -1;
	}

	vector<CTMuxerOutput *>::iterator iter;
	for (iter = m_outputs.begin(); iter != m_outputs.end(); iter++) {
        CTMuxerOutput *output = *iter;
		if (strcmp(output->filename, output_filename) == 0) {
			printf("url[%s] has already existed in CTMuxer\n", output_filename);
			return 0;
		}
	}

    
	CTMuxerOutput *output = (CTMuxerOutput *)malloc(sizeof(CTMuxerOutput));
    if (!output) {
        printf("out of memory, failed to allocate memory for CTMuxerOutput\n");
        return -1;
    }
	memset(output, 0, sizeof(*output));
	strncpy(output->filename, output_filename, sizeof(output->filename));
	avformat_alloc_output_context2(&output->ofmt_ctx, NULL, NULL, output->filename);
	if (!output->ofmt_ctx) {
		printf("Could not create output context\n");
		return -1;
	}
	strncpy(output->container_format, output->ofmt_ctx->oformat->name, sizeof(output->container_format));

	if (strstr(output->ofmt_ctx->oformat->name, "flv") != NULL ||
		strstr(output->ofmt_ctx->oformat->name, "mp4") != NULL ||
		strstr(output->ofmt_ctx->oformat->name, "mkv") != NULL) {
		output->need_audio_bitstream_filter = 1;
		output->need_video_bitstream_filter = 1;
	}

	m_outputs.push_back(output);

	return 0;
}

int CTMuxer::AddInputStreamInfo(int stream_type, AVCodecContext *codec_ctx, AVRational time_base)
{
	if (stream_type < 0 || stream_type >= CTMUXER_NUM_STREAM_TYPE) {
		return -1;
	}

	vector<CTMuxerOutput *>::iterator iter;
	for (iter = m_outputs.begin(); iter != m_outputs.end(); iter++) {
        CTMuxerOutput *output = *iter;
		AVStream *stream = CreateStream(output->ofmt_ctx, codec_ctx);
		if (!stream) {
			printf("Failed create video stream\n");
			return -1;
		}
        output->streams[stream_type] = stream;
	}

	m_input_stream_time_bases[stream_type] = time_base;

	if (0 != InitBitstreamFilter(stream_type, codec_ctx->codec_id)) {
		printf("failed to init bitstream_filter for stream_type[%d]\n", stream_type);
		return -1;
	}

	return 0;
}

int CTMuxer::AddInputStreamInfo(int stream_type, CTMuxerStreamInfo *stream_info)
{
	if (stream_type < 0 || stream_type >= CTMUXER_NUM_STREAM_TYPE) {
		return -1;
	}
	
	vector<CTMuxerOutput *>::iterator iter;
	for (iter = m_outputs.begin(); iter != m_outputs.end(); iter++) {
        CTMuxerOutput *output = *iter;
		AVStream *stream = CreateStream(output->ofmt_ctx, stream_info);
		if (!stream) {
			printf("Failed create video stream\n");
			return -1;
		}
        output->streams[stream_type] = stream;
	}

	if (0 != InitBitstreamFilter(stream_type, stream_info->codec_id)) {
		printf("failed to init bitstream_filter for stream_type[%d]\n", stream_type);
		return -1;
	}

	m_input_stream_infos[stream_type] = *stream_info;
	m_input_stream_time_bases[stream_type] = stream_info->time_base;
	return 0;
}

int CTMuxer::Mux(int stream_type, AVPacket *input_pkt)
{
	int ret = 0;

    printf("addr ret = %p\n", &ret);

	if (stream_type < 0 || stream_type >= CTMUXER_NUM_STREAM_TYPE) {
		return -1;
	}

	if (!m_inited) {
		Init();
	}
	
	printf("Write 1 Packet. size:%5d\tpts:%lld\n", input_pkt->size, input_pkt->pts);

 	AVRational in_time_base = m_input_stream_time_bases[stream_type];
	// vector<CTMuxerOutput *>::iterator iter;
    int num_outputs = m_outputs.size();
	//for (iter = m_outputs.begin(); iter != m_outputs.end(); iter++) {
    for (int i = 0; i < num_outputs; i++) {
        printf("ret = %d\n", ret);
	    AVPacket packet;
        CTMuxerOutput *output = m_outputs[i];
 		AVPacket *pkt = &packet;
 		av_copy_packet(pkt, input_pkt);

        AVPacket packet1;
        AVPacket *pkt1 = &packet1;
        av_copy_packet(pkt1, input_pkt);
 
 		AVStream *out_stream = output->streams[stream_type];
		if (stream_type == CTMUXER_STREAM_TYPE_AUDIO && m_aacbsfc &&
            output->need_audio_bitstream_filter) {
			ret = av_bitstream_filter_filter(m_aacbsfc, out_stream->codec, NULL, &pkt1->data, &pkt1->size, pkt->data, pkt->size, 0);
            printf("ret = %d\n", ret);
		} else if (stream_type == CTMUXER_STREAM_TYPE_VIDEO && m_h264bsfc &&
            output->need_video_bitstream_filter) {
			av_bitstream_filter_filter(m_h264bsfc, out_stream->codec, NULL, &pkt->data, &pkt->size, pkt->data, pkt->size, 0);
		}

		pkt->stream_index = out_stream->index;
		pkt->pos = -1;
		pkt->dts  = av_rescale_q_rnd(pkt->dts, in_time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt->pts  = av_rescale_q_rnd(pkt->pts, in_time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt->duration = av_rescale_q(pkt->duration, in_time_base, out_stream->time_base);

		//Write
 		ret = av_interleaved_write_frame(output->ofmt_ctx, pkt);
		av_packet_unref(pkt);
		if (ret < 0) {
			printf("Error muxing packet\n");
			return -1;
		}
	}
	return 0;
}

int CTMuxer::Init()
{
	vector<CTMuxerOutput *>::iterator iter;
	for (iter = m_outputs.begin(); iter != m_outputs.end(); iter++) {
        CTMuxerOutput *output = *iter;
		printf("==========Output Information==========\n");
		av_dump_format(output->ofmt_ctx, 0, output->filename, 1);
		printf("======================================\n");

		//Open output file
		if (!(output->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
			if (avio_open(&output->ofmt_ctx->pb, output->filename, AVIO_FLAG_WRITE) < 0) {
				printf("Could not open output file '%s'", output->filename);
				return -1;
			}
		}

		avformat_write_header(output->ofmt_ctx, NULL);
	}

	m_inited = 1;
	return 0;
}

AVStream *CTMuxer::CreateStream(AVFormatContext *ofmt_ctx, AVCodecContext *codec_ctx)
{
	AVStream *new_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!new_stream) {
		printf("Failed allocating output stream\n");
		return NULL;
	}

	if (avcodec_copy_context(new_stream->codec, codec_ctx) < 0) {
		printf("Failed to copy context from input to output stream codec context\n");
		return NULL;
	}

	new_stream->codec->codec_tag = 0;
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		new_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return new_stream;
}


AVStream *CTMuxer::CreateStream(AVFormatContext *ofmt_ctx, CTMuxerStreamInfo *stream_info)
{
	AVStream *new_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!new_stream) {
		printf("Failed allocating output stream\n");
		return NULL;
	}

	new_stream->codec->codec_id = stream_info->codec_id;
	new_stream->codec->codec_type = stream_info->codec_type;
	if (stream_info->codec_type == AVMEDIA_TYPE_VIDEO) {
		new_stream->codec->pix_fmt = stream_info->pix_fmt;
		new_stream->codec->width = stream_info->width; // dimension must be set
		new_stream->codec->height = stream_info->height;
		new_stream->codec->time_base.num = 1;
		new_stream->codec->time_base.num = stream_info->fps;  // time_base must be set
	}
	else {
		new_stream->codec->sample_fmt = stream_info->sample_fmt;
		new_stream->codec->sample_rate = stream_info->sample_rate;
		new_stream->codec->channel_layout = stream_info->channel_layout;
		new_stream->codec->channels = stream_info->channels;
		new_stream->codec->bit_rate = stream_info->bit_rate;
	}

	new_stream->codec->codec_tag = 0;
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		new_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return new_stream;
}

int CTMuxer::InitBitstreamFilter(int stream_type, AVCodecID codec_id)
{
	if (stream_type == CTMUXER_STREAM_TYPE_AUDIO && codec_id == AV_CODEC_ID_AAC) {
		if (!m_aacbsfc) {
			m_aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
			if (!m_aacbsfc) {
				return -1;
			}
		}
	} else if (stream_type == CTMUXER_STREAM_TYPE_VIDEO && codec_id == AV_CODEC_ID_H264) {
		if (!m_h264bsfc) {
			m_h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
			if (!m_h264bsfc) {
				return -1;
			}
		}
	}

	return 0;
}