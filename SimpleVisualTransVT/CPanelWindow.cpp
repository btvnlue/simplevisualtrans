#include "CPanelWindow.h"
#include <CommCtrl.h>
std::map<HWND, CPanelWindow*> CPanelWindow::views;

LRESULT CPanelWindow::ResizeCurrentWindow(HWND hcurr)
{
	RECT rct;
	::GetClientRect(hWnd, &rct);
	MoveWindow(hcurr, rct.left, rct.top, rct.right - rct.left, rct.bottom - rct.top, TRUE);
	return 0;
}
LRESULT CPanelWindow::WndProcInstance(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lst = 0;
	switch (message)
	{
		//case WM_PAINT:
		//{
		//	PAINTSTRUCT ps;
		//	HDC hdc = BeginPaint(hWnd, &ps);
		//	HBRUSH hrb = CreateSolidBrush(RGB(0xFF, 0xCF, 0xCF));
		//	HGDIOBJ hoj = SelectObject(hdc, hrb);
		//	RECT rct;
		//	::GetClientRect(hWnd, &rct);
		//	FillRect(hdc, &rct, hrb);
		//	SelectObject(hdc, hoj);
		//	EndPaint(hWnd, &ps);
		//}
		//break;
	case WM_COMMAND:
	case WM_NOTIFY:
	case WM_NOTIFYFORMAT:
	{
		HWND hpwnd = ::GetParent(hWnd);
		if (hpwnd) {
			lst = SendMessageW(hpwnd, message, wParam, lParam);
		}
	}
	break;
	case WM_SIZE:
		if (hCurrentWindow) {
			ResizeCurrentWindow(hCurrentWindow);
		}
		break;
	//case WM_PAINT:
	//	if (hCurrentWindow) {
	//		InvalidateRect(hCurrentWindow, NULL, TRUE);
	//	}
	//	break;
	default:
		DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}
	return lst;
}

LRESULT CPanelWindow::WndPanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lst = 0;

	if (views.find(hWnd) != views.end()) {
		lst = views[hWnd]->WndProcInstance(hWnd, message, wParam, lParam);
	}
	else {
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return lst;
}

ATOM CPanelWindow::panelClass = 0;
#define PANELCLASSNAME L"PanelView"
int CPanelWindow::RegisterPanelClass()
{
	if (panelClass == 0) {
		WNDCLASSEXW wcex;
		HINSTANCE hInstance = ::GetModuleHandle(NULL);

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = CPanelWindow::WndPanelProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = NULL;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = PANELCLASSNAME;
		wcex.hIconSm = NULL;

		panelClass = RegisterClassExW(&wcex);
	}
	return 0;

}

int CPanelWindow::InitInstance(HWND hpnt, int nshow)
{
	HINSTANCE hinst = GetModuleHandle(NULL);
	hWnd = CreateWindow(PANELCLASSNAME, PANELCLASSNAME, WS_CHILD | WS_VISIBLE,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, hpnt, nullptr, hinst, nullptr);

	if (!hWnd)
	{
		DWORD dLastErr = ::GetLastError();
		WCHAR wstr[1024];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dLastErr, 0, wstr, 1000, NULL);

		return FALSE;
	}

	ShowWindow(hWnd, nshow);
	return 1;
}

CPanelWindow::CPanelWindow()
	:hCurrentWindow(NULL)
{
}

CPanelWindow* CPanelWindow::CreatePanelWindow(HWND hparent)
{

	// TODO: Place code here.

	// Initialize global strings
	RegisterPanelClass();

	CPanelWindow* view = new CPanelWindow();
	BOOL bInit = view->InitInstance(hparent, SW_SHOW);
	// Perform application initialization:
	if (bInit)
	{
		views[view->hWnd] = view;
	}
	else {
		delete view;
		view = NULL;
	}

	return view;
}

int CPanelWindow::SwitchWindow(HWND hwnd)
{
	for (std::set<HWND>::iterator itsw = subwindows.begin()
		; itsw != subwindows.end()
		; itsw++) {
		if (hwnd == *itsw) {
			ShowWindow(hwnd, SW_SHOW);
			hCurrentWindow = hwnd;
			InvalidateRect(hWnd, NULL, TRUE);
			ResizeCurrentWindow(hCurrentWindow);
			SetFocus(hwnd);
		} else {
			ShowWindow(*itsw, SW_HIDE);
		}
	}

	return 0;
}

int CPanelWindow::PushWindow(HWND hwnd)
{
	subwindows.insert(hwnd);
	std::map<HWND, CPanelWindow*>::iterator itvw = views.find(hwnd);
	if (itvw == views.end()) {
		views[hwnd] = this;
	}
	return 0;
}

CPanelWindow::operator HWND() const
{
	return hWnd;
}
