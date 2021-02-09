#pragma once

#include <Windows.h>
#include <CommCtrl.h>

#include "TransAnalyzer.h"

#define WM_U_CLEARLISTVIEW WM_USER + 0x1233
#define WM_U_DELETELISTNODE WM_USER + 0x1234
#define WM_U_UPDATETORRENTNODEDETAIL WM_USER + 0x1235
#define WM_U_UPDATETORRENTGROUP WM_USER + 0x1236
#define WM_U_UPDATESESSION WM_USER + 0x1237
#define WM_U_UPDATETORRENTNODEFILE WM_USER + 0x1238
#define WM_U_REFRESHTORRENTDETAIL WM_USER + 0xBBB

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


class TreeItemParmDataHelper {
	static std::set<TreeParmData*> parms;
public:
	static TreeParmData* CreateTreeItemParmData(TreeParmData::TIPDTYPE type);
	static int ClearTreeItemParmData();
};

class CViewTreeFrame
{
	HWND hTree;
public:
	TransAnalyzer* analyzer;
	HWND hMain;
	TreeGroupShadow* torrentsViewOrgs;

	CViewTreeFrame(HWND htree);
	virtual ~CViewTreeFrame();
	operator HWND() const;
	TreeParmData* GetItemParm(HTREEITEM hti);
	HTREEITEM GetSelectedItem();
	int UpdateTreeViewTorrentDetailNode(TorrentNode* node);
	int UpdateTreeViewTorrentDetail(TorrentParmItems* items);
	HTREEITEM UpdateTreeViewTorrentDetailFiles(HTREEITEM hpnt, TorrentNodeFileNode* fnode);
	HTREEITEM UpdateTreeViewTorrentDetailTrackers(TorrentParmItems* items, TorrentNode* node);
	int GetTreeSelectedFiles(std::set<TorrentNodeFileNode*>& trfs);
	int GetTreeSelectedTorrents(std::set<TorrentNode*>& trts);
	void UpdateViewRemovedTorrents();
	int TreeViewSelectItem(TorrentNode* node);
	LRESULT ProcNotifyTree(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int TreeViewSelectItem(TorrentGroup* grp);
	int ProcContextMenuTree(int xx, int yy);
	void UpdateTreeViewGroup(TreeGroupShadow* gti);
	void UpdateViewTorrentGroup(TorrentGroup* grp);
	HTREEITEM UpdateTreeViewNodeItem(TreeGroupShadow* tgs, TorrentNode* node);
	int SyncGroup(TorrentGroup* group);
	int SelectTreeParentNode();
};

