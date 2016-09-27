#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

#include "CTDXVA2Display.h"

#define DBGMSG 

#pragma comment(lib, "Winmm.lib")

const TCHAR CLASS_NAME[] = TEXT("DXVA2 Test Display");
const TCHAR WINDOW_NAME[] = TEXT("DXVA2 Test Application");
HWND    g_Hwnd = NULL;
HANDLE  g_hTimer = NULL;
const UINT VIDEO_SUB_VX = 5;
const UINT VIDEO_SUB_VY = 3;
const UINT VIDEO_FPS = 60;
const UINT VIDEO_MSPF = (1000 + VIDEO_FPS / 2) / VIDEO_FPS;
BOOL g_bTimerSet = FALSE;
DWORD g_StartSysTime = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL InitializeTimer()
{
	g_hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	if (!g_hTimer)
	{
		DBGMSG((TEXT("CreateWaitableTimer failed with error %d.\n"), GetLastError()));
		return FALSE;
	}

	LARGE_INTEGER li = { 0 };

	if (!SetWaitableTimer(g_hTimer,
		&li,
		VIDEO_MSPF,
		NULL,
		NULL,
		FALSE))
	{
		DBGMSG((TEXT("SetWaitableTimer failed with error %d.\n"), GetLastError()));
		return FALSE;
	}

	g_bTimerSet = (timeBeginPeriod(1) == TIMERR_NOERROR);

	g_StartSysTime = timeGetTime();

	return TRUE;
}

BOOL InitializeWindow()
{
	WNDCLASS wc = { 0 };

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = CLASS_NAME;

	if (!RegisterClass(&wc))
	{
		DBGMSG((TEXT("RegisterClass failed with error %d.\n"), GetLastError()));
		return FALSE;
	}

	//
	// Start in Window mode regardless of the initial mode.
	//
	g_Hwnd = CreateWindow(CLASS_NAME,
		WINDOW_NAME,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		GetModuleHandle(NULL),
		NULL);

	if (!g_Hwnd)
	{
		DBGMSG((TEXT("CreateWindow failed with error %d.\n"), GetLastError()));
		return FALSE;
	}

	ShowWindow(g_Hwnd, SW_SHOWDEFAULT);
	UpdateWindow(g_Hwnd);

	return TRUE;
}

BOOL PreTranslateMessage(const MSG& msg)
{
	return TRUE;
}


INT MessageLoop()
{
	MSG msg = { 0 };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (PreTranslateMessage(msg))
			{
				continue;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);

			continue;
		}

		//
		// Wait until the timer expires or any message is posted.
		//
		if (MsgWaitForMultipleObjects(1,
			&g_hTimer,
			FALSE,
			INFINITE,
			QS_ALLINPUT) == WAIT_OBJECT_0)
		{
			;
		}
	}

	return INT(msg.wParam);
}

static CTDXVA2Display disp;


int main()
{
	INT wParam = 0;

	if (InitializeWindow() &&
		InitializeTimer())
	{
		disp.Init(g_Hwnd);
		disp.SetFrameResolution(1080, 720);
		disp.Display(NULL);
		wParam = MessageLoop();
	}
	return 0;
}