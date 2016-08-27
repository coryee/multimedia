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
#include "ish264decoder.h"
#include "mbuffer.h"
#include "avframebuffer.h"
#include "CTDisplay.h"

#define CTPLAYER_EC_OK			0
#define CTPLAYER_EC_FAILURE		-1

typedef enum  {
	LIVESTREAM_TYPE_UDP, 
} PLAYER_LIVESTREAM_TYPE;


class CTPlayer
{
public:
	CTPlayer();
	~CTPlayer();
	
	int setHandle(HWND hwnd);
	int play(const char *pcURL);
	int pause();
	int stop();

private:
	ISH264Decoder m_decoder;
	UDPServer	m_udp_server; // receive data through network
	MBUFFERSYSBuffer m_sys_buffer; // m_udp_server will put data into this buffer as a producer, 
								   // m_decoder will pull data out as a consumer. 
								   // note this buffer has only one producer and one consumer, so this is no need to sync with each other
	ISAVFrameBuffer *m_frame_buffer; // used to buffer the decoded frame. decoder is a producer. display is a consumer

	CTDisplay m_disp;
	HWND m_hwnd; // used to display
};

