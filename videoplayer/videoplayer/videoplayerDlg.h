
// videoplayerDlg.h : ͷ�ļ�
//

#pragma once
#include <stdio.h>
#include "ffmpeg_dxva2.h"
#include "threadutil.h"
#include "CTPlayer.h"

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL/SDL.h>
#ifdef __cplusplus
};
#endif
#endif


//Output YUV420P 
#define OUTPUT_YUV420P 0


// CvideoplayerDlg �Ի���
class CvideoplayerDlg : public CDialogEx
{
// ����
public:
	CvideoplayerDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_VIDEOPLAYER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��

public:
//	void Execute();
	int InitPlayer();
	int InitPlayer1();
// ʵ��
protected:
	HICON m_hIcon;


	AVFormatContext	*m_pFormatCtx;
	int				i, m_videoindex;
	AVCodecContext	*m_pCodecCtx;
	AVCodec			*m_pCodec;
	AVFrame	*m_pFrame, *m_pFrameYUV;
	AVPacket *m_packet;
	struct SwsContext *m_img_convert_ctx;
	//SDL
	int m_screen_w, m_screen_h;

	FILE *m_fp_yuv;
	int m_ret, m_got_picture;
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;
	SDL_Rect srcRect;

	CTPlayer player;


	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedPlay();
};
