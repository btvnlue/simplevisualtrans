#pragma once

#include <Windows.h>
#include <map>
#include <set>
#include <list>
#include <CommCtrl.h>

#include "CSplitWnd.h"
#include "TransAnalyzer.h"
#include "CProfileData.h"

#define MAX_LOADSTRING 100

class CPanelWindow;

struct ListParmData {
	enum LIPDTYPE {
		Parameter
		, Tracker
		, Content
		, File
		, Session
		, Group
		, Node
	} type;
	union {
		TorrentNode* node;
		TorrentNodeFileNode* file;
		SessionInfo* session;
		TorrentGroup* group;
	};
	bool idle;
};

struct TreeParmData {
	enum TIPDTYPE {
		Group
		, Torrent
		, Tracker
		, Attribute
		, Session
		, File
	} ItemType;
	union {
		TorrentGroup* group;
		TorrentNode* node;
		SessionInfo* session;
		TorrentNodeFileNode* file;
	};
};

class TreeItemParmDataHelper {
	static std::set<TreeParmData*> parms;
public:
	static TreeParmData* CreateTreeItemParmData(TreeParmData::TIPDTYPE type);
	static int ClearTreeItemParmData();
};


struct TorrentParmItems {
	HTREEITEM torrent;
	HTREEITEM size;
	HTREEITEM viewsize;
	HTREEITEM id;
	HTREEITEM name;
	HTREEITEM tracker;
	HTREEITEM file;
	DWORD readtick;
};

class TreeGroupShadow
{
	TreeGroupShadow();
public:
	TreeGroupShadow* parent;
	TorrentGroup* group;
	HTREEITEM hnode;
	std::map<TorrentGroup*, TreeGroupShadow*> subs;
	std::map<TorrentNode*, HTREEITEM> nodeitems;
	std::map<HTREEITEM, TorrentParmItems*> nodeDetailItems;

	virtual ~TreeGroupShadow();
	//TorrentGroup* getItemGroup(HTREEITEM hitem);
	int getTorrentItems(TorrentNode* node, std::set<HTREEITEM>& items);
	int removeItem(TorrentNode* node);
	HTREEITEM findTreeItem(TorrentNode* node);
	HTREEITEM findTreeItem(TorrentGroup* grp);
	static TreeGroupShadow* CreateTreeGroupShadow(TorrentGroup* grp, TreeGroupShadow* pnt);
	TreeGroupShadow* GetTreeGroupShadow(TorrentGroup* grp);
	TorrentParmItems* GetNodeParmItems(TorrentNode* node);
	int SyncGroup();
};

struct ColDef {
	wchar_t name[20];
	int width = 100;
};

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
	HWND hTree;
	HWND hList;
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
	HBRUSH hbWhite;
	HBRUSH hbGreen;
	HBRUSH hbRed;
	std::set<struct DELAYMESSAGE*> delaymsgs;
	std::list<ListParmData*> listparms;

	TreeGroupShadow* torrentsViewOrgs;
	//GroupTreeItem gTorrents;
	//GroupTreeItem gTrackers;
	std::map<unsigned long, std::set<HTREEITEM>> torrentNodes;
	enum LISTCONTENT {
		TORRENTGROUP
		, TORRENTLIST
		, TORRENTDETAIL
		, SESSIONINFO
		, TORRENTFILE
		, NONE
	} listContent;
	HPEN gradualpen[256];
	CProfileData profile;
	std::wstring prevPath;

	std::wstring remoteHost;
	std::wstring remotePath;
	std::wstring remoteUser;
	std::wstring remotePass;
	std::wstring remoteUrl;

	wchar_t** torrentDetailTitles;
	wchar_t** torrentSessionTitles;

	int refreshcount;
public:
	static WindowView* CreateView(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow);

	WindowView();
	virtual ~WindowView();
	void InitializeTorrentDetailTitle();
	void InitializeGradualPen();
	int FreeTorrentDetailTitle();
	int ShowView();
	ListParmData* GetListParmData(unsigned long idx);
	int UpdateTreeViewTorrentDetail(TorrentParmItems* items);
	HTREEITEM UpdateTreeViewTorrentDetailTrackers(TorrentParmItems* items, TorrentNode* node);
	HTREEITEM UpdateTreeViewTorrentDetailFiles(HTREEITEM hpnt, TorrentNodeFileNode* node);
	static int MyRegisterClass(_In_ HINSTANCE hInstance);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int GetListSelectedTorrents(std::set<TorrentNode*>& tts);
	int GetListSelectedFiles(std::set<TorrentNodeFileNode*>& tts);
	int CopyTorrentsDetailOnView();
	int CheckTorrentFilesOnView(BOOL check);
	int PriorityTorrentFilesOnView(int prt);
	int RemoveTorrentOnView(BOOL bContent);
	int GetTreeSelectedFiles(std::set<TorrentNodeFileNode*>& trfs);
	int GetTreeSelectedTorrents(std::set<TorrentNode*>& trts);
	void RefreshTorrentNodeFiles(TorrentNode* node);
	void RefreshTorrentNodeDetail(TorrentNode* node);
	//int RefreshListTorrentDetail(ReqParm rpm);
	int RefreshSession();
	int UpdateViewSession(SessionInfo* ssn);
	int RefreshActiveTorrents();
	void RefreshViewNode();
	void UpdateViewRemovedTorrents();
	int ProcContextMenuLog(int xx, int yy);
	int ProcContextMenuList(int xx, int yy);
	int ProcContextMenuTree(int xx, int yy);
	int ProcContextMenu(int xx, int yy);
	int SendStringClipboard(const std::wstring& str);
	int PauseTorrentsOnView(BOOL bresume);
	//int PauseTreeNode(BOOL bresume);
	//int CopyTreeNodeDetail();
	int CopyTreeNodeName();
	int SelectTreeParentNode(HWND htree);
	LRESULT CALLBACK WndProcInstance(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int DumpLogToClipboard(HWND hlog);
	LRESULT ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int InitializeStatusBar(HWND hsts);
	int CreateListColumnsSession(HWND hlist);
	int CreateListColumnsFiles(HWND hlist);
	int CreateListColumnsTorrent(HWND hlist);
	int UpdateListViewTorrentFiles(TorrentNodeFileNode* files);
	int UpdateListViewTorrentFileDetail(HWND hlist, int fii, TorrentNodeFileNode* cfl);
	int UpdateListViewSession(SessionInfo* ssn);
	int UpdateListViewTorrentDetailData(TorrentNode* node);
	int ListUpdateFiles(TorrentNodeFileNode* files);

	int ListUpdateSession(SessionInfo* ssn);
	int UpdateListViewTorrentDetail(TorrentNode* node);
	void ListUpdateTorrentTitle(TorrentNode* node);
	void ListUpdateSessionTitle(SessionInfo* ssn);
	int MakeListParmIdle(ListParmData* lpd);
	LRESULT ProcNotifyList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int TreeViewSelectItem(TorrentNode* node);
	int TreeViewSelectItem(TorrentGroup* grp);
	LRESULT ProcCustDrawListFile(LPNMLVCUSTOMDRAW& plcd);
	LRESULT ProcCustDrawListNodes(LPNMLVCUSTOMDRAW& plcd);
	LRESULT ProcCustDrawListDetail(LPNMLVCUSTOMDRAW& plcd);
	LRESULT ProcCustDrawListName(const LPNMLVCUSTOMDRAW& plcd, const COLORREF& ocr);
	LRESULT ProcCustDrawListPieces(const LPNMLVCUSTOMDRAW& plcd);
	LRESULT ProcNotifyTree(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
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
	//void InitializeWindowProfileSize(int& rtn);
	int CreateViewControls();
	int ViewLog(const WCHAR* msg);
	int UpdateListViewTorrents();
	void UpdateViewTorrentGroup(TorrentGroup* grp);
	HTREEITEM UpdateTreeViewNodeItem(TreeGroupShadow* tgs, TorrentNode* node);
	void UpdateTreeViewGroup(TreeGroupShadow* gti);
	//void updateTrackerView();
	int StartAnalysis();
	int ClearColumns(HWND hlist);
	int CreateListColumnsGroupList(HWND hlist);
	int CreateListColumnsNodeList(HWND hList);
	int CreateListColumns(HWND hlist, struct ColDef *cols);
	int UpdateListViewTorrentNodesDetail(HWND hlist, int iti, TorrentNode* nod);
	int UpdateListViewGroups(HWND hlist);
	int UpdateListViewTorrentGroupData(HWND hlist, int iti, TorrentGroup* nod);
	int UpdateListViewTorrentGroup(TorrentGroup* grp);
	int UpdateListViewTorrentNodes(TorrentGroup* grp);
	static VOID CALLBACK ViewProcTimer(_In_ HWND hwnd, _In_ UINT uMsg, _In_ UINT_PTR idEvent, _In_ DWORD dwTime);
	int ProcRefreshTimer(HWND hwnd, DWORD dwtime);
};

