#include "CSplitWnd.h"

#include <windowsx.h>

#define DEFVISISIZE 10

const WCHAR SZ_SPLITWINDOW_CLASS[] = L"SplitWndClass";
const WCHAR SZ_SPLITWINDOW_NAME[] = L"SplitWndName";

std::map<HWND, CSplitWnd*> CSplitWnd::selflist;
ATOM CSplitWnd::atomSplitClass = 0;

CSplitWnd::CSplitWnd()
	:hWnd(NULL)
	, hBrushBack(NULL)
	, leftSize(0)
	, leftRatio(0.4)
	, hLeft(NULL)
	, hRight(NULL)
	, style(LEFTRIGHT)
	, splitSize(SW_DEFAULT_SPLIT_SIZE)
	, onMove(FALSE)
	, visiSize(DEFVISISIZE)
	, fixedSize(0)
	, winId(0)
	, ssx(0)
	, ssy(0)
{
	hBrushBack = ::GetSysColorBrush(COLOR_BTNFACE);
	//hBrushBack = ::CreateSolidBrush(RGB(0xFF,0xEF,0xEF));
}

ATOM CSplitWnd::RegisterSplitWindowClass(HINSTANCE hinst)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndSplitProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hinst;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)COLOR_BTNSHADOW;
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = SZ_SPLITWINDOW_CLASS;
	wcex.hIconSm = NULL;

	return RegisterClassExW(&wcex);
}

int CSplitWnd::SetWindowCursor()
{
	if (style == LEFTRIGHT)
	{
		HCURSOR hcur = LoadCursor(NULL, IDC_SIZEWE);
		::SetCursor(hcur);
	}
	else if (style == TOPDOWN)
	{
		HCURSOR hcur = LoadCursor(NULL, IDC_SIZENS);
		::SetCursor(hcur);
	}
	return 0;
}

double CSplitWnd::SetRatio(double ratio)
{
	leftRatio = ratio;
	return leftRatio;
}

CSplitWnd* CSplitWnd::CreateSplitWindow(HWND hparent, int wid)
{
	CSplitWnd* self = new CSplitWnd();
	HINSTANCE hinst = ::GetModuleHandle(NULL);

	if (atomSplitClass == 0)
	{
		atomSplitClass = RegisterSplitWindowClass(hinst);
	}
	self->hWnd = ::CreateWindowEx(0, SZ_SPLITWINDOW_CLASS, SZ_SPLITWINDOW_NAME, WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, hparent, NULL, hinst, NULL);
	
	if (self->hWnd)
	{
		selflist.insert({ self->hWnd, self });
		self->SetWindowCursor();
		self->winId = wid==0?rand():wid;
	}
	else
	{
		DWORD dwe = ::GetLastError();
		delete self;
		self = NULL;
	}
	return self;
}

int CSplitWnd::DestroyInsideWindows()
{
	if (hLeft)
	{
		::DestroyWindow(hLeft);
		hLeft = NULL;
	}
	if (hRight)
	{
		::DestroyWindow(hRight);
		hRight = NULL;
	}
	return 0;
}

int CSplitWnd::StartMove(int xx, int yy)
{
	ssx = xx;
	ssy = yy;
	onMove = TRUE;
	SetCapture(hWnd);
	return 0;
}

int CSplitWnd::ProcOnMove(int xx, int yy)
{
	if (onMove)
	{
		if (style == LEFTRIGHT)
		{
			int mvsz = xx - ssx;
			leftSize += mvsz;
			if (leftSize < visiSize)
			{
				leftSize = visiSize;
			}
			ssx = xx;
		}
		if (style == TOPDOWN)
		{
			int mvsz = yy - ssy;
			leftSize += mvsz;
			if (leftSize < visiSize)
			{
				leftSize = visiSize;
			}
			ssy = yy;
		}
		ProcSize();
	}
	SetWindowCursor();
	
	return 0;
}

int CSplitWnd::StopMove(int xx, int yy)
{
	onMove = FALSE;
	RECT rct;
	::GetClientRect(hWnd, &rct);
	if (style == LEFTRIGHT)
	{
		leftRatio = (double)leftSize / (rct.right - rct.left);
	}
	if (style == TOPDOWN)
	{
		leftRatio = (double)leftSize / (rct.bottom - rct.top);
	}
	ReleaseCapture();
	return 0;
}
LRESULT CALLBACK CSplitWnd::WndSplitProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrst = 0;
	CSplitWnd* self = CSplitWnd::GetSplitWindow(hWnd);
	switch (message)
	{
	case WM_NOTIFY:
	case WM_COMMAND:
	case WM_DRAWITEM:
	{
		HWND hpnt = ::GetParent(hWnd);
		if (hpnt != INVALID_HANDLE_VALUE)
		{
			lrst = ::SendMessage(hpnt, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
		if (self)
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			RECT rct;

			::GetClientRect(hWnd, &rct);
			::FillRect(hdc, &rct, self->hBrushBack);
			EndPaint(hWnd, &ps);

			if (self->hLeft) {
				PostMessage(self->hLeft, message, wParam, lParam);
				//InvalidateRect(self->hLeft, NULL, TRUE);
			}
			if (self->hRight) {
				PostMessage(self->hRight, message, wParam, lParam);
				//InvalidateRect(self->hRight, NULL, TRUE);
			}
		}
	break;
	case WM_CLOSE:
		if (self)
		{
			self->DestroyInsideWindows();
		}
		break;
	case WM_LBUTTONDOWN:
		if (self)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);

			self->StartMove(xPos, yPos);
		}
		break;
	case WM_MOUSEMOVE:
		if (self)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);

			self->ProcOnMove(xPos, yPos);
			//::OutputDebugString(L"on mouse move\n");
		}
		break;
	case WM_LBUTTONUP:
		if (self)
		{
			self->StopMove(0, 0);
		}
		break;
	case WM_DESTROY:
		if (self)
		{
			self->DestroyInsideWindows();
		}
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		if (self)
		{
			WORD width = LOWORD(lParam);
			WORD height = HIWORD(lParam);
			self->leftSize = (int)(self->style == LEFTRIGHT ? self->leftRatio * width : self->leftRatio * height);
			self->ProcSize();
		}
		break;

	default:
		lrst = DefWindowProc(hWnd, message, wParam, lParam);
	}
	return lrst;
}

CSplitWnd::operator HWND() const
{
	return hWnd;
}

CSplitWnd* CSplitWnd::GetSplitWindow(HWND hwnd)
{
	std::map<HWND, CSplitWnd*>::iterator itsw = selflist.find(hwnd);
	CSplitWnd* self = NULL;
	if (itsw != selflist.end())
	{
		self = itsw->second;
	}
	return self;
}

BOOL CSplitWnd::SetWindow(HWND hwnd)
{
	BOOL rst = TRUE;

	if (hwnd)
	{
		if (hLeft)
		{
			if (hRight)
			{
				SetWindow(NULL);
				SetWindow(hwnd);
			}
			else
			{
				hRight = hwnd;
			}
		}
		else
		{
			hLeft = hwnd;
		}
		::ShowWindow(hwnd, SW_SHOW);
	}
	else
	{
		hLeft = NULL;
		hRight = NULL;
	}

	return 0;
}

int CSplitWnd::ProcSize()
{
	RECT rct;
	RECT wrct;
	::GetClientRect(hWnd, &rct);
	switch (style)
	{
	case LEFTRIGHT:
		if (hLeft)
		{
			::MoveWindow(hLeft, 0, 0, leftSize, rct.bottom, FALSE);
		}
		if (hRight)
		{
			::MoveWindow(hRight, leftSize + splitSize, 0, rct.right - leftSize - splitSize, rct.bottom, FALSE);
		}
		break;
	case TOPDOWN:
		if (hLeft)
		{
			::MoveWindow(hLeft, 0, 0, rct.right, leftSize, FALSE);
		}
		if (hRight)
		{
			::MoveWindow(hRight, 0, leftSize + splitSize, rct.right, rct.bottom - leftSize - splitSize, FALSE);
		}
		break;
	case DOCKDOWN:
		if (fixedSize == 0) {
			::GetWindowRect(hRight, &wrct);
			fixedSize = wrct.bottom - wrct.top;
		}
		if (hRight) {
			::MoveWindow(hRight, 0, rct.bottom - fixedSize, rct.right, fixedSize, FALSE);
		}
		if (hLeft) {
			::MoveWindow(hLeft, 0, 0, rct.right - rct.left, rct.bottom - fixedSize, FALSE);
		}
		break;
	case DOCKTOP:
		if (fixedSize == 0) {
			::GetWindowRect(hLeft, &wrct);
			fixedSize = wrct.bottom - wrct.top;
		}
		if (hLeft) {
			::MoveWindow(hLeft, 0, 0, rct.right, fixedSize, FALSE);
		}
		if (hRight) {
			::MoveWindow(hRight, 0, fixedSize, rct.right - rct.left, rct.bottom - fixedSize, FALSE);
		}
		break;
	}
	InvalidateRect(hWnd, NULL, FALSE);
	return 0;
}