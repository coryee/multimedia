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
	int SetPacketQueue(int stream_idx, CTAVPacketQueue *queue);
	int Start();
	int Stop();
	int Execute();
	int IsFinished();
private:
	int InitInternal();
private:
	char				m_uri[CTDEMUXER_MAX_URI];
	AVFormatContext		*m_ifmt_ctx;
	AVBitStreamFilterContext	*m_h264bsfc;
	AVPacket			m_packet;
	CTAVPacketQueue		*m_packet_queues[CTDEMUXER_QUEUE_NUM_STREAMS];

	int					m_video_idx;
	int					m_audio_idx;
	int					m_keep_running;
	int					m_is_running;
};

#endif