#include "WindowView.h"
#include "Resource.h"
#include "TransAnalyzer.h"
#include "CPanelWindow.h"

#include <array>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <ShlObj.h>
#include <CommCtrl.h>
#include <windowsx.h>
#include <CommCtrl.h>

#include <time.h>

#pragma comment(lib, "Comctl32.lib")

#define WM_U_INITWINDOW WM_USER + 0x222
#define WM_U_REFRESHREMOTE WM_USER + 0x333
#define WM_U_ADDNEWRESOURCE WM_USER + 0x444
#define WM_U_CLOSEFUNCSVIEW WM_USER + 0x555
#define WM_U_REMOVETORRENT_ WM_USER + 0x666
#define WM_U_STARTSESSION WM_USER + 0x777
#define WM_U_PAUSETORRENT WM_USER + 0x888
#define WM_U_DELAYMESSAGE WM_USER + 0x999
#define WM_U_REFRESHSESSION WM_USER + 0xAAA

ATOM WindowView::viewClass = 0;
std::map<HWND, WindowView*> WindowView::views;
WCHAR WindowView::szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR WindowView::szWindowClass[MAX_LOADSTRING];            // the main window class name
extern bool sortasc;

struct DELAYMESSAGE {
	HWND hwnd;
	DWORD delay;
	DWORD stamp;
	UINT msg;
	WPARAM wparam;
	LPARAM lparam;
	WCHAR* text;
};

WCHAR* buildText(const std::wstring& intxt)
{
	size_t ssz = intxt.length();
	ssz++;
	WCHAR* nbf = (WCHAR*)malloc(ssz * sizeof(WCHAR));
	wcscpy_s(nbf, ssz, intxt.c_str());

	return nbf;
}

int FormatNormalNumber(wchar_t* buf, size_t bufsz, long num)
{
	wsprintf(buf, L"%d", num);
	return 0;
}

int FormatNumberView(wchar_t* buf, size_t bufsz, unsigned __int64 size) 
{
	unsigned int ccn;
	wchar_t tbf[128];
	
	buf[0] = 0;
	tbf[0] = 0;
	if (size > 0) {
		while (size >= 1000) {
			ccn = size % 1000;
			wsprintf(tbf, L",%03d%s", ccn, buf);
			wsprintf(buf, L"%s", tbf);
			size /= 1000;
		}
		if (size > 0) {
			ccn = size % 1000;
			wsprintf(buf, L"%d%s", ccn, tbf);
		}
	}
	else {
		wsprintf(buf, L"0");
	}
	return 0;
}

int FormatNumberByteView(wchar_t* buf, size_t bufsz, unsigned __int64 size)
{
	unsigned int rst = size & 0x3FF;
	wchar_t smark[] = L"BKMGTPEZY";
	int smarkp = 0;

	if (size > 768) {
		if (size < (1 << 10)) {
			size = 0;
		}
		while (size > ((1 << 10) - 1)) {
			smarkp++;
			rst = size & 0x3FF;
			size >>= 10;
		}

		rst = (rst * 1000) >> 10;
		wsprintf(buf, L"%d.%03d %c", (unsigned int)size, rst, smark[smarkp]);
	}
	else {
		wsprintf(buf, L"%d %c", rst, smark[smarkp]);
	}
	return 0;
}

int FormatDualByteView(wchar_t* wbuf, int bsz, unsigned __int64 size)
{
	wchar_t dbuf[1024];
	FormatNumberByteView(dbuf, 1024, size);
	FormatNumberView(dbuf + 512, 512, size);
	wsprintf(wbuf, L"%s (%s)", dbuf + 512, dbuf);
	return 0;
}

WindowView* WindowView::CreateView(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.

	// Initialize global strings
	MyRegisterClass(hInstance);

	WindowView* view = new WindowView();
	BOOL bInit = view->InitInstance(hInstance, nCmdShow);
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

WindowView::WindowView()
	: hInst(NULL)
	, hLog(NULL)
	, hWnd(NULL)
	, splitLog(NULL)
	, splitTree(NULL)
	, canRefresh(false)
	, hToolImg(NULL)
	, hMain(NULL)
	, hNew(NULL)
	, hUrl(NULL)
	, panelState(NULL)
	, splitTool(NULL)
	, splitStatus(NULL)
	, delaytick(0)
	, refreshcount(0)
	, listview(NULL)
	, treeview(NULL)
{
	analyzer = new TransAnalyzer();

	//InitializeTorrentDetailTitle();
}

WindowView::~WindowView()
{
	delete analyzer;
	analyzer = NULL;

	TreeItemParmDataHelper::ClearTreeItemParmData();

	if (listview) {
		delete listview;
		listview = NULL;
	}

	if (treeview) {
		//if (treeview->callback) {
		//	delete treeview->callback;
		//	treeview->callback = NULL;
		//}
		delete treeview;
		treeview = NULL;
	}

}

int WindowView::ShowView()
{
	HACCEL hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_SIMPLEVISUALTRANS));

	MSG msg;
	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!IsDialogMessage(hUrl, &msg)) {
			if (!IsDialogMessage(hNew, &msg)) {
				if (!IsDialogMessage(hDelay, &msg)) {
//					if (!IsDialogMessage(hAb, &msg)) {
						if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
						{
							TranslateMessage(&msg);
							DispatchMessage(&msg);
						}
//					}
				}
			}
		}
	}

	return (int)msg.wParam;
}

int WindowView::MyRegisterClass(_In_ HINSTANCE hInstance)
{
	if (viewClass == 0) {
		WNDCLASSEXW wcex;

		LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		LoadStringW(hInstance, IDC_SIMPLEVISUALTRANS, szWindowClass, MAX_LOADSTRING);

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WindowView::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SIMPLEVISUALTRANS));
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SIMPLEVISUALTRANS);
		wcex.lpszClassName = szWindowClass;
		wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

		viewClass = RegisterClassExW(&wcex);
	}
	return 0;
}

LRESULT WindowView::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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

int WindowView::CopyTorrentsDetailOnView()
{
	TorrentNode* node;
	std::set<TorrentNode*> dts;
	std::wstringstream wss;

	listview->GetListSelectedTorrents(dts);
	if (dts.size() <= 0) {
		treeview->GetTreeSelectedTorrents(dts);
	}
	for (std::set<TorrentNode*>::iterator itds = dts.begin()
		; itds != dts.end()
		; itds++) {
		node = *itds;
		wss << TorrentNodeHelper::DumpTorrentNode(node) << L'\r' << std::endl;
	}
	if (wss.str().length() > 0) {
		SendStringClipboard(wss.str());
	}
	if (dts.size() > 0) {
		if (dts.size() > 1) {
			wss.str(std::wstring());
			wss << L"Copy [" << dts.size() << L"] torrents to clipboard";
			ViewLog(wss.str().c_str());
		}
		else {
			wss.str(std::wstring());
			wss << L"Copy torrent [" << (*dts.begin())->name << L"] to clipboard";
			ViewLog(wss.str().c_str());
		}
	}
	return 0;
}

int WindowView::CheckTorrentFilesOnView(BOOL check)
{
	std::set<TorrentNodeFileNode*> fns;
	FileReqParm frp = { 0 };
	listview->GetListSelectedFiles(fns);
	if (fns.size() <= 0) {
		treeview->GetTreeSelectedFiles(fns);
	}
	std::set<TorrentNode*> nodes;
	for (std::set<TorrentNodeFileNode*>::iterator itfs = fns.begin()
		; itfs != fns.end()
		; itfs++) {
		nodes.insert((*itfs)->node);
	}

	for (std::set<TorrentNode*>::iterator itnd = nodes.begin()
		; itnd != nodes.end()
		; itnd++) {
		frp.fileIds.clear();
		for (std::set<TorrentNodeFileNode*>::iterator itts = fns.begin()
			; itts != fns.end()
			; itts++) {
			frp.fileIds.insert((*itts)->id);
		}
		frp.setcheck = true;
		frp.check = check == TRUE;
		frp.torrentid = (*itnd)->id;
		int rdr = analyzer->PerformRemoteFileCheck(frp);
		std::wstring wrt = analyzer->GetErrorString(rdr);
		wchar_t wbuf[1024];
		wsprintf(wbuf, L"Remote change torrent [%s] files [%d] with result: [%s]", (*itnd)->name.c_str(), frp.fileIds.size(), wrt.c_str());
		ViewLog(wbuf);
	}
	return 0;
}

int WindowView::PriorityTorrentFilesOnView(int prt)
{
	std::set<TorrentNodeFileNode*> fns;
	FileReqParm frp = { 0 };
	listview->GetListSelectedFiles(fns);
	if (fns.size() <= 0) {
		treeview->GetTreeSelectedFiles(fns);
	}
	std::set<TorrentNode*> nodes;
	for (std::set<TorrentNodeFileNode*>::iterator itfs = fns.begin()
		; itfs != fns.end()
		; itfs++) {
		nodes.insert((*itfs)->node);
	}

	for (std::set<TorrentNode*>::iterator itnd = nodes.begin()
		; itnd != nodes.end()
		; itnd++) {
		frp.fileIds.clear();
		for (std::set<TorrentNodeFileNode*>::iterator itts = fns.begin()
			; itts != fns.end()
			; itts++) {
			frp.fileIds.insert((*itts)->id);
		}
		frp.setpriority = true;
		frp.priority = prt;
		frp.torrentid = (*itnd)->id;
		int rdr = analyzer->PerformRemoteFileCheck(frp);
		std::wstring wrt = analyzer->GetErrorString(rdr);
		wchar_t wbuf[1024];
		wsprintf(wbuf, L"Remote set torrent [%s] files [%d] with priority [%d] result: [%s]", (*itnd)->name.c_str(), frp.fileIds.size(), prt, wrt.c_str());
		ViewLog(wbuf);
	}
	return 0;
}

int WindowView::RemoveTorrentOnView(BOOL bContent)
{
	TorrentNode* node;
	std::set<TorrentNode*> dts;
	struct DELAYMESSAGE* dmsg;

	listview->GetListSelectedTorrents(dts);
	if (dts.size() <= 0) {
		treeview->GetTreeSelectedTorrents(dts);
	}

	std::wstringstream wss;
	for (std::set<TorrentNode*>::iterator itds = dts.begin()
		; itds != dts.end()
		; itds++) {
		node = *itds;
		dmsg = new struct DELAYMESSAGE();
		dmsg->hwnd = hWnd;
		dmsg->msg = WM_U_REMOVETORRENT_;
		dmsg->wparam = node->id;
		dmsg->lparam = bContent;
		dmsg->delay = 10000;
		wss.str(std::wstring());
		wss << L"delete torrent [" << node->name << L"]";
		dmsg->text = buildText(wss.str());
		PostMessage(hWnd, WM_U_DELAYMESSAGE, (WPARAM)dmsg, NULL);
	}

	return 0;
}


void WindowView::RefreshTorrentNodeFiles(TorrentNode* node)
{
	ReqParm rpm;
	rpm.torrentid = node->id;
	rpm.reqfiles = node->files.count == 0 ? true : false;
	rpm.reqfilestat = true;
	rpm.reqpieces = false;
	int rtn = analyzer->PerformRemoteRefreshTorrent(rpm);
	if (rtn == 0) {
		if (rpm.reqfiles) {
			treeview->UpdateTreeViewTorrentDetailNode(node);
		}
		UpdateListViewTorrents();
	}
}

void WindowView::RefreshTorrentNodeDetail(TorrentNode* node)
{
	ReqParm rpm;
	rpm.torrentid = node->id;
	rpm.reqfiles = node->files.count == 0 ? true : false;
	rpm.reqfilestat = false;
	rpm.reqpieces = true;
	int rtn = analyzer->PerformRemoteRefreshTorrent(rpm);
	if (rtn == 0) {
		treeview->UpdateTreeViewTorrentDetailNode(node);
		UpdateListViewTorrents();
	}
}

int WindowView::RefreshSession()
{
	int rtn = analyzer->PerformRemoteSessionStat();
	if (rtn == 0) {
		UpdateViewSession(&analyzer->session);
	}
	return 0;
}

int WindowView::UpdateViewSession(SessionInfo* ssn)
{
	wchar_t wbuf[1024];

	listview->UpdateListViewSession(ssn);
	
	FormatNumberByteView(wbuf + 512, 512, ssn->downloadspeed);
	wsprintf(wbuf, L"Down: %s/s", wbuf + 512);
	::SendMessage(hStatus, SB_SETTEXT, MAKEWPARAM(0, 0), (LPARAM)wbuf);

	FormatNumberByteView(wbuf + 512, 512, ssn->uploadspeed);
	wsprintf(wbuf, L"Up: %s/s", wbuf + 512);
	::SendMessage(hStatus, SB_SETTEXT, MAKEWPARAM(1, 0), (LPARAM)wbuf);

	wsprintf(wbuf, L"Torrents: %d", ssn->torrentcount);
	::SendMessage(hStatus, SB_SETTEXT, MAKEWPARAM(2, 0), (LPARAM)wbuf);
	return 0;
}

int WindowView::RefreshActiveTorrents()
{
	int rtn = analyzer->PerformRemoteRefresh();

	if (rtn == 0) {
		treeview->UpdateViewTorrentGroup(analyzer->groupRoot);
		treeview->UpdateViewRemovedTorrents();
	}
	else {
		std::wstring wes = analyzer->GetErrorString(rtn);
		std::wstringstream wss;
		wss << L"Perform remote refresh with result [" << wes << L"]";
		ViewLog(wss.str().c_str());
	}
	return 0;
}

void WindowView::RefreshViewNode()
{
	HTREEITEM hti = treeview->GetSelectedItem();
	if (hti) {
		TreeParmData* tip = treeview->GetItemParm(hti);
		if (tip) {
			switch (tip->ItemType) {
			case TreeParmData::Torrent:
				if (listview->listContent == CViewListFrame::LISTCONTENT::TORRENTDETAIL) {
					RefreshTorrentNodeDetail(tip->node);
				}
				break;
			case TreeParmData::File:
				if (listview->listContent == CViewListFrame::LISTCONTENT::TORRENTFILE) {
					RefreshTorrentNodeFiles(tip->file->node);
				}
				break;
			case TreeParmData::Group:
				if (
					(listview->listContent == CViewListFrame::LISTCONTENT::TORRENTGROUP) ||
					(listview->listContent == CViewListFrame::LISTCONTENT::TORRENTLIST)
					) {
					UpdateListViewTorrents();
				}
			}
		}
	}
}


int WindowView::ProcContextMenuLog(int xx, int yy)
{
	POINT pnt = { xx, yy };
	BOOL bst;
	if (panelState->hCurrentWindow == hLog) {
		HINSTANCE hinst = ::GetModuleHandle(NULL);
		HMENU hcm = LoadMenu(hinst, MAKEINTRESOURCE(IDR_CONTEXTPOPS));
		if (hcm) {
			HMENU hsm = GetSubMenu(hcm, 0);
			if (hsm) {
				ClientToScreen(hLog, &pnt);
				bst = TrackPopupMenu(hsm, TPM_LEFTALIGN | TPM_LEFTBUTTON, pnt.x, pnt.y, 0, hWnd, NULL);
				DestroyMenu(hsm);
			}
		}
	}
	return 0;
}

int WindowView::ProcContextMenu(int xx, int yy)
{
	POINT pnt = { xx, yy };
	POINT wpt;
	RECT rct;
	int rtn = 1;


	if (rtn > 0) {
		wpt = pnt;
		::ScreenToClient(*treeview, &wpt);
		::GetClientRect(*treeview, &rct);
		if (PtInRect(&rct, wpt)) {
			rtn = treeview->ProcContextMenuTree(wpt.x, wpt.y);
		}
	}
	
	if (rtn > 0) {
		wpt = pnt;
		::ScreenToClient(*listview, &wpt);
		::GetClientRect(*listview, &rct);
		if (PtInRect(&rct, wpt)) {
			rtn = listview->ProcContextMenuList(wpt.x, wpt.y);
		}
	}

	if (rtn > 0) {
		if (panelState->hCurrentWindow == hLog) {
			wpt = pnt;
			::ScreenToClient(hLog, &wpt);
			::GetClientRect(hLog, &rct);
			if (PtInRect(&rct, wpt)) {
				rtn = ProcContextMenuLog(wpt.x, wpt.y);
			}
		}
	}


	return rtn;
}

int WindowView::SendStringClipboard(const std::wstring& str)
{
	BOOL btn = OpenClipboard(hWnd);
	if (btn) {
		btn = EmptyClipboard();
	}
	else {
		return 1;
	}

	HGLOBAL hgcp = NULL;
	size_t sln = 0;
	if (btn) {
		sln = str.length();
		sln++;
		hgcp = GlobalAlloc(GMEM_MOVEABLE, sln * sizeof(wchar_t));
		btn = hgcp!=NULL?btn:FALSE;
	}
	else {
		CloseClipboard();
		return 2;
	}

	HANDLE hdat = NULL;
	if (btn) {
		wchar_t* pwbuf = (wchar_t*)GlobalLock(hgcp);
		memcpy_s(pwbuf, sln * sizeof(wchar_t), str.c_str(), sln * sizeof(wchar_t));
		pwbuf[sln - 1] = 0;
		GlobalUnlock(hgcp);
		hdat = SetClipboardData(CF_UNICODETEXT, hgcp);
	}
	else {
		CloseClipboard();
		return 3;
	}

	btn = CloseClipboard();
	return btn?0:4;
}

int WindowView::PauseTorrentsOnView(BOOL bresume)
{
	TorrentNode* node;
	std::set<TorrentNode*> dts;
	std::wstringstream wss;

	listview->GetListSelectedTorrents(dts);
	if (dts.size() <= 0) {
		treeview->GetTreeSelectedTorrents(dts);
	}
	for (std::set<TorrentNode*>::iterator itds = dts.begin()
		; itds != dts.end()
		; itds++) {
		node = *itds;
		PostMessage(hWnd, WM_U_PAUSETORRENT, node->id, bresume);
	}
	if (dts.size() > 0) {
		if (dts.size() > 1) {
			wss.str(std::wstring());
			wss << (bresume?L"Resume":L"Pause") << L" [" << dts.size() << L"] torrents to clipboard";
			ViewLog(wss.str().c_str());
		}
		else {
			wss.str(std::wstring());
			wss << (bresume ? L"Resume" : L"Pause") << L"torrent [" << (*dts.begin())->name << L"] to clipboard";
			ViewLog(wss.str().c_str());
		}
	}

	return 0;
}

int WindowView::CopyTreeNodeName()
{
	TorrentNode* node;
	std::set<TorrentNode*> dts;
	std::wstringstream wss;

	listview->GetListSelectedTorrents(dts);
	if (dts.size() <= 0) {
		treeview->GetTreeSelectedTorrents(dts);
	}
	for (std::set<TorrentNode*>::iterator itds = dts.begin()
		; itds != dts.end()
		; itds++) {
		node = *itds;
		wss << node->name << L'\r' << std::endl;
	}
	if (wss.str().length() > 0) {
		SendStringClipboard(wss.str());
	}
	if (dts.size() > 0) {
		if (dts.size() > 1) {
			wss.str(std::wstring());
			wss <<  L"Copy [" << dts.size() << L"] torrents name to clipboard";
			ViewLog(wss.str().c_str());
		}
		else {
			wss.str(std::wstring());
			wss << L"Copy torrent [" << (*dts.begin())->name << L"] name to clipboard";
			ViewLog(wss.str().c_str());
		}
	}

	return 0;
}

#define APPDEFAULTINIFILE L"visualtrans.ini"
LRESULT WindowView::WndProcInstance(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, DlgProcAbout);
			break;
		case IDM_TEST:
			analyzer->PerformRemoteSessionStat();
			break;
		case IDM_REMOVE:
		case IDM_DELETETREETORRENT:
			RemoveTorrentOnView(FALSE);
			break;
		case IDM_DELETETREETORRENTANDDISTCONTENT:
			RemoveTorrentOnView(TRUE);
			break;
		case IDM_PAUSELISTNODE:
		case IDM_PAUSETREENODE:
			PauseTorrentsOnView(FALSE);
			break;
		case IDM_RESUMELISTNODE:
		case IDM_RESUMETREENODE:
			PauseTorrentsOnView(TRUE);
			break;
		case IDM_SELECTNODEINTREE:
		{
			std::set<TorrentNode*> tns;
			listview->GetListSelectedTorrents(tns);
			if (tns.size() > 0) {
				treeview->TreeViewSelectItem(*tns.begin());
				PostMessage(*treeview, WM_SETFOCUS, NULL, NULL);
			}
		}
			break;
		case IDM_COPYTREENODENAME:
		case IDM_COPYLISTNODENAME:
			CopyTreeNodeName();
			break;
		case IDM_COPYTREENODE:
		case IDM_COPYLISTNODE:
			CopyTorrentsDetailOnView();
			break;
		case IDM_DELETELISTTORRENT:
			RemoveTorrentOnView(FALSE);
			break;
		case IDM_DELETELISTTORRENTANDCONTENT:
			RemoveTorrentOnView(TRUE);
			break;
		case IDM_CHECKFILES:
			CheckTorrentFilesOnView(TRUE);
			break;
		case IDM_UNCHECKFILES:
			CheckTorrentFilesOnView(FALSE);
			break;
		case IDM_PRIORITYFILESHIGH:
			PriorityTorrentFilesOnView(1);
			break;
		case IDM_PRIORITYFILESLOW:
			PriorityTorrentFilesOnView(-1);
			break;
		case IDM_PRIORITYFILESNORMAL:
			PriorityTorrentFilesOnView(0);
			break;
		case IDM_REFRESH:
			PostMessage(hWnd, WM_U_REFRESHREMOTE, 0, 0);
			break;
		case IDM_LOG:
			ViewLog(L"Outlog");
			panelState->SwitchWindow(hLog);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_FILE_ADDNEW:
			panelState->SwitchWindow(hNew);
			break;
		case IDM_SELECTPARENT:
			treeview->SelectTreeParentNode();
			break;
		case IDM_DUMPLOGCONTENT:
			DumpLogToClipboard(hLog);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_DROPFILES:
		if (hNew) {
			PostMessage(hNew, WM_DROPFILES, wParam, lParam);
			panelState->SwitchWindow(hNew);
		}
		else {
			DragFinish((HDROP)wParam);
		}
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_U_INITWINDOW:
		CreateViewControls();
	case WM_SIZE:
		if (hMain)
		{
			RECT rct;
			::GetClientRect(hWnd, &rct);
			::MoveWindow(hMain, rct.left, rct.top, rct.right, rct.bottom, FALSE);

			GetWindowRect(hWnd, &rct);
			std::wstringstream wss;
			wss << rct.left;
			profile.SetDefaultValue(L"WindowLeft", wss.str().c_str());
			wss.str(std::wstring());
			wss << rct.top;
			profile.SetDefaultValue(L"WindowTop", wss.str().c_str());
			wss.str(std::wstring());
			wss << rct.right;
			profile.SetDefaultValue(L"WindowRight", wss.str().c_str());
			wss.str(std::wstring());
			wss << rct.bottom;
			profile.SetDefaultValue(L"WindowBottom", wss.str().c_str());
		}
		break;
	case WM_CONTEXTMENU:
		ProcContextMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_U_REFRESHREMOTE:
		RefreshActiveTorrents();
		RefreshViewNode();

		canRefresh = true;
		break;
	case WM_U_REFRESHTORRENTDETAIL:
		RefreshTorrentNodeDetail((TorrentNode*)wParam);
		break;
	case WM_U_REFRESHSESSION:
		RefreshSession();
		canRefresh = true;
		break;
	case WM_U_DELETELISTNODE:
		listview->DeleteNodeItem((TorrentNode*)wParam);
		break;
	case WM_U_CLEARLISTVIEW:
		listview->ClearColumns();
		break;
	case WM_U_ADDNEWRESOURCE:
	{
		BOOL bit = (BOOL)lParam; //'false' as multi-lines analysis

		if (bit) {
			WCHAR* nrs = (WCHAR*)wParam;
			std::wstring wts = analyzer->PerformRemoteAddTorrent(nrs);
			wchar_t wbuf[1024];
			wsprintf(wbuf, L"Remote add torrent [%s] with result [%s]", nrs, wts.c_str());
			ViewLog(wbuf);
			free(nrs);
		}
		else {
			WCHAR* purl = (WCHAR*)wParam;
			std::wstringstream wss(purl);
			std::wstring lns;
			while (std::getline(wss, lns)) {
				lns.erase(std::remove(lns.begin(), lns.end(), L'\r'), lns.end());
				int len = (int)lns.length() + 1;
				if (len > 1) {
					WCHAR* newres = (WCHAR*)malloc(len * sizeof(WCHAR));
					wcscpy_s(newres, len, lns.c_str());

					PostMessage(hWnd, WM_U_ADDNEWRESOURCE, (WPARAM)newres, TRUE);
				}
			}
		}
	}
		break;
	case WM_U_UITREEPROCMSG:
	{
		int(*func)(void*) = (int(*)(void*))wParam;
		
		if (func) {
			(*func)((void*)lParam);
		}
	}
		break;
	case WM_U_CLOSEFUNCSVIEW:
		panelState->SwitchWindow(hLog);
		break;
	case WM_U_REMOVETORRENT_:
	{
		long tid = (long)wParam;
		BOOL bContent = (BOOL)lParam;
		int rdr = analyzer->PerformRemoteDelete(tid, bContent == TRUE);
		std::wstring wrt = analyzer->GetErrorString(rdr);
		wchar_t wbuf[1024];
		wsprintf(wbuf, L"Remote delete torrent [%d] with result: [%s]", tid, wrt.c_str());
		ViewLog(wbuf);
	}
	break;
	case WM_U_DELAYMESSAGE:
		panelState->SwitchWindow(hDelay);
		PostMessage(hDelay, WM_U_DELAYMESSAGE, wParam, lParam);
		break;
	case WM_U_STARTSESSION:
		StartAnalysis();
	break;
	case WM_U_TREESELECTNODE:
		treeview->TreeViewSelectItem((TorrentNode*)wParam);
	break;
	case WM_U_TREESELECTGROUP:
		treeview->TreeViewSelectItem((TorrentGroup*)wParam);
	break;
	case WM_U_PAUSETORRENT:
	{
		int tid = (int)wParam;
		BOOL resume = (BOOL)lParam;
		int rtr = analyzer->PerformRemoteStop(tid, resume?false:true);
		wchar_t wbuf[1024];
		std::wstring wrt = analyzer->GetErrorString(rtr);
		wsprintf(wbuf, L"Remote %s torrent [%d] with result: [%s]", resume?L"resume":L"pause", tid, wrt.c_str());
		ViewLog(wbuf);
	}
	break;
	case WM_U_UPDATETORRENTGROUP:
		listview->UpdateListViewTorrentGroup((TorrentGroup*)wParam);
		break;
	case WM_U_UPDATETORRENTNODEDETAIL:
		listview->UpdateListViewTorrentDetail((TorrentNode*)wParam);
		break;
	case WM_U_UPDATESESSION:
		listview->ListUpdateSession((SessionInfo*)wParam);
		break;
	case WM_U_UPDATETORRENTNODEFILE:
		listview->ListUpdateFiles((TorrentNodeFileNode*)wParam);
		break;
	case WM_NOTIFY:
		lrt = ProcNotify(hWnd, message, wParam, lParam);
		break;
	case WM_DESTROY:
	{
		WCHAR pbf[1024];
		DWORD bsz = GetCurrentDirectory(512, pbf + 512);
		if (bsz > 0) {
			wsprintf(pbf, L"%s\\%s", pbf + 512, APPDEFAULTINIFILE);
			profile.StoreProfile(pbf);
			std::wstringstream wss;
			wss << L"Store profile into [" << pbf << L"]";
			ViewLog(wss.str().c_str());
		}	
		PostQuitMessage(0);
	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return lrt;
}

int WindowView::DumpLogToClipboard(HWND hlog)
{
	int lic = ListBox_GetCount(hlog);
	WCHAR wbuf[4096];
	std::wstringstream wss;
	int itl;

	for (int ii = 0; ii < lic; ii++) {
		itl = ListBox_GetTextLen(hlog, ii);
		assert(itl < 4096);
		ListBox_GetText(hlog, ii, wbuf);
		wss << wbuf << L'\r' << std::endl;
	}

	if (wss.str().length() > 0) {
		SendStringClipboard(wss.str());
		ViewLog(L"Done dump log content into Clipboard.");
	}
	return 0;
}

LRESULT WindowView::ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrst = 0;
	LPNMHDR pnmh = (LPNMHDR)lParam;

	if (treeview) {
		if (pnmh->hwndFrom == *treeview) {
			lrst = treeview->ProcNotifyTree(hWnd, message, wParam, lParam);
		}
	}
	if (listview) {
		if (pnmh->hwndFrom == *listview) {
			lrst = listview->ProcNotifyList(hWnd, message, wParam, lParam);
		}
	}

	return lrst;
}

int WindowView::InitializeStatusBar(HWND hsts)
{
	int pars[4] = { 100, 200, 300, -1 };
	::SendMessage(hsts, SB_SETPARTS, 4, (LPARAM) &pars);

	return 0;
}

int FormatViewStatus(wchar_t* wbuf, size_t bsz, long status)
{
	switch (status) {
	case 4:
		wsprintf(wbuf, L"Downloading");
		break;
	case 6:
		wsprintf(wbuf, L"Seeding");
		break;
	case 0:
		wsprintf(wbuf, L"Paused");
		break;
	case 2:
		wsprintf(wbuf, L"Verify");
		break;
	case 1:
		wsprintf(wbuf, L"Pending");
		break;
	default:
		wsprintf(wbuf, L"Unknown");
	}
	return 0;
}
int FormatViewDouble(wchar_t* wbuf, size_t bsz, double dval)
{
	int ivv = (int)dval;
	int ipp = (int)((dval - ivv) * 100);
	wsprintf(wbuf, L"%d.%02d", ivv, ipp);
	return 0;
}
int FormatTimeSeconds(wchar_t* wbuf, size_t bsz, long secs)
{
	int tss = secs % 60;
	secs /= 60;
	int tmm = secs % 60;
	secs /= 60;
	int thh = secs % 24;
	secs /= 24;

	if (secs > 0) {
		wsprintf(wbuf, L"%d d %02d:%02d:%02d", secs, thh, tmm, tss);
	}
	else {
		wsprintf(wbuf, L"%02d:%02d:%02d", thh, tmm, tss);
	}
	return 0;
}

int FormatIntegerDate(wchar_t* wbuf, size_t bsz, long mtt)
{
	if (mtt > 0) {
		tm ttm;
		//_gmtime32_s(&ttm, &mtt);
		_localtime32_s(&ttm, &mtt);
		wsprintf(wbuf, L"%4d-%02d-%02d %02d:%02d:%02d", ttm.tm_year + 1900, ttm.tm_mon + 1, ttm.tm_mday, ttm.tm_hour, ttm.tm_min, ttm.tm_sec);
	}
	else {
		wsprintf(wbuf, L"");
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK WindowView::DlgProcAbout(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		WCHAR damsg[] = L"Thanks for using this application.\r\nThis application is yet in staging period, \r\nwhich funtionalities to be tested.\r\nPlease contact \r\n\r\nbtvnlue@users.sourceforge.net\r\n\r\nfor any of BUGs and questions.\r\nThank you.";
		HWND htxt = GetDlgItem(hDlg, IDC_EDITDESC);
		if (htxt) {
			Edit_SetText(htxt, damsg);
		}
	}
	return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDC_CANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

#define DELAYPROCTIMER 0x1234

VOID WindowView::TimerProcDlgDelay(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	WindowView* self = (WindowView*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	struct DELAYMESSAGE* msg;

	if (self->delaytick > 0) {
		ULARGE_INTEGER lit;
		lit.QuadPart = GetTickCount64();
		DWORD ctk = lit.LowPart;
		if (ctk > self->delaytick) {
			for (std::set<struct DELAYMESSAGE*>::iterator itdm = self->delaymsgs.begin()
				; itdm != self->delaymsgs.end()
				; itdm++) {
				msg = *itdm;
				::PostMessage(msg->hwnd, msg->msg, msg->wparam, msg->lparam);
				if (msg->text) {
					free(msg->text);
				}
				delete msg;
			}			
			self->delaymsgs.clear();
			self->delaytick = 0;
			KillTimer(hwnd, DELAYPROCTIMER);
			PostMessage(hwnd, WM_U_CLOSEFUNCSVIEW, 0, 0);
			HWND hTxt = GetDlgItem(hwnd, IDC_LISTTASKS);
			if (hTxt) {
				ListBox_ResetContent(hTxt);
			}
		}
		else {
			DWORD bsk = self->delaytick - ctk;
			bsk *= 100;
			bsk /= 10000;
			HWND hprg = GetDlgItem(hwnd, IDC_PROGRESSSECONDS);
			if (hprg) {
				SendMessage(hprg, PBM_SETPOS, bsk, 0);
			}
			bsk /= 10;
			HWND htxt = GetDlgItem(hwnd, IDC_READSECS);
			if (htxt) {
				std::wstringstream wss;
				wss << bsk << L"s";
				Static_SetText(htxt, wss.str().c_str());
			}
		}
	}
}

INT_PTR CALLBACK WindowView::DlgProcDelay(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	INT_PTR rip = (INT_PTR)TRUE;
	WindowView* self = (WindowView*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

	switch (message) {
	case WM_INITDIALOG:
	{
		HWND hprg = GetDlgItem(hDlg, IDC_PROGRESSSECONDS);
		if (hprg) {
			SendMessage(hprg, PBM_SETSTEP, 1, 0);
		}
	}
	break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDB_NOW) {
			ULARGE_INTEGER lit;
			lit.QuadPart = GetTickCount64();
			self->delaytick = lit.LowPart;
		}
		else if (LOWORD(wParam) == IDB_CANCEL) {
			DELAYMESSAGE* msg;
			for (std::set<struct DELAYMESSAGE*>::iterator itdm = self->delaymsgs.begin()
				; itdm != self->delaymsgs.end()
				; itdm++) {
				msg = *itdm;
				if (msg->text) {
					free(msg->text);
				}
				delete msg;
			}
			self->delaymsgs.clear();
			ULARGE_INTEGER lit;
			lit.QuadPart = GetTickCount64();
			self->delaytick = lit.LowPart;
		}
		break;
	case WM_U_CLOSEFUNCSVIEW:
		PostMessage(self->hWnd, WM_U_CLOSEFUNCSVIEW, wParam, lParam);
		break;
	case WM_U_DELAYMESSAGE:
	{
		struct DELAYMESSAGE* dlmsg = (struct DELAYMESSAGE*)wParam;
		self->delaymsgs.insert(dlmsg);
		ULARGE_INTEGER lit;
		lit.QuadPart = GetTickCount64();
		self->delaytick = lit.LowPart + 10000;
		SetTimer(hDlg, DELAYPROCTIMER, 900, TimerProcDlgDelay);

		HWND hTxt = GetDlgItem(hDlg, IDC_LISTTASKS);
		if (hTxt) {
			ListBox_InsertString(hTxt, 0, dlmsg->text);
		}
	}
		break;
	case WM_SIZE:
		self->MoveDlgDelayControls(hDlg);
		break;
	default:
		rip = (INT_PTR)FALSE;
	}

	return rip;
}

INT_PTR CALLBACK WindowView::DlgProcUrl(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowView* self = (WindowView*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	INT_PTR rip = (INT_PTR)TRUE;

	switch (message)
	{
	case WM_INITDIALOG:
	{
		HWND hht = GetDlgItem(hDlg, IDC_EDITRPCPATH);
		if (hht) {
			Edit_SetText(hht, L"transmission/rpc");
		}
		hht = GetDlgItem(hDlg, IDC_EDITHOST);
		if (hht) {
			Edit_SetText(hht, L"127.0.0.1:9091");
		}
		hht = GetDlgItem(hDlg, IDC_EDITUSER);
		if (hht) {
			Edit_SetText(hht, L"transmission");
		}
		hht = GetDlgItem(hDlg, IDC_EDITPASSWD);
		if (hht) {
			Edit_SetText(hht, L"transmission");
		}
	}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_CONNECT) {
			HWND hctl;
			WCHAR wbuf[1024];
			hctl = GetDlgItem(hDlg, IDC_EDITHOST);
			if (hctl) {
				Edit_GetText(hctl, wbuf, 1024);
				self->remoteUrl = wbuf;
			}
			hctl = GetDlgItem(hDlg, IDC_EDITRPCPATH);
			if (hctl) {
				Edit_GetText(hctl, wbuf, 1024);
				self->remotePath = wbuf;
			}
			hctl = GetDlgItem(hDlg, IDC_EDITUSER);
			if (hctl) {
				Edit_GetText(hctl, wbuf, 1024);
				self->remoteUser = wbuf;
			}
			hctl = GetDlgItem(hDlg, IDC_EDITPASSWD);
			if (hctl) {
				Edit_GetText(hctl, wbuf, 1024);
				self->remotePass = wbuf;
			}
			hctl = GetDlgItem(hDlg, IDC_EDITURL);
			if (hctl) {
				Edit_GetText(hctl, wbuf, 1024);
				self->remoteUrl = wbuf;
			}
			self->profile.SetDefaultValue(L"RemoteHost", self->remoteHost);
			self->profile.SetDefaultValue(L"RemoteUser", self->remoteUser);
			self->profile.SetDefaultValue(L"RemotePath", self->remotePath);
			self->profile.SetDefaultValue(L"RemotePass", self->remotePass);
			self->profile.SetDefaultValue(L"RemoteUrl", self->remoteUrl);

			PostMessage(self->hWnd, WM_U_STARTSESSION, 0, 0);
		}
		else if (LOWORD(wParam) == IDC_EDITHOST) {
			if (HIWORD(wParam) == EN_CHANGE) {
				self->DlgProcUrlUpdateUrl(hDlg);
			}
		}
		break;
	case WM_SHOWWINDOW:
	case WM_SIZE:
		self->MoveDlgUrlControls(hDlg);
		break;
	default:
		rip = FALSE;
	}
	return rip;
}

int WindowView::DlgProcUrlUpdateUrl(const HWND hdlg)
{
	std::wstringstream wss;
	WCHAR wbuf[1024];
	HWND hht = GetDlgItem(hdlg, IDC_EDITHOST);
	if (hht) {
		int etl = Edit_GetText(hht, wbuf, 1024);
		if (etl > 0) {
			wss << L"http://" << wbuf;
		}
		hht = GetDlgItem(hdlg, IDC_EDITRPCPATH);
		if (hht) {
			etl = Edit_GetText(hht, wbuf, 1024);
			if (etl > 0) {
				wss << L"/" << wbuf;
			}
		}
	}

	if (wss.str().length() > 0) {
		HWND hht = GetDlgItem(hdlg, IDC_EDITURL);
		if (hht) {
			Edit_SetText(hht, wss.str().c_str());
		}
	}

	return 0;
}

#define MULTIFILENAMEBUF 8192

INT_PTR CALLBACK WindowView::DlgProcAddNew(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowView* self = (WindowView*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

	switch (message)
	{
	case WM_INITDIALOG:
		DragAcceptFiles(hDlg, TRUE);
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_ADDNEW) {
			HWND hurl = GetDlgItem(hDlg, IDC_EDITLOCALFILE);
			WCHAR* ppath = (WCHAR*)malloc(MULTIFILENAMEBUF * sizeof(WCHAR));
			Edit_GetText(hurl, ppath, MULTIFILENAMEBUF);
			size_t wsz = wcslen(ppath);
			if (wsz > 0) {
				SendMessage(self->hWnd, WM_U_ADDNEWRESOURCE, (WPARAM)ppath, 0);
			}
			Edit_SetText(hurl, L"");
			SendMessage(self->hWnd, WM_U_CLOSEFUNCSVIEW, (WPARAM)hDlg, 0);
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_CANCELNEW) {
			HWND hurl = GetDlgItem(hDlg, IDC_EDITLOCALFILE);
			Edit_SetText(hurl, L"");
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_BUTTONBROWSE) {
			WCHAR* szFile = (WCHAR*)malloc(MULTIFILENAMEBUF * sizeof(WCHAR));

			OPENFILENAME ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = self->hWnd;
			ofn.lpstrFile = szFile;
			// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
			// use the contents of szFile to initialize itself.
			ofn.lpstrFile[0] = '\0';
			ofn.nMaxFile = MULTIFILENAMEBUF;
			ofn.lpstrFilter = L"All\0*.*\0Torrent\0*.torrent\0";
			ofn.nFilterIndex = 2;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

			if (GetOpenFileName(&ofn)) {
				if (ofn.nFileOffset > 0) {
					std::wstringstream wss;
					wchar_t* pet = (wchar_t*)malloc(MULTIFILENAMEBUF * sizeof(wchar_t));
					HWND hurl = GetDlgItem(hDlg, IDC_EDITLOCALFILE);
					Edit_GetText(hurl, pet, MULTIFILENAMEBUF);
					wss << pet;
					free(pet);
					if (szFile[ofn.nFileOffset - 1] == 0) {
						std::wstring wpt = szFile;
						DWORD dps = ofn.nFileOffset;
						bool keepseek = szFile[dps + 1] != 0;
						DWORD ips;
						while (keepseek) {
							wss << wpt << L"\\" << szFile + dps << L'\r' << std::endl;
							ips = (DWORD)wcslen(szFile + dps);
							dps += ips + 1;
							keepseek = szFile[dps] != 0 ? keepseek : FALSE;
							keepseek = dps < ofn.nMaxFile ? keepseek : FALSE;
						}
					}
					else {
						wss << szFile << L'\r' << std::endl;
					}
					Edit_SetText(hurl, wss.str().c_str());
				}
			}
			free(szFile);
			return (INT_PTR)TRUE;
		}
		break;
	case WM_SHOWWINDOW:
	case WM_SIZE:
		self->MoveDlgAddNewControls(hDlg);
		break;
	case WM_DROPFILES:
	{
		HDROP hdrop = (HDROP)wParam;
		wchar_t* wbuf;
		UINT ddf = 0xFFFFFFFF;
		ddf = DragQueryFile(hdrop, ddf, NULL, 0);
		int bsz;
		std::wstringstream wss;
		wbuf = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
		for (UINT ii = 0; ii < ddf; ii++) {
			bsz = DragQueryFile(hdrop, ii, wbuf, MAX_PATH);
			if (bsz > 0) {
				wss << wbuf << L'\r' << std::endl;
			}
		}
		free(wbuf);

		if (wss.str().length() > 0) {
			HWND hedt = GetDlgItem(hDlg, IDC_EDITLOCALFILE);
			bsz = Edit_GetTextLength(hedt);
			wbuf = (wchar_t*)malloc((bsz + 1) * sizeof(wchar_t));
			bsz = Edit_GetText(hedt, wbuf, bsz + 1);
			if (bsz > 0) {
				wss << wbuf;
			}
			Edit_SetText(hedt, wss.str().c_str());
		}
		DragFinish(hdrop);
	}
		
		break;
	}
	return (INT_PTR)FALSE;
}

#define DEMG 3
#define CTLH 12

/**
Name: MoveDlgControlPos
Description: Move control and adjust height by font height
Return: Font height 
*/
int MoveDlgControlPos(HWND hctl, int xx, int yy, int weight)
{
	HDC hdc = GetDC(hctl);
	TEXTMETRIC tmc = { 0 };
	GetTextMetrics(hdc, &tmc);
	MoveWindow(hctl, xx, yy, weight, tmc.tmHeight, TRUE);

	return tmc.tmHeight + DEMG;
}

int WindowView::MoveDlgDelayControls(const HWND hDlg)
{
	RECT rct;
	GetClientRect(hDlg, &rct);
	DWORD wwd = rct.right - rct.left;
	DWORD whg = rct.bottom - rct.top;

	DWORD cxx = wwd;
	DWORD cyy = whg;

	HWND hctl;
	hctl = GetDlgItem(hDlg, IDB_NOW);
	RECT crct;
	if (hctl) {
		GetClientRect(hctl, &crct);
		MoveWindow(hctl, cxx - crct.right - DEMG, cyy - crct.bottom - DEMG, crct.right - crct.left, crct.bottom - crct.top, TRUE);
		cxx -= crct.right + DEMG;
		cyy -= crct.bottom + DEMG;
	}

	hctl = GetDlgItem(hDlg, IDB_CANCEL);
	if (hctl) {
		GetClientRect(hctl, &crct);
		MoveWindow(hctl, wwd - crct.right - DEMG, cyy - crct.bottom - DEMG, crct.right - crct.left, crct.bottom - crct.top, TRUE);
		cyy -= crct.bottom + DEMG;
	}

	hctl = GetDlgItem(hDlg, IDC_PROGRESSSECONDS);
	if (hctl) {
		crct.bottom = 16;
		MoveWindow(hctl, DEMG, DEMG, wwd - 2 * DEMG, crct.bottom, TRUE);
		cyy = crct.bottom + DEMG;
	}

	hctl = GetDlgItem(hDlg, IDC_LISTTASKS);
	if (hctl) {
		crct.bottom = 16;
		MoveWindow(hctl, DEMG, cyy + DEMG, cxx - 2 * DEMG, whg - cyy - DEMG, TRUE);
		cyy = crct.bottom + DEMG;
	}
	
	hctl = GetDlgItem(hDlg, IDC_READSECS);
	if (hctl) {
		crct.right = 36;
		crct.bottom = 16;
		MoveDlgControlPos(hctl, wwd - crct.right - DEMG, cyy + DEMG, crct.right);
		cyy = crct.bottom + DEMG;
	}
	return 0;
}

int WindowView::MoveDlgUrlControls(const HWND hDlg)
{
	RECT rct;
	GetClientRect(hDlg, &rct);
	DWORD wwd = rct.right - rct.left;
	DWORD whg = rct.bottom - rct.top;
	DWORD rch = 0;

	DWORD sxx = 0;
	DWORD syy = 0;

	sxx = DEMG;
	syy = DEMG;

	HWND hctl;
	hctl = GetDlgItem(hDlg, IDC_STATICHOST);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd / 2 - 2 * DEMG);
	}

	sxx += wwd / 2;
	hctl = GetDlgItem(hDlg, IDC_STATICRPC);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd / 2 - 2 * DEMG);
	}

	syy += rch + DEMG;
	sxx = DEMG;
	hctl = GetDlgItem(hDlg, IDC_EDITHOST);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd / 2 - 2 * DEMG);
	}

	sxx += wwd / 2;
	hctl = GetDlgItem(hDlg, IDC_EDITRPCPATH);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd / 2 - 2 * DEMG);
	}

	sxx = DEMG;
	syy += rch + DEMG*2;
	hctl = GetDlgItem(hDlg, IDC_STATICUSER);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd / 2 - 2 * DEMG);
	}

	sxx += wwd / 2;
	hctl = GetDlgItem(hDlg, IDC_STATICPASSWD);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd / 2 - 2 * DEMG);

	}
	
	sxx = DEMG;
	syy += rch + DEMG;
	hctl = GetDlgItem(hDlg, IDC_EDITUSER);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd / 2 - 2 * DEMG);
	}
	sxx += wwd / 2;
	hctl = GetDlgItem(hDlg, IDC_EDITPASSWD);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd / 2 - 2 * DEMG);
	}

	sxx = DEMG;
	syy += rch + DEMG * 2;
	hctl = GetDlgItem(hDlg, IDC_STATICURL);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd - 2 * DEMG);
	}
	sxx = DEMG;
	syy += rch + DEMG * 2;
	hctl = GetDlgItem(hDlg, IDC_EDITURL);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, wwd - 2 * DEMG);
	}

	sxx = DEMG;
	syy += rch + DEMG*2;
	hctl = GetDlgItem(hDlg, IDC_CONNECT);
	if (hctl) {
		rch = MoveDlgControlPos(hctl, sxx, syy, 84);
	}
	
	return 0;
}

int WindowView::MoveDlgAddNewControls(const HWND hDlg)
{
	RECT rct;
	HWND hctl;
	GetClientRect(hDlg, &rct);
	DWORD wwd = rct.right - rct.left;
	DWORD whg = rct.bottom - rct.top;
	hctl = GetDlgItem(hDlg, IDC_ADDNEW);
	if (hctl) {
		GetClientRect(hctl, &rct);
		MoveWindow(hctl, wwd - rct.right - DEMG, whg - rct.bottom - DEMG, rct.right - rct.left, rct.bottom - rct.top, TRUE);
	}
	hctl = GetDlgItem(hDlg, IDC_CANCELNEW);
	if (hctl) {
		GetClientRect(hctl, &rct);
		MoveWindow(hctl, wwd - rct.right - DEMG, whg - rct.bottom * 22 / 10 - DEMG, rct.right - rct.left, rct.bottom - rct.top, TRUE);
	}
	hctl = GetDlgItem(hDlg, IDC_BUTTONBROWSE);
	if (hctl) {
		GetClientRect(hctl, &rct);
		MoveWindow(hctl, wwd - rct.right - DEMG, whg - rct.bottom * 34 / 10 - DEMG, rct.right - rct.left, rct.bottom - rct.top, TRUE);
	}
	hctl = GetDlgItem(hDlg, IDC_EDITLOCALFILE);
	if (hctl) {
		MoveWindow(hctl, DEMG, 0, wwd - rct.right * 12 / 10 - DEMG * 2, whg - DEMG, FALSE);
	}
	
	return 0;
}


BOOL WindowView::InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		DWORD dLastErr = ::GetLastError();
		WCHAR wstr[1024];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dLastErr, 0, wstr, 1000, NULL);

		std::wcout << wstr;
		return FALSE;
	}

	DragAcceptFiles(hWnd, TRUE);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	PostMessage(hWnd, WM_U_INITWINDOW, 0, 0);

	return TRUE;
}

int WindowView::InitializeWindowProfile()
{
	int rtn = 0;
	WCHAR pbf[1024];
	DWORD bsz = GetCurrentDirectory(512, pbf + 512);
	if (bsz > 0) {
		wsprintf(pbf, L"%s\\%s", pbf + 512, APPDEFAULTINIFILE);
		profile.LoadProfile(pbf);
		std::wstringstream wss;
		wss << L"Load profile from [" << pbf << L"]";
		ViewLog(wss.str().c_str());
	}

	rtn = InitializeWindowProfileSize();
	rtn = InitializeWindowProfileUrl();
	rtn = InitializeWindowProfileMisc();
	return rtn;
}

int WindowView::UpdateUrlDlgData(HWND hdlg)
{
	HWND hctl;
	
	hctl = GetDlgItem(hdlg, IDC_EDITURL);
	if (hctl) {
		Edit_SetText(hctl, remoteUrl.c_str());
	}

	hctl = GetDlgItem(hdlg, IDC_EDITHOST);
	if (hctl) {
		Edit_SetText(hctl, remoteHost.c_str());
	}

	hctl = GetDlgItem(hdlg, IDC_EDITUSER);
	if (hctl) {
		Edit_SetText(hctl, remotePass.c_str());
	}

	hctl = GetDlgItem(hdlg, IDC_EDITPASSWD);
	if (hctl) {
		Edit_SetText(hctl, remotePass.c_str());
	}

	hctl = GetDlgItem(hdlg, IDC_EDITPASSWD);
	if (hctl) {
		Edit_SetText(hctl, remotePass.c_str());
	}

	hctl = GetDlgItem(hdlg, IDC_EDITRPCPATH);
	if (hctl) {
		Edit_SetText(hctl, remotePath.c_str());
	}
	return 0;
}

int WindowView::InitializeWindowProfileMisc()
{
	std::wstring vvv;
	profile.GetDefaultValue(L"LocalSortDirection", vvv);
	if (vvv.length() > 0) {
		std::wstringstream wss(vvv);
		wss >> sortasc;
	}

	return 0;
}

int WindowView::InitializeWindowProfileUrl()
{
	profile.GetDefaultValue(L"RemoteHost", remoteHost);
	profile.GetDefaultValue(L"RemotePath", remotePath);
	profile.GetDefaultValue(L"RemoteUser", remoteUser);
	profile.GetDefaultValue(L"RemotePass", remotePass);
	profile.GetDefaultValue(L"RemoteUrl", remoteUrl);

	if (remoteUrl.length() > 0) {
		UpdateUrlDlgData(hUrl);
	}
	return 0;
}

int WindowView::InitializeWindowProfileSize()
{
	int rtn = 0;
	std::wstring vvv;
	RECT rct = { 0 };

	if (profile.GetDefaultValue(L"WindowTop", vvv)) {
		std::wstringstream wss(vvv);
		wss >> rct.top;
		rtn = 1;
	}
	if (profile.GetDefaultValue(L"WindowLeft", vvv)) {
		std::wstringstream wss(vvv);
		wss >> rct.left;
	}
	if (profile.GetDefaultValue(L"WindowRight", vvv)) {
		std::wstringstream wss(vvv);
		wss >> rct.right;
	}
	if (profile.GetDefaultValue(L"WindowBottom", vvv)) {
		std::wstringstream wss(vvv);
		wss >> rct.bottom;
	}

	if (rtn) {
		MoveWindow(hWnd, rct.top, rct.left, rct.right - rct.left, rct.bottom - rct.top, TRUE);
	}
	return rtn;
}

int WindowView::InitializeToolbar(HWND htool)
{
	TBBUTTON tbButtons[] =
	{
		{ MAKELONG(0,  0), IDM_FILE_ADDNEW,  TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"" }
		,{ MAKELONG(1,  0), IDM_COPYTREENODE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L""}
		,{ MAKELONG(2,  0), IDM_SELECTPARENT, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L""}
		,{ MAKELONG(3,  0), IDM_PAUSETREENODE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L""}
		,{ MAKELONG(4,  0), IDM_RESUMETREENODE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L""}
		,{ MAKELONG(5,  0), IDM_COPYTREENODENAME, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L""}
	};

	int bitmapSize = 16;
	int nbtn = sizeof(tbButtons) / sizeof(TBBUTTON);
	//hToolImg = ImageList_Create(bitmapSize, bitmapSize, ILC_COLOR16 | ILC_MASK, nbtn, 0);
	hToolImg  = ImageList_LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BMPTOOLIMG), bitmapSize, 5, CLR_DEFAULT, IMAGE_BITMAP, LR_DEFAULTSIZE);

	// Set the image list.
	SendMessage(htool, TB_SETIMAGELIST, (WPARAM)0, (LPARAM)hToolImg);

	// Load the button images.
	//SendMessage(htool, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
	//SendMessage(htool, TB_LOADIMAGES, (WPARAM)IDB_BMPTOOLIMG, (LPARAM)GetModuleHandle(NULL));
	
	// Add buttons.
	SendMessage(htool, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	SendMessage(htool, TB_ADDBUTTONS, (WPARAM)nbtn, (LPARAM)& tbButtons);

	// Resize the toolbar, and then show it.
	SendMessage(htool, TB_AUTOSIZE, 0, 0);
	ShowWindow(htool, TRUE);

	return 0;
}

int WindowView::CreateViewControls()
{
	InitCommonControls();

	//////////////////// Create status split
	splitStatus = CSplitWnd::CreateSplitWindow(hWnd);
	::ShowWindow(*splitStatus, SW_SHOW);
	splitStatus->style = CSplitWnd::DOCKDOWN;

	splitTool = CSplitWnd::CreateSplitWindow(*splitStatus, 12234);
	hStatus = CreateWindow(STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 200, 10, *splitStatus, NULL, hInst, NULL);

	splitStatus->SetWindow(*splitTool);
	splitStatus->SetWindow(hStatus);

	/////////////////////// Create toolbar split
	splitTool->style = CSplitWnd::DOCKTOP;
	splitTool->fixedSize = 28;

	splitLog = CSplitWnd::CreateSplitWindow(*splitTool);
	hTool = CreateWindow(TOOLBARCLASSNAME, L"", WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT, 0, 0, 200, 16, *splitTool, NULL, hInst, NULL);

	splitTool->SetWindow(hTool);
	splitTool->SetWindow(*splitLog);

	ShowWindow(*splitTool, SW_SHOW);
	ShowWindow(hTool, SW_SHOW);

	//Create South Log
	::ShowWindow(*splitLog, SW_SHOW);
	splitLog->style = CSplitWnd::TOPDOWN;

	splitTree = CSplitWnd::CreateSplitWindow(*splitLog);
	panelState = CPanelWindow::CreatePanelWindow(*splitLog);

	hLog = CreateWindow(WC_LISTBOX, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_VISIBLE, 0, 0, 200, 10, *panelState, NULL, hInst, NULL);
	panelState->PushWindow(hLog);
	hNew = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOGNEW), *panelState, DlgProcAddNew);
	SetWindowLongPtr(hNew, GWLP_USERDATA, (LONG_PTR)this);
	hUrl = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOGURL), *panelState, DlgProcUrl);
	SetWindowLongPtr(hUrl, GWLP_USERDATA, (LONG_PTR)this);
	hDelay = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOGDELAY), *panelState, DlgProcDelay);
	SetWindowLongPtr(hDelay, GWLP_USERDATA, (LONG_PTR)this);
	panelState->PushWindow(hNew);
	panelState->PushWindow(hUrl);
	panelState->PushWindow(hDelay);

	//panelState->SwitchWindow(hUrl);
	panelState->SwitchWindow(hLog);

	splitLog->SetWindow(*splitTree);
	splitLog->SetWindow(*panelState);
	splitLog->SetRatio(0.7);

	//Create Left Tree
	::ShowWindow(*splitTree, SW_SHOW);
	splitTree->style = CSplitWnd::LEFTRIGHT;

	HWND htree = CreateWindow(WC_TREEVIEW, L"", WS_CHILD | WS_VSCROLL | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_HASLINES, 0, 0, 100, 100, *splitTree, NULL, hInst, NULL);
	HWND hlist = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | LVS_REPORT | WS_VISIBLE, 0, 0, 200, 10, *splitTree, NULL, hInst, NULL);
	listview = new CViewListFrame(hlist);
	listview->hMain = hWnd;
	listview->profile = &profile;
	listview->analyzer = analyzer;

	treeview = new CViewTreeFrame(htree);
	treeview->analyzer = analyzer;
	treeview->hMain = hWnd;
	//TreeCallBack* tcb = new TreeCallBack();
	//tcb->parent = this;
	//treeview->callback = tcb;

	splitTree->SetWindow(*treeview);
	splitTree->SetWindow(*listview);
	splitTree->SetRatio(0.2);

	//ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
	listview->InitStyle();

	/////////////////// Setup system font
	HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
//	::SendMessage(hList, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hLog, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(*treeview, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hUrl, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hNew, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hDelay, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));

	InitializeStatusBar(hStatus);
	InitializeToolbar(hTool);
	InitializeWindowProfile();

	hMain = *splitStatus;

	if (remoteUrl.length() > 0) {
		PostMessage(hWnd, WM_U_STARTSESSION, 0, 0);
	}
	else {
		panelState->SwitchWindow(hUrl);
	}

	return 0;
}

int WindowView::ViewLog(const WCHAR* msg)
{
	if (hLog) {
		SYSTEMTIME stm;
		WCHAR lbuf[4096];
		::GetLocalTime(&stm);
		wsprintf(lbuf, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] : %s"
			, stm.wYear, stm.wMonth, stm.wDay
			, stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds
			, msg);
		ListBox_InsertString(hLog, 0, lbuf);
	}

	return 0;
}

int WindowView::UpdateListViewTorrents()
{
	HTREEITEM htv = treeview->GetSelectedItem();

	if (htv) {
		TreeParmData* tip = treeview->GetItemParm(htv);
		if (tip) {
			switch (tip->ItemType) {
			case TreeParmData::Group:
				if (tip->group->subs.size() > 0) {
					listview->UpdateListViewGroups();
				}
				else {
					listview->UpdateListViewTorrentNodes(tip->group);
				}
				break;
			case TreeParmData::Torrent:
				listview->UpdateListViewTorrentDetailData(tip->node);
				break;
			case TreeParmData::File:
				listview->UpdateListViewTorrentFiles(tip->file);
				break;
			}

		}
	}
	return 0;
}


int WindowView::StartAnalysis()
{
	WCHAR wbuf[1024];

	if (remoteUrl.length() > 0) {
		analyzer->StartClient(remoteUrl.c_str(), remoteUser.c_str(), remotePass.c_str());
		treeview->SyncGroup(analyzer->groupRoot);

		wsprintf(wbuf, L"Login to remote site [%s] user/password [%s:%s]", remoteUrl.c_str(), remoteUser.c_str(), remotePass.c_str());
		ViewLog(wbuf);

		treeview->UpdateViewTorrentGroup(analyzer->groupRoot);
		SetTimer(hWnd, 0x123, 1000, ViewProcTimer);
		canRefresh = true;
		panelState->SwitchWindow(hLog);
	}
	else {
		ViewLog(L"No valid URL found");
		panelState->SwitchWindow(hUrl);
	}
	return 0;
}

VOID WindowView::ViewProcTimer(_In_ HWND hwnd, _In_ UINT uMsg, _In_ UINT_PTR idEvent, _In_ DWORD dwTime)
{
	if (views.find(hwnd) != views.end()) {
		views[hwnd]->ProcRefreshTimer(hwnd, dwTime);
	}

	return VOID();
}

int WindowView::ProcRefreshTimer(HWND hwnd, DWORD dwtime)
{
	if (canRefresh) {
		refreshcount++;
		if (refreshcount % 3 == 0) {
			canRefresh = false;
			PostMessage(hwnd, WM_U_REFRESHREMOTE, 0, 0);
		}
		else {
			canRefresh = false;
			PostMessage(hwnd, WM_U_REFRESHSESSION, 0, 0);
		}
	}
	return 0;
}

TreeGroupShadow::TreeGroupShadow()
	: parent(NULL)
	, hnode(NULL)
{
}

TreeGroupShadow::~TreeGroupShadow()
{
	for (std::map<TorrentGroup*, TreeGroupShadow*>::iterator itgi = subs.begin()
		; itgi != subs.end()
		; itgi++) {
		delete itgi->second;
	}
	subs.clear();
	for (std::map<HTREEITEM, TorrentParmItems*>::iterator itdi = nodeDetailItems.begin()
		; itdi != nodeDetailItems.end()
		; itdi++) {
		delete itdi->second;
	}
	nodeDetailItems.clear();
}

int TreeGroupShadow::getTorrentItems(TorrentNode* node, std::set<HTREEITEM>& items)
{
	std::map<TorrentNode*, HTREEITEM>::iterator itni = nodeitems.find(node);
	if (itni != nodeitems.end()) {
		items.insert(itni->second);
	}
	for (std::map<TorrentGroup*, TreeGroupShadow*>::iterator itgt = subs.begin()
		; itgt != subs.end()
		; itgt++) {
		itgt->second->getTorrentItems(node, items);
	}
	return 0;
}

int TreeGroupShadow::removeItem(TorrentNode* node)
{
	std::map<TorrentNode*, HTREEITEM>::iterator itni = nodeitems.find(node);
	if (itni != nodeitems.end()) {
		nodeitems.erase(node);
	}
	for (std::map<TorrentGroup*, TreeGroupShadow*>::iterator itsb = subs.begin()
		; itsb != subs.end()
		; itsb++) {
		itsb->second->removeItem(node);
	}
	return 0;
}

HTREEITEM TreeGroupShadow::findTreeItem(TorrentNode* node)
{
	HTREEITEM hti = NULL;
	std::map<TorrentNode*, HTREEITEM>::iterator ithf = nodeitems.find(node);
	if (ithf != nodeitems.end()) {
		hti = ithf->second;
	}
	else {
		std::map<TorrentGroup*, TreeGroupShadow*>::iterator itgf = subs.begin();
		bool keepseek = itgf != subs.end();
		while (keepseek) {
			hti = itgf->second->findTreeItem(node);
			keepseek = hti == NULL ? keepseek : false;
			itgf++;
			keepseek = itgf != subs.end() ? keepseek : false;
		}
	}
	return hti;
}

HTREEITEM TreeGroupShadow::findTreeItem(TorrentGroup* grp)
{
	HTREEITEM hti = NULL;
	std::map<TorrentGroup*, TreeGroupShadow*>::iterator itgf = subs.find(grp);
	if (itgf != subs.end()) {
		hti = itgf->second->hnode;
	}
	else {
		itgf = subs.begin();
		bool keepseek = itgf != subs.end();
		while (keepseek) {
			hti = itgf->second->findTreeItem(grp);
			keepseek = hti == NULL ? keepseek : false;
			itgf++;
			keepseek = itgf != subs.end() ? keepseek : false;
		}
	}
	return hti;
}

TreeGroupShadow* TreeGroupShadow::CreateTreeGroupShadow(TorrentGroup* grp, TreeGroupShadow* pnt)
{
	TreeGroupShadow* tgs = new TreeGroupShadow();
	tgs->group = grp;
	if (pnt) {
		tgs->parent = pnt;
		pnt->subs[grp] = tgs;
	}
	return tgs;
}

TreeGroupShadow* TreeGroupShadow::GetTreeGroupShadow(TorrentGroup* grp)
{
	TreeGroupShadow* mgs = NULL;
	if (group == grp) {
		mgs = this;
	}
	if (mgs == NULL) {
		std::map<TorrentGroup*, TreeGroupShadow*>::iterator itgs = subs.begin();
		bool keepseek = itgs != subs.end();
		while (keepseek) {
			mgs = itgs->second->GetTreeGroupShadow(grp);
			
			keepseek = mgs ? false : keepseek;
			itgs++;
			keepseek = itgs == subs.end() ? false : keepseek;
		}
	}
	return mgs;
}

TorrentParmItems* TreeGroupShadow::GetNodeParmItems(TorrentNode* node)
{
	TorrentParmItems* tpi = NULL;
	HTREEITEM hti;
	std::map<TorrentNode*, HTREEITEM>::iterator itth = nodeitems.find(node);

	if (itth == nodeitems.end()) {
		std::map<TorrentGroup*, TreeGroupShadow*>::iterator ittg = subs.begin();
		bool keepseek = ittg != subs.end();
		while (keepseek) {
			tpi = ittg->second->GetNodeParmItems(node);
			keepseek = tpi ? false : keepseek;
			ittg++;
			keepseek = ittg == subs.end() ? false : keepseek;
		}
	}
	else {
		hti = itth->second;
		std::map<HTREEITEM, TorrentParmItems*>::iterator itpi = nodeDetailItems.find(hti);

		if (itpi == nodeDetailItems.end()) {
			tpi = new TorrentParmItems();
			*tpi = { 0 };
			tpi->torrent = hti;
			nodeDetailItems.insert(std::pair<HTREEITEM, TorrentParmItems*>(hti, tpi));
		}
		else {
			tpi = itpi->second;
		}
	}
	return tpi;
}

int TreeGroupShadow::SyncGroup()
{
	std::map<TorrentGroup*, TreeGroupShadow*>::iterator itgs;
	TreeGroupShadow* tgs;
	std::set<TorrentGroup*> scp;
	TorrentGroup* tgp;

	for (std::map<std::wstring, TorrentGroup*>::iterator itsb = group->subs.begin()
		; itsb != group->subs.end()
		; itsb++) {
		scp.insert(itsb->second);
	}

	for (std::map<std::wstring, TorrentGroup*>::iterator itsb = group->subs.begin()
		; itsb != group->subs.end()
		; itsb++) {
		tgp = itsb->second;
		itgs = subs.find(tgp);
		if (itgs == subs.end()) {
			tgs = TreeGroupShadow::CreateTreeGroupShadow(tgp, this);
		}
		else {
			tgs = itgs->second;
		}
		tgs->SyncGroup();

		scp.erase(tgp);
	}

	for (std::set<TorrentGroup*>::iterator itts = scp.begin()
		; itts != scp.end()
		; itts++) {
		itgs = subs.find(*itts);
		if (itgs != subs.end()) {
			delete itgs->second;
		}
		subs.erase(*itts);
	}
	return 0;
}

//int WindowView::ProcessUICallBackNotice(long cmd, long txn, int(*cbfunc)(void*, long, long), void* lparm)
//{
//	UIMSGDATA* umd = new UIMSGDATA();
//	umd->cmd = cmd;
//	umd->txn = cmd;
//	umd->func = cbfunc;
//	umd->lparam = lparm;
//
//	PostMessage(hWnd, WM_U_UIPROCMSG, (WPARAM)umd, 0);
//
//	return 0;
//}
//
//int WindowView::TreeCallBack::ProcessTreeNotice(long cmd, long txn, int(*cbfunc)(void*, long, long), void* lparm)
//{
//	parent->ProcessUICallBackNotice(cmd, txn, cbfunc, lparm);
//	return 0;
//}