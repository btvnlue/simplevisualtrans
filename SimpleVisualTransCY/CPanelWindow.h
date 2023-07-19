#pragma once

#include <Windows.h>
#include <set>
#include <map>

class CPanelWindow
{
	std::set<HWND> subwindows;
	static std::map<HWND, CPanelWindow*> views;
	HWND hWnd;
	LRESULT ResizeCurrentWindow(HWND hcurr);
	LRESULT WndProcInstance(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndPanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static ATOM panelClass;
	static int RegisterPanelClass();
	int InitInstance(HWND hpnt, int nshow);
public:
	CPanelWindow();
	static CPanelWindow* CreatePanelWindow(HWND hparent);
	int SwitchWindow(HWND hwnd);
	int PushWindow(HWND hwnd);
	HWND hCurrentWindow;
	operator HWND() const;
};

