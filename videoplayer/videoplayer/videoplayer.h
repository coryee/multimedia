
// videoplayer.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CvideoplayerApp: 
// �йش����ʵ�֣������ videoplayer.cpp
//

class CvideoplayerApp : public CWinApp
{
public:
	CvideoplayerApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CvideoplayerApp theApp;