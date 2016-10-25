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

#define CTMUXER_MAX_PATH	256


class CTMuxer
{
public:
	CTMuxer();
	virtual ~CTMuxer();
	int SetOutputFileName(char *output_filename);
	int SetVideoCodecContext(AVCodecContext *codec);
	int SetAudioCodecContext(AVCodecContext *codec);
	int MuxVideo(AVPacket *video_pkt);
	int MuxAudio(AVPacket *audio_pkt);
private:
	int Init();
	AVStream *CreateNewStream(AVCodecContext *codec);
	
private:
	AVFormatContext *ofmt_ctx;
	AVOutputFormat *ofmt;
	AVStream *video_stream;
	AVStream *audio_stream;

	char m_output_filename[CTMUXER_MAX_PATH];
	int  m_inited;
};

#endif