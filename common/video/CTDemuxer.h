#ifndef _CTDEMUXER_H
#define _CTDEMUXER_H

#include "CTAVBuffer.h"
#define CTDEMUXER_MAX_URI	256

enum {
	CTDEMUXER_QUEUE_VIDEO_STREAM = 0,
	CTDEMUXER_QUEUE_AUDIO_STREAM,
	CTDEMUXER_QUEUE_NUM_STREAMS
};

class CTDemuxer
{
public:
	CTDemuxer();
	~CTDemuxer();

	int Init(char *uri);
	int SetOutputFile(char *video_output_file, char *audio_output_file);
	int SetPacketQueue(int stream_idx, CTAVPacketQueue *queue);
	int Start();
	int Stop();
	int Execute();
	int IsFinished();
private:
	int InitInternal();
	int GetADTS4AACPacket(AVPacket *pkt, char *buffer, int buffer_size);
private:
	char				m_uri[CTDEMUXER_MAX_URI];
	char				m_video_output_file[CTDEMUXER_MAX_URI];
	char				m_audio_output_file[CTDEMUXER_MAX_URI];
	AVFormatContext		*m_ifmt_ctx;
	AVBitStreamFilterContext	*m_h264bsfc;
	AVPacket			m_packet;
	CTAVPacketQueue		*m_packet_queues[CTDEMUXER_QUEUE_NUM_STREAMS];

	int					m_video_idx;
	int					m_audio_idx;
	int					m_is_aac_codec;
	int					m_keep_running;
	int					m_is_running;
};

#endif