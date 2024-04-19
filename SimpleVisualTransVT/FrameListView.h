#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <vector>

#include "ViewCommons.h"

#define WM_U_LIST_ACTIVATENODE WM_USER + 0x1234

enum class LISTSORTCOL {
	NONE
	, ID
	, NAME
	, SIZE
	, LOCATION
	, STATUS
	, TRACKER
	, DOWNLOAD
	, DOWNSPEED
	, UPSPEED
	, RATIO
	, COUNT
};

enum class LISTCONTENT {
	NONE
	, TORRENTLIST
	, GROUPLIST
	, TORRENTDETAIL
	, SESSION
	, TORRENTFILE
};

struct LISTCONST {
	LISTSORTCOL listsort;
	unsigned long listchangesequence = 0;
	LISTCONTENT content;
	bool sortasc = true;
	ViewNode* currentListNode = NULL;
	std::map<int, LISTSORTCOL> sortcolmap;
	std::map<LISTCONTENT, LISTSORTCOL> sortcontentmap;
};

struct ColDefVT {
	wchar_t name[20];
	int width = 100;
	LISTSORTCOL sortcol;
};

struct RequestCallbackInfo {
	HWND hMain;
	UINT msgUpdateGroup;
};

class FrameListView
{
private:
	HWND hList;
	LISTCONST listviewconst;
	HPEN gradualpenvt[256];
	HBRUSH hbGreen;
	HBRUSH hbRed;
	std::vector<ViewNode*> ownernodes;
	std::vector<std::wstring> torrentcontenttitles;
	std::vector <TorrentNodeFile*> files;

public:
	FrameListView(HWND hlist);
	virtual ~FrameListView();
	int UpdateTorrentNode(TorrentNodeVT * node, ViewNode * vgroup);
	int Initialize();
	int UpdateFileStat(TorrentNodeVT * node, TorrentNodeFile * fnode);
	int UpdateItemGroup(int iix, TorrentGroupVT * group);
	//int UpdateItemNode(long iit, TorrentNodeVT * node);
	int InsertGroupItem(TorrentGroupVT * group);
	int AddGroupGroup(TorrentClientRequest * req, TorrentGroupVT * group);
	//int AddTorrentListNode(TorrentClientRequest * req, TorrentNodeVT * node);
	//int InsertNodeItem(TorrentNodeVT * node);
	//int AddNodeFile(TorrentClientRequest * req, std::vector<TorrentNodeFile*>* filelist);
	//int InsertNodeFileStat(TorrentNodeFile * file);
	//void UpdateNodeFileStat(TorrentNodeFile * file, long lvc);
	int RemoveTorrent(ViewNode * vnode);
	int GetSelectedNodes(std::set<TorrentNodeVT*>& nlist);
	int GetSelectedFiles(std::set<TorrentNodeFile*>& flist);
	int GetSelectedViewNodes(std::set<ViewNode*>& vlist);
	int SortTorrentList(LISTSORTCOL col);
	int SortTorrentFile(LISTSORTCOL col);
	int IncreaseSequenceNo();
	LRESULT ProcNotifyCustomDrawNodeList(LPNMLVCUSTOMDRAW & plcd);
	LRESULT ProcNotifyCustomDrawNodeDetailPiece(LPNMLVCUSTOMDRAW & plcd);
	LRESULT ProcNotifyCustomDrawNodeDetail(LPNMLVCUSTOMDRAW & plcd);
	LRESULT ProcNotifyCustomDraw(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT ProcNotifyGetDispInfo(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT ProcNotifyGetDispInfoTorrentContent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT ProcNotifyGetDispInfoTorrentList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT ProcNotifyGetDispInfoTorrentFile(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT ProcNotifyGetDispInfoGroupList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int CreateColumns(HWND hlist, ColDefVT * cols);
	int CreateGroupColumns();
	int CreateTorrentListColumns();
	int CreateTorrentFileColumns();
	int CreateTorrentColumns();
	int UpdateTorrentContentWidth(HWND hlist);
	int UpdateTorrentContent(TorrentNodeVT * node);
	int UpdateTorrentContentData(TorrentNodeVT * node);
	int UpdateTorrentContentTitle();
	int UpdateGroupView(TorrentGroupVT * group);
	int AutoHeaderWidth();
	int ClearList();
	HWND GetWnd();
	ViewNode* GetCurrentViewNode();
	int SetCurrentViewNode(ViewNode* vnode);
	void ChangeViewNodeFile(ViewNode * vnode);
	void ChangeViewNodeSession(ViewNode * vnode);
	void ChangeViewNodeTorrent(ViewNode * vnode);
	void ChangeViewNodeGroupGroup(ViewNode * vnode);
	void ChangeViewNodeGroup(ViewNode * vnode);
	LISTCONTENT ChangeContent(LISTCONTENT ctt);
};

