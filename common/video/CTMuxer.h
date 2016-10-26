#ifndef CTMUXER_H_
#define CTMUXER_H_

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavformat/avformat.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif
#endif

#include <vector>
using namespace std;

#define CTMUXER_MAX_CONTAINER_FORMAT_SIZE	64
#define CTMUXER_MAX_PATH				256
// #define CTMUXER_

enum 
{
	CTMUXER_STREAM_TYPE_VIDEO = 0,
	CTMUXER_STREAM_TYPE_AUDIO,
	CTMUXER_NUM_STREAM_TYPE,
};

typedef struct CTMuxerStreamInfo {
	AVCodecID codec_id;
	AVMediaType codec_type;
	AVRational time_base;

	// the following four members are used for audio stream
	AVPixelFormat pix_fmt;
	int width;
	int height;
	int fps;

	// the following five members are used for audio stream
	AVSampleFormat sample_fmt; // AV_SAMPLE_FMT_S16/AV_SAMPLE_FMT_S16P;;
	int sample_rate; // 44100
	uint64_t channel_layout;
	int channels;
	int bit_rate; // 64000;
} CTMuxerStreamInfo;


typedef struct CTMuxerOutput {
	AVFormatContext *ofmt_ctx;
	AVStream  *streams[CTMUXER_NUM_STREAM_TYPE];
	char filename[CTMUXER_MAX_PATH];
	char container_format[CTMUXER_MAX_CONTAINER_FORMAT_SIZE];
	int  need_audio_bitstream_filter;
	int  need_video_bitstream_filter;
} CTMuxerOutput;

// note: calling sequence
//	1. AddOutputFileName()
//  2. AddInputStreamInfo()
//  3. MuxPacket()
class CTMuxer
{
public:
	CTMuxer();
	virtual ~CTMuxer();
	int AddOutputFileName(const char *output_filename);
	int AddInputStreamInfo(int stream_type, AVCodecContext *codec_ctx, AVRational time_base);
	int AddInputStreamInfo(int stream_type, CTMuxerStreamInfo *stream_info);
	int Mux(int stream_type, AVPacket *pkt);

private:
	int Init();
	AVStream *CreateStream(AVFormatContext *ofmt_ctx, AVCodecContext *codec_ctx);
	AVStream *CreateStream(AVFormatContext *ofmt_ctx, CTMuxerStreamInfo *stream_info);
	int InitBitstreamFilter(int stream_type, AVCodecID codec_id);
	
private:
	AVStream  *m_output_streams[CTMUXER_NUM_STREAM_TYPE];
	CTMuxerStreamInfo m_input_stream_infos[CTMUXER_NUM_STREAM_TYPE];
	AVRational	m_input_stream_time_bases[CTMUXER_NUM_STREAM_TYPE];
	vector<CTMuxerOutput> m_outputs;
	AVBitStreamFilterContext *m_aacbsfc;
	AVBitStreamFilterContext* m_h264bsfc;
	int  m_inited;
};

#endif