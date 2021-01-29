// SimpleVisualTrans.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "SimpleVisualTrans.h"
#include "WindowView.h"

#include <fstream>

int Log(const wchar_t* msg, bool bAppend);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	Log(L"Debug Log Start...", false);
	WindowView *view = WindowView::CreateView(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
	if (view) {
		view->ShowView();
		delete view;
	}
}

int Log(const wchar_t* msg)
{
	Log(msg, true);
	return 0;
}

int Log(const wchar_t* msg, bool bAppend)
{
	std::wfstream ofs;
	if (bAppend) {
		ofs.open(L"debug.log", std::ios_base::app | std::ios_base::out);
	}
	else {
		ofs.open(L"debug.log", std::ios_base::out);
	}
	//HANDLE hflog = CreateFile(L"debug.log", GENERIC_WRITE | FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	//if (hflog) {
	SYSTEMTIME stm;
	::GetLocalTime(&stm);

	size_t wsl = wcslen(msg);
	wsl += 1024;
	wsl *= sizeof(WCHAR);
	WCHAR* logsbuf = (WCHAR*)malloc(wsl);
	//DWORD lnw;
	if (logsbuf) {
		swprintf(logsbuf, wsl, L"[%4d-%02d-%02d %02d:%02d:%02d.%03d] %s\n", stm.wYear, stm.wMonth, stm.wDay, stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds, msg);
		//::OutputDebugString(logsbuf);
		ofs << logsbuf << std::endl;

		free(logsbuf);
	}
	ofs.flush();
	ofs.close();
	return 0;
}



