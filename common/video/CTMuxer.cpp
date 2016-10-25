#include "CTMuxer.h"


CTMuxer::CTMuxer()
{
	ofmt_ctx = NULL;
	ofmt = NULL;
	video_stream = NULL;
	audio_stream = NULL;
	memset(m_output_filename, 0, CTMUXER_MAX_PATH);
	m_inited = 0;

}


CTMuxer::~CTMuxer()
{
}

int CTMuxer::SetOutputFileName(char *output_filename)
{
	strncpy(m_output_filename, output_filename, sizeof(m_output_filename));
	if (ofmt_ctx) {
		avformat_free_context(ofmt_ctx);
	}

	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, m_output_filename);
	if (!ofmt_ctx) {
		printf("Could not create output context\n");
		return -1;
	}

	return 0;
}

int CTMuxer::SetVideoCodecContext(AVCodecContext *codec)
{
	video_stream = CreateNewStream(codec);
	if (!video_stream) {
		printf("Failed create video stream\n");
		return -1;
	}
	return 0;
}

int CTMuxer::SetAudioCodecContext(AVCodecContext *codec)
{
	audio_stream = CreateNewStream(codec);
	if (!audio_stream) {
		printf("Failed create video stream\n");
		return -1;
	}
	return 0;
}

int CTMuxer::MuxVideo(AVPacket *video_pkt)
{
	if (!m_inited) {
		Init();
	}



	return 0;
}

int CTMuxer::MuxAudio(AVPacket *audio_pkt)
{
	if (!m_inited) {
		Init();
	}

	return 0;
}

int CTMuxer::Init()
{
	avformat_write_header(ofmt_ctx, NULL);
	m_inited = 1;
	return 0;
}

AVStream *CTMuxer::CreateNewStream(AVCodecContext *codec)
{
	AVStream *new_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!new_stream) {
		printf("Failed allocating output stream\n");
		return NULL;
	}

	if (avcodec_copy_context(new_stream->codec, codec) < 0) {
		printf("Failed to copy context from input to output stream codec context\n");
		return NULL;
	}

	new_stream->codec->codec_tag = 0;
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		new_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return new_stream;
}