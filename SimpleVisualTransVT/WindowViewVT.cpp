#include "WindowViewVT.h"
#include "Resource.h"
#include "RibbonHandlerMain.h"
#include "ViewCommons.h"

#include <map>
#include <set>
#include <CommCtrl.h>
#include <windowsx.h>
#include <algorithm>
#include <sstream>
#include <strsafe.h>

#pragma comment(lib, "Comctl32.lib")
#ifdef UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#define WM_U_INITWINDOWVIEW WM_USER + 0x111
#define WM_U_UPDATETORRENT WM_USER + 0x222
//#define WM_U_UPDATELISTFILE WM_USER + 0x333
#define WM_U_UPDATELISTTORRENTGROUP WM_USER + 0x334
#define WM_U_UPDATELISTTORRENTNODE WM_USER + 0x335
#define WM_U_UPDATELISTSESSION WM_USER + 0x336
#define WM_U_UPDATEVIEWNODE WM_USER + 0x337
//#define WM_U_LISTADDGROUPNODE WM_USER + 0x338
#define WM_U_REMOVETORRENT WM_USER + 0x339
#define WM_U_LISTADDGROUPGROUP WM_USER + 0x340
#define WM_U_UPDATEGROUP WM_USER + 0x341
#define WM_U_PROCREFRESH WM_USER + 0x342
#define WM_U_REQUESTERROR WM_USER + 0x343
//#define WM_U_ACTIVATENODE WM_USER + 0x344
#define WM_U_UPDATENODEFILE WM_USER + 0x345
#define WM_U_UPDATENODEFILESTATUS WM_USER + 0x346
#define WM_U_ADDNEWTORRENTLINK WM_USER + 0x347
#define WM_U_VIEWLOGMESSAGE WM_USER + 0x348
#define WM_U_STARTREFRESHTIMER WM_USER + 0x349
#define WM_U_REQUESTREFRESHSESSION WM_USER + 0x350
#define WM_U_PROCREFRESHSESSION WM_USER + 0x351
#define WM_U_RIBBONCOMMAND WM_USER + 0x352
#define WM_U_RESIZECONTENT WM_USER + 0x353
#define TID_REFRESH_TORRENTS 0x2234
ATOM atomWindowClass = 0;
#define MAX_LOADSTRING 100

WCHAR szTitle[MAX_LOADSTRING] = L"SimpleVisualTransVT";                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING] = L"SIMPLEVISUALTRANSVT";            // the main window class name

class WindowViewVTMap : public std::map<HWND, WindowViewVT*>
{
    HANDLE locker;
public:
    WindowViewVTMap() {
        locker = ::CreateEvent(NULL, FALSE, TRUE, NULL);
    }
    virtual ~WindowViewVTMap() {
        ::CloseHandle(locker);
    }
    bool lock() {
        if (::WaitForSingleObject(locker, INFINITE) == WAIT_OBJECT_0) {
            return true;
        }
        return false;
    }
    bool unlock() {
        return SetEvent(locker);
    }
    WindowViewVT* GetWindowView(HWND hwnd) {
        WindowViewVT* view = NULL;

        viewMap.lock();
        std::map<HWND, WindowViewVT*>::iterator itvm = viewMap.find(hwnd);
        if (itvm != viewMap.end()) {
            view = itvm->second;
        }
        viewMap.unlock();

        return view;
    }
} viewMap;

WNDPROC lvAddOriProc = NULL;
WNDPROC lvLocationOriProc = NULL;
WNDPROC lvProfileOriProc = NULL;
WNDPROC edProfileOriProc = NULL;

WindowViewVT::WindowViewVT()
    :
    hWnd(NULL)
    , hContent(NULL)
    , hTool(NULL)
    , client(NULL)
    , doneInit(FALSE)
    , hInstance(NULL)
    , list(NULL)
    , hTree(NULL)
    , hLog(NULL)
    , hStatus(NULL)
    , live(FALSE)
    , splitLog(NULL)
    , splitStatus(NULL)
    , splitTool(NULL)
    , splitTree(NULL)
    , iParmEdit(-1)
    , hProfileEdit(NULL)
    , hProfile(NULL)
    , hAddTorrent(NULL)
    , panel(NULL)
    , pdrop(NULL)
    , listenToClipboard(FALSE)
	, ribbonFramework(NULL)
    , console{ 0 }
{
    client = new TorrentClient();
    client->Start();
	eConsole = CreateEvent(NULL, FALSE, TRUE, NULL);
}

WindowViewVT::~WindowViewVT()
{
	if (eConsole) {
		CloseHandle(eConsole);
		eConsole = NULL;
	}
    if (listenToClipboard) {
        RemoveClipboardFormatListener(hWnd);
        listenToClipboard = FALSE;
    }
    if (lvAddOriProc) {
        ::SetWindowLongPtr(hAddTorrent, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lvAddOriProc));
    }
	if (lvLocationOriProc) {
		::SetWindowLongPtr(hLocation, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lvLocationOriProc));
	}
	if (lvProfileOriProc) {
		::SetWindowLongPtr(hProfile, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lvProfileOriProc));
	}
	if (edProfileOriProc) {
		::SetWindowLongPtr(hProfileEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(edProfileOriProc));
	}
    if (client) {
        client->Stop();
        delete client;
        client = NULL;
    }
    if (pdrop) {
        RevokeDragDrop(hWnd);
        pdrop = NULL;
    }
    if (console.psinfo.dwProcessId) {
        WaitForSingleObject(console.psinfo.hProcess, INFINITE);
        CloseHandle(console.psinfo.hProcess);
        CloseHandle(console.psinfo.hThread);
    }
}
LRESULT APIENTRY EditBoxKeypressSubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT lst = 0;

	if (uMsg == WM_KEYUP) {
		if (wParam == VK_RETURN) {
			HWND hpnt = GetParent(hwnd);
			if (hpnt) {
				SetFocus(hpnt);
			}
		}
	}
	else {
		lst = CallWindowProc(edProfileOriProc, hwnd, uMsg, wParam, lParam);
	}
	return lst;
}

LRESULT APIENTRY ListViewAddSubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lst = 0;

    if (uMsg == WM_COMMAND) {
        HWND hpnt = GetParent(hwnd);
        if (hpnt) {
            lst = SendMessage(hpnt, uMsg, wParam, lParam);
        }
    }
    else {
        lst = CallWindowProc(lvAddOriProc, hwnd, uMsg, wParam, lParam);
    }
    return lst;
}

ATOM WindowViewVT::RegisterClass(HINSTANCE hInstance)
{
    if (atomWindowClass == 0) {
        WNDCLASSEXW wcex;

        wcex.cbSize = sizeof(WNDCLASSEX);

        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SIMPLEVISUALTRANSVT));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SIMPLEVISUALTRANSVT);
        wcex.lpszClassName = szWindowClass;
        wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

        atomWindowClass = RegisterClassExW(&wcex);
    }
    return atomWindowClass;
}

DWORD WINAPI WindowViewVT::ThWindowViewVTProc(_In_ LPVOID lParam)
{
    WindowViewVT* self = (WindowViewVT*)lParam;

    InitCommonControls();

    HWND hwnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, self->hInstance, nullptr);

    if (hwnd)
    {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        self->hWnd = hwnd;
        viewMap.lock();
        viewMap[hwnd] = self;
        viewMap.unlock();

        self->live = TRUE;
        self->doneInit = TRUE;

        HACCEL hAccelTable = LoadAccelerators(self->hInstance, MAKEINTRESOURCE(IDC_SIMPLEVISUALTRANSVT));

        MSG msg;

        // Main message loop:
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    else {
        self->doneInit = TRUE;
    }

    self->live = FALSE;
    return 0;
}

WindowViewVT* WindowViewVT::InitInstance(HINSTANCE hinst, int nCmdShow)
{
    DWORD tid;
    WindowViewVT* view = new WindowViewVT();
    view->hInstance = hinst;
    view->doneInit = FALSE;
    CreateThread(NULL, 0, ThWindowViewVTProc , view, 0, &tid);
    while (!view->doneInit) {
        Sleep(100);
    }
    if (view->hWnd) {
        ::PostMessage(view->hWnd, WM_U_INITWINDOWVIEW, 0, 0);
    }
    return view;
}

WindowViewVT* WindowViewVT::StartWindow(HINSTANCE hinst, HINSTANCE hprev)
{
    RegisterClass(hinst);
    WindowViewVT* view = InitInstance(hinst, 0);

    return view;
}

ViewNode* WindowViewVT::GetGroupViewNode(TorrentGroupVT* group, bool createnew)
{
    ViewNode* vnd = NULL;
    if (group) {
        std::map<TorrentGroupVT*, ViewNode*>::iterator itgm = groupviewmap.find(group);
        if (itgm == groupviewmap.end()) {
            if (createnew) {
                if (group->subs.size() > 0) {
                    vnd = new ViewNode(VG_TYPE::GROUPGROUP);
                }
                else {
                    vnd = new ViewNode(VG_TYPE::GROUP);
                }
                vnd->group.groupnode = group;
                vnd->parent = GetGroupViewNode(group->parent, createnew);
				if (vnd->parent) {
					vnd->parent->AddNode_(vnd);
				}
                groupviewmap[group] = vnd;
                Tree_AddGroupNode(vnd);
            }
        }
        else {
            vnd = itgm->second;
        }
    }
    return vnd;
}

int WindowViewVT::Tree_AddFileNode(ViewNode* vnode)
{
	WCHAR wbuf[1024];
    if (treeviewnodemap.find(vnode) == treeviewnodemap.end()) {
        if (vnode->parent) {
			std::map<ViewNode*, HTREEITEM>::iterator itvi = treeviewnodemap.find(vnode->parent);
            if (itvi == treeviewnodemap.end()) {
                Tree_AddFileNode(vnode->parent);
            }
			itvi = treeviewnodemap.find(vnode->parent);
			if (itvi != treeviewnodemap.end()) {
				HTREEITEM hph = itvi->second;
				TVINSERTSTRUCT tvi;
				tvi = { 0 };

				tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
				StringCbPrintf(wbuf, 1024, L"%s (%d)", vnode->name.c_str(), vnode->torrent.file->totalcount);
				tvi.item.cchTextMax = (int)wcslen(wbuf);
				tvi.item.pszText = (LPWSTR)wbuf;
				tvi.hParent = hph;
				tvi.item.lParam = (LPARAM)vnode;
				tvi.hInsertAfter = TVI_LAST;
				HTREEITEM hch = TreeView_InsertItem(hTree, &tvi);
				treeviewnodemap[vnode] = hch;

				UINT igi = TreeView_GetItemState(hTree, hph, TVIS_EXPANDED);
				if (igi == 0) {
					TreeView_Expand(hTree, hph, TVE_EXPAND);
					TreeView_Expand(hTree, hph, TVE_COLLAPSE);
				}
			}
        }
    }
	std::for_each(vnode->subnodes.begin(), vnode->subnodes.end(), [this](ViewNode* vfn) {
		Tree_AddFileNode(vfn);
	});
	return 0;
}

int WindowViewVT::Tree_UpdateTorrentNodeFile(TorrentNodeVT* node)
{
	std::set<ViewNode*> vnodes;
	std::for_each(rootviewnodes.begin(), rootviewnodes.end(), [this, node, &vnodes](ViewNode* vnode) {
		vnode->GetViewNodeSet(node, vnodes);
	});
	std::for_each(vnodes.begin(), vnodes.end(), [this](ViewNode* vnode) {
		if (vnode->torrent.node->path.subfiles.count > 0) {
			vnode->torrent.filenode->AddFileViewNode(&vnode->torrent.node->path);
			Tree_AddFileNode(vnode->torrent.filenode);
		}
	});

	return 0;
}

int WindowViewVT::ViewUpdateNodeFile(TorrentClientRequest* req, TASKPARM* parm)
{
    TorrentNodeVT* node = parm->node;
    
    delete parm;

    if (node) {
        Tree_UpdateTorrentNodeFile(node);
    }
    return 0;
}

int WindowViewVT::ViewUpdateNodeFileStatus(TorrentClientRequest* req, TASKPARM* parm)
{
    TorrentNodeVT* node = parm->node;
    TorrentNodeFile* fnode = parm->file;
    delete parm;

    if (node) {
        list->UpdateFileStat(node, fnode);
    }
    else {
        list->AutoHeaderWidth();
    }
    return 0;
}

int WindowViewVT::ViewUpdateGroup(TorrentClientRequest* req, TASKPARM* parm)
{
    TorrentGroupVT* pgroup = parm->pgroup;
    TorrentGroupVT* group = parm->group;
    delete parm;

    if (group == NULL) {
        delete req;
    }
    else {
		list->UpdateItemGroup(0, group);
    }
    return 0;
}

int WindowViewVT::ViewUpdateTorrent(TorrentClientRequest* req, TASKPARM* parm)
{
    TorrentNodeVT* node = parm->node;
    TorrentGroupVT* group = parm->group;
    delete parm;

    if (node == NULL) {
        delete req;
        PostMessage(hWnd, WM_U_STARTREFRESHTIMER, 0, 0);
		PostMessage(hWnd, WM_U_REQUESTREFRESHSESSION, 0, 0);
    } else {
        Tree_UpdateTorrentNode(group, node);
		ViewNode* vgroup = GetGroupViewNode(group, false);
		if (vgroup) {
			list->UpdateTorrentNode(node, vgroup);
		}
    }
    return 0;
}

void WindowViewVT::Tree_UpdateTorrentNode(TorrentGroupVT* group, TorrentNodeVT* node)
{
    ViewNode* cvg = GetGroupViewNode(group, true);
    if (cvg) {
        ViewNode* cvn = cvg->GetViewNode(node);
        if (cvn == NULL) {
            cvn = new ViewNode(VG_TYPE::TORRENT);
            cvn->torrent.node = node;
            cvg->AddNode_(cvn);
            Tree_AddTorrentNode(cvn);
        }
    }
}

void WindowViewVT::Tree_AddGroupNode(ViewNode* vnode)
{
    TVINSERTSTRUCT tvi;
    wchar_t tbuf[1024];
    tvi = { 0 };
	HTREEITEM hpt = NULL;

	if (vnode->parent) {
		if (treeviewnodemap.find(vnode->parent) != treeviewnodemap.end()) {
			hpt = treeviewnodemap[vnode->parent];
		}
		wsprintf(tbuf, L"%s", vnode->group.groupnode->name.c_str());
		tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
		tvi.item.cchTextMax = (int)wcslen(tbuf);
		tvi.item.pszText = (LPWSTR)tbuf;
		tvi.hParent = hpt;
		tvi.item.lParam = (LPARAM)vnode;
		tvi.hInsertAfter = vnode->parent ? TVI_ROOT : TVI_LAST;
		HTREEITEM hct = TreeView_InsertItem(hTree, &tvi);
		treeviewnodemap[vnode] = hct;
		if (hpt == NULL) {
			rootviewnodes.insert(vnode);
		}
	}
}

void WindowViewVT::Tree_AddTorrentNode(ViewNode* vnode)
{
    TVINSERTSTRUCT tvi;
    wchar_t tbuf[1024];
    tvi = { 0 };
	HTREEITEM hpt = NULL;
	if (treeviewnodemap.find(vnode->parent) != treeviewnodemap.end()) {
		hpt = treeviewnodemap[vnode->parent];
	}
    wsprintf(tbuf, L"%s", vnode->torrent.node->name.c_str());
    tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.item.cchTextMax = (int)wcslen(tbuf);
    tvi.item.pszText = (LPWSTR)tbuf;
    tvi.hParent = hpt;
    tvi.item.lParam = (LPARAM)vnode;
    tvi.hInsertAfter = TVI_LAST;
	HTREEITEM hct = TreeView_InsertItem(hTree, &tvi);
	treeviewnodemap[vnode] = hct;
	if (hpt == NULL) {
		rootviewnodes.insert(vnode);
	}

    //ViewNode* pnode = new ViewNode(ViewNode::ATTRIBUTE);
    ViewNode* pnode = new ViewNode(VG_TYPE::ATTRIBUTE);
    pnode->torrent.node = vnode->torrent.node;
	pnode->parent = vnode;
	vnode->torrent.sizenode = pnode;
    FormatNumberByteView(tbuf + 512, 512, vnode->torrent.node->size);
    wsprintf(tbuf, L"Size: %s", tbuf + 512);
    tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.item.cchTextMax = (int)wcslen(tbuf);
    tvi.item.pszText = (LPWSTR)tbuf;
    tvi.hParent = hct;
    tvi.item.lParam = (LPARAM)pnode;
    tvi.hInsertAfter = TVI_LAST;
    HTREEITEM hat = TreeView_InsertItem(hTree, &tvi);
	treeviewnodemap[pnode] = hat;

	//pnode = new ViewNode(ViewNode::ATTRIBUTE);
    pnode = new ViewNode(VG_TYPE::ATTRIBUTE);
    pnode->torrent.node = vnode->torrent.node;
	pnode->parent = vnode;
	vnode->torrent.idnode = pnode;
    wsprintf(tbuf + 512, L"%d", vnode->torrent.node->id);
    wsprintf(tbuf, L"Id: %s", tbuf + 512);
    tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.item.cchTextMax = (int)wcslen(tbuf);
    tvi.item.pszText = (LPWSTR)tbuf;
    tvi.hParent = hct;
    tvi.item.lParam = (LPARAM)pnode;
    tvi.hInsertAfter = TVI_LAST;
    hat = TreeView_InsertItem(hTree, &tvi);
	treeviewnodemap[pnode] = hat;

    pnode = new ViewNode(VG_TYPE::FILE);
    pnode->torrent.node = vnode->torrent.node;
    pnode->name = vnode->torrent.node->path.fullpath;
    vnode->torrent.filenode = pnode;
	pnode->torrent.file = &vnode->torrent.node->path;
	pnode->parent = vnode;
    wsprintf(tbuf, L"Path: %s", pnode->name.c_str());
    tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.item.cchTextMax = (int)wcslen(tbuf);
    tvi.item.pszText = (LPWSTR)tbuf;
    tvi.hParent = hct;
    tvi.item.lParam = (LPARAM)pnode;
    tvi.hInsertAfter = TVI_LAST;
    hat = TreeView_InsertItem(hTree, &tvi);
	treeviewnodemap[pnode] = hat;
}


int WindowViewVT::ListAdd_RegNewTorrentIntoList(WCHAR* path, BOOL isfile)
{
    LVITEM lvi;
    lvi.mask = LVIF_TEXT;
    lvi.pszText = (LPWSTR)(isfile?L"Torrent File":L"URL");
    lvi.cchTextMax = (int)wcslen(lvi.pszText);
    lvi.iSubItem = 0;
    lvi.iItem = ListView_GetItemCount(hAddTorrent);

    int iix = ListView_InsertItem(hAddTorrent, &lvi);
    ListView_SetItemText(hAddTorrent, iix, 1, path);

    return 0;
}

int WindowViewVT::ListAdd_NewTorrentRawLines(WCHAR* path)
{
    std::wstringstream wss(path);
    std::wstring lns;
    while (std::getline(wss, lns)) {
        lns.erase(std::remove(lns.begin(), lns.end(), L'\r'), lns.end());
        int len = (int)lns.length() + 1;
        if (len > 1) {
            WCHAR* newres = new WCHAR[len];
            wcscpy_s(newres, len, lns.c_str());

            ListAdd_NewTorrent(newres);
            delete newres;
        }
    }
    delete path;
    return 0;
}

int WindowViewVT::ListAdd_NewTorrent(WCHAR* path)
{
    HANDLE htf = CreateFile(
        path
        , GENERIC_READ
        , FILE_SHARE_READ | FILE_SHARE_WRITE
        , NULL
        , OPEN_EXISTING
        , FILE_ATTRIBUTE_NORMAL
        , NULL
    );
	if (htf == INVALID_HANDLE_VALUE) {
		size_t fpp = std::wstring(path).find(L"http");
		bool puturl = fpp == 0;
		if (puturl == false) {
			fpp = std::wstring(path).find(L"magnet");
			puturl = fpp == 0;
		}
		if (puturl) {
			ListAdd_RegNewTorrentIntoList(path, FALSE);
		}
    } else {
        ListAdd_RegNewTorrentIntoList(path, TRUE);
        CloseHandle(htf);
    }
    panel->SwitchWindow(hAddTorrent);
    return 0;
}

int WindowViewVT::ViewProcClipboardUpdate()
{
    BOOL brt = OpenClipboard(hWnd);
    if (brt) {
        HANDLE hgd = GetClipboardData(CF_UNICODETEXT);
        if (hgd) {
            WCHAR* wbuf = (WCHAR*)GlobalLock(hgd);
            size_t bsz = wcslen(wbuf);
            if (bsz > 0) {
                WCHAR* wtb = new WCHAR[bsz + 1];
                wcscpy_s(wtb, bsz + 1, wbuf);
                PostMessage(hWnd, WM_U_ADDNEWTORRENTLINK, (WPARAM)wtb, 0);
            }
            GlobalUnlock(hgd);
        }
        brt = CloseClipboard();
    }
    return 0;
}

int WindowViewVT::ViewProcContextMenu(HWND hwnd, int xx, int yy)
{
    POINT pnt = { xx, yy };
    //POINT wpt;
    //RECT rct;
    int rtn = 1;


    //if (rtn > 0) {
        //wpt = pnt;
        //::ScreenToClient(*treeview, &wpt);
        //::GetClientRect(*treeview, &rct);
        //if (PtInRect(&rct, wpt)) {
    HINSTANCE hinst = ::GetModuleHandle(NULL);
    HMENU hcm = LoadMenu(hinst, MAKEINTRESOURCE(IDR_CONTEXTPOP));
    if (hcm) {
        HMENU hsm = GetSubMenu(hcm, 0);
        if (hsm) {
            //ClientToScreen(hwnd, &pnt);
            BOOL bst = TrackPopupMenu(hsm, TPM_LEFTALIGN | TPM_LEFTBUTTON, pnt.x, pnt.y, 0, hWnd, NULL);
            DestroyMenu(hsm);
        }
    }
    //}
    //}

    return 0;
}

int WindowViewVT::Tree_RemoveTorrent(TorrentGroupVT* group, TorrentNodeVT* node)
{
    ViewNode* vnd = GetGroupViewNode(group, false);
    if (vnd) {
        ViewNode* tvd = vnd->GetViewNode(node);
        if (tvd) {
			if (treeviewnodemap.find(tvd) != treeviewnodemap.end()) {
				HTREEITEM hnt = treeviewnodemap[tvd];
					TreeView_DeleteItem(hTree, hnt);
					vnd->RemoveNode(tvd);
					treeviewnodemap.erase(tvd);
			}
        }
    }
    return 0;
}

int WindowViewVT::ViewProcRemoveTorrent(TorrentClientRequest* req, TASKPARM* parm)
{
    TorrentNodeVT* node = parm->node;
    TorrentGroupVT* group = parm->group;
    delete parm;


	ViewNode* vnd = GetGroupViewNode(group, false);
	if (vnd) {
		ViewNode* tvd = vnd->GetViewNode(node);
		if (tvd) {
			list->RemoveTorrent(tvd);
		}
	}
	Tree_RemoveTorrent(group, node);
	return 0;
}

int WindowViewVT::ViewProcRibbonCommand(ULONG cmd)
{
	switch (cmd) {
	case cmdAddTorrent:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_FILE_ADDNEW, 0), 0);
		break;
	case cmdPauseTorrent:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_CONTEXT_PAUSE, 0), 0);
		break;
	case cmdResumeTorrent:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_CONTEXT_RESUME, 0), 0);
		break;
	case cmdDeleteTorrent:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_CONTEXT_DELETE, 0), 0);
		break;
	case cmdDeleteContentTorrent:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_CONTEXT_DELETEDATA, 0), 0);
		break;
	case cmdExit:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_EXIT, 0), 0);
		break;
	case cmdUpperNode:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_SELECTPARENT, 0), 0);
		break;
	case cmdVerifyTorrent:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_CONTEXT_VERIFY, 0), 0);
		break;
	case cmdProfile:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_PROCPROFILE, 0), 0);
		break;
	case cmdSetLocation:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_CONTEXT_SETLOCATION, 0), 0);
		break;
	case cmdStartDaemon:
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(ID_FILE_STARTDAEMON, 0), 0);
		break;
	}
	return 0;
}
LRESULT WindowViewVT::WndInstanceProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lst = 0;

	switch (message)
	{
	case WM_COMMAND:
		ViewProcCommand(hwnd, message, wParam, lParam);
		break;
	case WM_PAINT:
		ViewProcPaint(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CONTEXTMENU:
		ViewProcContextMenu((HWND)wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_U_LIST_ACTIVATENODE:
	{
		ViewNode* vnode = (ViewNode*)wParam;
		std::map<ViewNode*, HTREEITEM>::iterator itvt = treeviewnodemap.find(vnode);
		if (itvt != treeviewnodemap.end()) {
			TreeView_Select(hTree, itvt->second, TVGN_CARET);
			TreeView_Select(hTree, itvt->second, TVGN_FIRSTVISIBLE);
		}
	}
	break;
	case WM_U_REQUESTERROR:
	{
		WCHAR* msg = (WCHAR*)wParam;
		long ecode = (long)lParam;
		ViewLog(msg);
		free(msg);
		if (ecode == 1) {
			ViewProcProfile();
		}
	}
	break;
	case WM_U_INITWINDOWVIEW:
		ViewProcInitiate();
		list->Initialize();
		::PostMessage(hwnd, WM_U_PROCREFRESH, 0, 0);
		::PostMessage(hwnd, WM_SIZE, 0, 0);  // size correction for behaviour in Windows7
		break;
	case WM_U_UPDATENODEFILE:
		ViewUpdateNodeFile((TorrentClientRequest*)wParam, (TASKPARM*)lParam);
		break;
	case WM_U_UPDATENODEFILESTATUS:
		ViewUpdateNodeFileStatus((TorrentClientRequest*)wParam, (TASKPARM*)lParam);
		break;
	case WM_U_UPDATEGROUP:
		ViewUpdateGroup((TorrentClientRequest*)wParam, (TASKPARM*)lParam);
		break;
	case WM_U_UPDATETORRENT:
		ViewUpdateTorrent((TorrentClientRequest*)wParam, (TASKPARM*)lParam);
		break;
	case WM_U_REMOVETORRENT:
		ViewProcRemoveTorrent((TorrentClientRequest*)wParam, (TASKPARM*)lParam);
		break;
	case WM_U_UPDATEVIEWNODE:
		ViewProcRequestUpdateViewNode((ViewNode*)wParam);
		break;
	//case WM_U_LISTADDGROUPNODE:
	//	list->AddTorrentListNode((TorrentClientRequest*)wParam, (TorrentNodeVT*)lParam);
	//	break;
	case WM_U_LISTADDGROUPGROUP:
	{
		TASKPARM* parm = (TASKPARM*)lParam;
		TorrentGroupVT* group = parm->group;
		delete parm;

		list->AddGroupGroup((TorrentClientRequest*)wParam, group);
	}
	break;
	//case WM_U_UPDATELISTFILE:
	//{
	//	TASKPARM* parm = (TASKPARM*)lParam;
	//	std::vector<TorrentNodeFile*>* flist = parm->filelist;
	//	delete parm;
	//	list->AddNodeFile((TorrentClientRequest*)wParam, flist);
	//}
 //       break;
    case WM_U_ADDNEWTORRENTLINK:
        ListAdd_NewTorrentRawLines((WCHAR*)wParam);
        break;
    case WM_U_VIEWLOGMESSAGE:
        ViewLog((WCHAR*)wParam);
        break;
    case WM_U_STARTREFRESHTIMER:
        SetTimer(hWnd, TID_REFRESH_TORRENTS, (UINT)(wParam==0?2200:wParam), NULL);
        break;
	case WM_U_REQUESTREFRESHSESSION:
		ViewRequestRefreshSession();
		break;
    case WM_SIZE: 
        ResizeContents(hwnd);
        break;
    case WM_NOTIFY:
        lst = ProcNotify(hwnd, message, wParam, lParam);
        break;
    case WM_CLIPBOARDUPDATE:
        ViewProcClipboardUpdate();
        break;
    case WM_NOTIFYFORMAT:
        if (lParam == NF_QUERY) {
            lst = NFR_UNICODE;
        }
        break;
    case WM_TIMER:
		switch (wParam) {
		case TID_REFRESH_TORRENTS:
			KillTimer(hwnd, TID_REFRESH_TORRENTS);
			ViewRequestClientRefresh(true, false);
			break;
		}
        break;
    case WM_U_PROCREFRESH:
        ViewRequestClientRefresh(false, false);
        break;
	case WM_U_PROCREFRESHSESSION:
		ViewProcRefreshSession((SessionInfo*)wParam, (TorrentClientRequest*)lParam);
		break;
	case WM_U_RIBBONCOMMAND:
		ViewProcRibbonCommand(wParam);
		break;
	case WM_U_RESIZECONTENT:
		ResizeContents(hWnd);
		break;
    default:
        lst = DefWindowProc(hwnd, message, wParam, lParam);
    }

    return lst;
}

int WindowViewVT::ViewLog(const std::wstring& logmsg)
{
    if (hLog) {
        SYSTEMTIME stm;
        WCHAR* lbuf;
        size_t lbz = (logmsg.length() + 128) * sizeof(WCHAR);
        lbuf = (WCHAR*)malloc(lbz);
        ::GetLocalTime(&stm);
        wsprintf(lbuf, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] : %s"
            , stm.wYear, stm.wMonth, stm.wDay
            , stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds
            , logmsg.c_str());
        ListBox_InsertString(hLog, 0, lbuf);
		
        free(lbuf);
        panel->SwitchWindow(hLog);
    }

    return 0;
}

void WindowViewVT::ResizeContents(const HWND& hWnd)
{
    if (hContent)
    {
        RECT rct;
        ::GetClientRect(hWnd, &rct);
		if (ribbonFramework) {
			IUIRibbon* pview;
			ribbonFramework->GetView(0, __uuidof(IUIRibbon), (void**)&pview);
			if (pview) {
				UINT32 uht;
				HRESULT hr = pview->GetHeight(&uht);
				if (SUCCEEDED(hr)) {
					rct.top += uht;
				}
			}
		}
		::MoveWindow(hContent, rct.left, rct.top, rct.right - rct.left, rct.bottom - rct.top, FALSE);
    }
}

int WindowViewVT::InitializeStatusBar(HWND hstat)
{
	int pars[4] = { 150, 300, 400, -1 };
	::SendMessage(hstat, SB_SETPARTS, 4, (LPARAM)&pars);

	return 0;
}

int WindowViewVT::InitializeToolbar(HWND htool)
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
    HIMAGELIST hToolImg = ImageList_LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BMPTOOLIMG), bitmapSize, 5, CLR_DEFAULT, IMAGE_BITMAP, LR_DEFAULTSIZE);

    // Set the image list.
    SendMessage(htool, TB_SETIMAGELIST, (WPARAM)0, (LPARAM)hToolImg);

    // Load the button images.
    //SendMessage(htool, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
    //SendMessage(htool, TB_LOADIMAGES, (WPARAM)IDB_BMPTOOLIMG, (LPARAM)GetModuleHandle(NULL));

    // Add buttons.
    SendMessage(htool, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(htool, TB_ADDBUTTONS, (WPARAM)nbtn, (LPARAM)&tbButtons);

    // Resize the toolbar, and then show it.
    SendMessage(htool, TB_AUTOSIZE, 0, 0);
    ShowWindow(htool, TRUE);

    return 0;
}

int List_ClearList(HWND hlist)
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

int List_CreateColumns(HWND hlist, struct ColDefVT* cols)
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

int WindowViewVT::InitializeLocationListView(HWND hlist, HWND hedit, HWND hbtn)
{
    List_ClearList(hlist);
    ListView_SetExtendedListViewStyle(hlist, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

    HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    ::SendMessage(hlist, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hedit, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hbtn, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));

    struct ColDefVT cols[] = {
    { L"Input", 150, LISTSORTCOL::NONE }
    , { L"Value", 500, LISTSORTCOL::NONE }
    , {0}
    };

    List_CreateColumns(hlist, cols);
    HWND hhw = ListView_GetHeader(hlist);
    if (hhw) {
        LONG_PTR hws = GetWindowLongPtr(hhw, GWL_STYLE);
        hws |= HDS_FLAT;
        hws &= ~HDS_BUTTONS;
        SetWindowLongPtr(hhw, GWL_STYLE, hws);
    }

	LVITEM lvi;
	lvi.mask = LVIF_TEXT;
	lvi.pszText = (LPWSTR)L"Set Location";
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	lvi.iSubItem = 0;
	lvi.iItem = ListView_GetItemCount(hlist);

	ListView_InsertItem(hlist, &lvi);
	ListLocation_LocateButtons(hlist);

    return 0;
}

int WindowViewVT::ListLocation_LocateButtons(HWND hlist) {
	if (hLocationSet) {
		RECT rc;
		ListView_GetSubItemRect(hlist, 0, 1, LVIR_BOUNDS, &rc);
		ListView_RedrawItems(hlist, 0, 0);
		BringWindowToTop(hLocationSet);
		MoveWindow(hLocationSet, rc.left, rc.top, 100, rc.bottom - rc.top, TRUE);
	}
	return 0;
}

int WindowViewVT::InitializeProfileListView(HWND hlist, HWND hedit, HWND hbtn, HWND hdmbtn)
{
	List_ClearList(hlist);
	ListView_SetExtendedListViewStyle(hlist, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

	HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	::SendMessage(hlist, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hedit, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hbtn, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hdmbtn, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));

	struct ColDefVT cols[] = {
	{ L"Location", 150 }
	, { L"Path", 500 }
	, {0}
	};

	List_CreateColumns(hlist, cols);
	HWND hhw = ListView_GetHeader(hlist);
	if (hhw) {
		LONG_PTR hws = GetWindowLongPtr(hhw, GWL_STYLE);
		hws |= HDS_FLAT;
		hws &= ~HDS_BUTTONS;
		SetWindowLongPtr(hhw, GWL_STYLE, hws);
	}

	LVITEM lvi;
	lvi.mask = LVIF_TEXT;
	lvi.pszText = (LPWSTR)L"Update Profile";
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	lvi.iSubItem = 0;
	lvi.iItem = ListView_GetItemCount(hlist);
	ListView_InsertItem(hlist, &lvi);

	lvi.pszText = (LPWSTR)L"Enable Local Daemon";
	lvi.cchTextMax = 0;
	lvi.iItem = ListView_GetItemCount(hlist);
	ListView_InsertItem(hlist, &lvi);

	if (hbtn) {
		RECT rc;
		ListView_GetSubItemRect(hlist, 0, 1, LVIR_BOUNDS, &rc);
		ListView_RedrawItems(hlist, 0, 0);
		BringWindowToTop(hbtn);
		MoveWindow(hbtn, rc.left, rc.top, 100, rc.bottom - rc.top, TRUE);
	}

	if (hdmbtn) {
		RECT rc;
		ListView_GetSubItemRect(hlist, 1, 1, LVIR_BOUNDS, &rc);
		ListView_RedrawItems(hlist, 1, 1);
		BringWindowToTop(hdmbtn);
		MoveWindow(hdmbtn, rc.left, rc.top, 100, rc.bottom - rc.top, TRUE);
	}

	return 0;
}

int WindowViewVT::InitializeAddListView(HWND hlist)
{
    List_ClearList(hlist);
    ListView_SetExtendedListViewStyle(hlist, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

    HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    ::SendMessage(hlist, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));

    struct ColDefVT cols[] = {
    { L"Action", 150 }
    , { L"Item", 500 }
    , {0}
    };

    List_CreateColumns(hlist, cols);
    HWND hhw = ListView_GetHeader(hlist);
    if (hhw) {
        LONG_PTR hws = GetWindowLongPtr(hhw, GWL_STYLE);
        hws |= HDS_FLAT;
        hws &= ~HDS_BUTTONS;
        SetWindowLongPtr(hhw, GWL_STYLE, hws);
    }

    LVITEM lvi;
    lvi.mask = LVIF_TEXT;
    lvi.pszText = (LPWSTR)L"Add Torrents";
    lvi.cchTextMax = (int)wcslen(lvi.pszText);
    lvi.iSubItem = 0;
    lvi.iItem = ListView_GetItemCount(hlist);

    ListView_InsertItem(hlist, &lvi);
    ListAdd_LocateSubmitButton();
    return 0;
}

int WindowViewVT::ListAdd_LocateSubmitButton() {
    if (hAddButton) {
        RECT rc;
        ListView_GetSubItemRect(hAddTorrent, 0, 1, LVIR_BOUNDS, &rc);
        ListView_RedrawItems(hAddTorrent, 0, 0);
        BringWindowToTop(hAddButton);
        MoveWindow(hAddButton, rc.left, rc.top, 100, rc.bottom - rc.top, TRUE);
    }
    if (hAddClear) {
        RECT rc;
        ListView_GetSubItemRect(hAddTorrent, 0, 1, LVIR_BOUNDS, &rc);
        ListView_RedrawItems(hAddTorrent, 0, 0);
        BringWindowToTop(hAddButton);
        MoveWindow(hAddClear, rc.left + 100 + 2, rc.top, 100, rc.bottom - rc.top, TRUE);
    }
    return 0;
}
int WindowViewVT::InitializeListView(HWND hlist)
{
    ListView_SetExtendedListViewStyle(hlist, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    HWND hhd = ListView_GetHeader(hlist);
    if (hhd) {
        LONG_PTR lps = GetWindowLongPtr(hhd, GWL_STYLE);
        lps |= HDS_FLAT;
        //lps &= ~HDS_BUTTONS;
        SetWindowLongPtr(hhd, GWL_STYLE, lps);
    }

    HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    ::SendMessage(hlist, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));

    return 0;
}

class DropTargetWindowsView : public IDropTarget
{
    static ULONG refcount;
    HWND hWnd;
public:
    DropTargetWindowsView(HWND hwnd) {
        hWnd = hwnd;
    }
    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        refcount++;
        return refcount;
    }
    virtual ULONG STDMETHODCALLTYPE Release()
    {
        refcount--;
        if (refcount == 0) {
            delete this;
            return 0;
        }
        return refcount;
    }
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
    {
        if (riid == IID_IUnknown) {
            AddRef();
            *ppvObject = this;
        }
        else if (riid == IID_IDropTarget) {
            AddRef();
            *ppvObject = this;
        }
        else {
            *ppvObject = NULL;
            return E_NOINTERFACE;
        }
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE DragEnter(
        /* [unique][in] */ __RPC__in_opt IDataObject* pDataObj,
        /* [in] */ DWORD grfKeyState,
        /* [in] */ POINTL pt,
        /* [out][in] */ __RPC__inout DWORD* pdwEffect)
    {
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE DragOver(
        /* [in] */ DWORD grfKeyState,
        /* [in] */ POINTL pt,
        /* [out][in] */ __RPC__inout DWORD* pdwEffect)
    {
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE DragLeave(void)
    {
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Drop(
        /* [unique][in] */ __RPC__in_opt IDataObject* pDataObj,
        /* [in] */ DWORD grfKeyState,
        /* [in] */ POINTL pt,
        /* [out][in] */ __RPC__inout DWORD* pdwEffect)
    {
        FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium;

        HRESULT hr = pDataObj->GetData(&fmte, &medium);
        if (SUCCEEDED(hr))
        {
            UINT const count = DragQueryFile((HDROP)medium.hGlobal, -1, NULL, 0);
            WCHAR* szPath;
            int cpc;
            for (unsigned int ii = 0; ii < count; ii++) {
                szPath = new WCHAR[MAX_PATH];
                cpc = DragQueryFile((HDROP)medium.hGlobal, ii, szPath, MAX_PATH);
                if (cpc > 0) {
                    PostMessage(hWnd, WM_U_ADDNEWTORRENTLINK, (WPARAM)szPath, 0);
                }
                else {
                    delete szPath;
                }
            }
            ReleaseStgMedium(&medium);
            return S_OK;
        }

        fmte = { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        hr = pDataObj->GetData(&fmte, &medium);
        if (SUCCEEDED(hr))
        {
            WCHAR* szPath;
            void* vpd = GlobalLock(medium.hGlobal);
            if (vpd) {
                size_t tln = wcslen((WCHAR*)vpd);
                szPath = new WCHAR[tln + 1];
                wcscpy_s(szPath, tln + 1, (WCHAR*)vpd);
                PostMessage(hWnd, WM_U_ADDNEWTORRENTLINK, (WPARAM)szPath, 0);
           }
            GlobalUnlock(medium.hGlobal);
            ReleaseStgMedium(&medium);
            return S_OK;
        }

        return S_OK;
    }

};

ULONG DropTargetWindowsView::refcount = 0;

int WindowViewVT::InitializeDragDrop(HWND hwnd)
{
    IDropTarget* idt = new DropTargetWindowsView(hwnd);
    HRESULT hr = RegisterDragDrop(hwnd, idt);
    if (hr == S_OK) {
        pdrop = idt;
    }
    else {
        delete idt;
    }
    return hr==S_OK?0:-1;
}

class WVVTRobbinHandler : public IUICommandHandler
{
	HWND hParent;
	unsigned long refcnt;
	unsigned int callbackMessage;
public:
	WVVTRobbinHandler(HWND hwnd, UINT cbMsg)
		: hParent(hwnd)
		, refcnt(0)
		, callbackMessage(cbMsg)
	{}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
	{
		HRESULT hr = S_OK;

		if (((riid == __uuidof(IUnknown))
			|| (riid == __uuidof(IUICommandHandler)))) {
			AddRef();
			*ppvObject = this;
		}
		else {
			*ppvObject = NULL;
			hr = E_NOINTERFACE;
		}
		return hr;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return InterlockedIncrement(&refcnt);
	}
	virtual ULONG STDMETHODCALLTYPE Release()
	{
		ULONG rct = InterlockedDecrement(&refcnt);
		if (rct == 0) {
			delete this;
		}
		return rct;
	}
	virtual /* [annotation] */
		_Check_return_
		HRESULT STDMETHODCALLTYPE Execute(
			UINT32 commandId,
			UI_EXECUTIONVERB verb,
			/* [annotation][in] */
			_In_opt_  const PROPERTYKEY *key,
			/* [annotation][in] */
			_In_opt_  const PROPVARIANT *currentValue,
			/* [annotation][in] */
			_In_opt_  IUISimplePropertySet *commandExecutionProperties)
	{
		switch (verb) {
		case UI_EXECUTIONVERB_EXECUTE:
			PostMessage(hParent, callbackMessage, commandId, 0);
			break;
		}
		return S_OK;
	}

	virtual /* [annotation] */
		_Check_return_
		HRESULT STDMETHODCALLTYPE UpdateProperty(
			UINT32 commandId,
			/* [in] */ REFPROPERTYKEY key,
			/* [annotation][in] */
			_In_opt_  const PROPVARIANT *currentValue,
			/* [out] */ PROPVARIANT *newValue)
	{
		return E_NOTIMPL;
	}

};


class WVVTRobbinApplication : public IUIApplication
{
	unsigned long refcnt;
	HWND hParent;
	WVVTRobbinHandler* handler;
	unsigned int callbackMessage;
public:
	WVVTRobbinApplication(HWND hwnd, UINT cbMsg)
		:hParent(hwnd)
		, handler(NULL)
		, refcnt(0)
		, callbackMessage(cbMsg)
	{}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
	{
		HRESULT hr = S_OK;

		if (((riid == __uuidof(IUnknown))
			|| (riid == __uuidof(IUIApplication)))) {
			AddRef();
			*ppvObject = this;
		}
		else {
			*ppvObject = NULL;
			hr = E_NOINTERFACE;
		}
		return hr;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return InterlockedIncrement(&refcnt);
	}

	virtual ULONG STDMETHODCALLTYPE Release()
	{
		ULONG rct = InterlockedDecrement(&refcnt);
		if (rct == 0) {
			delete this;
		}
		return rct;
	}
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged(
		UINT32 viewId,
		UI_VIEWTYPE typeID,
		/* [in] */ IUnknown *view,
		UI_VIEWVERB verb,
		INT32 uReasonCode)
	{
		switch (verb) {
		case UI_VIEWVERB_SIZE:
			PostMessage(hParent, WM_U_RESIZECONTENT, 0, 0);
			break;
		}
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand(
		UINT32 commandId,
		UI_COMMANDTYPE typeID,
		/* [out] */ IUICommandHandler **commandHandler)
	{
		HRESULT hr = S_OK;
		if (handler == NULL) {
			handler = new WVVTRobbinHandler(hParent, callbackMessage);
		}

		if (handler) {
			hr = handler->QueryInterface(__uuidof(IUICommandHandler), (void**)commandHandler);
		}
		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand(
		UINT32 commandId,
		UI_COMMANDTYPE typeID,
		/* [annotation][in] */
		_In_opt_  IUICommandHandler *commandHandler)
	{
		if (commandHandler) {
			commandHandler->Release();
		}
		return S_OK;
	}
};

int WindowViewVT::InitializeRobbin(HINSTANCE hinst, HWND hwnd)
{
	HRESULT hr = ::CoCreateInstance(
		CLSID_UIRibbonFramework,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&ribbonFramework));
	if (SUCCEEDED(hr))
	{
		WVVTRobbinApplication* papp = new WVVTRobbinApplication(hwnd, WM_U_RIBBONCOMMAND);
		hr = ribbonFramework->Initialize(hwnd, papp);
	}
	if (SUCCEEDED(hr))
	{
		hr = ribbonFramework->LoadUI(hinst, L"SIMPLEVISUALTRANS_RIBBON");
		ribbonFramework->FlushPendingInvalidations();
	}
	return 0;
}

LRESULT WindowViewVT::ViewProcInitiate()
{
    HRESULT hr = OleInitialize(NULL);
    InitCommonControls();
	InitializeRobbin(GetModuleHandle(NULL), hWnd);

    //////////////////// Create status split
    splitStatus = CSplitWnd::CreateSplitWindow(hWnd);
    ::ShowWindow(*splitStatus, SW_SHOW);
    splitStatus->style = CSplitWnd::SPW_STYLE::DOCKDOWN;

    //splitTool = CSplitWnd::CreateSplitWindow(*splitStatus);
	splitLog = CSplitWnd::CreateSplitWindow(*splitStatus);
    hStatus = CreateWindow(STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 200, 10, *splitStatus, NULL, hInstance, NULL);

    //splitStatus->SetWindow(*splitTool);
	splitStatus->SetWindow(*splitLog);
    splitStatus->SetWindow(hStatus);

    /////////////////////// Create Toolbar
    
    //hTool = CreateWindow(TOOLBARCLASSNAME, L"", WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT, 0, 0, 200, 16, *splitTool, NULL, hInstance, NULL);

    //splitTool->style = CSplitWnd::SPW_STYLE::DOCKTOP;
    //splitTool->fixedSize = 28;
    //splitTool->SetWindow(hTool);
    //splitTool->SetWindow(*splitLog);

    /////////////////////// Create LogList
    splitTree = CSplitWnd::CreateSplitWindow(*splitLog);
    panel = CPanelWindow::CreatePanelWindow(*splitLog);

    splitLog->style = CSplitWnd::SPW_STYLE::TOPDOWN;
    splitLog->SetWindow(*splitTree);
    splitLog->SetWindow(*panel);
    splitLog->SetRatio(0.8);

    hLog = CreateWindow(WC_LISTBOX, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_VISIBLE | LBS_NOINTEGRALHEIGHT, 0, 0, 200, 10, *panel, NULL, hInstance, NULL);
    hProfile = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | LVS_REPORT | WS_VISIBLE | LVS_SHOWSELALWAYS | LVS_SINGLESEL, 0, 0, 200, 10, *panel, NULL, hInstance, NULL);
	hLocation = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | LVS_REPORT | WS_VISIBLE | LVS_SHOWSELALWAYS | LVS_SINGLESEL, 0, 0, 200, 10, *panel, NULL, hInstance, NULL);
    hAddTorrent = CreateWindowW(WC_LISTVIEW, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | LVS_REPORT | WS_VISIBLE | LVS_SHOWSELALWAYS | LVS_SINGLESEL, 0, 0, 200, 10, *panel, NULL, hInstance, NULL);
    hProfileEdit = CreateWindow(WC_EDIT, L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 0, 100, 20, *panel, (HMENU)IDC_GRIDINPUT_EDIT, hInstance, NULL);
	hProfileBtn = CreateWindow(WC_BUTTON, L"Connect", WS_CHILD | WS_VISIBLE | BS_FLAT, 0, 0, 100, 20, hProfile, (HMENU)IDC_GRIDINPUT_CONNECT, hInstance, NULL);
	hProfileDaemonBtn = CreateWindow(WC_BUTTON, L"Enable Daemon", WS_CHILD | WS_VISIBLE | BS_FLAT | BS_CHECKBOX, 0, 0, 100, 20, hProfile, (HMENU)IDC_GRIDINPUT_DAEMON, hInstance, NULL);
	hLocationEdit = CreateWindow(WC_EDIT, L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 0, 100, 20, *panel, (HMENU)IDC_GRIDLOCATION_EDIT, hInstance, NULL);
	hLocationSet = CreateWindow(WC_BUTTON, L"Set", WS_CHILD | WS_VISIBLE | BS_FLAT, 0, 0, 100, 20, hLocation, (HMENU)IDC_GRIDLOCATION_SETLOCATION, hInstance, NULL);
    hAddButton = CreateWindow(WC_BUTTON, L"Submit", WS_CHILD | WS_VISIBLE | BS_FLAT, 0, 0, 100, 20, hAddTorrent, (HMENU)IDC_GRIDADD_BUTTON, hInstance, NULL);
    hAddClear = CreateWindow(WC_BUTTON, L"Clear", WS_CHILD | WS_VISIBLE | BS_FLAT, 0, 0, 100, 20, hAddTorrent, (HMENU)IDC_GRIDADD_CLEAR, hInstance, NULL);
    hAddHeader = ListView_GetHeader(hAddTorrent);
    SetParent(hProfileEdit, hProfile);
    SetParent(hLocationEdit, hLocation);
    BringWindowToTop(hProfileEdit);
    BringWindowToTop(hAddButton);
    BringWindowToTop(hAddClear);
    ShowWindow(hProfileEdit, SW_HIDE);
	ShowWindow(hLocationEdit, SW_HIDE);
    panel->PushWindow(hLog);
    panel->PushWindow(hProfile);
	panel->PushWindow(hLocation);
    panel->PushWindow(hAddTorrent);

    lvAddOriProc = (WNDPROC)GetWindowLongPtr(hAddTorrent, GWLP_WNDPROC);
    lvAddOriProc = (WNDPROC)SetWindowLongPtr(hAddTorrent, GWLP_WNDPROC, (LONG_PTR)&ListViewAddSubProc);
	lvLocationOriProc = (WNDPROC)GetWindowLongPtr(hLocation, GWLP_WNDPROC);
	lvLocationOriProc = (WNDPROC)SetWindowLongPtr(hLocation, GWLP_WNDPROC, (LONG_PTR)&ListViewAddSubProc);
	lvProfileOriProc = (WNDPROC)GetWindowLongPtr(hProfile, GWLP_WNDPROC);
	lvProfileOriProc = (WNDPROC)SetWindowLongPtr(hProfile, GWLP_WNDPROC, (LONG_PTR)&ListViewAddSubProc);
	edProfileOriProc = (WNDPROC)GetWindowLongPtr(hProfileEdit, GWLP_WNDPROC);
	edProfileOriProc = (WNDPROC)SetWindowLongPtr(hProfileEdit, GWLP_WNDPROC, (LONG_PTR)&EditBoxKeypressSubProc);

    /////////////////////// Create TreeView
    hTree = CreateWindow(WC_TREEVIEW, L"", WS_CHILD | WS_VSCROLL | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_HASLINES, 0, 0, 100, 100, *splitTree, NULL, hInstance, NULL);
    HWND hlist = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | LVS_REPORT | WS_VISIBLE | LVS_OWNERDATA, 0, 0, 200, 10, *splitTree, NULL, hInstance, NULL);
	list = new FrameListView(hlist);

    splitTree->style = CSplitWnd::SPW_STYLE::LEFTRIGHT;
    splitTree->SetWindow(hTree);
    splitTree->SetWindow(hlist);
    splitTree->SetRatio(0.3);

    /////////////////////// Initiate Tool Bar
    //InitializeToolbar(hTool);
	InitializeStatusBar(hStatus);
    InitializeListView(hlist);
    InitializeProfileListView(hProfile, hProfileEdit, hProfileBtn, hProfileDaemonBtn);
	panel->SwitchWindow(hLocation);
	InitializeLocationListView(hLocation, hLocationEdit, hLocationSet);
    panel->SwitchWindow(hAddTorrent);
    InitializeAddListView(hAddTorrent);
 	//panel->SwitchWindow(hLocation);
	panel->SwitchWindow(hLog);
	HINSTANCE hinst = ::GetModuleHandle(NULL);
	//InitializeRobbin(hinst, hWnd);

    /////////////////////////// Setup windows font
    HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    //	::SendMessage(hList, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
    ::SendMessage(hLog, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
    ::SendMessage(hStatus, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
    ::SendMessage(hTree, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
    ::SendMessage(hlist, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
    ::SendMessage(hProfileEdit, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
    ::SendMessage(hAddTorrent, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
    ::SendMessage(hAddButton, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
    ::SendMessage(hAddClear, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));

    /////////////////////// Refresh Window
    hContent = *splitStatus;
    ResizeContents(hWnd);

    /////////////////////// Setup drag and drop
    InitializeDragDrop(hWnd);

    /////////////////////// Setup Clipboard
    AddClipboardFormatListener(hWnd);
    listenToClipboard = true;

    return 0;
}

int WindowViewVT::ParmView_Profile(CProfileData* prof)
{
	InitializeProfileListView(hProfile, hProfileEdit, hProfileBtn, hProfileDaemonBtn);

    panel->SwitchWindow(hProfile);
    parmconst.type = PARMTYPE::PROFILE;
    parmconst.profile = prof;

	std::wstring url;
	std::wstring user;
	std::wstring passwd;
	prof->GetDefaultValue(L"RemoteUrl", url);
	prof->GetDefaultValue(L"RemoteUser", user);
	prof->GetDefaultValue(L"RemotePass", passwd);

    LVITEM lvi = { 0 };
	long lvc = ListView_GetItemCount(hProfile) - 1;
    lvi.mask = LVIF_TEXT;
    lvi.iSubItem = 0;
	lvi.cchTextMax = 0;

	lvi.pszText = (LPWSTR)L"Remote URL";
	lvi.iItem = lvc + 1;
	lvc = ListView_InsertItem(hProfile, &lvi);
	ListView_SetItemText(hProfile, lvc, 1, (LPWSTR)url.c_str());

	lvi.pszText = (LPWSTR)L"Remote Username";
    lvi.iItem = lvc + 1;
    lvc = ListView_InsertItem(hProfile, &lvi);
	ListView_SetItemText(hProfile, lvc, 1, (LPWSTR)user.c_str());

	lvi.pszText = (LPWSTR)L"Remote Password";
    lvi.iItem = lvc + 1;
    lvc = ListView_InsertItem(hProfile, &lvi);
    ListView_SetItemText(hProfile, lvc, 1, (LPWSTR)passwd.c_str());

    return 0;
}

int WindowViewVT::ViewProcProfile()
{
    if (client) {
        if (client->profiledataloader) {
            ParmView_Profile(client->profiledataloader);
        }
    }
    return 0;
}

int WindowViewVT::ListProfile_Connect(HWND hlist)
{
    if (parmconst.type == PARMTYPE::PROFILE) {
        WCHAR tbuf[1024];
        ListView_GetItemText(hlist, 1, 1, tbuf, 1024);
        std::wstring url(tbuf);
        ListView_GetItemText(hlist, 2, 1, tbuf, 1024);
        std::wstring user(tbuf);
        ListView_GetItemText(hlist, 3, 1, tbuf, 1024);
        std::wstring passwd(tbuf);

        if (parmconst.profile) {
            parmconst.profile->SetDefaultValue(L"RemoteUrl", url);
            parmconst.profile->SetDefaultValue(L"RemoteUser", user);
            parmconst.profile->SetDefaultValue(L"RemotePass", passwd);
            parmconst.profile->StoreProfile();
            client->LoadProfileSite();
            PostMessage(hWnd, WM_U_PROCREFRESH, 0, 0);
            ViewLog(L"[success] Update profile");
        }
    }
    return 0;
}

int WindowViewVT::ViewProcNewTorrentsRequest(const std::wstring& reqlink)
{
    TorrentClientRequest* tcr = new TorrentClientRequest();
    tcr->req = TorrentClientRequest::REQ::ADDNEWTORRENT;
    tcr->addtorrent.hwnd = hWnd;
    int rrc = (int)reqlink.length() + 1;
    tcr->addtorrent.torrentlink = new WCHAR[rrc + 1];
    wcscpy_s(tcr->addtorrent.torrentlink, rrc, reqlink.c_str());
    tcr->addtorrent.cbaddresult = [](TorrentClientRequest* req, int rcode) {
        WCHAR* wbuf;
        wbuf = new WCHAR[wcslen(req->addtorrent.torrentlink) + 128];
        if (req->addtorrent.result) {
            wsprintf(wbuf, L"[%s] add [%s]", req->addtorrent.result, req->addtorrent.torrentlink);
            delete req->addtorrent.result;
        }
        else {
            if (rcode == 0) {
                wsprintf(wbuf, L"Success add [%s]", req->addtorrent.torrentlink);
            }
            else {
                wsprintf(wbuf, L"Failed add [%s]", req->addtorrent.torrentlink);
            }
        }
        SendMessageW(req->addtorrent.hwnd, WM_U_VIEWLOGMESSAGE, (WPARAM)wbuf, 0);
        delete wbuf;
        delete req->addtorrent.torrentlink;
        delete req;
        return 0;
    };
    client->PutTask(new TorrentClient::GeneralRequestTask(client, tcr));
    return 0;
}

int WindowViewVT::ListAdd_ClearInList()
{
    InitializeAddListView(hAddTorrent);
    return 0;
}

int WindowViewVT::ListLocation_SetLocation(HWND hlist)
{
	WCHAR wbuf[4096];
	if (hlist) {
		int itc = ListView_GetNextItem(hlist, 0, LVNI_SELECTED);
		if (itc > 0) {
			ListView_GetItemText(hlist, itc, 1, wbuf, 4096);
			ViewProcSetTorrentLocation(wbuf);
		}
	}
	return 0;
}

int WindowViewVT::ListAdd_RequestInList()
{
    WCHAR wbuf[4096];
    if (hAddTorrent) {
        int itc = ListView_GetItemCount(hAddTorrent);
        if (itc > 1) {
            for (int ii = 1; ii < itc; ii++) {
                ListView_GetItemText(hAddTorrent, ii, 1, wbuf, 4096);
                ViewProcNewTorrentsRequest(wbuf);
            }
            InitializeAddListView(hAddTorrent);
        }
        
    }
    return 0;
}

int WindowViewVT::Tree_GetSelectedFiles(std::set<TorrentNodeFile*>& lnodes)
{
    HTREEITEM htv = TreeView_GetSelection(hTree);
    ViewNode* vnd;

    if (htv) {
        TVITEM tvi = { 0 };
        tvi.hItem = htv;
        tvi.mask = TVIF_PARAM;
        BOOL btn = TreeView_GetItem(hTree, &tvi);
        if (btn) {
            vnd = (ViewNode*)tvi.lParam;
            if (vnd) {
                if (vnd->type == VG_TYPE::FILE) {
                    std::vector<TorrentNodeFile*> lfn;
                    vnd->GetFileNodes(lfn);
                    lnodes.insert(lfn.begin(), lfn.end());
                }
            }
        }
    }
    return 0;
}

int WindowViewVT::Tree_GetSelectedNodes(std::set<TorrentNodeVT*>& lnodes)
{
    HTREEITEM htv = TreeView_GetSelection(hTree);
    ViewNode* vnd;

    if (htv) {
        TVITEM tvi = { 0 };
        tvi.hItem = htv;
        tvi.mask = TVIF_PARAM;
        BOOL btn = TreeView_GetItem(hTree, &tvi);
        if (btn) {
            vnd = (ViewNode*)tvi.lParam;
            if (vnd) {
                vnd->GetTorrentNodes(lnodes);
            }
        }
    }
    return 0;
}

int WindowViewVT::ViewProcDeleteTorrent(bool deldata)
{
    std::set<TorrentNodeVT*> dnodes;
    list->GetSelectedNodes(dnodes);
    if (dnodes.size() <= 0) {
        Tree_GetSelectedNodes(dnodes);
    }

    std::for_each(dnodes.begin(), dnodes.end(), [this, deldata](TorrentNodeVT* node) {
        TorrentClientRequest* tcr = new TorrentClientRequest();
        tcr->req = TorrentClientRequest::REQ::DELETETORRENT;
        tcr->deletetorrent.torrentid = node->id;
        tcr->deletetorrent.deletedata = deldata;
        tcr->deletetorrent.node = node;
        tcr->deletetorrent.hwnd = hWnd;
        tcr->deletetorrent.result = NULL;
        tcr->deletetorrent.cbdelresult = [](TorrentClientRequest* req, int rcode) {
            WCHAR wbuf[2048];
            if (req->deletetorrent.result) {
                wsprintf(wbuf, L"[%s] delete [%s]", req->deletetorrent.result, req->deletetorrent.node->name.c_str());
                delete req->deletetorrent.result;
            }
            else {
                if (rcode == 0) {
                    wsprintf(wbuf, L"Success delete [%s]", req->deletetorrent.node->name.c_str());
                }
                else {
                    wsprintf(wbuf, L"Failed delete [%s]", req->deletetorrent.node->name.c_str());
                }
            }
            SendMessageW(req->deletetorrent.hwnd, WM_U_VIEWLOGMESSAGE, (WPARAM)wbuf, 0);
            delete req;
            return 0;
        };
        client->PutTask(new TorrentClient::GeneralRequestTask(client, tcr));

        });

    return 0;
}

int WindowViewVT::ViewProcRefreshSession(SessionInfo* ssi, TorrentClientRequest* req)
{
	if (ssi) {
		WCHAR wbuf[1024];
		FormatNumberByteView(wbuf + 512, 512, ssi->downspeed);
		wsprintf(wbuf, L"Down: %s/s", wbuf + 512);
		::SendMessage(hStatus, SB_SETTEXT, MAKEWPARAM(0, 0), (LPARAM)wbuf);

		FormatNumberByteView(wbuf + 512, 512, ssi->upspeed);
		wsprintf(wbuf, L"Up: %s/s", wbuf + 512);
		::SendMessage(hStatus, SB_SETTEXT, MAKEWPARAM(1, 0), (LPARAM)wbuf);
	}

	if (req) {
		delete req;
	}
	return 0;
}

int WindowViewVT::ViewRequestRefreshSession()
{
	TorrentClientRequest* tcr = new TorrentClientRequest();
	tcr->req = TorrentClientRequest::REQ::REFRESHSESSION;
	tcr->session.hwnd = hWnd;
	tcr->session.cbsessionresult = [](TorrentClientRequest* req, SessionInfo* session) {
		PostMessage(req->session.hwnd, WM_U_PROCREFRESHSESSION, (WPARAM)session, (LPARAM)req);
		return 0;
	};
	client->PutTask(new TorrentClient::GeneralRequestTask(client, tcr));

	return 0;
}

DWORD WINAPI ThWindowViewVTConsoleProc(_In_ LPVOID lParam)
{
    WindowViewVT* self = (WindowViewVT*)lParam;
    bool keeprun = true;
    bool keepseek;
    int size = 4096;
    CHAR* wbuf = new CHAR[size + 1];
    BOOL brd;
    DWORD dread;
    std::wstring wtr;
    std::wstringstream wss;
    DWORD rpos;
    DWORD spos;

    while (keeprun) {
        brd = ReadFile(self->console.hErrRead, wbuf, size, &dread, NULL);
        if (dread > 0) {
            wss.str(wtr);
            keepseek = true;
            rpos = 0;
            spos = 0;
            while (keepseek) {
                if (wbuf[rpos] == '\n') {
                    wbuf[rpos] = 0;
                    wss << wbuf + spos;
                    spos = rpos + 1;

                    wtr = wss.str();
                    wtr.erase(std::remove(wtr.begin(), wtr.end(), L'\r'), wtr.end());
                    wss.str(L"");
                    self->ViewLog(wtr);
                }
                rpos++;
                keepseek = rpos < dread ? keepseek : false;
            }
            if (rpos > spos) {
                wbuf[rpos] = 0;
                wss << wbuf + spos;
            }
        }
        /*
                dread = WaitForSingleObject(self->console.hOutRead, 100);
                if (dread == WAIT_OBJECT_0) {
                    brd = ReadFile(self->console.hOutRead, wbuf, size * sizeof(CHAR), &dread, NULL);
                    if (dread > 0) {
                        wss.str(L"");
                        wbuf[dread] = 0;
                        wss << wbuf;

                        self->ViewLog(wss.str());
                    }
                }
        */
    }

    delete wbuf;
    return 0;
}

int WindowViewVT::ViewProcHandleConsoleProcess()
{
    DWORD tid;
    hThConsole = ::CreateThread(NULL, 0, ThWindowViewVTConsoleProc, this, 0, &tid);
    return 0;
}

int WindowViewVT::ViewProcCreateDaemonProcess()
{
    if (console.psinfo.dwProcessId == 0) {
        //SECURITY_DESCRIPTOR sds = { 0 };
        SECURITY_ATTRIBUTES sat = { 0 };
        sat.nLength = sizeof(SECURITY_ATTRIBUTES);
        sat.bInheritHandle = TRUE;
        sat.lpSecurityDescriptor = NULL;

        BOOL btn = CreatePipe(&console.hInRead, &console.hInWrite, &sat, 0);
        //btn = CreatePipe(&console.hOutRead, &console.hOutWrite, &sat, 0);
        btn = CreatePipe(&console.hErrRead, &console.hErrWrite, &sat, 0);
        
        console.start = { 0 };
        console.start.cb = sizeof(STARTUPINFO);
        console.start.hStdInput = console.hInRead;
        console.start.hStdOutput = console.hErrWrite;
        console.start.hStdError = console.hErrWrite;
        console.start.dwFlags |= STARTF_USESTDHANDLES;

		WCHAR pbf[1024];
		DWORD bsz = GetCurrentDirectory(512, pbf + 512);
		WCHAR daemonapp[] = L"transmission_daemon.exe";
		WCHAR daemonparms[] = L"-f";
		wsprintf(pbf, L"%s\\%s", pbf + 512, daemonapp);
		WCHAR pps[1024];
		wsprintf(pps, L"%s %s", daemonapp, daemonparms);
        btn = ::CreateProcess(
            pbf
            , pps
            , NULL
            , NULL
            , TRUE
//            , CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW
			, CREATE_NEW_PROCESS_GROUP
            , NULL
            , NULL
            , &console.start
            , &console.psinfo);
        if (btn) {
			ViewProcHandleConsoleProcess();
		}
        else {
			console = { 0 };
		}
    }
    return 0;
}

int WindowViewVT::ViewProcSetTorrentLocation(WCHAR* location)
{
	std::set<TorrentNodeVT*> dnodes;
	list->GetSelectedNodes(dnodes);
	if (dnodes.size() <= 0) {
		Tree_GetSelectedNodes(dnodes);
	}
	std::for_each(dnodes.begin(), dnodes.end(), [this, location](TorrentNodeVT* node) {
		int pcp = wcscmp(node->path.fullpath.c_str(), location);
		if (pcp != 0) {
			TorrentClientRequest* tcr = new TorrentClientRequest();
			tcr->req = TorrentClientRequest::REQ::SETLOCATION;
			tcr->location.hwnd = hWnd;
			tcr->location.location = new WCHAR[wcslen(location) + 1];
			tcr->location.node = node;
			wcscpy_s(tcr->location.location, wcslen(location) + 1, location);
			tcr->location.torrentid = node->id;
			tcr->location.cblocationresult = [](TorrentClientRequest* req, int rcode) {
				WCHAR wbuf[2048];
				if (req->location.result) {
					wsprintf(wbuf, L"[%s] set location [%s] to [%s]", req->location.result, req->location.node->name.c_str(), req->location.location);
					delete req->location.result;
					SendMessageW(req->pausetorrent.hwnd, WM_U_VIEWLOGMESSAGE, (WPARAM)wbuf, 0);
				}
				delete req->location.location;
				delete req;
				return 0;
			};
			client->PutTask(new TorrentClient::GeneralRequestTask(client, tcr));
		}
		});
	return 0;
}

int WindowViewVT::ViewProcSetLocation()
{
	std::set<TorrentNodeVT*> dnodes;
	list->GetSelectedNodes(dnodes);
	if (dnodes.size() <= 0) {
		Tree_GetSelectedNodes(dnodes);
	}

	std::set<std::wstring> pts;
	std::for_each(dnodes.begin(), dnodes.end(), [&pts](TorrentNodeVT* node) {
		pts.insert(node->path.fullpath);
	});

	panel->SwitchWindow(hLocation);
	InitializeLocationListView(hLocation, hLocationEdit, hLocationSet);
	long lvc = 1;

	std::for_each(pts.begin(), pts.end(), [&lvc, this](const std::wstring& path) {
		LVITEM lvi = { 0 };
		lvi.mask = LVIF_TEXT;
		lvi.pszText = (LPWSTR)L"Location";
		lvi.cchTextMax = wcslen(lvi.pszText);
		lvi.iItem = lvc;
		lvi.iSubItem = 0;
		lvc = ListView_InsertItem(hLocation, &lvi);

		ListView_SetItemText(hLocation, lvc, 1, (LPWSTR)path.c_str());
		lvc++;
	});

	ListLocation_LocateButtons(hLocation);

	return 0;
}

int WindowViewVT::ViewProcPauseTorrent(bool dopause)
{
    std::set<TorrentNodeVT*> dnodes;
	list->GetSelectedNodes(dnodes);
    if (dnodes.size() <= 0) {
        Tree_GetSelectedNodes(dnodes);
    }

    std::for_each(dnodes.begin(), dnodes.end(), [this, dopause](TorrentNodeVT* node) {
        TorrentClientRequest* tcr = new TorrentClientRequest();
        tcr->req = TorrentClientRequest::REQ::PAUSETORRENT;
        tcr->pausetorrent.torrentid = node->id;
        tcr->pausetorrent.dopause = dopause;
        tcr->pausetorrent.node = node;
        tcr->pausetorrent.hwnd = hWnd;
        tcr->pausetorrent.result = NULL;
        tcr->pausetorrent.cbpauseresult = [](TorrentClientRequest* req, int rcode) {
            WCHAR wbuf[2048];
            if (req->pausetorrent.result) {
                wsprintf(wbuf, L"[%s] %s [%s]", req->pausetorrent.result, req->pausetorrent.dopause ? L"pause" : L"resume", req->pausetorrent.node->name.c_str());
                delete req->pausetorrent.result;
            }
            else {
                if (rcode == 0) {
                    wsprintf(wbuf, L"Success %s [%s]", req->pausetorrent.dopause ? L"pause" : L"resume", req->pausetorrent.node->name.c_str());
                }
                else {
                    wsprintf(wbuf, L"Failed %s [%s]", req->pausetorrent.dopause ? L"pause" : L"resume", req->pausetorrent.node->name.c_str());
                }
            }
            SendMessageW(req->pausetorrent.hwnd, WM_U_VIEWLOGMESSAGE, (WPARAM)wbuf, 0);
            delete req;
            return 0;
        };
        client->PutTask(new TorrentClient::GeneralRequestTask(client, tcr));

        });

    return 0;
}

int WindowViewVT::ViewProcVerifyTorrent()
{
    std::set<TorrentNodeVT*> dnodes;
	list->GetSelectedNodes(dnodes);
    if (dnodes.size() <= 0) {
        Tree_GetSelectedNodes(dnodes);
    }

    std::for_each(dnodes.begin(), dnodes.end(), [this](TorrentNodeVT* node) {
        TorrentClientRequest* tcr = new TorrentClientRequest();
        tcr->req = TorrentClientRequest::REQ::VERIFYTORRENT;
        tcr->verifytorrent.torrentid = node->id;
        tcr->verifytorrent.node = node;
        tcr->verifytorrent.hwnd = hWnd;
        tcr->verifytorrent.result = NULL;
        tcr->verifytorrent.cbverifyresult = [](TorrentClientRequest* req, int rcode) {
            WCHAR wbuf[2048];
            if (req->verifytorrent.result) {
                wsprintf(wbuf, L"[%s] verify [%s]", req->verifytorrent.result, req->verifytorrent.node->name.c_str());
                delete req->verifytorrent.result;
            }
            else {
                if (rcode == 0) {
                    wsprintf(wbuf, L"Success verify [%s]", req->verifytorrent.node->name.c_str());
                }
                else {
                    wsprintf(wbuf, L"Failed verify [%s]", req->verifytorrent.node->name.c_str());
                }
            }
            SendMessageW(req->verifytorrent.hwnd, WM_U_VIEWLOGMESSAGE, (WPARAM)wbuf, 0);
            delete req;
            return 0;
        };
        client->PutTask(new TorrentClient::GeneralRequestTask(client, tcr));

        });

    return 0;
}

int WindowViewVT::ViewProcFileRequest(bool hascheck, bool ischeck, bool haspriority, int priority)
{
 //   std::set<TorrentNodeVT*> dnodes;
	//list->GetSelectedNodes(dnodes);
 //   if (dnodes.size() <= 0) {
 //       Tree_GetSelectedNodes(dnodes);
 //   }

 //   std::for_each(dnodes.begin(), dnodes.end(), [this, hascheck, ischeck, haspriority, priority](TorrentNodeVT* snd) {
 //       ViewProcNodeFileRequest(snd, hascheck, ischeck, haspriority, priority);
 //       });

	ViewProcNodeFileRequest(NULL, hascheck, ischeck, haspriority, priority);

    return 0;
}

int WindowViewVT::ViewProcNodeFileRequest(TorrentNodeVT* node, bool hascheck, bool ischeck, bool haspriority, int priority)
{
    std::set<TorrentNodeFile*> fnodes;
	list->GetSelectedFiles(fnodes);
    if (fnodes.size() <= 0) {
        Tree_GetSelectedFiles(fnodes);
    }

	if (fnodes.size() > 0) {
		if (treeviewconst.currentTreeNode) {
			node = treeviewconst.currentTreeNode->GetTorrentNode();
			if (node) {
				TorrentClientRequest* tcr = new TorrentClientRequest();
				tcr->req = TorrentClientRequest::REQ::TORRENTFILEREQUEST;
				tcr->filerequest.torrentid = node->id;
				tcr->filerequest.node = node;
				tcr->filerequest.hascheck = hascheck;
				tcr->filerequest.ischeck = ischeck;
				tcr->filerequest.haspriority = haspriority;
				tcr->filerequest.priority = priority;
				tcr->filerequest.hwnd = hWnd;
				tcr->filerequest.result = NULL;
				tcr->filerequest.fileidscount = (long)fnodes.size();
				tcr->filerequest.fileids = new long[tcr->filerequest.fileidscount];
				int ii = 0;
				for (std::set<TorrentNodeFile*>::iterator itfn = fnodes.begin()
					; itfn != fnodes.end()
					; itfn++) {
					tcr->filerequest.fileids[ii] = (*itfn)->id;
					ii++;
				}
				tcr->filerequest.cbfileresult = [](TorrentClientRequest* req, int rcode) {
					WCHAR wbuf[2048];
					if (req->filerequest.result) {
						if (req->filerequest.hascheck) {
							wsprintf(wbuf, L"[%s] %s [%d] files in [%s]", req->filerequest.result, req->filerequest.ischeck ? L"mark" : L"unmark", req->filerequest.fileidscount, req->filerequest.node->name.c_str());
						}
						else if (req->filerequest.haspriority) {
							wsprintf(wbuf, L"[%s] priority set [%s] [%s]", req->filerequest.result, req->filerequest.priority == 0 ? L"normal" : (req->filerequest.priority > 0 ? L"high" : L"low"), req->filerequest.node->name.c_str());
						}
						else {
							wsprintf(wbuf, L"[%s] request files change [%s]", req->filerequest.result, req->filerequest.node->name.c_str());
						}
						delete req->filerequest.result;
					}
					else {
						if (rcode == 0) {
							wsprintf(wbuf, L"Success request files change [%s]", req->filerequest.node->name.c_str());
						}
						else {
							wsprintf(wbuf, L"Failed request files change [%s]", req->filerequest.node->name.c_str());
						}
					}
					SendMessageW(req->filerequest.hwnd, WM_U_VIEWLOGMESSAGE, (WPARAM)wbuf, 0);
					if (req->filerequest.fileids) {
						delete req->filerequest.fileids;
					}
					delete req;
					return 0;
				};
				client->PutTask(new TorrentClient::GeneralRequestTask(client, tcr));
			}
		}
    }
    return 0;
}

DWORD WINAPI ThRemoteTerminateConsole(_In_ LPVOID lParam)
{
    BOOL btn = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
    return btn;
}

int WindowViewVT::ListProfile_EnableDaemon(HWND hlist, HWND hdaemon)
{
	std::wstring value;
	client->profiledataloader->GetDefaultValue(L"EnableDaemon", value);
	bool daemon = value.compare(L"1") == 0;
	daemon = daemon == false;
	client->profiledataloader->SetDefaultValue(L"EnableDaemon", daemon ? L"1" : L"0");
	client->profiledataloader->StoreProfile();

	if (hdaemon != NULL) {
		Button_SetCheck(hdaemon, daemon ? TRUE : FALSE);
	}
	return 0;
}

LRESULT WindowViewVT::ViewProcCommand(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lst = 0;
    int wmId = LOWORD(wParam);
    // Parse the menu selections:
    switch (wmId)
    {
    case IDM_ABOUT:
        DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
        break;
    case IDM_EXIT:
        DestroyWindow(hWnd);
        break;
	case IDC_GRIDINPUT_CONNECT:
		if (HIWORD(wParam) == BN_CLICKED) {
			ListProfile_Connect(hProfile);
		}
		break;
    case IDC_GRIDADD_BUTTON:
        if (HIWORD(wParam) == BN_CLICKED) {
            ListAdd_RequestInList();
        }
        break;
    case IDC_GRIDADD_CLEAR:
        if (HIWORD(wParam) == BN_CLICKED) {
            ListAdd_ClearInList();
        }
        break;
	case IDC_GRIDLOCATION_SETLOCATION:
		if (HIWORD(wParam) == BN_CLICKED) {
			ListLocation_SetLocation(hLocation);
		}
		break;
	case IDC_GRIDINPUT_DAEMON:
		if (HIWORD(wParam) == BN_CLICKED) {
			ListProfile_EnableDaemon(hProfile, hProfileDaemonBtn);
		}
		break;

	case ID_CONTEXT_SETLOCATION:
		ViewProcSetLocation();
		break;
    case ID_CONTEXT_EXCLUDE:
        ViewProcFileRequest(true, false, false, 0);
        break;
    case ID_CONTEXT_INCLUDE:
        ViewProcFileRequest(true, true, false, 0);
        break;
    case ID_CONTEXT_LOWPRIORITY:
        ViewProcFileRequest(false, true, true, -1);
        break;
    case ID_CONTEXT_NORMALPRIORITY:
        ViewProcFileRequest(false, true, true, 0);
        break;
    case ID_CONTEXT_HIGHPRIORITY:
        ViewProcFileRequest(false, true, true, 1);
        break;
    case ID_CONTEXT_DELETE:
        ViewProcDeleteTorrent(false);
        break;
    case ID_CONTEXT_DELETEDATA:
        ViewProcDeleteTorrent(true);
        break;
    case ID_CONTEXT_PAUSE:
    case IDM_PAUSETREENODE:
        ViewProcPauseTorrent(true);
        break;
    case IDM_RESUMETREENODE:
    case ID_CONTEXT_RESUME:
        ViewProcPauseTorrent(false);
        break;
    case ID_FILE_STARTDAEMON:
        ViewProcCreateDaemonProcess();
        break;
    case ID_FILE_ADDTORRENT:
    case IDM_FILE_ADDNEW:
        panel->SwitchWindow(hAddTorrent);
        ListAdd_LocateSubmitButton();
        break;
	case ID_CONTEXT_VERIFY:
		ViewProcVerifyTorrent();
		break;
    case ID_CONTEXT_SELECTPARENT:
    case IDM_SELECTPARENT:
    {
        HTREEITEM hst = TreeView_GetSelection(hTree);
        if (hst) {
            hst = TreeView_GetNextItem(hTree, hst, TVGN_PARENT);
            if (hst) {
                TreeView_Select(hTree, hst, TVGN_CARET);
                TreeView_Select(hTree, hst, TVGN_FIRSTVISIBLE);
            }
        }
    }
    break;
    case ID_FILE_STOPDAEMON:
        if (console.psinfo.dwProcessId) {
            //DWORD tid;
            /*CHAR stc = 0x3;
            DWORD dwt;
            WriteFile(console.hInWrite, &stc, 1, &dwt, NULL);
            CloseHandle(console.hInWrite);
            CloseHandle(console.psinfo.hProcess);
            CloseHandle(console.psinfo.hThread);
            console.psinfo.dwProcessId = 0;*/
            

//            GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, console.psinfo.dwProcessId);
            //BOOL btn = GenerateConsoleCtrlEvent(CTRL_C_EVENT, console.psinfo.dwProcessId);
            //DWORD dle = GetLastError();
            //WaitForSingleObject(console.psinfo.hProcess, INFINITE);
            BOOL btn = TerminateProcess(console.psinfo.hProcess, 999);
            if (btn) {
                CloseHandle(console.hInWrite);
                CloseHandle(console.psinfo.hProcess);
                CloseHandle(console.psinfo.hThread);
                CloseHandle(console.hInRead);
                CloseHandle(console.hErrWrite);
                CloseHandle(console.hErrRead);

                console.psinfo.dwProcessId = 0;
            }
            /*HANDLE hth = CreateRemoteThread(console.psinfo.hProcess, NULL, 0, ThRemoteTerminateConsole, NULL, 0, &tid);
            if (hth) {
                WaitForSingleObject(hth, INFINITE);
                CloseHandle(hth);
            }*/
/*            BOOL btn;
            btn = AttachConsole(console.psinfo.dwProcessId);
            btn = SetConsoleCtrlHandler(NULL, TRUE);
            btn = GenerateConsoleCtrlEvent(CTRL_C_EVENT, console.psinfo.dwProcessId);
            lst = btn;
*/
        }
        break;
    case ID_PROCTEST:
        ViewRequestClientRefresh(false, true);
        break;
    case ID_PROCPROFILE:
        ViewProcProfile();
        break;
    case IDC_GRIDINPUT_EDIT:
    {
        HWND hedt = (HWND)lParam;
        if (hedt == hProfileEdit) {
            if (HIWORD(wParam) == EN_KILLFOCUS) {
                WCHAR tbuf[1024];
                GetWindowText(hedt, tbuf, 1024);
                ShowWindow(hedt, SW_HIDE);
                if (iParmEdit >= 0) {
                    ListView_SetItemText(hProfile, iParmEdit, 1, tbuf);
                    iParmEdit = -1;
                }
                //ViewParmUpdate(hProfile);
            }
        }
    }
    break;
	case IDC_GRIDLOCATION_EDIT:
	{
		HWND hedt = (HWND)lParam;
		if (hedt == hLocationEdit) {
			if (HIWORD(wParam) == EN_KILLFOCUS) {
				WCHAR tbuf[1024];
				GetWindowText(hedt, tbuf, 1024);
				ShowWindow(hedt, SW_HIDE);
				if (iParmEdit >= 0) {
					ListView_SetItemText(hLocation, iParmEdit, 1, tbuf);
					iParmEdit = -1;
				}
			}
		}
	}
	break;

    default:
        lst = DefWindowProc(hWnd, message, wParam, lParam);
    }
    return lst;
}

void WindowViewVT::ViewRequestClientRefresh(bool recentonly, bool reqfiles)
{
    TorrentClientRequest* req = new TorrentClientRequest();
    req->req = TorrentClientRequest::REQ::REFRESHTORRENTS;

    req->refresh.hwnd = hWnd;
    req->refresh.recent = recentonly;
    req->refresh.reqfiles = false;
    req->refresh.reqfilestat = false;
    req->refresh.tid = -1;
    if (treeviewconst.currentTreeNode) {
        switch (treeviewconst.currentTreeNode->type)
        {
        case VG_TYPE::TORRENT:
        {
            req->refresh.tid = treeviewconst.currentTreeNode->torrent.node->id;
            req->refresh.reqfiles = reqfiles;
        }
        break;
        case VG_TYPE::FILE:
        {
            req->refresh.tid = treeviewconst.currentTreeNode->torrent.node->id;
            req->refresh.reqfilestat = true;
        }
        }
    }
    req->refresh.cbgroupnode = [](TorrentClientRequest* req, TorrentNodeVT* node, TorrentGroupVT* group) {
        TASKPARM* parm = new TASKPARM();
        parm->node = node;
        parm->group = group;
        ::PostMessage(req->refresh.hwnd, WM_U_UPDATETORRENT, (WPARAM)req, (LPARAM)parm);
        return 0;
    };
    req->refresh.cbremovenode = [](TorrentClientRequest* req, TorrentNodeVT* node, TorrentGroupVT* group) {
        TASKPARM* parm = new TASKPARM();
        parm->node = node;
        parm->group = group;
        
        ::PostMessage(req->refresh.hwnd, WM_U_REMOVETORRENT, (WPARAM)req, (LPARAM)parm);
        return 0;
    };
    req->refresh.cbgroupgroup = [](TorrentClientRequest* req, TorrentGroupVT* group, TorrentGroupVT* pgroup) {
        TASKPARM* parm = new TASKPARM();
        parm->group = group;
        parm->pgroup = pgroup;
        ::PostMessage(req->refresh.hwnd, WM_U_UPDATEGROUP, (WPARAM)req, (LPARAM)parm);
        return 0;
    };
    req->refresh.cbnodefile = [](TorrentClientRequest* req, TorrentNodeVT* node) {
        TASKPARM* parm = new TASKPARM();
        parm->node = node;
        ::PostMessage(req->refresh.hwnd, WM_U_UPDATENODEFILE, (WPARAM)req, (LPARAM)parm);
        return 0;
    };
    req->refresh.cbnodefilestat = [](TorrentClientRequest* req, TorrentNodeVT* node, TorrentNodeFile* fnode) {
        TASKPARM* parm = new TASKPARM();
        parm->node = node;
        parm->file = fnode;
        ::PostMessage(req->refresh.hwnd, WM_U_UPDATENODEFILESTATUS, (WPARAM)req, (LPARAM)parm);
        return 0;
    };
    req->refresh.cbfailed = [](TorrentClientRequest* req, std::wstring error, long ecode) {
        size_t esz = (error.length() + 1) * sizeof(WCHAR);
        WCHAR* estr = (WCHAR*)malloc(esz);
        if (estr) {
            wcscpy_s(estr, error.length() + 1, error.c_str());
            ::PostMessage(req->refresh.hwnd, WM_U_REQUESTERROR, (WPARAM)estr, ecode);
        }
        delete req;
        return 0;
    };
    client->PutTask(new TorrentClient::RefreshTorrentTask(client, req));
    //ViewLog(L"send client request");
}

void WindowViewVT::ViewProcPaint(const HWND& hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    // TODO: Add any drawing code that uses hdc here...
    EndPaint(hWnd, &ps);
}

LRESULT CALLBACK WindowViewVT::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lst = 0;
    WindowViewVT* wvv = viewMap.GetWindowView(hWnd);

    if (wvv) {
        lst = wvv->WndInstanceProc(hWnd, message, wParam, lParam);
    }
    else {
		//switch (message)
		//{
		//case WM_CREATE:
		//	CoInitialize(NULL);
		//	InitializeRobbin(GetModuleHandle(NULL), hWnd);
		//	break;
		//default:
			lst = DefWindowProc(hWnd, message, wParam, lParam);
		//	break;
		//}
    }
    return lst;
}

// Message handler for about box.
INT_PTR CALLBACK WindowViewVT::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

BOOL WindowViewVT::IsRunning()
{
    BOOL rtn = FALSE;
    std::set<HWND> rms;

    viewMap.lock();
    for (WindowViewVTMap::iterator itvm = viewMap.begin()
        ; itvm != viewMap.end()
        ; itvm++) {
        if (itvm->second->live) {
            rtn = TRUE;
        }
        else {
            rms.insert(itvm->first);
        }
    }
    WindowViewVT* cwv;
    for (std::set<HWND>::iterator itws = rms.begin()
        ; itws != rms.end()
        ; itws++) {
        cwv = viewMap[*itws];
        delete cwv;
        viewMap.erase(*itws);
    }
    viewMap.unlock();
    return rtn;
}

#define LWBUFSZ 4096
int WindowViewVT::ListParm_ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	NMLISTVIEW* plist = (NMLISTVIEW*)lParam;

	switch (plist->hdr.code)
	{
	case NM_DBLCLK:
	{
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
		WCHAR* tbuf = new WCHAR[LWBUFSZ];
		if (lpnmitem->hdr.hwndFrom == hProfile) {
			if (lpnmitem->iItem >= 0) {
				if (lpnmitem->iSubItem == 1) {
					RECT rc;
					
					ShowWindow(hProfileEdit, SW_SHOW);
					ListView_GetItemText(hProfile, lpnmitem->iItem, lpnmitem->iSubItem, tbuf, LWBUFSZ);
					SetWindowText(hProfileEdit, tbuf);
					iParmEdit = lpnmitem->iItem;
					ListView_GetSubItemRect(lpnmitem->hdr.hwndFrom, lpnmitem->iItem, lpnmitem->iSubItem, LVIR_BOUNDS, &rc);
					MoveWindow(hProfileEdit, rc.left, rc.top, rc.right - rc.left - 1, rc.bottom - rc.top - 1, TRUE);
					SendMessage(hProfileEdit, EM_SETSEL, 0, -1);
					SetFocus(hProfileEdit);
				}
			}
		}
		if (lpnmitem->hdr.hwndFrom == hLocation) {
			if (lpnmitem->iItem > 0) {
				if (lpnmitem->iSubItem == 1) {
					RECT rc;
					
					ShowWindow(hLocationEdit, SW_SHOW);
					ListView_GetItemText(hLocation, lpnmitem->iItem, lpnmitem->iSubItem, tbuf, LWBUFSZ);
					SetWindowText(hLocationEdit, tbuf);
					iParmEdit = lpnmitem->iItem;
					ListView_GetSubItemRect(lpnmitem->hdr.hwndFrom, lpnmitem->iItem, lpnmitem->iSubItem, LVIR_BOUNDS, &rc);
					MoveWindow(hLocationEdit, rc.left, rc.top, rc.right - rc.left - 1, rc.bottom - rc.top - 1, TRUE);
					SendMessage(hLocationEdit, EM_SETSEL, 0, -1);
					SetFocus(hLocationEdit);
				}
			}
		}
		delete tbuf;
	}
	break;
	}
	return 0;
}

LRESULT WindowViewVT::Tree_ProcNotifyCustomDraw(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lrt = 0;
    LPNMTVCUSTOMDRAW pntd = (LPNMTVCUSTOMDRAW)lParam;
    lrt = CDRF_DODEFAULT;
    switch (pntd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        lrt = CDRF_NOTIFYITEMDRAW;
        break;
    case CDDS_ITEMPREPAINT:
    {
        ViewNode* vnd = (ViewNode*)pntd->nmcd.lItemlParam;
        if (vnd) {
            if (vnd->type == VG_TYPE::TORRENT) {
                if (vnd->torrent.node->status == 0) {
                    pntd->clrTextBk = RGB(0xCF, 0xCF, 0xCF);
                }
                if (vnd->torrent.node->status == 4) {
                    pntd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
                    pntd->clrText = RGB(0x00, 0x3F, 0x00);
                }
                else if (vnd->torrent.node->error.length() > 0) {
                    pntd->clrText = RGB(0xFF, 0x00, 0x00);
                }
            }
        }
        break;
    }
    }
    return lrt;
}

LRESULT WindowViewVT::Tree_ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lrt = 0;
    NMTREEVIEW* pntree = (NMTREEVIEW*)lParam;

    switch (pntree->hdr.code)
    {
    case TVN_SELCHANGED:
    {
        //ViewLog(L"on item change");
        if (pntree->itemNew.hItem)
        {
            if (pntree->itemNew.lParam) {
                ViewNode* vnode = reinterpret_cast<ViewNode*>(pntree->itemNew.lParam);
                treeviewconst.currentTreeNode = vnode;
                PostMessage(hWnd, WM_U_UPDATEVIEWNODE, (WPARAM)vnode, 0);
                TreeView_Expand(hTree, pntree->itemNew.hItem, TVE_EXPAND);
				bool reqfile = vnode->torrent.file == NULL;
                ViewRequestClientRefresh(true, reqfile);
            }
        }
    }
    break;
    case NM_CUSTOMDRAW:
        lrt = Tree_ProcNotifyCustomDraw(hWnd, message, wParam, lParam);
        break;
    }
    return lrt;
}

LRESULT WindowViewVT::ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lrst = 0;
    LPNMHDR pnmh = (LPNMHDR)lParam;

    if (hTree) {
        if (pnmh->hwndFrom == hTree) {
            lrst = Tree_ProcNotify(hWnd, message, wParam, lParam);
        }
    }
    if (list) {
        if (pnmh->hwndFrom == list->GetWnd()) {
            lrst = list->ProcNotify(hWnd, message, wParam, lParam);
        }
    }
    if (hProfile) {
        if (pnmh->hwndFrom == hProfile) {
            lrst = ListParm_ProcNotify(hWnd, message, wParam, lParam);
        }
    }
	if (hLocation) {
		if (pnmh->hwndFrom == hLocation) {
			lrst = ListParm_ProcNotify(hWnd, message, wParam, lParam);
		}
	}
    if (hAddTorrent) {
       if (pnmh->hwndFrom == hAddHeader) {
           LPNMHEADER phd = (LPNMHEADER)lParam;
            if (phd->hdr.code == HDN_ITEMCHANGED) {
                ListAdd_LocateSubmitButton();
                
                lrst = FALSE;
            }
        }
    }

    return lrst;
}

LRESULT WindowViewVT::ViewProcRequestUpdateViewNode(ViewNode* vnode)
{
	if (list->GetCurrentViewNode() != treeviewconst.currentTreeNode) {
		list->ClearList();
		list->SetCurrentViewNode(treeviewconst.currentTreeNode);
		switch (vnode->type) {
		case VG_TYPE::GROUPGROUP:
			//list->ChangeContent(LISTCONTENT::GROUPLIST);
			//list->CreateGroupColumns(vnode->group);
			//listupdatereq = new TorrentClientRequest();
			//
			//listupdatereq->sequence = list->IncreaseSequenceNo();
			//listupdatereq->req = TorrentClientRequest::REQ::QUERYGROUPGROUPS;
			//listupdatereq->getgroupgroups.group = vnode->group;
			//listupdatereq->getgroupgroups.hwnd = hWnd;
			//listupdatereq->getgroupgroups.cbquery = [](TorrentClientRequest* req, TorrentGroupVT* sgroup) {
			//	TASKPARM* parm = new TASKPARM();
			//	parm->group = sgroup;
			//	PostMessage(req->getgroupnodes.hwnd, WM_U_LISTADDGROUPGROUP, (WPARAM)req, (LPARAM)parm);
			//	return 0;
			//};
			//client->RequestTaskProc(listupdatereq);
			break;
		case VG_TYPE::GROUP:
			//list->ChangeContent(LISTCONTENT::TORRENTLIST);
			//list->CreateTorrentListColumns(vnode->group.groupnode);
			//listupdatereq = new TorrentClientRequest();
			//
			//listupdatereq->sequence = list->IncreaseSequenceNo();
			//listupdatereq->req = TorrentClientRequest::REQ::QUERYGROUPTORRENTS;
			//listupdatereq->getgroupnodes.group = vnode->group.groupnode;
			//listupdatereq->getgroupnodes.hwnd = hWnd;
			//listupdatereq->getgroupnodes.cbquery = [](TorrentClientRequest* req, TorrentNodeVT* node) {
			//	PostMessage(req->getgroupnodes.hwnd, WM_U_LISTADDGROUPNODE, (WPARAM)req, (WPARAM)node);
			//	return 0;
			//};
			//client->RequestTaskProc(listupdatereq);
			break;
		case VG_TYPE::TORRENT:
			//list->ChangeContent(LISTCONTENT::TORRENTDETAIL);
			//list->CreateTorrentColumns(vnode->torrent.node);
			//list->UpdateTorrentContent(vnode->torrent.node);
			break;
		case VG_TYPE::SESSION:
			//list->ChangeContent(LISTCONTENT::SESSION);
			//::PostMessage(hWnd, WM_U_UPDATELISTSESSION, (WPARAM)vnode->session, 0);
			break;
		case VG_TYPE::FILE:
			//list->ChangeContent(LISTCONTENT::TORRENTFILE);
			//list->CreateTorrentFileColumns(vnode->torrent.node);
			//UpdateViewNodeFiles(vnode, hWnd);

			break;
		}
	}
	return 0;
}

//int WindowViewVT::UpdateViewNodeFiles(ViewNode* vnode, HWND hwnd)
//{
//	if (vnode->type == VG_TYPE::FILE)
//	{
//		TASKPARM* parm = new TASKPARM();
//		parm->file = vnode->torrent.file;
//		parm->node = vnode->torrent.node;
//		parm->filelist = new std::vector<TorrentNodeFile*>();
//		vnode->GetFileNodes(*parm->filelist);
//		::PostMessage(hwnd, WM_U_UPDATELISTFILE, NULL, (LPARAM)parm);
//	}
//	return 0;
//}
