
// videoplayerDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "videoplayer.h"
#include "videoplayerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define PIX_FMT_NV12		AV_PIX_FMT_NV12
#define PIX_FMT_YUV420P		AV_PIX_FMT_YUV420P

// CvideoplayerDlg 对话框



CvideoplayerDlg::CvideoplayerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_VIDEOPLAYER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CvideoplayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CvideoplayerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDPLAY, &CvideoplayerDlg::OnBnClickedPlay)
END_MESSAGE_MAP()


// CvideoplayerDlg 消息处理程序

BOOL CvideoplayerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	// test

	char pcProto[8];
	char pcIPAddr[32];
	int iPort;
	char *pcURL = "udp://10.0.70.189:10001";
	int iret = sscanf(pcURL, "%*//%^[:]", pcIPAddr, &iPort);
	iret = sscanf(pcURL, "%[^/]//%[^:]:%d", pcProto, pcIPAddr, &iPort);


	

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CvideoplayerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CvideoplayerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


static int PlayerExecute(void *player)
{
	CvideoplayerDlg *thePlayer = (CvideoplayerDlg *)player;
	thePlayer->InitPlayer();
	return(0);
}

AVPixelFormat GetHwFormat(AVCodecContext *s, const AVPixelFormat *pix_fmts)
{
	InputStream* ist = (InputStream*)s->opaque;
	ist->active_hwaccel_id = HWACCEL_DXVA2;
	ist->hwaccel_pix_fmt = AV_PIX_FMT_DXVA2_VLD;
	return ist->hwaccel_pix_fmt;
}

int CvideoplayerDlg::InitPlayer()
{
	player.setHandle(this->GetDlgItem(IDC_SCREEN)->GetSafeHwnd());
	player.play("udp://10.0.70.189:10001");

	return 0;
}
int CvideoplayerDlg::InitPlayer1()
{
#if 0
	//FFmpeg
	
	char filepath[] = "E:\\testfile\\videoplayback_720.mp4";

	av_register_all();
	avformat_network_init();

	m_pFormatCtx = avformat_alloc_context();

	if (avformat_open_input(&m_pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if (avformat_find_stream_info(m_pFormatCtx, NULL) < 0) {
		printf("Couldn't find stream information.\n");
		return -1;
	}
	m_videoindex = -1;
	for (i = 0; i < m_pFormatCtx->nb_streams; i++)
		if (m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			m_videoindex = i;
			break;
		}
	if (m_videoindex == -1) {
		printf("Didn't find a video stream.\n");
		return -1;
	}
	m_pCodecCtx = m_pFormatCtx->streams[m_videoindex]->codec;
	m_pCodec = avcodec_find_decoder(m_pCodecCtx->codec_id);
	if (m_pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}

	m_pCodecCtx->thread_count = 1;
	InputStream *ist = new InputStream();
	ist->hwaccel_id = HWACCEL_AUTO;
	ist->hwaccel_device = "dxva2";
	ist->dec = m_pCodec;
	ist->dec_ctx = m_pCodecCtx;

	m_pCodecCtx->opaque = ist;
	
	if (0 == dxva2_init(m_pCodecCtx))
	{
		int kk = 0;
		m_pCodecCtx->get_buffer2 = ist->hwaccel_get_buffer;
		m_pCodecCtx->get_format = GetHwFormat;
		m_pCodecCtx->thread_safe_callbacks = 1;
	}



	if (avcodec_open2(m_pCodecCtx, m_pCodec, NULL) < 0) {
		printf("Could not open codec.\n");
		return -1;
	}



	m_packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	//Output Information-----------------------------
	printf("------------- File Information ------------------\n");
	av_dump_format(m_pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");

#if OUTPUT_YUV420P 
	m_fp_yuv = fopen("output.yuv", "wb+");
#endif  

	ThreadHandle handle;
	// CTCreateThread()


	
	//------------------------------

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	//	SDL_CreateWindowFrom()
	HWND hwnd = this->GetDlgItem(IDC_SCREEN)->GetSafeHwnd();
	RECT screenRect;
	this->GetDlgItem(IDC_SCREEN)->GetClientRect(&screenRect);
	screen = SDL_CreateWindowFrom((void *)hwnd);
	m_screen_h = screenRect.bottom - screenRect.top;
	m_screen_w = screenRect.right - screenRect.left;

	/*m_screen_w = m_pCodecCtx->width;
	m_screen_h = m_pCodecCtx->height;
 	screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
 		m_screen_w, m_screen_h,
 		SDL_WINDOW_OPENGL);*/
	
	
	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	//sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m_pCodecCtx->width, m_pCodecCtx->height);
	

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = m_screen_w;
	sdlRect.h = m_screen_h;

	bool gotGapH = false;
	int leftx = 0;
	int topy = 0;
	double iRatio = (double)m_pCodecCtx->width / (double)m_pCodecCtx->height;
	int videoWidth = iRatio * m_screen_h;
	int videoHeight;
	if (videoWidth > m_screen_w)
	{
		videoWidth = m_screen_w;
		videoHeight = videoWidth / iRatio;
		gotGapH = true;
		topy = (m_screen_h - videoHeight) / 2;
	}
	else
	{
		videoHeight = m_screen_h;
		gotGapH = false;
		leftx = (m_screen_w - videoWidth) / 2;
	}
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, videoWidth, videoHeight/*m_screen_w, m_screen_h*/);


	m_img_convert_ctx = sws_getContext(m_pCodecCtx->width, m_pCodecCtx->height, PIX_FMT_NV12, videoWidth, videoHeight, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
//	m_img_convert_ctx = sws_getContext(m_pCodecCtx->width, m_pCodecCtx->height, PIX_FMT_NV12, m_pCodecCtx->width, m_pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	m_pFrame = av_frame_alloc();
	m_pFrameYUV = av_frame_alloc();
	uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, m_pCodecCtx->width, m_pCodecCtx->height));
	avpicture_fill((AVPicture *)m_pFrameYUV, out_buffer, PIX_FMT_YUV420P, m_pCodecCtx->width, m_pCodecCtx->height);
	//uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, tmpWidth, tmpHeight));
	//avpicture_fill((AVPicture *)m_pFrameYUV, out_buffer, PIX_FMT_YUV420P, tmpWidth, tmpHeight);

	int nb_frame = 0;
	while (av_read_frame(m_pFormatCtx, m_packet) >= 0) {
		if (m_packet->stream_index == m_videoindex) {
			//Decode
			m_ret = avcodec_decode_video2(m_pCodecCtx, m_pFrame, &m_got_picture, m_packet);
			if (m_ret < 0) {
				printf("Decode Error.\n");
				return -1;
			}
			if (m_got_picture) {


				if (0 != dxva2_retrieve_data_call(m_pCodecCtx, m_pFrame))
					continue;
				int height = sws_scale(m_img_convert_ctx, (const uint8_t* const*)m_pFrame->data, m_pFrame->linesize, 0, m_pCodecCtx->height,
					m_pFrameYUV->data, m_pFrameYUV->linesize);


#if 0
				int y_size = tmpWidth * tmpHeight; // m_pFrameYUV->width * m_pCodecCtx->height;
				FILE *fp = fopen("pic_yuv420p.yuv", "wb");
				fwrite(m_pFrameYUV->data[0], 1, y_size, fp);    //Y 
				fwrite(m_pFrameYUV->data[1], 1, y_size / 4, fp);  //U
				fwrite(m_pFrameYUV->data[2], 1, y_size / 4, fp);  //V
				fclose(fp);
#endif

																//SDL---------------------------
#if 0
				SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
#else
				srcRect.x = 0;
				srcRect.y = 0;
				srcRect.w = videoWidth;
				srcRect.h = videoHeight;
				SDL_UpdateYUVTexture(sdlTexture, &srcRect,
					m_pFrameYUV->data[0], m_pFrameYUV->linesize[0],
					m_pFrameYUV->data[1], m_pFrameYUV->linesize[1],
					m_pFrameYUV->data[2], m_pFrameYUV->linesize[2]);
#endif	

				srcRect.x = leftx;
				srcRect.y = topy;
				if (gotGapH)
				{
					srcRect.w = m_screen_w;
					srcRect.h = videoHeight;
				}
				else
				{
					srcRect.w = videoWidth;
					srcRect.h = m_screen_h;
				}
				SDL_RenderClear(sdlRenderer);
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &srcRect);
				SDL_RenderPresent(sdlRenderer);
				av_frame_unref(m_pFrame);
				//SDL End-----------------------
				//Delay 40ms
				SDL_Delay(30);
				//				if (pFrame->format == AV_PIX_FMT_DXVA2_VLD)
				//				{
				//					int kk = 0;
				//				}
				printf("number_frame:%d\n", nb_frame++);
			}
		}
		av_free_packet(m_packet);
	}

	//FIX: Flush Frames remained in Codec
	while (1) {
		m_ret = avcodec_decode_video2(m_pCodecCtx, m_pFrame, &m_got_picture, m_packet);
		if (m_ret < 0)
			break;
		if (!m_got_picture)
			break;
		sws_scale(m_img_convert_ctx, (const uint8_t* const*)m_pFrame->data, m_pFrame->linesize, 0, m_pCodecCtx->height, m_pFrameYUV->data, m_pFrameYUV->linesize);


	}

	sws_freeContext(m_img_convert_ctx);

#if OUTPUT_YUV420P 
	fclose(m_fp_yuv);
#endif 

	//av_free(out_buffer);
	av_free(m_pFrameYUV);
	avcodec_close(m_pCodecCtx);
	avformat_close_input(&m_pFormatCtx);
#endif
	return 0;
}


void CvideoplayerDlg::OnBnClickedPlay()
{
	// TODO: 在此添加控件通知处理程序代码
// 	ThreadHandle handle;
// 	CTCreateThread(&handle, (ThreadFunc)PlayerExecute, this);

	TRACE("start");
	printf("start\n");
	player.setHandle(this->GetDlgItem(IDC_SCREEN)->GetSafeHwnd());
	player.play("udp://10.0.70.189:10001");

}
