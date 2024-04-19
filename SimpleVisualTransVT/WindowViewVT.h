#pragma once

#include <Windows.h>
#include <CSplitWnd.h>
#include <CommCtrl.h>
#include <oleidl.h>
#include <UIRibbon.h>

#include "TorrentClient.h"
#include "CPanelWindow.h"
#include "FrameListView.h"

DECLARE_HANDLE (HSELF);

class ViewNodeFile
{
public:

};

enum class PARMTYPE {
	NONE
	, PROFILE
	, UPLOADER
};

struct TASKPARM {
	TorrentNodeVT* node;
	TorrentGroupVT* group;
	TorrentGroupVT* pgroup;
	TorrentNodeFile* file;
	std::vector<TorrentNodeFile*>* filelist;
};

struct CONSOLEINFO {
	HANDLE hInRead;
	HANDLE hInWrite;
	HANDLE hOutRead;
	HANDLE hOutWrite;
	HANDLE hErrRead;
	HANDLE hErrWrite;
	STARTUPINFO start;
	PROCESS_INFORMATION psinfo;
};
class WindowViewVT
{
	HINSTANCE hInstance;
	HWND hWnd;
	BOOL doneInit;
	BOOL live;
	BOOL listenToClipboard;
	CSplitWnd* splitStatus;
	CSplitWnd* splitTool;
	CSplitWnd* splitTree;
	CSplitWnd* splitLog;
	CPanelWindow* panel;
	HWND hStatus;
	HWND hTool;
	HWND hLog;
	HWND hProfile;
	HWND hLocation;
	HWND hAddTorrent;
	HWND hProfileEdit;
	HWND hProfileBtn;
	HWND hProfileDaemonBtn;
	HWND hLocationEdit;
	HWND hLocationSet;
	HWND hAddButton;
	HWND hAddClear;
	HWND hAddHeader;
	HWND hTree;
	//HWND hList;
	HWND hContent;
	HANDLE hThConsole;
	HANDLE eConsole;
	TorrentClient* client;
	FrameListView* list;
	int iParmEdit;
	//std::map<TorrentNodeVT*, ViewNode*> torrentviewmap;
	std::map<TorrentGroupVT*, ViewNode*> groupviewmap;
	std::map<ViewNode*, HTREEITEM> treeviewnodemap;
	std::set<ViewNode*> rootviewnodes;
	IDropTarget* pdrop;
	IUIFramework* ribbonFramework;

public:
	CONSOLEINFO console;
	struct TREECONST {
		ViewNode* currentTreeNode = NULL;
	} treeviewconst;
	class PARMCONST {
	public:
		PARMTYPE type;
		int edititem;
		CProfileData* profile = NULL;
	} parmconst;
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static ATOM RegisterClass(HINSTANCE hInstance);
	static DWORD WINAPI ThWindowViewVTProc(LPVOID lpParameter);
	static WindowViewVT* InitInstance(HINSTANCE hInstance, int nCmdShow);
	static WindowViewVT* StartWindow(HINSTANCE hinst, HINSTANCE hprev);

	ViewNode* GetGroupViewNode(TorrentGroupVT* group, bool createnew);

	WindowViewVT();
	virtual ~WindowViewVT();

	int InitializeToolbar(HWND htool);
	int InitializeLocationListView(HWND hlist, HWND hedit, HWND hbtn);
	int ListLocation_LocateButtons(HWND hlist);
	int InitializeProfileListView(HWND hlist, HWND hedit, HWND hbtn, HWND hdmbtn);
	int InitializeAddListView(HWND hlist);
	int ListAdd_LocateSubmitButton();
	int InitializeListView(HWND hlist);
	static BOOL IsRunning();

	int Tree_AddFileNode(ViewNode* vnode);
	int Tree_UpdateTorrentNodeFile(TorrentNodeVT * node);
	int ViewUpdateNodeFile(TorrentClientRequest* req, TASKPARM* parm);
	int ViewUpdateNodeFileStatus(TorrentClientRequest* req, TASKPARM* parm);
	int ListParm_ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int ListAdd_RegNewTorrentIntoList(WCHAR* path, BOOL isfile);
	int ListAdd_NewTorrentRawLines(WCHAR* path);
	int ListAdd_NewTorrent(WCHAR* path);
	int ViewProcClipboardUpdate();
	int ViewProcContextMenu(HWND hwnd, int xx, int yy);
	int Tree_RemoveTorrent(TorrentGroupVT* group, TorrentNodeVT* node);
	int ViewProcRemoveTorrent(TorrentClientRequest* req, TASKPARM* parm);
	LRESULT ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT ViewProcRequestUpdateViewNode(ViewNode * vnode);
	int UpdateViewNodeFiles(ViewNode * vnode, HWND hwnd);
	void ResizeContents(const HWND& hWnd);
	int InitializeStatusBar(HWND hstat);
	LRESULT Tree_ProcNotifyCustomDraw(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT Tree_ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void Tree_AddGroupNode(ViewNode* vnode);
	void Tree_AddTorrentNode(ViewNode* vnode);
	void Tree_UpdateTorrentNode(TorrentGroupVT* group, TorrentNodeVT* node);
	int InitializeDragDrop(HWND hwnd);
	int InitializeRobbin(HINSTANCE hinst, HWND hwnd);
	LRESULT ViewProcInitiate();
	void ViewProcPaint(const HWND& hWnd);
	int ViewUpdateGroup(TorrentClientRequest* req, TASKPARM* parm);
	int ViewUpdateTorrent(TorrentClientRequest* req, TASKPARM* parm);
	int ViewProcNewTorrentsRequest(const std::wstring& reqlink);
	int ListAdd_ClearInList();
	int ListLocation_SetLocation(HWND hlist);
	int ListAdd_RequestInList();
	int Tree_GetSelectedFiles(std::set<TorrentNodeFile*>& lnodes);
	int Tree_GetSelectedNodes(std::set<TorrentNodeVT*>& lnodes);
	int ViewProcDeleteTorrent(bool deldata);
	int ViewProcRefreshSession(SessionInfo * ssi, TorrentClientRequest * req);
	int ViewRequestRefreshSession();
	int ViewProcCreateDaemonProcess();
	int ViewProcSetTorrentLocation(WCHAR * location);
	int ViewProcSetLocation();
	int ViewProcHandleConsoleProcess();
	int ViewProcPauseTorrent(bool dopause);
	int ViewProcVerifyTorrent();
	int ViewProcFileRequest(bool hascheck, bool ischeck, bool haspriority, int priority);
	int ViewProcNodeFileRequest(TorrentNodeVT* node, bool hascheck, bool ischeck, bool haspriority, int priority);
	int ListProfile_EnableDaemon(HWND hlist);
	int ListProfile_EnableDaemon(HWND hlist, HWND hdeamon);
	LRESULT ViewProcCommand(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	void ViewRequestClientRefresh(bool recentonly, bool reqfiles);
	int ParmView_Profile(CProfileData* prof);
	int ViewProcProfile();
	int ListProfile_Connect(HWND hlist);
	int ViewProcRibbonCommand(ULONG cmd);
	LRESULT WndInstanceProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int ViewLog(const std::wstring& logmsg);
};

