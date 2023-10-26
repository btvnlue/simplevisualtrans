#pragma once

#include <Windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <CommCtrl.h>

#include "TorrentNode.h"

class ViewManager;
class TransmissionProfile;
class TransmissionManager;

class ViewCommand
{
public:
	virtual ~ViewCommand() {
		if (callback) {
			delete callback;
		}
	}
	std::wstring name;
	int code = 0;
	DWORD timetorun = 0;
	virtual int Process(ViewManager* mng) = 0;
	ViewCommand* callback = NULL;
};

class VCInit : public ViewCommand
{
public:
	int Process(ViewManager* mng);
};

#define LISTSORT_ID 0
#define LISTSORT_NAME 1
#define LISTSORT_SIZE 2
#define LISTSORT_DOWNSPEED 3
#define LISTSORT_UPSPEED 4
#define LISTSORT_STATUS 5
#define LISTSORT_RATIO 6
#define LISTSORT_LOCATION 7
#define LISTSORT_TRACKER 8
#define LISTSORT_ERROR 9

#define LISTFILESORT_ID 0
#define LISTFILESORT_HAS 1
#define LISTFILESORT_PR 2
#define LISTFILESORT_NAME 3
#define LISTFILESORT_PATH 4
#define LISTFILESORT_SIZE 5

#define TNP_ID 0
#define TNP_NAME 1
#define TNP_SIZE 2
#define TNP_ADDDATE 3
#define TNP_STARTDATE 4
#define TNP_DONEDATE 5
#define TNP_DOWNLOADED 6
#define TNP_UPLOADED 7
#define TNP_LEFT 8
#define TNP_AVIALABLE 9
#define TNP_DOWNSPEED 10
#define TNP_UPSPEED 11
#define TNP_STATUS 12
#define TNP_PIECES 13
#define TNP_PIECECOUNT 14
#define TNP_PIECESIZE 15
#define TNP_PATH 16
#define TNP_ERROR 17

class VCRotation : public ViewCommand
{
public:
	int Process(ViewManager* mgr);

};

#define LTV_REQUEST 0
#define LTV_REFRESH 1
#define LTV_RESULT 2
#define LTV_VIEWCHANGE 3
#define LTV_DELETENODE 4
#define LTV_COMMITNODE 5
#define LTV_COMMENTMESSAGE 6
#define LTV_RESULT_TORRENT 7
#define LTV_RESULT_FILE 8
#define LTV_RESULT_VIEWNODE 9

#define LTV_FINALCOUNT 10 /////////////////////// the last#, for view update/setcount

struct ViewLogItem
{
	int id;
	long timestamp;
	WCHAR data[1024] = { 0 };
};

class ViewManager
{
	static DWORD WINAPI ThViewManagerProc(LPVOID lpParam);
	HANDLE hTh;
	BOOL keeprun;
	HANDLE hEvent;

	std::list<ViewCommand*> cmds;
	std::map<std::wstring, TransmissionProfile*> profiles;

	ViewCommand* GetCommand();

public:
	HWND hTree;
	HWND hList;
	HWND hLog;
	HWND hWnd;
	HWND hInput;
	ViewNode* clipboardRoot;
	TransmissionProfile* defaultprofile;
	ItemArray<ViewLogItem*> log;

	ViewManager();
	virtual ~ViewManager();
	int Start();
	int Stop();
	int DoneProfileLoading(TransmissionProfile* prof);
	TransmissionManager* transmissionmgr;
	int PutCommand(ViewCommand* cmd);
	//int ViewUpdateProfileTorrent(TransmissionProfile *prof, long tid);
	int ViewUpdateInvalidViewNode(ViewNode* vnd);
	int RefreshCurrentNode();
	
	int ChangeSelectedViewNode(ViewNode* vnd);
	int LogToView(int logid, const std::wstring& logmsg);
	//int TimerRefreshTorrent(TransmissionProfile* prof);
	int ProcessClipboardEntry();
	int SelectClipboardRoot();
	int ViewUpdateViewNode(ViewNode* vnd);
	int AddLinkByDrop(WCHAR* pth);
	int ViewCommitTorrents(int delay);
	int ViewDeleteContent(BOOL withfiles);
	int ViewPauseTorrent(BOOL dopause);
	int ViewEnableFile(BOOL enable);
	int ViewPriorityFile(int setpriority);
	int ViewVerifyTorrent();
	int ViewSetLocation();
	int ViewSetLimit(int vslup);
	int GetSelectedViewNodes(std::set<ViewNode*>& vds);
	int ProcessInputCommand();
	int ProcessInputCommandSetLocation(std::vector<std::wstring>& cmds);
	int ProcessInputCommandSetLimit(std::vector<std::wstring>& cmds);
	int ProcessInputCommandEnableLimit(std::vector<std::wstring>& cmds);
	int ProcessInputCommandSetTorrentLimit(std::vector<std::wstring>& cmds);


	TransmissionProfile* GetCurrentProfile();
	int FullRefreshProfileNodes(TransmissionProfile* prof);
	int RefreshProfileNodeDetail(TransmissionProfile* prof, long tid);
	int RefreshProfileStatus(TransmissionProfile* prof);
	TransmissionProfile* GetViewNodeProfile(ViewNode* vnd);
	TorrentNode* GetCurrentTorrent();
	int ShowContextMenu(int xx, int yy);

	/////////////////////////////////////////////////////////
	// Tree section for treeview update
	/////////////////////////////////////////////////////////

	std::map<ViewNode*, HTREEITEM> tree_node_handle_map;
	HTREEITEM TreeUpdateViewNode(ViewNode* vnd);
	int TreeMoveParenetViewNode();
	int TreeExpandItem(HTREEITEM hitem);
	int TreeSortTorrentGroup(HTREEITEM hitem);
	int ViewSortProfileTotal(TransmissionProfile* prof);

	/////////////////////////////////////////////////////////
	// List section for listview update
	/////////////////////////////////////////////////////////
	std::vector<ViewNode*> currentnodelist;
	ViewNode* currentnode;
	long pieceItemIndex;
	int listsortindex;
	bool listsortdesc;
	//std::map<int, std::wstring> tnpnames;
	//std::map<int, std::wstring> tnpvalues;
	std::vector<std::pair<std::wstring, std::wstring>> tnpdisp;

	int ListClearContent();
	int ListSwitchContent();
	int ListSortTorrents(std::vector<ViewNode*>& tns, int sidx, bool asc);
	int ListSortFiles(std::vector<ViewNode*>& tns, int sidx, bool asc);
	int ListUpdateViewNode(ViewNode* vnd);
	int ListSortTorrentGroup(int sidx);
	int ListBuildTorrentContent(ViewNode* vnd);
	int ListBuildClipboardContent(ViewNode* vnd);
	int ListBuildFileContent(ViewNode* vnd);
	int ListUpdateViewNodeInvalid(ViewNode* vnd);
	int ListGetSelectedViewNodes(std::set<ViewNode*>& vds);
};

