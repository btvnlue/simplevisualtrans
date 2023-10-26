#pragma once
#include <Windows.h>
#include <set>
#include <list>
#include <map>
#include <CommCtrl.h>

struct TorrentNode;
struct TorrentNodeFileNode;
struct SessionInfo;
class TorrentGroup;
class CProfileData;
class TransAnalyzer;

#define WM_U_TREESELECTNODE WM_USER + 0x123a
#define WM_U_TREESELECTGROUP WM_USER + 0x123b

struct ColDef {
	wchar_t name[20];
	int width = 100;
};

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

class CViewListFrame
{
	HWND hList;
	std::list<ListParmData*> listparms;

	HBRUSH hbWhite;
	HBRUSH hbGreen;
	HBRUSH hbRed;
	HPEN gradualpen[256];

	wchar_t** torrentDetailTitles;
	wchar_t** torrentSessionTitles;

public:
	enum class LISTCONTENT {
		TORRENTGROUP
		, TORRENTLIST
		, TORRENTDETAIL
		, SESSIONINFO
		, TORRENTFILE
		, NONE
	} listContent;
	std::map<TorrentNode*, int> nodeLocation;
	CProfileData* profile;
	TransAnalyzer* analyzer;
	HWND hMain;
	CViewListFrame(HWND hlist);
	void InitializeGradualPen();
	virtual ~CViewListFrame();
	void InitStyle();
	operator HWND() const;
	int TreeViewSelectItem(TorrentNode* node);
	int TreeViewSelectItem(TorrentGroup* group);
	int GetListSelectedTorrents(std::set<TorrentNode*>& tts);
	int GetListSelectedFiles(std::set<TorrentNodeFileNode*>& tfs);
	int UpdateListViewSession(SessionInfo* ssn);
	int ClearColumns();
	int DeleteNodeItem(TorrentNode* node);
	int ProcContextMenuList(int xx, int yy);
	int MakeListParmIdle(ListParmData* lpd);
	LRESULT ProcNotifyList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT ProcCustDrawListNodes(LPNMLVCUSTOMDRAW& plcd);
	LRESULT ProcCustDrawListDetail(LPNMLVCUSTOMDRAW& plcd);
	LRESULT ProcCustDrawListName(const LPNMLVCUSTOMDRAW& plcd, const COLORREF& ocr);
	LRESULT ProcCustDrawListPieces(const LPNMLVCUSTOMDRAW& plcd);
	LRESULT ProcCustDrawListFile(LPNMLVCUSTOMDRAW& plcd);
	ListParmData* GetListParmData(unsigned long idx);
	int CreateListColumnsSession();
	int CreateListColumnsFiles();
	int CreateListColumnsTorrent();
	int CreateListColumns(ColDef* cols);
	int UpdateListViewTorrentFileDetail(HWND hlist, int fii, TorrentNodeFileNode* cfl);
	int UpdateListViewTorrentFiles(TorrentNodeFileNode* files);
	int UpdateListViewTorrentDetailData(TorrentNode* node);
	int ListUpdateFiles(TorrentNodeFileNode* files);
	int ListUpdateSession(SessionInfo* ssn);
	int UpdateListViewTorrentDetail(TorrentNode* node);
	void ListUpdateTorrentTitle(TorrentNode* node);
	void ListUpdateSessionTitle(SessionInfo* ssn);
	void InitializeTorrentDetailTitle();
	int FreeTorrentDetailTitle();
	static int CBUpdateGroupNode(bool, void*, TorrentGroup*, TorrentNode*);
	int UpdateListViewTorrentNodes(TorrentGroup* grp);
	int UpdateListViewTorrentNodesDetail(HWND hlist, int iti, TorrentNode* nod);
	int UpdateListViewGroups();
	int UpdateListViewTorrentGroupData(HWND hlist, int iti, TorrentGroup* nod);
	int CreateListColumnsGroupList();
	int CreateListColumnsNodeList();
	int UpdateListViewTorrentGroup(TorrentGroup* grp);
};

