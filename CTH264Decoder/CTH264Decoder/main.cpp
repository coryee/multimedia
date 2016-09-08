#include <stdio.h>

#define inline __inline
#include "CTH264Decoder.h"
#include "CTAVBuffer.h"


#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
#ifdef __cplusplus
};
#endif



static CTAVPacketQueue gPacketQueue;
static CTAVPacketQueue *gpPacketQueue = &gPacketQueue;
static CTAVFrameBuffer *gpFrameBuffer = NULL;

int main(int argc, char* argv[])
{
	AVFormatContext	*pFormatCtx;
	int				i, videoindex;
	AVPacket *packet;
	int ret, got_picture;

	char *outputfile = "output.yuv420p";
	FILE *fpOutput = fopen(outputfile, "wb");
	char *filepath = "E:\\work\\testfiles\\When_You_Believe.mp4";
	av_register_all();
	pFormatCtx = avformat_alloc_context();
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
		videoindex = i;
		break;
	}
	if (videoindex == -1){
		printf("Didn't find a video stream.\n");
		return -1;
	}

	CTAVPacketQueueInit(gpPacketQueue, 20);
	CTH264Decoder decoder;
	if (H264DEC_EC_OK != decoder.Init(pFormatCtx->streams[videoindex], H264DEC_MODE_PACKETQUEUE))
		return -1;
	decoder.SetInputPacketQueue(gpPacketQueue);
	gpFrameBuffer = decoder.OutputFrameBuffer();
	decoder.Start();

	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	for (;;) {
		while (1)
		{
			if (av_read_frame(pFormatCtx, packet) < 0)
				break;
			if (packet->stream_index == videoindex)
				break;
		}
		
		while (CTAVPacketQueuePut(gpPacketQueue, packet) == CTAV_BUFFER_EC_FULL)
		{
			Sleep(1);
		}

		while (CTAVFrameBufferNumFrames(gpFrameBuffer) > 0)
		{
			CTAVFrame *pFrame = CTAVFrameBufferFirstFrame(gpFrameBuffer);
			printf("width:%d", pFrame->pFrame->width);
		}
		av_packet_unref(packet);
	}
	//--------------
	avformat_close_input(&pFormatCtx);

	return 0;
}

