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
#define WM_U_REFRESHTORRENTDETAIL WM_USER + 0xBBB

ATOM WindowView::viewClass = 0;
std::map<HWND, WindowView*> WindowView::views;
WCHAR WindowView::szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR WindowView::szWindowClass[MAX_LOADSTRING];            // the main window class name
bool sortasc = true;

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

int FormatViewSize(wchar_t* buf, size_t bufsz, unsigned __int64 size) 
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

int FormatByteSize(wchar_t* buf, size_t bufsz, unsigned __int64 size)
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
	FormatByteSize(dbuf, 1024, size);
	FormatViewSize(dbuf + 512, 512, size);
	wsprintf(wbuf, L"%s (%s)", dbuf + 512, dbuf);
	return 0;
}

//int FormatFloatNumber(wchar_t* wbuf, int bsz, double num)
//{
//	int inm = (int)num;
//	int ifn = (int)((num - inm) * 10000 * (num<0?-1:1));
//
//	wsprintf(wbuf, L"%d.%04d", inm, ifn);
//	return 0;
//}
//

ListParmData* WindowView::GetListParmData(unsigned long idx)
{
	ListParmData* lpd = NULL;

	if (listparms.size() > 0) {
		lpd = *listparms.begin();
		if (lpd->idle) {
			lpd->idle = false;
			listparms.pop_front();
			listparms.push_back(lpd);
		}
		else {
			lpd = NULL;
		}
	}

	if (lpd == NULL) {
		lpd = new ListParmData();
		lpd->idle = false;
		listparms.push_back(lpd);
	}

	return lpd;
}

int WindowView::UpdateTreeViewTorrentDetail(TorrentParmItems* items)
{
	wchar_t tbuf[1024];
	TreeParmData* ntgip;
	TorrentNode* trt = NULL;

	TVITEM tvi = { 0 };
	tvi.hItem = items->torrent;
	tvi.mask = TVIF_PARAM;
	BOOL btn = TreeView_GetItem(hTree, &tvi);
	if (btn) {
		if (tvi.lParam) {
			TreeParmData* tip = (TreeParmData*)tvi.lParam;
			if (tip->ItemType == TreeParmData::Torrent) {
				trt = tip->node;
			}
		}
	}

	if (trt) {
		if (items->id == NULL) {
			TVINSERTSTRUCT tvi;

			tvi = { 0 };
			tbuf[0] = 0;
			tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
			tvi.item.pszText = tbuf;
			tvi.item.cchTextMax = 0;
			ntgip = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Attribute);
			tvi.item.lParam = (LPARAM)ntgip;
			ntgip->node = trt;
			tvi.hParent = items->torrent;
			items->size = TreeView_InsertItem(hTree, &tvi);
			items->viewsize = TreeView_InsertItem(hTree, &tvi);
			items->id = TreeView_InsertItem(hTree, &tvi);
			items->tracker = TreeView_InsertItem(hTree, &tvi);;
		}

		if (trt->updatetick > items->readtick) {
			ULARGE_INTEGER ltt;
			ltt.QuadPart = GetTickCount64();
			items->readtick = ltt.LowPart;

			TVITEM tiu = { 0 };
			tiu.mask = TVIF_TEXT;
			tiu.hItem = items->id;
			wsprintf(tbuf, L"ID: %d", trt->id);
			tiu.pszText = tbuf;
			tiu.cchTextMax = (int)wcslen(tbuf);
			TreeView_SetItem(hTree, &tiu);

			FormatViewSize(tbuf + 512, 512, trt->size);
			wsprintf(tbuf, L"Size: %s", tbuf + 512);
			tiu.cchTextMax = (int)wcslen(tbuf);
			tiu.hItem = items->size;
			TreeView_SetItem(hTree, &tiu);

			FormatByteSize(tbuf + 512, 512, trt->size);
			wsprintf(tbuf, L"Size (View): %s", tbuf + 512);
			tiu.cchTextMax = (int)wcslen(tbuf);
			tiu.hItem = items->viewsize;
			TreeView_SetItem(hTree, &tiu);

			wsprintf(tbuf, L"%s (%s)", trt->name.c_str(), tbuf + 512);
			tiu.cchTextMax = (int)wcslen(tbuf);
			tiu.hItem = items->torrent;
			TreeView_SetItem(hTree, &tiu);

			wsprintf(tbuf, L"Trackers (%d)", trt->trackerscount);
			tiu.cchTextMax = (int)wcslen(tbuf);
			tiu.hItem = items->tracker;
			TreeView_SetItem(hTree, &tiu);


			if (trt->trackerscount > 0) {
				UpdateTreeViewTorrentDetailTrackers(items, trt);
			}

			if (items->file == NULL) {
				if (trt->files.node) {
					items->file = UpdateTreeViewTorrentDetailFiles(items->torrent, trt->files.node);
				}
			}
		}
	}
	return 0;
}

HTREEITEM WindowView::UpdateTreeViewTorrentDetailTrackers(TorrentParmItems* items, TorrentNode* node)
{
	HTREEITEM hnt = NULL;
	TVINSERTSTRUCT tvi = { 0 };
	TVITEM tim = { 0 };
	TreeParmData* tpd;

	if (node->trackerscount > 0) {
		if (items->tracker) {
			if (node->trackerscount > 1) {
				hnt = (HTREEITEM)::SendMessage(hTree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)items->tracker);
				for (unsigned int ii = 0; ii < node->trackerscount; ii++) {
					if (hnt == NULL) {
						tvi.item.mask = TVIF_PARAM;
						tvi.hParent = items->tracker;
						tpd = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Tracker);
						tpd->node = node;
						tvi.item.lParam = (LPARAM)tpd;
						hnt = TreeView_InsertItem(hTree, &tvi);
					}
					if (hnt) {
						tim.mask = TVIF_TEXT;
						tim.hItem = hnt;
						tim.pszText = (LPWSTR)node->trackers[ii].c_str();
						tim.cchTextMax = (int)wcslen(tim.pszText);
						TreeView_SetItem(hTree, &tim);
					}
					hnt = (HTREEITEM)::SendMessage(hTree, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hnt);
				}
			}
			else {
				tim.hItem = items->tracker;
				tim.mask = TVIF_PARAM;
				TreeView_GetItem(hTree, &tim);

				TreeParmData* tpd = (TreeParmData*)tim.lParam;
				tpd->ItemType = TreeParmData::Tracker;
				tpd->node = node;

				tim.mask = TVIF_TEXT;
				tim.hItem = items->tracker;
				tim.pszText = (LPWSTR)node->trackers[0].c_str();
				tim.cchTextMax = (int)wcslen(tim.pszText);
				TreeView_SetItem(hTree, &tim);
			}
		}
	}

	return hnt;
}

HTREEITEM WindowView::UpdateTreeViewTorrentDetailFiles(HTREEITEM hpnt, TorrentNodeFileNode* fnode)
{
	WCHAR wbuf[1024];
	HTREEITEM hnt;
	TVINSERTSTRUCT tvi = { 0 };
	tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
	if (fnode->type == TorrentNodeFileNode::DIR) {
		std::set<TorrentNodeFileNode*> fls;
		TorrentNodeHelper::GetNodeFileSet(fnode, fls);
		wsprintf(wbuf, L"%s (%d)", fnode->name.c_str(), fls.size());
	}
	else {
		wsprintf(wbuf, L"%s", fnode->name.c_str());
	}
	tvi.item.pszText = (LPWSTR)wbuf;
	tvi.item.cchTextMax = (int)wcslen(tvi.item.pszText);
	tvi.hParent = hpnt;
	TreeParmData* ntgip = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::File);
	ntgip->file = fnode;
	tvi.item.lParam = (LPARAM)ntgip;
	hnt = TreeView_InsertItem(hTree, &tvi);
	for (std::set<TorrentNodeFileNode*>::iterator itfn = fnode->sub.begin()
		; itfn != fnode->sub.end()
		; itfn++) {
		UpdateTreeViewTorrentDetailFiles(hnt, *itfn);
	}

	return hnt;
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
	, hList(NULL)
	, hLog(NULL)
	, hTree(NULL)
	, hWnd(NULL)
	, splitLog(NULL)
	, splitTree(NULL)
	, listContent(NONE)
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
	, torrentsViewOrgs(NULL)
{
	analyzer = new TransAnalyzer();

	InitializeGradualPen();
	InitializeTorrentDetailTitle();
	
	hbGreen = ::CreateSolidBrush(RGB(0xCF, 0xFF, 0xCF));
	hbWhite = ::CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
	hbRed = ::CreateSolidBrush(RGB(0xFF, 0xCF, 0xCF));
}

WindowView::~WindowView()
{
	delete analyzer;
	analyzer = NULL;

	TreeItemParmDataHelper::ClearTreeItemParmData();

	for (int ii = 0; ii < 256; ii++) {
		DeleteObject(gradualpen[ii]);
	}

	FreeTorrentDetailTitle();

	if (torrentsViewOrgs) {
		delete torrentsViewOrgs;
		torrentsViewOrgs = NULL;
	}

	for (std::list<ListParmData*>::iterator itlp = listparms.begin()
		; itlp != listparms.end()
		; itlp++) {
		delete* itlp;
	}
	listparms.clear();

	DeleteObject(hbGreen);
	DeleteObject(hbWhite);
}

void WindowView::InitializeTorrentDetailTitle()
{
	wchar_t whs[][32] = {
		L"ID"
		,L"Name"
		,L"Size"
		,L"Size (View)"
		,L"Download Rate"
		,L"Upload Rate"
		,L"Status"
		,L"Status(View)"
		,L"Path"
		,L"Download Time"
		,L"Seed Time"
		,L"Error"
		,L"Downloaded Size"
		,L"Uploaded Size"
		,L"Corrupt Size"
		,L"Ratio"
		,L"Left Size"
		,L"Available"
		,L"Privacy"
		,L"Finished"
		,L"Stalled"
		,L"Done Date"
		,L"Activity Date"
		,L"Start Date"
		,L"Added Date"
		,L"Piece Count"
		,L"Piece Size"
		,L"Piece Status"
	};

	wchar_t whn[][32] = {
		L"Available Torrents"
		, L"Total Download"
		, L"Total Upload"
		, L"Download Speed"
		, L"Upload Speed"
	};

	int ays = sizeof(whs);
	ays /= sizeof(wchar_t);
	ays /= 32;
	int sln;
	torrentDetailTitles = (wchar_t**)malloc(ays * sizeof(wchar_t*));
	if (torrentDetailTitles) {
		for (int ii = 0; ii < ays; ii++) {
			sln = wcsnlen_s(whs[ii], 32);
			torrentDetailTitles[ii] = (wchar_t*)malloc((sln + 1) * sizeof(wchar_t*));
			wcscpy_s(torrentDetailTitles[ii], sln + 1, whs[ii]);
		}
	}

	ays = sizeof(whn);
	ays /= sizeof(wchar_t);
	ays /= 32;
	torrentSessionTitles = (wchar_t**)malloc(ays * sizeof(wchar_t*));
	if (torrentSessionTitles) {
		for (int ii = 0; ii < ays; ii++) {
			sln = wcsnlen_s(whn[ii], 32);
			torrentSessionTitles[ii] = (wchar_t*)malloc((sln + 1) * sizeof(wchar_t*));
			wcscpy_s(torrentSessionTitles[ii], sln + 1, whn[ii]);
		}
	}
}

void WindowView::InitializeGradualPen()
{
	for (unsigned int ii = 0; ii < 256; ii++) {
		if (ii < 128) {
			gradualpen[ii] = CreatePen(PS_SOLID, 1, RGB(0xFF, 0x7F + ii, 0x7F));
		}
		else {
			gradualpen[ii] = CreatePen(PS_SOLID, 1, RGB(0xFF - (ii - 0x80), 0xFF, 0x7F));
		}
	}
}

int WindowView::FreeTorrentDetailTitle()
{
	int tsz = _msize(torrentDetailTitles);
	tsz /= sizeof(wchar_t*);

	if (tsz > 0) {
		for (int ii = 0; ii < tsz; ii++) {
			free(torrentDetailTitles[ii]);
		}
		free(torrentDetailTitles);
		torrentDetailTitles = NULL;
	}

	tsz = _msize(torrentSessionTitles);
	tsz /= sizeof(wchar_t*);

	if (tsz > 0) {
		for (int ii = 0; ii < tsz; ii++) {
			free(torrentSessionTitles[ii]);
		}
		free(torrentSessionTitles);
		torrentSessionTitles = NULL;
	}
	return 0;
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

int WindowView::GetListSelectedTorrents(std::set<TorrentNode*>& tts)
{
	int sit = -1;
	bool keepseek = true;
	LVITEM lvi = { 0 };
	BOOL btn;
	ListParmData* lpd;

	if (listContent == LISTCONTENT::TORRENTLIST) {
		while (keepseek) {
			sit = ListView_GetNextItem(hList, sit, LVNI_SELECTED);
			if (sit >= 0) {
				lvi.iItem = sit;
				lvi.mask = LVIF_PARAM;
				btn = ListView_GetItem(hList, &lvi);
				if (btn) {
					lpd = (ListParmData*)lvi.lParam;
					if (lpd->type == ListParmData::Node) {
						tts.insert(lpd->node);
					}
				}
			}
			else {
				keepseek = false;
			}
		}

	}
	return 0;
}

int WindowView::GetListSelectedFiles(std::set<TorrentNodeFileNode*>& tfs)
{
	int sit = -1;
	bool keepseek = true;
	LVITEM lvi = { 0 };
	BOOL btn;
	ListParmData* lpd;
	if (listContent == LISTCONTENT::TORRENTFILE) {
		while (keepseek) {
			sit = ListView_GetNextItem(hList, sit, LVNI_SELECTED);
			if (sit >= 0) {
				lvi.iItem = sit;
				lvi.mask = LVIF_PARAM;
				btn = ListView_GetItem(hList, &lvi);
				if (btn) {
					lpd = (ListParmData*)lvi.lParam;
					if (lpd->type == ListParmData::File) {
						tfs.insert(lpd->file);
					}
				}
			}
			else {
				keepseek = false;
			}
		}

	}
	return 0;
}

int WindowView::CopyTorrentsDetailOnView()
{
	TorrentNode* node;
	std::set<TorrentNode*> dts;
	std::wstringstream wss;

	GetListSelectedTorrents(dts);
	if (dts.size() <= 0) {
		GetTreeSelectedTorrents(dts);
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
	GetListSelectedFiles(fns);
	if (fns.size() <= 0) {
		GetTreeSelectedFiles(fns);
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
	GetListSelectedFiles(fns);
	if (fns.size() <= 0) {
		GetTreeSelectedFiles(fns);
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
		wsprintf(wbuf, L"Remote delete torrent [%s] files [%d] with result: [%s]", (*itnd)->name.c_str(), frp.fileIds.size(), wrt.c_str());
		ViewLog(wbuf);
	}
	return 0;
}

int WindowView::RemoveTorrentOnView(BOOL bContent)
{
	TorrentNode* node;
	std::set<TorrentNode*> dts;
	struct DELAYMESSAGE* dmsg;

	GetListSelectedTorrents(dts);
	if (dts.size() <= 0) {
		GetTreeSelectedTorrents(dts);
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

int WindowView::GetTreeSelectedFiles(std::set<TorrentNodeFileNode*>& trfs)
{
	HTREEITEM hti = TreeView_GetSelection(hTree);
	std::wstringstream wss;

	if (hti) {
		TVITEM tvi = { 0 };
		tvi.hItem = hti;
		tvi.mask = TVIF_PARAM;
		BOOL btn = TreeView_GetItem(hTree, &tvi);
		if (btn) {
			if (tvi.lParam) {
				TreeParmData* tip = (TreeParmData*)tvi.lParam;
				if (tip->ItemType == TreeParmData::File) {
					TorrentNodeHelper::GetNodeFileSet(tip->file, trfs);
				}
			}
		}
	}
	return 0;
}

int WindowView::GetTreeSelectedTorrents(std::set<TorrentNode*>& trts)
{
	HTREEITEM hti = TreeView_GetSelection(hTree);
	std::wstringstream wss;

	if (hti) {
		TVITEM tvi = { 0 };
		tvi.hItem = hti;
		tvi.mask = TVIF_PARAM;
		BOOL btn = TreeView_GetItem(hTree, &tvi);
		if (btn) {
			if (tvi.lParam) {
				TreeParmData* tip = (TreeParmData*)tvi.lParam;
				if (tip->ItemType == TreeParmData::Torrent) {
					trts.insert(tip->node);
				}
				else if (tip->ItemType == TreeParmData::Group) {
					tip->group->GetNodes(trts);
				}
			}
		}
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
		TorrentParmItems* tpi = torrentsViewOrgs->GetNodeParmItems(node);
		if (rpm.reqfiles) {
			UpdateTreeViewTorrentDetail(tpi);
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
		TorrentParmItems* tpi = torrentsViewOrgs->GetNodeParmItems(node);
		UpdateTreeViewTorrentDetail(tpi);
		UpdateListViewTorrents();
	}
}

//int WindowView::RefreshListTorrentDetail(ReqParm rpm)
//{
//	std::wstring wtn = analyzer->PerformRemoteRefreshTorrent(rpm);
//	updateTorrentsView(&analyzer->groupRoot);
//
//	return 0;
//}

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

	if (listContent == LISTCONTENT::SESSIONINFO) {
		UpdateListViewSession(ssn);
	}
	
	FormatByteSize(wbuf + 512, 512, ssn->downloadspeed);
	wsprintf(wbuf, L"Down: %s/s", wbuf + 512);
	::SendMessage(hStatus, SB_SETTEXT, MAKEWPARAM(0, 0), (LPARAM)wbuf);

	FormatByteSize(wbuf + 512, 512, ssn->uploadspeed);
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
		UpdateViewTorrentGroup(analyzer->groupRoot);
		UpdateViewRemovedTorrents();
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
	HTREEITEM hti = TreeView_GetSelection(hTree);
	if (hti) {
		TVITEM tvi = { 0 };
		tvi.hItem = hti;
		tvi.mask = TVIF_PARAM;
		BOOL btn = TreeView_GetItem(hTree, &tvi);
		if (btn) {
			if (tvi.lParam) {
				TreeParmData* tip = (TreeParmData*)tvi.lParam;
				switch (tip->ItemType) {
				case TreeParmData::Torrent:
					if (listContent == TORRENTDETAIL) {
						RefreshTorrentNodeDetail(tip->node);
					}
					break;
				case TreeParmData::File:
					if (listContent == TORRENTFILE) {
						RefreshTorrentNodeFiles(tip->file->node);
					}
					break;
				case TreeParmData::Group:
					if (
						(listContent == TORRENTGROUP) ||
						(listContent == TORRENTLIST)
						) {
						UpdateListViewTorrents();
					}
				}
			}
		}
	}
}

void WindowView::UpdateViewRemovedTorrents()
{
	int rid;
	std::set<HTREEITEM> rts;
	TorrentNode* trt;

	if (analyzer->removed.size() > 0) {
		HTREEITEM hsi = TreeView_GetSelection(hTree);
		TVITEM tvi = { 0 };

		for (std::list<int>::iterator itrm = analyzer->removed.begin()
			; itrm != analyzer->removed.end()
			; itrm++) {
			rid = *itrm;
			rts.clear();
			trt = analyzer->GetTorrentNodeById(rid);
			if (trt) {
				torrentsViewOrgs->getTorrentItems(trt, rts);

				for (std::set<HTREEITEM>::iterator itti = rts.begin()
					; itti != rts.end()
					; itti++) {
					if (*itti == hsi) {
						if (listContent == LISTCONTENT::TORRENTDETAIL) {
							ClearColumns(hList);
						}
					}

					tvi.hItem = *itti;
					tvi.mask = TVIF_PARAM;
					BOOL btn = TreeView_GetItem(hTree, &tvi);
					if (btn) {
						TreeParmData* tip = (TreeParmData*)tvi.lParam;
						if (tip->ItemType == TreeParmData::Torrent) {
							TorrentNode* node = tip->node;
							LVFINDINFO lfi = { 0 };
							lfi.flags = LVFI_PARAM;
							lfi.lParam = (LPARAM)node;
							int fii = ListView_FindItem(hList, -1, &lfi);
							if (fii >= 0) {
								ListView_DeleteItem(hList, fii);
							}
						}
					}


					TreeView_DeleteItem(hTree, *itti);
				}
			torrentsViewOrgs->removeItem(trt);
			}
			analyzer->RemoveTorrent(rid);
		}
		analyzer->removed.clear();
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


int WindowView::ProcContextMenuList(int xx, int yy)
{
	POINT pnt = { xx, yy };
	BOOL bst;
	if ((listContent == LISTCONTENT::TORRENTLIST) 
		|| (listContent == LISTCONTENT::TORRENTFILE)) {
		HINSTANCE hinst = ::GetModuleHandle(NULL);
		HMENU hcm = LoadMenu(hinst, MAKEINTRESOURCE(IDR_CONTEXTPOPS));
		if (hcm) {
			HMENU hsm = GetSubMenu(hcm, 1);
			if (hsm) {
				ClientToScreen(hList, &pnt);
				bst = TrackPopupMenu(hsm, TPM_LEFTALIGN | TPM_LEFTBUTTON, pnt.x, pnt.y, 0, hWnd, NULL);
				DestroyMenu(hsm);
			}
		}
	}
	return 0;
}
int WindowView::ProcContextMenuTree(int xx, int yy)
{
	RECT rct;
	POINT pnt = { xx, yy };
	HTREEITEM hti = TreeView_GetSelection(hTree);
	if (hti) {
		*(HTREEITEM*)& rct = hti;
		BOOL bst = TreeView_GetItemRect(hTree, hti, &rct, TRUE);
		//bst = ((*(HTREEITEM*)&rct = hti), (BOOL)SendMessage(hTree, TVM_GETITEMRECT, TRUE, (LPARAM)& rct));
		if (bst) {
			if (PtInRect(&rct, pnt)) {
				HINSTANCE hinst = ::GetModuleHandle(NULL);
				HMENU hcm = LoadMenu(hinst, MAKEINTRESOURCE(IDR_CONTEXTPOPS));
				if (hcm) {
					HMENU hsm = GetSubMenu(hcm, 0);
					if (hsm) {
						ClientToScreen(hTree, &pnt);
						bst = TrackPopupMenu(hsm, TPM_LEFTALIGN | TPM_LEFTBUTTON, pnt.x, pnt.y, 0, hWnd, NULL);
						DestroyMenu(hsm);
					}
				}
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
		::ScreenToClient(hTree, &wpt);
		::GetClientRect(hTree, &rct);
		if (PtInRect(&rct, wpt)) {
			rtn = ProcContextMenuTree(wpt.x, wpt.y);
		}
	}
	
	if (rtn > 0) {
		wpt = pnt;
		::ScreenToClient(hList, &wpt);
		::GetClientRect(hList, &rct);
		if (PtInRect(&rct, wpt)) {
			rtn = ProcContextMenuList(wpt.x, wpt.y);
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

	GetListSelectedTorrents(dts);
	if (dts.size() <= 0) {
		GetTreeSelectedTorrents(dts);
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

//int WindowView::PauseTreeNode(BOOL bresume)
//{
//	HTREEITEM htv = TreeView_GetSelection(hTree);
//	if (htv) {
//		TVITEM tvi = { 0 };
//		tvi.hItem = htv;
//		tvi.mask = TVIF_PARAM;
//		BOOL btn = TreeView_GetItem(hTree, &tvi);
//		if (btn) {
//			if (tvi.lParam) {
//				TreeItemParmData* tip = (TreeItemParmData*)tvi.lParam;
//				if (tip->ItemType == TreeItemParmData::Group) {
//					std::wstring tds;
//					std::wstringstream wss;
//					for (std::map<unsigned long, TorrentNode*>::iterator itgp = tip->group->torrents.begin()
//						; itgp != tip->group->torrents.end()
//						; itgp++) {
//						::PostMessage(hWnd, WM_U_PAUSETORRENT, itgp->first, bresume);
//					}
//				}
//				else if (tip->ItemType == TreeItemParmData::Torrent) {
//					TorrentNode* tnd = tip->node;
//					::PostMessage(hWnd, WM_U_PAUSETORRENT, tnd->id, bresume);
//				}
//			}
//		}
//	}
//
//	return 0;
//}

//int WindowView::CopyTreeNodeDetail()
//{
//	HTREEITEM htv = TreeView_GetSelection(hTree);
//	if (htv) {
//		TVITEM tvi = { 0 };
//		tvi.hItem = htv;
//		tvi.mask = TVIF_PARAM;
//		BOOL btn = TreeView_GetItem(hTree, &tvi);
//		if (btn) {
//			if (tvi.lParam) {
//				TreeItemParmData* tip = (TreeItemParmData*)tvi.lParam;
//				if (tip->ItemType == TreeItemParmData::Group) {
//					std::wstring tds;
//					std::wstringstream wss;
//					for (std::map<unsigned long, TorrentNode*>::iterator itgp = tip->group->torrents.begin()
//						; itgp != tip->group->torrents.end()
//						; itgp++) {
//						tds = TorrentNodeHelper::DumpTorrentNode(itgp->second);
//						wss << tds << L'\r' << std::endl;
//					}
//					if (wss.str().length() > 0) {
//						SendStringClipboard(wss.str());
//						wss.str(std::wstring());
//						wss << L"Copy [" << tip->group->torrents.size() << L"] torrent(s) to clipboard";
//						ViewLog(wss.str().c_str());
//					}
//				}
//				else if (tip->ItemType == TreeItemParmData::Torrent) {
//					TorrentNode* tnd = tip->node;
//					std::wstring wsr = TorrentNodeHelper::DumpTorrentNode(tnd);
//					SendStringClipboard(wsr);
//					std::wstringstream wss;
//					wss << L"Copy torrent [" << tip->node->name << L"] to clipboard";
//					ViewLog(wss.str().c_str());
//				}
//			}
//		}
//	}
//
//	return 0;
//}

int WindowView::CopyTreeNodeName()
{
	TorrentNode* node;
	std::set<TorrentNode*> dts;
	std::wstringstream wss;

	GetListSelectedTorrents(dts);
	if (dts.size() <= 0) {
		GetTreeSelectedTorrents(dts);
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

int WindowView::SelectTreeParentNode(HWND htree)
{
	HTREEITEM hst = TreeView_GetSelection(htree);
	if (hst) {
		hst = TreeView_GetNextItem(htree, hst, TVGN_PARENT);
		if (hst) {
			TreeView_Select(htree, hst, TVGN_CARET);
			TreeView_Select(htree, hst, TVGN_FIRSTVISIBLE);
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
			GetListSelectedTorrents(tns);
			if (tns.size() > 0) {
				TreeViewSelectItem(*tns.begin());
				PostMessage(hTree, WM_SETFOCUS, NULL, NULL);
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
			SelectTreeParentNode(hTree);
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

	if (pnmh->hwndFrom == hTree) {
		lrst = ProcNotifyTree(hWnd, message, wParam, lParam);
	}
	if (pnmh->hwndFrom == hList) {
		lrst = ProcNotifyList(hWnd, message, wParam, lParam);
	}

	return lrst;
}

int WindowView::InitializeStatusBar(HWND hsts)
{
	int pars[4] = { 100, 200, 300, -1 };
	::SendMessage(hsts, SB_SETPARTS, 4, (LPARAM) &pars);

	return 0;
}

int WindowView::CreateListColumnsSession(HWND hlist)
{
	struct ColDef cols[] = {
	{ L"Session", 150 }
	, { L"Value", 200 }
	, {0}
	};

	CreateListColumns(hlist, cols);
	listContent = LISTCONTENT::SESSIONINFO;
	return 0;
}

int WindowView::CreateListColumnsFiles(HWND hlist)
{
	struct ColDef cols[] = {
	{ L"Id", 50}
	, { L"In", 50 }
	, { L"Priority", 50}
	, { L"Path", 150}
	, { L"File", 150 }
	, { L"Size", 200 }
	, { L"Downloaded Size", 200 }
	, {0}
	};

	CreateListColumns(hlist, cols);
	listContent = LISTCONTENT::TORRENTFILE;
	return 0;
}

int WindowView::CreateListColumnsTorrent(HWND hlist)
{
	struct ColDef cols[] = {
	{ L"Parameter", 150 }
	, { L"Value", 200 }
	, {0}
	};

	CreateListColumns(hlist, cols);
	listContent = TORRENTDETAIL;
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

int WindowView::UpdateListViewTorrentFiles(TorrentNodeFileNode* files)
{
	if (listContent == LISTCONTENT::TORRENTFILE) {
		TorrentNodeFileNode* cfl;
		std::set<TorrentNodeFileNode*> fst;
		LVFINDINFO lfi = { 0 };
		LVITEM lvi = { 0 };
		std::wstring wst;
		ULARGE_INTEGER llt;
		ListParmData* lpd;

		TorrentNodeHelper::GetNodeFileSet(files, fst);
		int lvc = ListView_GetItemCount(hList);
		for (int ii = 0; ii < lvc; ii++) {
			lvi.mask = LVIF_PARAM;
			lvi.iItem = ii;
			ListView_GetItem(hList, &lvi);

			lpd = (ListParmData*)lvi.lParam;
			if (lpd->type == ListParmData::File) {
				cfl = lpd->file;
				fst.erase(cfl);
				if (cfl->updatetick > cfl->readtick) {
					llt.QuadPart = GetTickCount64();
					cfl->readtick = llt.LowPart;
					UpdateListViewTorrentFileDetail(hList, ii, cfl);
				}
			}
		}
		for (std::set<TorrentNodeFileNode*>::iterator itfn = fst.begin()
			; itfn != fst.end()
			; itfn++ ) {
			cfl = *itfn;
			if (cfl->file) {
				lvi.mask = LVIF_TEXT;
				lvi.pszText = (LPWSTR)L"";
				lvi.cchTextMax = 0;
				lvi.iItem = lvc + 1;
				lvi.lParam = (LPARAM)cfl;
				lvi.iSubItem = 0;
				lvc = ListView_InsertItem(hList, &lvi);

				lvi.iItem = lvc;
				lvi.mask = LVIF_PARAM;
				lpd = GetListParmData(lvc);
				lpd->type = ListParmData::File;
				lpd->file = cfl;
				lvi.lParam = (LPARAM)lpd;
				ListView_SetItem(hList, &lvi);
				UpdateListViewTorrentFileDetail(hList, lvc, cfl);
			}
		}
	}
	return 0;
}

int WindowView::UpdateListViewTorrentFileDetail(HWND hlist, int fii, TorrentNodeFileNode* cfl)
{
	WCHAR wbuf[128];
	LVITEM lvi;

	lvi.mask = LVIF_TEXT;
	wsprintf(wbuf, L"%d", cfl->id);
	lvi.pszText = wbuf;
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	lvi.iItem = fii;
	lvi.iSubItem = 0;
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 1;
	lvi.pszText = (LPWSTR)(cfl->check?L"Yes":L"No");
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 2;
	wsprintf(wbuf, L"%d", cfl->priority);
	lvi.pszText = (LPWSTR)wbuf;
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 3;
	lvi.pszText = (LPWSTR)cfl->path.c_str();
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 4;
	lvi.pszText = (LPWSTR)cfl->name.c_str();
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 5;
	FormatViewSize(wbuf, 128, cfl->file->size);
	lvi.pszText = wbuf;
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 6;
	FormatViewSize(wbuf+32, 32, cfl->done);
	double dsp = cfl->file->size > 0 ? ((double)cfl->done) / cfl->file->size : 0;
	FormatViewDouble(wbuf + 64, 64, dsp * 100);
	wsprintf(wbuf, L"%s (%s%%)", wbuf + 32, wbuf + 64);
	lvi.pszText = wbuf;
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	return 0;
}

int WindowView::UpdateListViewSession(SessionInfo* ssn)
{
	wchar_t wbuf[128];
	int iti = 0;

	wsprintf(wbuf, L"%d", ssn->torrentcount);
	ListView_SetItemText(hList, iti, 1, wbuf);
	iti++;

	FormatByteSize(wbuf, 128, ssn->downloaded);
	ListView_SetItemText(hList, iti, 1, wbuf);
	iti++;

	FormatByteSize(wbuf, 128, ssn->uploaded);
	ListView_SetItemText(hList, iti, 1, wbuf);
	iti++;

	FormatByteSize(wbuf+64, 64, ssn->downloadspeed);
	wsprintf(wbuf, L"%s/s", wbuf + 64);
	ListView_SetItemText(hList, iti, 1, wbuf);
	iti++;

	FormatByteSize(wbuf+64, 64, ssn->uploadspeed);
	wsprintf(wbuf, L"%s/s", wbuf + 64);
	ListView_SetItemText(hList, iti, 1, wbuf);
	iti++;

	return 0;
}

int WindowView::UpdateListViewTorrentDetailData(TorrentNode* node)
{
	wchar_t wbuf[128];
	int iti = 0;

	if (node->updatetick > node->readtick) {
		ULARGE_INTEGER lit;
		lit.QuadPart = GetTickCount64();
		node->readtick = lit.LowPart;

		wsprintf(wbuf, L"%d", node->id);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		ListView_SetItemText(hList, iti, 1, (LPWSTR)node->name.c_str());
		iti++;
		FormatViewSize(wbuf, 128, node->size);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatByteSize(wbuf, 128, node->size);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatByteSize(wbuf + 64, 64, node->downspeed);
		wsprintf(wbuf, L"%s/s", wbuf + 64);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatByteSize(wbuf + 64, 64, node->upspeed);
		wsprintf(wbuf, L"%s/s", wbuf + 64);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		wsprintf(wbuf, L"%d", node->status);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatViewStatus(wbuf, 128, node->status);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		ListView_SetItemText(hList, iti, 1, (LPWSTR)node->path.c_str());
		iti++;
		FormatTimeSeconds(wbuf + 64, 64, node->downloadTime);
		wsprintf(wbuf, L"%s (%d s)", wbuf + 64, node->downloadTime);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatTimeSeconds(wbuf + 64, 64, node->seedTime);
		wsprintf(wbuf, L"%s (%d s)", wbuf + 64, node->seedTime);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		ListView_SetItemText(hList, iti, 1, (LPWSTR)node->error.c_str());
		iti++;
		FormatDualByteView(wbuf, 128, node->downloaded);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatDualByteView(wbuf, 128, node->uploaded);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatByteSize(wbuf, 128, node->corrupt);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatViewDouble(wbuf, 128, node->ratio);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		FormatViewSize(wbuf + 64, 64, node->leftsize);
		double dlp = node->size > 0 ? ((double)node->leftsize) / node->size * 100 : 0;
		FormatViewDouble(wbuf + 96, 32, dlp);
		wsprintf(wbuf, L"%s (%s%%)", wbuf + 64, wbuf + 96);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		double dpc = node->leftsize > 0 ? ((double)node->desired) / node->leftsize : 0;
		FormatViewSize(wbuf + 32, 32, node->desired);
		FormatViewDouble(wbuf + 64, 64, dpc * 100);
		wsprintf(wbuf, L"%s (%s%%)", wbuf + 32, wbuf + 64);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		wsprintf(wbuf, L"%s", node->privacy ? L"Yes" : L"No");
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		wsprintf(wbuf, L"%s", node->done ? L"Yes" : L"No");
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
		wsprintf(wbuf, L"%s", node->stalled ? L"Yes" : L"No");
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatIntegerDate(wbuf, 128, node->donedate);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatIntegerDate(wbuf, 128, node->activedate);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatIntegerDate(wbuf, 128, node->startdate);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatIntegerDate(wbuf, 128, node->adddate);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		wsprintf(wbuf, L"%d", node->piececount);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatDualByteView(wbuf, 128, node->piecesize);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		ListView_SetItemText(hList, iti, 1, (LPWSTR)node->pieces.c_str());
		iti++;

		if (node->trackerscount == 1) {
			wsprintf(wbuf, L"%s", node->trackers[0].c_str());
		}
		else {
			FormatNormalNumber(wbuf, 128, node->trackerscount);
		}
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
	}

	return 0;
}

int WindowView::ListUpdateFiles(TorrentNodeFileNode* files)
{
	ClearColumns(hList);
	CreateListColumnsFiles(hList);

	UpdateListViewTorrentFiles(files);

	return 0;
}

int WindowView::ListUpdateSession(SessionInfo* ssn)
{
	ClearColumns(hList);
	CreateListColumnsSession(hList);

	ListUpdateSessionTitle(ssn);
	UpdateListViewSession(ssn);

	return 0;
}

int WindowView::UpdateListViewTorrentDetail(TorrentNode* node)
{
	ClearColumns(hList);
	CreateListColumnsTorrent(hList);

	ListUpdateTorrentTitle(node);
	node->readtick = 0;
	UpdateListViewTorrentDetailData(node);

	::SendMessage(hList, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
	::SendMessage(hList, LVM_SETCOLUMNWIDTH, 1, LVSCW_AUTOSIZE_USEHEADER);

	return 0;
}

void WindowView::ListUpdateTorrentTitle(TorrentNode* node)
{
	LVITEM lvi = { 0 };
	int iti = 0;
	ListParmData* lpd;

	int tts = _msize(torrentDetailTitles) / sizeof(wchar_t*);

	for (int ii = 0; ii < tts; ii++) {
		lvi.mask = LVIF_TEXT;
		lvi.pszText = (LPWSTR)torrentDetailTitles[ii];
		lvi.cchTextMax = (int)wcslen(lvi.pszText);
		lvi.iItem = iti;
		iti = ListView_InsertItem(hList, &lvi);

		lvi.iItem = iti;
		lvi.mask = LVIF_PARAM;
		lpd = GetListParmData(iti);
		lpd->type = ListParmData::Parameter;
		lpd->node = node;
		lvi.lParam = (LPARAM)lpd;
		ListView_SetItem(hList, &lvi);

		iti++;
	}

	lvi.mask = LVIF_TEXT;
	lvi.pszText = (LPWSTR)L"Tracker";
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	lvi.iItem = iti;
	iti = ListView_InsertItem(hList, &lvi);

	lvi.iItem = iti;
	lvi.mask = LVIF_PARAM;
	lpd = GetListParmData(iti);
	lpd->type = ListParmData::Tracker;
	lpd->node = node;
	lvi.lParam = (LPARAM)lpd;
	ListView_SetItem(hList, &lvi);
	iti++;
}

void WindowView::ListUpdateSessionTitle(SessionInfo* ssn)
{
	LVITEM lvi = { 0 };
	int iti = 0;
	ListParmData* lpd;


	int tts = _msize(torrentSessionTitles) / sizeof(wchar_t*);

	for (int ii = 0; ii < tts; ii++) {
		lvi.mask = LVIF_TEXT;
		lvi.pszText = (LPWSTR)torrentSessionTitles[ii];
		lvi.cchTextMax = (int)wcslen(lvi.pszText);
		lvi.iItem = iti;
		iti = ListView_InsertItem(hList, &lvi);

		lvi.iItem = iti;
		lvi.mask = LVIF_PARAM;
		lpd = GetListParmData(iti);
		lpd->type = ListParmData::Parameter;
		lpd->session = ssn;
		lvi.lParam = (LPARAM)lpd;
		ListView_SetItem(hList, &lvi);

		iti++;
	}
}

#define INTCOMP(I1,I2) (I1)<(I2)?-1:((I1)>(I2)?1:0)

int CALLBACK LVComp_Files(LPARAM lp1, LPARAM lp2, LPARAM lpsort)
{
	int icr = 0;
	ListParmData* lpd1 = (ListParmData*)lp1;
	ListParmData* lpd2 = (ListParmData*)lp2;

	TorrentNodeFileNode* nd1 = lpd1->file;
	TorrentNodeFileNode* nd2 = lpd2->file;
	long iitem = (long)lpsort;

	if (iitem == 0) {
		icr = INTCOMP(nd1->id, nd2->id);
	}
	else if (iitem == 3) {
		icr = nd1->path.compare(nd2->path);
	}
	else if (iitem == 4) {
		icr = nd1->name.compare(nd2->name);
	}
	else if (iitem == 5) {
		icr = INTCOMP(nd1->file->size, nd2->file->size);
	}
	else if (iitem == 6) {
		icr = INTCOMP(nd1->file->size>0?((double)nd1->file->donesize)/nd1->file->size:0, nd2->file->size>0?((double)nd2->file->donesize)/nd2->file->size:0);
	}

	icr *= sortasc ? 1 : -1;
	return icr;
}

int CALLBACK LVComp_Nodes(LPARAM lp1, LPARAM lp2, LPARAM lpsort)
{
	ListParmData* lpd1 = (ListParmData*)lp1;
	ListParmData* lpd2 = (ListParmData*)lp2;

	TorrentNode* nd1 = lpd1->node;
	TorrentNode* nd2 = lpd2->node;
	long iitem = (long)lpsort;

	int icr = 0;

	if (iitem == 0) {
		icr = INTCOMP(nd1->id, nd2->id);
	}
	else if (iitem == 1) {
		icr = nd1->name.compare(nd2->name);
	}
	else if ((iitem == 2) || (iitem == 3)) {
		icr = INTCOMP(nd1->size, nd2->size);
	}
	else if (iitem == 4) {
		if (nd1->trackerscount > 0) {
			if (nd2->trackerscount > 0) {
				icr = nd1->trackers[0].compare(nd2->trackers[0]);
			}
			else {
				icr = 1;
			}
		}
		else {
			if (nd2->trackerscount > 0) {
				icr = -1;
			}
			else {
				icr = 0;
			}
		}
	}
	else if (iitem == 5) {
		icr = INTCOMP(nd1->downspeed, nd2->downspeed);
	}
	else if (iitem == 6) {
		icr = INTCOMP(nd1->upspeed, nd2->upspeed);
	}
	else if (iitem == 7) {
		icr = INTCOMP(nd1->status, nd2->status);
	}
	else if (iitem == 8) {
		icr = INTCOMP(nd1->ratio, nd2->ratio);
	}

	icr *= sortasc ? 1 : -1;
	return icr;
}

int CALLBACK LVComp_Group(LPARAM lp1, LPARAM lp2, LPARAM lpsort)
{
	ListParmData* lpd1 = (ListParmData*)lp1;
	ListParmData* lpd2 = (ListParmData*)lp2;
	
	TorrentGroup* nd1 = lpd1->group;
	TorrentGroup* nd2 = lpd2->group;
	long iitem = (long)lpsort;
	int icr = 0;

	switch (iitem) {
	case 0:
		icr = nd1->name.compare(nd2->name);
		break;
	case 1:
	case 2:
		icr = nd1->size > nd2->size ? 1 : (nd1->size < nd2->size ? -1 : 0);
		break;
	case 3:
	{
		size_t nd1s = nd1->torrents.size();
		size_t nd2s = nd2->torrents.size();

		icr = nd1s > nd2s ? 1 : (nd1s < nd2s ? -1 : 0);
	}
	break;
	case 4:
		icr = nd1->downspeed > nd2->downspeed ? 1 : (nd1->downspeed < nd2->downspeed ? -1 : 0);
		break;
	case 5:
		icr = nd1->upspeed > nd2->upspeed ? 1 : (nd1->upspeed < nd2->upspeed ? -1 : 0);
		break;
	default:
	break;
	}

	icr *= sortasc ? 1 : -1;

	return icr;
}

int WindowView::MakeListParmIdle(ListParmData* lpd)
{
	lpd->idle = true;
	listparms.remove(lpd);
	listparms.push_front(lpd);
	return 0;
}

LRESULT WindowView::ProcNotifyList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	NMLISTVIEW* plist = (NMLISTVIEW*)lParam;

	switch (plist->hdr.code)
	{
	case LVN_DELETEITEM:
	{
		ListParmData* lpd = (ListParmData*)plist->lParam;
		MakeListParmIdle(lpd);
	}
		break;
	case LVN_COLUMNCLICK:
	{
		if (listContent == TORRENTLIST) {
			sortasc = sortasc == false;
			ListView_SortItems(hList, LVComp_Nodes, plist->iSubItem);
			std::wstringstream wss;
			wss << plist->iSubItem;
			profile.SetDefaultValue(L"LocalListSort", wss.str());
			wss.str(std::wstring());
			wss << sortasc;
			profile.SetDefaultValue(L"LocalSortDirection", wss.str());
		}
		else if (listContent == TORRENTGROUP) {
			sortasc = sortasc == false;
			ListView_SortItems(hList, LVComp_Group, plist->iSubItem);
			std::wstringstream wss;
			wss << plist->iSubItem;
			profile.SetDefaultValue(L"LocalGroupSort", wss.str());
			wss.str(std::wstring());
			wss << sortasc;
			profile.SetDefaultValue(L"LocalSortDirection", wss.str());
		}
		else if (listContent == TORRENTFILE) {
			sortasc = sortasc == false;
			ListView_SortItems(hList, LVComp_Files, plist->iSubItem);

			std::wstringstream wss;
			wss << plist->iSubItem;
			profile.SetDefaultValue(L"LocalListFileSort", wss.str());
			wss.str(std::wstring());
			wss << sortasc;
			profile.SetDefaultValue(L"LocalSortDirection", wss.str());

		}
	}
	break;
	case NM_DBLCLK:
	{
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
		LVITEM lit = { 0 };
		lit.mask = LVIF_PARAM;
		lit.iItem = lpnmitem->iItem;
		lit.iSubItem = lpnmitem->iSubItem;
		ListParmData* lpd;

		ListView_GetItem(hList, &lit);
		lpd = (ListParmData*)lit.lParam;
		switch (listContent) {
		case TORRENTGROUP:
			if (lpd->type == ListParmData::Group) {
				TreeViewSelectItem(lpd->group);
			}
			break;
		case TORRENTLIST:
			if (lpd->type == ListParmData::Node) {
				TreeViewSelectItem(lpd->node);
			}
			break;
		case TORRENTDETAIL:
			if (lpd->type == ListParmData::Tracker) {
				TorrentNode* node = lpd->node;
				if (node->trackerscount == 1) {
					TorrentGroup* group = analyzer->GetTrackerGroup(node->trackers[0]);
					if (group) {
						TreeViewSelectItem(group);
					}
				}
			}
		}
	}
	break;
	case NM_CUSTOMDRAW:
	{
		LPNMLVCUSTOMDRAW plcd = (LPNMLVCUSTOMDRAW)lParam;
		switch (listContent) {
		case TORRENTDETAIL:
			lrt = ProcCustDrawListDetail(plcd);
			break;
		case TORRENTLIST:
			lrt = ProcCustDrawListNodes(plcd);
			break;
		case TORRENTFILE:
			lrt = ProcCustDrawListFile(plcd);
			break;
		}
	} // customdraw
	} // notify messages
	return lrt;
}

int WindowView::TreeViewSelectItem(TorrentNode* node)
{
	HTREEITEM hti = torrentsViewOrgs->findTreeItem(node);

	if (hti) {
		TreeView_Select(hTree, hti, TVGN_CARET);
		TreeView_Select(hTree, hti, TVGN_FIRSTVISIBLE);
	}
	return 0;
}

int WindowView::TreeViewSelectItem(TorrentGroup* grp)
{
	HTREEITEM hti = torrentsViewOrgs->findTreeItem(grp);
	if (hti) {
		TreeView_Select(hTree, hti, TVGN_CARET);
		TreeView_Select(hTree, hti, TVGN_FIRSTVISIBLE);
	}
	return 0;
}

LRESULT WindowView::ProcCustDrawListFile(LPNMLVCUSTOMDRAW& plcd)
{
	LRESULT lrt = CDRF_DODEFAULT;
	switch (plcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		lrt = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
	{
		ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
		TorrentNodeFileNode* cfn = lpd->file;
		plcd->clrTextBk = cfn->check ? plcd->clrTextBk : RGB(0xCF, 0xCF, 0xCF);
		plcd->clrText = cfn->priority > 0 ? RGB(0x00, 0x7F, 0x00) : (cfn->priority < 0 ? RGB(0x3F, 0x3F, 0xCF) : plcd->clrText);
	}
	break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
	{
		ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
		TorrentNode* ctn = lpd->node;
		if (ctn->status == 0) {
			plcd->clrTextBk = RGB(0xCF, 0xCF, 0xCF);
		}
		if (ctn->status == 4) {
			plcd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
			//plcd->clrFace = RGB(0xCF, 0xFF, 0xCF);
			plcd->clrText = RGB(0x00, 0x3F, 0x00);
		}
		if (ctn->error.length() > 0) {
			plcd->clrText = RGB(0xFF, 0x00, 0x00);
		}
	}
	break;
	}
	return lrt;
}

LRESULT WindowView::ProcCustDrawListNodes(LPNMLVCUSTOMDRAW& plcd)
{
	LRESULT lrt = CDRF_DODEFAULT;
	switch (plcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		lrt = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
	{
		int sli = ListView_GetNextItem(hList, ((int)plcd->nmcd.dwItemSpec) - 1, LVNI_SELECTED);
		if (sli != plcd->nmcd.dwItemSpec) {
			ListParmData* ctn = (ListParmData*)plcd->nmcd.lItemlParam;
			if (ctn->type == ListParmData::Node) {
				lrt = ctn->node->error.length() > 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
				lrt = ctn->node->status == 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
				lrt = ctn->node->status == 4 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
			}
		}
	}
	break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
	{
		ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
		if (lpd->type == ListParmData::Node) {
			TorrentNode* ctn = lpd->node;
			if (ctn->status == 0) {
				plcd->clrTextBk = RGB(0xCF, 0xCF, 0xCF);
			}
			if (ctn->status == 4) {
				int nameisub = 1;
				if (plcd->iSubItem == nameisub) {
					RECT src;
					ListView_GetSubItemRect(hList, plcd->nmcd.dwItemSpec, nameisub, LVIR_BOUNDS, &src);
					FillRect(plcd->nmcd.hdc, &src, hbGreen);
					src.left += 2 * GetSystemMetrics(SM_CXEDGE);
					//InflateRect(&src, -1, -1);
					if (lpd->node->size > 0) {
						RECT urc(src);
						urc.right = (long)((src.right - src.left) * lpd->node->leftsize/lpd->node->size) + src.left;
						FillRect(plcd->nmcd.hdc, &urc, hbRed);
						//LVITEM lvi = { 0 };
						//lvi.iSubItem = 1;
						//lvi.pszText = wbuf;
						//lvi.cchTextMax = 1024;
						//int tls = ::SendMessage(hFileList, LVM_GETITEMTEXT, plcd->nmcd.dwItemSpec, (LPARAM)&lvi);
						DrawText(plcd->nmcd.hdc, lpd->node->name.c_str(), lpd->node->name.length(), &src, DT_LEFT);
						lrt = CDRF_SKIPDEFAULT;
					}
				}

				if (lrt != CDRF_SKIPDEFAULT) {
					plcd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
					plcd->clrText = RGB(0x00, 0x3F, 0x00);
				}
			}
			if (ctn->error.length() > 0) {
				plcd->clrText = RGB(0xFF, 0x00, 0x00);
			}
		}
	}
	break;
	}
	return lrt;
}
LRESULT WindowView::ProcCustDrawListDetail(LPNMLVCUSTOMDRAW& plcd)
{
	LRESULT lrt = CDRF_DODEFAULT;
	switch (plcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		lrt = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
		switch (plcd->nmcd.dwItemSpec) {
		case 1:
		case 6:
		case 7:
		case 11:
		{
			ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;

			TorrentNode* ctn = lpd->node;
			lrt = ctn->status == 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
			lrt = ctn->error.length() > 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
		}
		break;
		case 27:
			lrt = CDRF_NOTIFYSUBITEMDRAW;
			break;
		default:
			lrt = CDRF_DODEFAULT;
			break;
		}
		break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
		lrt = CDRF_DODEFAULT;
		switch (plcd->nmcd.dwItemSpec) {
		case 27:
			if (plcd->iSubItem == 1)
			{
				lrt = ProcCustDrawListPieces(plcd);
			}
			break;
		case 1:
		case 6:
		case 7:
		{
			ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;

			TorrentNode* ctn = lpd->node;
			if (ctn->status == 0) {
				plcd->clrTextBk = RGB(0xCC, 0xCC, 0xCC);
			}
		}
		case 11:
		{
			ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
			TorrentNode* ctn = lpd->node;
			if (ctn->error.length() > 0) {
				plcd->clrText = RGB(0xFF, 0x00, 0x00);
			}
		}
		break;
		}
	}

	return lrt;
}

LRESULT WindowView::ProcCustDrawListName(const LPNMLVCUSTOMDRAW& plcd, const COLORREF& ocr)
{
	LRESULT lrt = CDRF_DODEFAULT;
	RECT src;
	ListView_GetSubItemRect(hList, plcd->nmcd.dwItemSpec, 1, LVIR_BOUNDS, &src);
	int imm = 3 * GetSystemMetrics(SM_CXEDGE);
	src.left += imm;
	if (src.right > src.left) {
		ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
		TorrentNode* ctn = lpd->node;
		DrawText(plcd->nmcd.hdc, ctn->name.c_str(), (DWORD)ctn->name.length(), &src, DT_LEFT);
		SetTextColor(plcd->nmcd.hdc, ocr);
		lrt = CDRF_SKIPDEFAULT;
	}

	return lrt;
}

LRESULT WindowView::ProcCustDrawListPieces(const LPNMLVCUSTOMDRAW& plcd)
{
	RECT src;
	HGDIOBJ hpo;
	ListView_GetSubItemRect(hList, plcd->nmcd.dwItemSpec, 1, LVIR_BOUNDS, &src);
	src.left += 3 * GetSystemMetrics(SM_CXEDGE);
	InflateRect(&src, -1, -1);
	hpo = SelectObject(plcd->nmcd.hdc, gradualpen[0]);
	ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
	TorrentNode* ctn = lpd->node;
	int ide = 0;
	double mgs;
	int ipi = 0;
	unsigned int spi = 0;
	int idx = 0;
	int cwd = src.right - src.left;
	HPEN hpen;
	LRESULT lrt = CDRF_DODEFAULT;
	if (ctn->pieces.length() > 0) {
		if (cwd > 0) {
			while (spi < ctn->piececount) {
				ide += ctn->piecesdata[spi];
				idx++;
				spi++;
				mgs = spi * cwd;
				mgs /= ctn->piececount;

				if (ipi < (int)mgs) {
					ide = ide * 255 / idx;
					hpen = gradualpen[ide];
					SelectObject(plcd->nmcd.hdc, hpen);
					while (ipi < (int)mgs) {
						MoveToEx(plcd->nmcd.hdc, ipi + src.left, src.top, NULL);
						LineTo(plcd->nmcd.hdc, ipi + src.left, src.bottom);
						ipi++;
					}
					ide = 0;
					idx = 0;
				}
			}
			SelectObject(plcd->nmcd.hdc, hpo);
			lrt = CDRF_SKIPDEFAULT;
		}
	}

	return lrt;
}

LRESULT WindowView::ProcNotifyTree(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	NMTREEVIEW* pntree = (NMTREEVIEW*)lParam;

	switch (pntree->hdr.code)
	{
	case TVN_SELCHANGED:
	{
		if (pntree->itemNew.hItem)
		{
			if (pntree->itemNew.lParam) {
				TreeParmData* tipd = reinterpret_cast<TreeParmData*>(pntree->itemNew.lParam);
				if (tipd->ItemType == TreeParmData::Group) {
					UpdateListViewTorrentGroup(tipd->group);
					TreeView_Expand(hTree, pntree->itemNew.hItem, TVE_EXPAND);
				}
				else if (tipd->ItemType == TreeParmData::Torrent) {
					UpdateListViewTorrentDetail(tipd->node);
					TreeView_Expand(hTree, pntree->itemNew.hItem, TVE_EXPAND);
					PostMessage(hWnd, WM_U_REFRESHTORRENTDETAIL, (WPARAM)tipd->node, 0);
				}
				else if (tipd->ItemType == TreeParmData::Session) {
					ListUpdateSession(tipd->session);
				}
				else if (tipd->ItemType == TreeParmData::File) {
					ListUpdateFiles(tipd->file);
				}
			}
		}
	}
	break;
	case NM_DBLCLK:
	{
		HTREEITEM htv = TreeView_GetSelection(hTree);
		WCHAR wbuf[1024];

		if (htv) {
			TVITEM tvi = { 0 };
			tvi.hItem = htv;
			tvi.mask = TVIF_PARAM | TVIF_TEXT;
			tvi.pszText = wbuf;
			tvi.cchTextMax = 1024;
			BOOL btn = TreeView_GetItem(hTree, &tvi);
			if (btn) {
				TreeParmData* tpd = (TreeParmData*)tvi.lParam;
				if (tpd->ItemType == TreeParmData::Tracker) {
					TorrentGroup* grp = analyzer->GetTrackerGroup(tvi.pszText);
					if (grp) {
						TreeViewSelectItem(grp);
					}
				}
			}
		}
	}
	break;
	case NM_CUSTOMDRAW:
	{
		LPNMTVCUSTOMDRAW pntd = (LPNMTVCUSTOMDRAW)lParam;
		lrt = CDRF_DODEFAULT;
		switch (pntd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			lrt = CDRF_NOTIFYITEMDRAW;
			break;
		case CDDS_ITEMPREPAINT:
		{
			TreeParmData* tpd = (TreeParmData*)pntd->nmcd.lItemlParam;
			if (tpd->ItemType == TreeParmData::Torrent) {
				if (tpd->node->status == 0) {
					pntd->clrTextBk = RGB(0xCC, 0xCC, 0xCC);
				}
				if (tpd->node->status == 4) {
					pntd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
					pntd->clrText = RGB(0x00, 0x3F, 0x00);
				}
				else if (tpd->node->error.length() > 0) {
					pntd->clrText = RGB(0xFF, 0x00, 0x00);
				}
			}
		}
		break;
		}
	}
	break;
	}
	return lrt;
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

	hTree = CreateWindow(WC_TREEVIEW, L"", WS_CHILD | WS_VSCROLL | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_HASLINES, 0, 0, 100, 100, *splitTree, NULL, hInst, NULL);
	hList = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | LVS_REPORT | WS_VISIBLE, 0, 0, 200, 10, *splitTree, NULL, hInst, NULL);

	splitTree->SetWindow(hTree);
	splitTree->SetWindow(hList);
	splitTree->SetRatio(0.2);

	//ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
	ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	HWND hhd = ListView_GetHeader(hList);
	if (hhd) {
		LONG_PTR lps = GetWindowLongPtr(hhd, GWL_STYLE);
		lps |= HDS_FLAT;
		SetWindowLongPtr(hhd, GWL_STYLE, lps);
	}

	/////////////////// Setup system font
	HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	::SendMessage(hList, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hLog, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hTree, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
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
	HTREEITEM htv = TreeView_GetSelection(hTree);

	if (htv) {
		TVITEM tvi = { 0 };
		tvi.hItem = htv;
		tvi.mask = TVIF_PARAM;
		BOOL btn = TreeView_GetItem(hTree, &tvi);
		if (btn) {
			if (tvi.lParam) {
				TreeParmData* tip = (TreeParmData*)tvi.lParam;
				if (tip->ItemType == TreeParmData::Group) {
					TorrentGroup* cgp = tip->group;
					if (listContent == TORRENTLIST) {
						UpdateListViewTorrentNodes(cgp);

						std::wstring vvv;
						profile.GetDefaultValue(L"LocalListSort", vvv);
						if (vvv.length() > 0) {
							std::wstringstream wss(vvv);
							int sit;
							wss >> sit;
							if (sit >= 0) {
								ListView_SortItems(hList, LVComp_Nodes, sit);
							}
						}
					}
					else if (listContent == TORRENTGROUP) {
						UpdateListViewGroups(hList);

						std::wstring vvv;
						profile.GetDefaultValue(L"LocalGroupSort", vvv);
						if (vvv.length() > 0) {
							std::wstringstream wss(vvv);
							int sit;
							wss >> sit;
							if (sit >= 0) {
								ListView_SortItems(hList, LVComp_Group, sit);
							}
						}

					}
				}
				else if (tip->ItemType == TreeParmData::Torrent) {
					TorrentNode* tnd = tip->node;
					if (listContent == TORRENTDETAIL) {
						if (tnd->updatetick > tnd->readtick) {
							int lic = ListView_GetItemCount(hList);
							UpdateListViewTorrentDetailData(tnd);
						}
					}
				}
				else if (tip->ItemType == TreeParmData::File) {
					TorrentNodeFileNode* fnf = tip->file;
					if (listContent == TORRENTFILE) {
						UpdateListViewTorrentFiles(fnf);
						
						std::wstring vvv;
						profile.GetDefaultValue(L"LocalListFileSort", vvv);
						if (vvv.length() > 0) {
							std::wstringstream wss(vvv);
							int sit;
							wss >> sit;
							if (sit >= 0) {
								ListView_SortItems(hList, LVComp_Files, sit);
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

void WindowView::UpdateViewTorrentGroup(TorrentGroup* grp)
{
	wchar_t tbuf[1024];
	TreeGroupShadow* ngi;
	TreeParmData* npd;

	TVINSERTSTRUCT tvi;
	if (torrentsViewOrgs) {
		ngi = torrentsViewOrgs->GetTreeGroupShadow(grp);
		if (ngi) {
			if (ngi->hnode == NULL) {
				tvi = { 0 };
				wsprintf(tbuf, L"%s", grp->name.c_str());
				tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
				tvi.item.cchTextMax = (int)wcslen(tbuf);
				tvi.item.pszText = (LPWSTR)tbuf;
				tvi.hParent = NULL;
				npd = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Session);
				npd->session = &analyzer->session;
				tvi.item.lParam = (LPARAM)npd;
				tvi.hInsertAfter = TVI_ROOT;
				ngi->hnode = TreeView_InsertItem(hTree, &tvi);
			}
			UpdateTreeViewGroup(ngi);
			//updateListViewRefresh();

			TreeView_Expand(hTree, ngi->hnode, TVE_EXPAND);
		}
	}
}

HTREEITEM WindowView::UpdateTreeViewNodeItem(TreeGroupShadow* tgs, TorrentNode* node)
{
	std::map<TorrentNode*, HTREEITEM>::iterator ittt = tgs->nodeitems.find(node);
	TVINSERTSTRUCT tis;
	wchar_t tbuf[1024];
	TreeParmData* ntipd;
	HTREEITEM hti;

	if (ittt == tgs->nodeitems.end()) {
		tis = { 0 };
		tis.item.mask = TVIF_TEXT | TVIF_PARAM;
		wsprintf(tbuf, L"%s", node->name.c_str());
		tis.item.cchTextMax = (int)wcslen(tbuf);
		tis.item.pszText = (LPWSTR)tbuf;
		tis.hParent = tgs->hnode;
		ntipd = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Torrent);
		ntipd->node = node;
		tis.item.lParam = (LPARAM)ntipd;

		hti = TreeView_InsertItem(hTree, &tis);
		if (hti) {
			tgs->nodeitems[node] = hti;
		}
	}
	else {
		hti = ittt->second;
	}

	return hti;
}

void WindowView::UpdateTreeViewGroup(TreeGroupShadow* gti)
{
	TVINSERTSTRUCT tis;
	wchar_t tbuf[1024];
	TorrentGroup* grp = gti->group;
	TreeParmData* ntipd;
	TorrentParmItems* ims;
	std::map<unsigned long, HTREEITEM>::iterator ittt;
	HTREEITEM hti;
	TorrentNode* node;

	for (std::map<unsigned long, TorrentNode*>::iterator ittn = grp->torrents.begin()
		; ittn != grp->torrents.end()
		; ittn++) {
		node = ittn->second;
		hti = UpdateTreeViewNodeItem(gti, node);
		if (hti) {
			ims = gti->GetNodeParmItems(node);
			UpdateTreeViewTorrentDetail(ims);
		}
	}

	TreeGroupShadow* ntg;
	std::map<TorrentGroup*, TreeGroupShadow*>::iterator itgg;
	TorrentGroup* ttg;
	LARGE_INTEGER itt;

	for (std::map<std::wstring, TorrentGroup*>::iterator ittg = grp->subs.begin()
		; ittg != grp->subs.end()
		; ittg++) {
		ttg = ittg->second;
		ntg = torrentsViewOrgs->GetTreeGroupShadow(ttg);
		if (ntg) {
			if (ntg->hnode == NULL) {
				tis = { 0 };
				tis.item.mask = TVIF_TEXT | TVIF_PARAM;
				FormatByteSize(tbuf + 512, 512, ttg->size);
				wsprintf(tbuf, L"(%s) %s", tbuf + 512, ttg->name.c_str());
				tis.item.cchTextMax = (int)wcslen(tbuf);
				tis.item.pszText = (LPWSTR)tbuf;
				ntipd = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Group);
				ntipd->group = ttg;
				tis.hParent = gti->hnode;
				tis.item.lParam = (LPARAM)ntipd;

				hti = TreeView_InsertItem(hTree, &tis);
				ntg->hnode = hti;
			}
			else {
				if (ttg->updatetick > ttg->readtick) {
					itt.QuadPart = GetTickCount64();
					ttg->readtick = itt.LowPart;
					TVITEM tvi;
					tvi.hItem = ntg->hnode;
					tvi.mask = TVIF_TEXT;
					FormatByteSize(tbuf + 512, 512, ittg->second->size);
					wsprintf(tbuf, L"(%s) %s", tbuf + 512, ittg->second->name.c_str());
					tvi.pszText = tbuf;
					tvi.cchTextMax = wcslen(tbuf);
					TreeView_SetItem(hTree, &tvi);
				}
			}
			UpdateTreeViewGroup(ntg);
		}
	}
}

int WindowView::StartAnalysis()
{
	WCHAR wbuf[1024];

	if (remoteUrl.length() > 0) {
		analyzer->StartClient(remoteUrl.c_str(), remoteUser.c_str(), remotePass.c_str());
		if (torrentsViewOrgs == NULL) {
			torrentsViewOrgs = TreeGroupShadow::CreateTreeGroupShadow(analyzer->groupRoot, NULL);
		}
		torrentsViewOrgs->SyncGroup();

		wsprintf(wbuf, L"Login to remote site [%s] user/password [%s:%s]", remoteUrl.c_str(), remoteUser.c_str(), remotePass.c_str());
		ViewLog(wbuf);

		UpdateViewTorrentGroup(analyzer->groupRoot);
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

int WindowView::ClearColumns(HWND hlist)
{
	ListView_DeleteAllItems(hlist);

	HWND hhd = ListView_GetHeader(hlist);
	if (hhd) {
		int hdc = Header_GetItemCount(hhd);
		if (hdc > 0) {
			for (int ii = 0; ii < hdc; ii++) {
				ListView_DeleteColumn(hlist, 0);
			}
		}
	}
	return 0;
}

int WindowView::CreateListColumnsGroupList(HWND hlist)
{
	struct ColDef cols[] = {
		{L"Name", 200}
		, { L"Size", 100 }
		, { L"ViewSize", 100 }
		, { L"Count", 100}
		, { L"Download/s", 100}
		, { L"Upload/s", 100}
		, {0}
	};

	CreateListColumns(hlist, cols);
	listContent = TORRENTGROUP;
	return 0;
}

int WindowView::CreateListColumnsNodeList(HWND hlist)
{
	struct ColDef cols[] = {
		{ L"ID", 30 }
		, { L"Name", 200 }
		, { L"Size", 100 }
		, { L"ViewSize", 100 }
		, { L"Tracker", 100 }
		, { L"Download", 50 }
		, { L"Upload", 50 }
		, { L"Status", 50 }
		, { L"Ratio", 50 }
		, {0}
	};

	CreateListColumns(hlist, cols);
	listContent = TORRENTLIST;
	return 0;
}

int WindowView::CreateListColumns(HWND hlist, struct ColDef *cols)
{
	LVCOLUMN ilc = { 0 };
	int ii = 0;
	int rtn;

	while (cols[ii].name[0]) {
		ilc.mask = LVCF_TEXT | LVCF_WIDTH;
		ilc.pszText = cols[ii].name;
		ilc.cchTextMax = (int)wcslen(cols[ii].name);
		ilc.cx = cols[ii].width;

		rtn = ListView_InsertColumn(hlist, ii, &ilc);

		ii++;
	}

	return 0;
}

int WindowView::UpdateListViewTorrentNodesDetail(HWND hlist, int iti, TorrentNode* nod)
{
	wchar_t wbuf[128];
	int isub = 0;

	FormatViewSize(wbuf, 128, nod->id);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	ListView_SetItemText(hlist, iti, isub, (LPWSTR)nod->name.c_str());
	isub++;

	FormatViewSize(wbuf, 128, nod->size);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	FormatByteSize(wbuf, 128, nod->size);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	if (nod->trackerscount > 0) {
		ListView_SetItemText(hlist, iti, isub, (LPWSTR)nod->trackers[0].c_str());
	}
	isub++;

	FormatByteSize(wbuf, 128, nod->downspeed);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	FormatByteSize(wbuf, 128, nod->upspeed);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	FormatViewStatus(wbuf, 128, nod->status);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	FormatViewDouble(wbuf, 128, nod->ratio);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	ULARGE_INTEGER lit;
	lit.QuadPart = GetTickCount64();
	nod->readtick = lit.LowPart;

	return 0;
}

int WindowView::UpdateListViewGroups(HWND hlist)
{
	if (listContent == LISTCONTENT::TORRENTGROUP) {
		int lsc = ListView_GetItemCount(hlist);
		LVITEM lvi;
		BOOL btn;
		ListParmData* lpd;

		for (int ii = 0; ii < lsc; ii++) {
			lvi.mask = LVIF_PARAM;
			lvi.iItem = ii;
			
			btn = ListView_GetItem(hlist, &lvi);
			if (btn) {
				lpd = (ListParmData*)lvi.lParam;
				if (lpd->type == ListParmData::Group) {
					UpdateListViewTorrentGroupData(hlist, ii, lpd->group);
				}
			}
		}
	}
	return 0;
}

int WindowView::UpdateListViewTorrentGroupData(HWND hlist, int iti, TorrentGroup* nod)
{
	wchar_t wbuf[128];
	int itt = 0;

	itt++;

	FormatViewSize(wbuf, 128, nod->size);
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	FormatByteSize(wbuf, 128, nod->size);
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	wsprintf(wbuf, L"%d", nod->torrents.size());
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	FormatByteSize(wbuf + 64, 64, nod->downspeed);
	wsprintf(wbuf, L"%s/s", wbuf + 64);
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	FormatByteSize(wbuf + 64, 64, nod->upspeed);
	wsprintf(wbuf, L"%s/s", wbuf + 64);
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	return 0;
}

int WindowView::UpdateListViewTorrentGroup(TorrentGroup* grp)
{

	LVITEM lvi = { 0 };
	int iti = 0;
	ListParmData* lpd;

	ClearColumns(hList);

	if (grp->subs.size() > 0) {
		CreateListColumnsGroupList(hList);
		for (std::map<std::wstring, TorrentGroup*>::iterator itgp = grp->subs.begin()
			; itgp != grp->subs.end()
			; itgp++) {
			lvi.mask = LVIF_TEXT;
			lvi.pszText = (LPWSTR) itgp->second->name.c_str();
			lvi.cchTextMax = (int)wcslen(lvi.pszText);
			lvi.iItem = iti;
			iti = ListView_InsertItem(hList, &lvi);

			lvi.iItem = iti;
			lvi.mask = LVIF_PARAM;
			lpd = GetListParmData(iti);
			lpd->type = ListParmData::Group;
			lpd->group = itgp->second;
			lvi.lParam = (LPARAM)lpd;
			ListView_SetItem(hList, &lvi);

			UpdateListViewTorrentGroupData(hList, iti, itgp->second);
			iti++;
		}
	} else {
		CreateListColumnsNodeList(hList);
		UpdateListViewTorrentNodes(grp);

		std::wstring vvv;
		profile.GetDefaultValue(L"LocalListSort", vvv);
		if (vvv.length() > 0) {
			std::wstringstream wss(vvv);
			int sit;
			wss >> sit;
			if (sit >= 0) {
				ListView_SortItems(hList, LVComp_Nodes, sit);
			}
		}

	}
	return 0;
}

int WindowView::UpdateListViewTorrentNodes(TorrentGroup* grp)
{
	std::set<TorrentNode*> nodes;
	ListParmData* lpd;
	wchar_t wbuf[1024];

	grp->GetNodes(nodes);

	int itc = ListView_GetItemCount(hList);
	LVITEM lvi = { 0 };
	for (int ii = 0; ii < itc; ii++) {
		lvi.iItem = ii;
		lvi.mask = LVIF_PARAM;
		ListView_GetItem(hList, &lvi);
		lpd = (ListParmData*)lvi.lParam;
		if (lpd->type == ListParmData::Node) {
			if (nodes.find(lpd->node) != nodes.end()) {
				nodes.erase(lpd->node);
				if (lpd->node->updatetick > lpd->node->readtick) {
					UpdateListViewTorrentNodesDetail(hList, ii, lpd->node);
				}
			}
		}
	}
	itc++;
	for (std::set<TorrentNode*>::iterator itnd = nodes.begin()
		; itnd != nodes.end()
		; itnd++) {
		lvi.mask = LVIF_TEXT;
		wsprintf(wbuf, L"%d", (*itnd)->id);
		lvi.pszText = wbuf;
		lvi.cchTextMax = (int)wcslen(lvi.pszText);
		lvi.iItem = itc;
		itc = ListView_InsertItem(hList, &lvi);

		lvi.iItem = itc;
		lvi.mask = LVIF_PARAM;
		lpd = GetListParmData(itc);
		lpd->type = ListParmData::Node;
		lpd->node = *itnd;
		lvi.lParam = (LPARAM)lpd;
		ListView_SetItem(hList, &lvi);

		UpdateListViewTorrentNodesDetail(hList, itc, *itnd);
		itc++;
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

std::set<TreeParmData*> TreeItemParmDataHelper::parms;

TreeParmData* TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::TIPDTYPE type)
{
	TreeParmData* npd = new TreeParmData();
	npd->ItemType = type;
	parms.insert(npd);
	return npd;
}

int TreeItemParmDataHelper::ClearTreeItemParmData()
{
	for (std::set<TreeParmData*>::iterator itpd = parms.begin()
		; itpd != parms.end()
		; itpd++) {
		delete* itpd;
	}
	parms.clear();
	return 0;
}
