#include <stdio.h>
#include "CTDemuxer.h"
#include "threadutil.h"

CTAVPacketQueue video_queue;
CTAVPacketQueue audio_queue;


static char test_file[MAX_PATH] = "E:\\work\\testfiles\\wuyanzhu.flv";
static char output_file_v[MAX_PATH];
static char output_file_a[MAX_PATH];

static FILE *fp_v;
static FILE *fp_a;
static int g_exit = 0;


static int Write2File(void *arg)
{
	AVPacket packet;
	int num_video_packet = 0;
	int num_audio_packet = 0;
	int got_packet = 0;
	while (!g_exit) {
		if (CTAV_BUFFER_EC_OK == CTAVPacketQueueGet(&video_queue, &packet)) {
			fwrite(packet.data, 1, packet.size, fp_v);
			av_packet_unref(&packet);
			printf("num_video_packet: %d\n", ++num_video_packet);
		}
		if (CTAV_BUFFER_EC_OK == CTAVPacketQueueGet(&audio_queue, &packet)) {
			fwrite(packet.data, 1, packet.size, fp_a);
			av_packet_unref(&packet);
			printf("num_audio_packet: %d\n", ++num_audio_packet);
		}
	}

	while (CTAV_BUFFER_EC_OK == CTAVPacketQueueGet(&video_queue, &packet)) {
		fwrite(packet.data, 1, packet.size, fp_v);
		av_packet_unref(&packet);
		printf("num_video_packet: %d\n", ++num_video_packet);
	}
	while (CTAV_BUFFER_EC_OK == CTAVPacketQueueGet(&audio_queue, &packet)) {
		fwrite(packet.data, 1, packet.size, fp_a);
		av_packet_unref(&packet);
		printf("num_audio_packet: %d\n", ++num_audio_packet);
	}

	fclose(fp_v);
	fclose(fp_a);

	return(0);
}


int TestOutput2File()
{
	CTDemuxer demuxer;

	sprintf(output_file_v, "%s.h264", test_file);
	sprintf(output_file_a, "%s.aac", test_file);

	if (0 != demuxer.Init(test_file))
		return -1;
	demuxer.SetOutputFile(output_file_v, output_file_a);
	demuxer.Start();

	while (!demuxer.IsFinished()) {
		Sleep(1000);
	}

	g_exit = 1;
	Sleep(5000);

	return 0;
}

int TestOutput2Queue()
{
	CTDemuxer demuxer;

	sprintf(output_file_v, "%s.h264", test_file);
	sprintf(output_file_a, "%s.aac", test_file);
	fp_v = fopen(output_file_v, "wb+");
	fp_a = fopen(output_file_a, "wb+");

	if (fp_v == NULL || fp_a == NULL)
		return -1;

	CTAVPacketQueueInit(&video_queue, 16);
	CTAVPacketQueueInit(&audio_queue, 16);

	if (0 != demuxer.Init(test_file))
		return -1;
	demuxer.SetPacketQueue(CTDEMUXER_QUEUE_VIDEO_STREAM, &video_queue);
	demuxer.SetPacketQueue(CTDEMUXER_QUEUE_AUDIO_STREAM, &audio_queue);
	demuxer.Start();

	CTThreadHandle handle;
	if (CTCreateThread(&handle, (CTThreadFunc)Write2File, NULL) != 0)
		return -1;
	CTCloseThreadHandle(handle);

	printf("hello world\n");

	while (!demuxer.IsFinished()) {
		Sleep(1000);
	}

	g_exit = 1;
	Sleep(5000);

	CTAVPacketQueueDeInit(&video_queue);
	CTAVPacketQueueDeInit(&audio_queue);
	return 0;
}

int main()
{
	TestOutput2File();
	//TestOutput2Queue();
	return 0;
}