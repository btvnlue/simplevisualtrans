#pragma once

#include <Windows.h>
#include <map>

#define SW_DEFAULT_SPLIT_SIZE 6

class CSplitWnd
{
	HWND hWnd;
	static std::map<HWND, CSplitWnd*> selflist;
	static ATOM atomSplitClass;
	HBRUSH hBrushBack;
	int leftSize;
	int splitSize;
	double leftRatio;
	HWND hLeft;
	HWND hRight;
	WCHAR wbuf[1024] = { 0 };

	CSplitWnd();
	static ATOM RegisterSplitWindowClass(HINSTANCE hinst);
	static LRESULT CALLBACK WndSplitProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int ssx, ssy;
	BOOL onMove;
	int visiSize;
public:
	enum SPW_STYLE {
		LEFTRIGHT
		, TOPDOWN
		, DOCKTOP
		, DOCKDOWN
	} style;
	int fixedSize;
	int winId;
	static CSplitWnd* CreateSplitWindow(HWND hparent, int wid = 0);
	static CSplitWnd* GetSplitWindow(HWND hwnd);
	operator HWND() const;
	BOOL SetWindow(HWND hwnd);
	int ProcSize();
	int DestroyInsideWindows();
	int StartMove(int xx, int yy);
	int ProcOnMove(int xx, int yy);
	int StopMove(int xx, int yy);
	int SetWindowCursor();
	double SetRatio(double ratio);
};

