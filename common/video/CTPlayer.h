#pragma once


#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL.h>
#ifdef __cplusplus
};
#endif

#include "udpserver.h"
#include "CTH264Decoder.h"
#include "mbuffer.h"
#include "CTAVBuffer.h"
#include "CTDisplay.h"

#define CTPLAYER_MAX_URL		256

#define CTPLAYER_EC_OK			0
#define CTPLAYER_EC_FAILURE		-1

typedef enum  {
	LIVESTREAM_TYPE_UDP, 
} PLAYER_LIVESTREAM_TYPE;

typedef enum CTPlayerStatus {
	CTPLAYER_STATUS_DEFAULT,
	CTPLAYER_STATUS_INITED,
	CTPLAYER_STATUS_RUNNING,
	CTPLAYER_STATUS_STOPPED
} CTPlayerStatus;

class CTPlayer
{
public:
	CTPlayer();
	~CTPlayer();
	
	int SetHandle(HWND hwnd);
	int Play(const char *url);
	int Pause();
	int Stop();
	int Execute();
private:
	int Init();
	
private:
	char m_url[CTPLAYER_MAX_URL];
	AVFormatContext	*m_format_ctx;
	AVPacket	m_packet;
	int m_video_idx;
	CTH264Decoder m_decoder;
	UDPServer	m_udp_server; // receive data through network
	MBUFFERSYSBuffer m_sys_buffer; // m_udp_server will put data into this buffer as a producer, 
								   // m_decoder will pull data out as a consumer. 
								   // note this buffer has only one producer and one consumer, so this is no need to sync with each other
	CTAVPacketQueue *m_packet_queue;
	CTAVFrameBuffer *m_frame_buffer; // used to buffer the decoded frame. decoder is a producer. display is a consumer

	CTDisplay m_display;
	HWND m_hwnd; // used to display
	int m_keep_running;
	CTPlayerStatus m_status;
};

