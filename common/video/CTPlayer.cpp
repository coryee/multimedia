#include "CTPlayer.h"


static int readDataFromFile(void *arg)
{
	MBUFFERSYSBuffer *gSysbuffer = (MBUFFERSYSBuffer *)arg;
	char *pInFile = "../../data/When_You_Believe.h264";
	FILE *fpInFile = fopen(pInFile, "rb");
	if (fpInFile == NULL)
		return -1;
	int iTotalSize = 0;
	while (!feof(fpInFile))
	{
		int iAvaiLen = MBUFFERSYSBufferSpaceAvailableToEnd(gSysbuffer);
		if (iAvaiLen > 0)
		{
			int iReadSize = fread(MBUFFERSYSAppendDataPos(gSysbuffer), 1, iAvaiLen, fpInFile);
			if (iReadSize < 0)
				break;
			MBUFFERSYSExtend(iReadSize, gSysbuffer);
			iTotalSize += iReadSize;
		}
		else
			Sleep(1);
	}

	return 0;
}

CTPlayer::CTPlayer()
{
	
}


int CTPlayer::setHandle(HWND hwnd)
{
	m_hwnd = hwnd;
	return CTPLAYER_EC_FAILURE;
}

int CTPlayer::play(const char *pcURL)
{
	char pcIPAddr[16];
	int iPort;

	if (memcmp(pcURL, "udp", 3) != 0) // udp
		return CTPLAYER_EC_FAILURE;

	if (sscanf(pcURL, "%*[^/]//%[^:]:%d", pcIPAddr, &iPort) != 2)
		return CTPLAYER_EC_FAILURE;

	MBUFFERSYSBufferInit(NULL, 0, 0, 1024 * 1024 * 10, &m_sys_buffer);
	/*m_udp_server.setLocalIPPort(pcIPAddr, iPort);
	m_udp_server.setMode(UDPS_WORK_MODE_DUMP2BUFFER);
	m_udp_server.setSYSBuffer(&m_sys_buffer);
	m_udp_server.start();*/

	// test
	ThreadHandle thread;
	CTCreateThread(&thread, (ThreadFunc)readDataFromFile, &m_sys_buffer);

	if (H264DEC_EC_OK != m_decoder.Init())
		return CTPLAYER_EC_FAILURE;

	m_decoder.SetInputBuffer(&m_sys_buffer);
	m_frame_buffer = m_decoder.GetOutputBuffer();
	m_decoder.StartDecode();

	m_disp.init(m_hwnd);
	m_disp.setAVFrameBuffer(CTDISP_BUFFER_INDEX_VIDEO, m_decoder.GetOutputBuffer());
	m_disp.start();
	

	return CTPLAYER_EC_OK;
}

int CTPlayer::pause()
{
	return CTPLAYER_EC_OK;
}

int CTPlayer::stop()
{
	return CTPLAYER_EC_OK;
}

CTPlayer::~CTPlayer()
{
}
