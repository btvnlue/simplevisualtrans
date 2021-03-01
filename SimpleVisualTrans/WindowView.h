#pragma once

#include <Windows.h>
#include <map>
#include <set>
#include <list>
#include <CommCtrl.h>

#include "CSplitWnd.h"
#include "TransAnalyzer.h"
#include "CProfileData.h"
#include "CViewListFrame.h"
#include "CViewTreeFrame.h"

#define MAX_LOADSTRING 100

class CPanelWindow;

class WindowView
{
	static ATOM viewClass;
	static std::map<HWND, WindowView*> views;
	static WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
	static WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

	HINSTANCE hInst;
	HWND hWnd;
	CSplitWnd* splitTree;
	CSplitWnd* splitLog;
	CSplitWnd* splitStatus;
	CSplitWnd* splitTool;
	CPanelWindow* panelState;
	CViewListFrame* listview;
	CViewTreeFrame* treeview;
	HWND hLog;
	HWND hNew;
	HWND hUrl;
	HWND hStatus;
	HWND hTool;
	HWND hMain;
	HWND hDelay;
	TransAnalyzer* analyzer;
	bool canRefresh;
	HIMAGELIST hToolImg;
	DWORD delaytick;
	std::set<struct DELAYMESSAGE*> delaymsgs;

	std::map<unsigned long, std::set<HTREEITEM>> torrentNodes;
	CProfileData profile;
	std::wstring prevPath;

	std::wstring remoteHost;
	std::wstring remotePath;
	std::wstring remoteUser;
	std::wstring remotePass;
	std::wstring remoteUrl;

	int refreshcount;

	//class TreeCallBack : public CViewTreeFrame::ITreeCallBack
	//{
	//public:
	//	WindowView* parent;
	//	int ProcessTreeNotice(long cmd, long txn, int(*cbfunc)(void*, long, long), void* lparm);
	//};

	//TreeCallBack* cbtree;
public:
	static WindowView* CreateView(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow);

	WindowView();
	virtual ~WindowView();
	int ShowView();
	static int MyRegisterClass(_In_ HINSTANCE hInstance);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int ProcessUICallBackNotice(long cmd, long txn, int(*cbfunc)(void*, long, long), void* lparm);
	int CopyTorrentsDetailOnView();
	int CheckTorrentFilesOnView(BOOL check);
	int PriorityTorrentFilesOnView(int prt);
	int RemoveTorrentOnView(BOOL bContent);
	void RefreshTorrentNodeFiles(TorrentNode* node);
	void RefreshTorrentNodeDetail(TorrentNode* node);
	int RefreshSession();
	int UpdateViewSession(SessionInfo* ssn);
	int RefreshActiveTorrents();
	void RefreshViewNode();
	int ProcContextMenuLog(int xx, int yy);
	int ProcContextMenu(int xx, int yy);
	int SendStringClipboard(const std::wstring& str);
	int PauseTorrentsOnView(BOOL bresume);
	int CopyTreeNodeName();
	LRESULT CALLBACK WndProcInstance(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int DumpLogToClipboard(HWND hlog);
	LRESULT ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int InitializeStatusBar(HWND hsts);
	static INT_PTR CALLBACK DlgProcAbout(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK DlgProcUrl(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	int DlgProcUrlUpdateUrl(const HWND hdlg);
	static VOID CALLBACK TimerProcDlgDelay(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
	static INT_PTR CALLBACK DlgProcDelay(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK DlgProcAddNew(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

	int MoveDlgDelayControls(const HWND hDlg);
	int MoveDlgUrlControls(const HWND hDlg);
	int MoveDlgAddNewControls(const HWND hDlg);
	BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);
	int InitializeWindowProfile();
	int UpdateUrlDlgData(HWND hdlg);
	int InitializeWindowProfileMisc();
	int InitializeWindowProfileUrl();
	int InitializeWindowProfileSize();
	int InitializeToolbar(HWND htool);
	int CreateViewControls();
	int ViewLog(const WCHAR* msg);
	int UpdateListViewTorrents();
	int StartAnalysis();
	static VOID CALLBACK ViewProcTimer(_In_ HWND hwnd, _In_ UINT uMsg, _In_ UINT_PTR idEvent, _In_ DWORD dwTime);
	int ProcRefreshTimer(HWND hwnd, DWORD dwtime);
};

