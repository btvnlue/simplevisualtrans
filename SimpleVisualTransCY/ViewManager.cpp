#include "ViewManager.h"
#include "TransmissionProfile.h"
#include "TransmissionManager.h"

#include <set>
#include <vector>
#include <windowsx.h>
#include <algorithm>
#include <sstream>
#include <string>

WCHAR wbuf[1024];

class VCCreateClipboardRoot : public ViewCommand
{
public:
	virtual int Process(ViewManager* mgr) {
		if (mgr->clipboardRoot == NULL) {
			mgr->clipboardRoot = ViewNode::NewViewNode(VNT_CLIPBOARD);
			mgr->clipboardRoot->name = L"Clipboard";
			mgr->ViewUpdateViewNode(mgr->clipboardRoot);
		}
		return 0;
	}
};

DWORD WINAPI ViewManager::ThViewManagerProc(LPVOID lpParam)
{
	ViewManager* self = reinterpret_cast<ViewManager*>(lpParam);
	ViewCommand* cmd;

	while (self->keeprun) {
		cmd = self->GetCommand();
		if (cmd == NULL) {
			::Sleep(100);
		}
		else {
			cmd->Process(self);
			if (cmd->callback) {
				cmd->callback->code = cmd->code;
				cmd->callback->Process(self);
			}
			delete cmd;
		}
	}

	return 0;

}

int ViewManager::Start()
{
	if (transmissionmgr == nullptr) {
		transmissionmgr = new TransmissionManager();
	}
	transmissionmgr->Start();


	DWORD tid;
	if (hEvent == NULL) {
		hEvent = ::CreateEvent(NULL, FALSE, TRUE, NULL);
	}
	VCCreateClipboardRoot* ccc = new VCCreateClipboardRoot();
	PutCommand(ccc);
	keeprun = TRUE;
	hTh = ::CreateThread(NULL, 0, ThViewManagerProc, this, 0, &tid);


	return 0;
}

int ViewManager::Stop()
{
	DWORD dcd;

	if (hTh) {
		keeprun = FALSE;
		dcd = STILL_ACTIVE;
		while (dcd == STILL_ACTIVE) {
			::Sleep(100);
			::GetExitCodeThread(hTh, &dcd);
		}
	}

	if (transmissionmgr) {
		transmissionmgr->Stop();
		delete transmissionmgr;
		transmissionmgr = nullptr;
	}

	if (hEvent) {
		CloseHandle(hEvent);
		hEvent = NULL;
	}
	return 0;
}

class TMCBRefreshTorrents : public CmdRefreshTorrentsCB
{
public:
	TransmissionProfile* profile;
	ViewManager* view;
	int Process(ItemArray<TorrentNode*>* nodes, CmdRefreshTorrents *cmd) {
		if (cmd->returncode == 0) {
			if (nodes) {
				if (profile) {
					if (nodes->count > 0) {
						std::set<long> nts;
						std::set<long> uts;
						int rtn;
						std::set<long> ids;

						wsprintf(wbuf, L"Found [%d] torrents from [%s]...", nodes->count, profile->url.c_str());
						view->LogToView(VLI_RESULT, wbuf);

						for (std::map<long, TorrentNode*>::iterator ittn = profile->torrents.begin();
							ittn != profile->torrents.end();
							ittn++) {
							if (ittn->second->valid) {
								ids.insert(ittn->first);
							}
						}
						for (unsigned int ii = 0; ii < nodes->count; ii++) {
							rtn = profile->UpdateTorrent(&nodes->items[ii]);
							if (rtn == UPN_NEWNODE) {
								nts.insert(nodes->items[ii].id);
							}
							else {
								ids.erase(nodes->items[ii].id);
								if (rtn == UPN_UPDATE) {
									uts.insert(nodes->items[ii].id);
								}
							}
						}

						// new nodes, update in view, TreeView, ListView
						for (std::set<long>::iterator itid = nts.begin();
							itid != nts.end();
							itid++) {
							view->ViewUpdateProfileTorrent(profile, *itid);
						}
						for (std::set<long>::iterator itud = uts.begin();
							itud != uts.end();
							itud++) {
							view->ViewUpdateProfileTorrent(profile, *itud);
						}

						std::map<long, TorrentNode*>::iterator ittn;
						for (std::set<long>::iterator itdl = ids.begin();
							itdl != ids.end();
							itdl++) {
							ittn = profile->torrents.find(*itdl);
							if (ittn != profile->torrents.end()) {
								ittn->second->valid = false;
							}
							profile->UpdateViewNodeInvalid(*itdl);
							view->ViewUpdateProfileTorrent(profile, *itdl);
						}
						UpdateWindow(view->hList);
					}
				}
			}
			else {
				wsprintf(wbuf, L"Done refreshing torrents from [%s]...", profile->url.c_str());
				view->LogToView(VLI_VIEWCHANGE, wbuf);

				//view->TimerRefreshTorrent(profile);
			}
		} // cmd->return = 0
		else {
			switch (cmd->returncode) {
			case CRT_E_TIMEOUT:
				wsprintf(wbuf, L"Error in refreshing torrents: [TIME_OUT]...");
				break;
			default:
				wsprintf(wbuf, L"Error in refreshing torrents: [%d]...", cmd->returncode);
			}

			view->LogToView(VLI_VIEWCHANGE, wbuf);
		}
		return 0;
	}
};

TMCBRefreshTorrents *crt = NULL;

int ViewManager::DoneProfileLoading(TransmissionProfile * prof)
{
	profiles[prof->name] = prof;
	prof->profileview->name = prof->url;
	defaultprofile = prof;

	wsprintf(wbuf, L"Done loading [%s] profile.", prof->name.c_str());
	LogToView(VLI_REQUEST, wbuf);

	CmdRefreshTorrents *cmd = new CmdRefreshTorrents();
	cmd->profile = prof;

	crt = crt == NULL ? new TMCBRefreshTorrents() : crt;
	crt->profile = prof;
	crt->view = this;

	cmd->callback = crt;

	wsprintf(wbuf, L"Refresh torrents from [%s]...", prof->url.c_str());
	LogToView(VLI_REFRESH, wbuf);

	transmissionmgr->PutCommand(cmd);

	return 0;
}

int ViewManager::PutCommand(ViewCommand * cmd)
{
	if (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {
		cmds.push_back(cmd);
		::SetEvent(hEvent);
	}
	return 0;
}

int ViewManager::ViewUpdateProfileTorrent(TransmissionProfile * prof, long tid)
{
	std::set<ViewNode*> vns;
	ViewNode* vnd;
	prof->GetTorrentViewNodes(tid, vns);
	for (std::set<ViewNode*>::iterator itvn = vns.begin();
		itvn != vns.end();
		itvn++) {
		vnd = *itvn;
		if (vnd->GetTorrent()) {
			if (vnd->GetTorrent()->valid == false) {
				vnd->valid = false;
			}
		}
		ViewUpdateViewNode(*itvn);
	}
	return 0;
}

int ViewManager::ViewUpdateInvalidViewNode(ViewNode* vnd)
{
	std::map<ViewNode*, HTREEITEM>::iterator ittm;
	if (vnd->valid == false) {
		// remove from tree view
		ittm = tree_node_handle_map.find(vnd);
		if (ittm != tree_node_handle_map.end()) {
			PostMessage(hTree, TVM_DELETEITEM, 0, (LPARAM)ittm->second);
		}
		// remove from list view
		ListUpdateViewNodeInvalid(vnd);
	}
	return 0;
}

int ViewManager::RefreshProfiles()
{
	CmdRefreshTorrents* cmd;
	for (std::map<std::wstring, TransmissionProfile*>::iterator itpf = profiles.begin();
		itpf != profiles.end();
		itpf++) {
		if (itpf->second->inrefresh == false) {
			cmd = new CmdRefreshTorrents();
			cmd->profile = itpf->second;
			crt = crt == NULL ? new TMCBRefreshTorrents() : crt;
			crt->profile = cmd->profile;
			crt->view = this;
			cmd->callback = crt;

			wsprintf(wbuf, L"Refresh torrents from [%s]...", cmd->profile->url.c_str());
			LogToView(VLI_REFRESH, wbuf);

			transmissionmgr->PutCommand(cmd);
		}
	}

	return 0;
}

int ViewManager::ChangeSelectedViewNode(ViewNode * vnd)
{
	if (currentnode != vnd) {
		currentnode = vnd;
		ListSwichContent();
	}
	return 0;
}

HTREEITEM ViewManager::TreeUpdateViewNode(ViewNode * vnd)
{
	HTREEITEM rtn = NULL;

	std::map<ViewNode*, HTREEITEM>::iterator itvn = tree_node_handle_map.find(vnd);
	if (itvn == tree_node_handle_map.end()) {
		HTREEITEM pth = NULL;
		if (vnd->parent) {
			pth = TreeUpdateViewNode(vnd->parent);
		}

		TVINSERTSTRUCT tis = { 0 };
		tis.hParent = pth;
		tis.hInsertAfter = TVI_LAST;

		tis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
		tis.item.cChildren = I_CHILDRENCALLBACK;
		tis.item.pszText = LPSTR_TEXTCALLBACK;
		tis.item.lParam = (LPARAM)vnd;

		rtn = TreeView_InsertItem(hTree, &tis);
		tree_node_handle_map[vnd] = rtn;
	}
	else {
		rtn = itvn->second;
		TVITEM tvi;
		tvi.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
		tvi.hItem = itvn->second;
		tvi.cChildren = I_CHILDRENCALLBACK;
		tvi.pszText = LPSTR_TEXTCALLBACK;
		tvi.lParam = (LPARAM)vnd;

		TreeView_SetItem(hTree, &tvi);
	}
	return rtn;
}

int ViewManager::TreeMoveParenetViewNode()
{
	if (currentnode) {
		std::map<ViewNode*, HTREEITEM>::iterator ithm = tree_node_handle_map.find(currentnode);
		if (ithm != tree_node_handle_map.end()) {
			HTREEITEM hpi = TreeView_GetNextItem(hTree, ithm->second, TVGN_PARENT);
			if (hpi) {
				PostMessage(hTree, TVM_SELECTITEM, (WPARAM)TVGN_CARET, (LPARAM)hpi);
			}
		}
	}
	return 0;
}

int ViewManager::TreeExpandItem(HTREEITEM hitem)
{
	TVITEM tim;
	tim.mask = TVIF_HANDLE | TVIF_PARAM;
	tim.hItem = hitem;
	TreeView_GetItem(hTree, &tim);

	ViewNode* vnd = (ViewNode*)tim.lParam;
	ViewNode* snd;
	std::map<ViewNode*, HTREEITEM>::iterator ittm;
	if (vnd) {
		for (std::list<ViewNode*>::iterator itvn = vnd->nodes.begin();
			itvn != vnd->nodes.end();
			itvn++) {
			snd = *itvn;
			TreeUpdateViewNode(snd);
		}
	}
	return 0;
}

int ViewManager::ListClearContent()
{
	ListView_DeleteAllItems(hList);

	HWND hhd = ListView_GetHeader(hList);
	if (hhd) {
		int hdc = Header_GetItemCount(hhd);
		for (int ii = 0; ii < hdc; ii++) {
			//		Header_DeleteItem(hhd, 0); // cause un-consistent, click-select out of work
			ListView_DeleteColumn(hList, 0);
		}
	}
	return 0;
}

int ViewManager::ListSwichContent()
{
	if (currentnode) {
		int vnt = currentnode->GetType();
		switch (vnt) {
		case VNT_GROUP:
		{
			ListClearContent();

			int cols = 0;
			LVCOLUMN lvc;
			lvc.iSubItem = 0;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"ID";
			lvc.cx = 50;
			lvc.fmt = LVCFMT_LEFT;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Name";
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.cx = 500;
			lvc.fmt = LVCFMT_LEFT;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Size";
			lvc.fmt = LVCFMT_RIGHT;
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.cx = 100;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Download";
			lvc.fmt = LVCFMT_RIGHT;
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.cx = 100;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.fmt = LVCFMT_RIGHT;
			lvc.pszText = (LPWSTR)L"Upload";
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.cx = 100;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Status";
			lvc.fmt = LVCFMT_RIGHT;
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.cx = 100;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Ratio";
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.fmt = LVCFMT_RIGHT;
			lvc.cx = 100;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Location";
			lvc.fmt = LVCFMT_LEFT;
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.cx = 100;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			std::set<ViewNode*> tts;
			currentnode->GetAllTorrentsViewNode(tts);
			currentnodelist.clear();
			currentnodelist.insert(currentnodelist.begin(), tts.begin(), tts.end());

			ListSortTorrentGroup(listsortindex);
		}
		break;
		case VNT_TORRENT:
		{
			ListClearContent();

			int cols = 0;
			LVCOLUMN lvc;
			lvc.iSubItem = 0;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Parameter";
			lvc.cx = 200;
			lvc.fmt = LVCFMT_LEFT;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = 0;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Value";
			lvc.cx = 630;
			lvc.fmt = LVCFMT_LEFT;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			ListBuildTorrentNodeContent(currentnode);
			ListView_SetItemCountEx(hList, tnpvalues.size(), LVSICF_NOINVALIDATEALL);

		}
		break;
		case VNT_CLIPBOARD:
		case VNT_CLIPBOARD_ITEM:
		{
			ListClearContent();

			int cols = 0;
			LVCOLUMN lvc;
			lvc.iSubItem = 0;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Type";
			lvc.cx = 50;
			lvc.fmt = LVCFMT_LEFT;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = 0;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"URL";
			lvc.cx = 700;
			lvc.fmt = LVCFMT_LEFT;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			ListBuildClipboardContent(currentnode);
			ListView_SetItemCountEx(hList, currentnodelist.size(), LVSICF_NOINVALIDATEALL);
		}
		break;
		}
	}
	else {	//viewnode vnd : null
		ListClearContent();
	}
	return 0;
}


class torrentless
{
	int colindex;
	bool sortasc;
public:
	torrentless(int cid, bool asc) : colindex(cid), sortasc(asc) {}
	bool operator()(ViewNode* lt, ViewNode* rt) {
		bool rtn = false;
		int icc;
		bool equ = false;
		TorrentNode* ltt = lt->GetTorrent();
		TorrentNode* rtt = rt->GetTorrent();
		if (ltt) {
			if (rtt) {
				switch (colindex) {
				case LISTSORT_NAME:
					icc = ltt->name.compare(rtt->name);
					rtn = icc < 0;
					equ = icc == 0;
					break;
				case LISTSORT_ID:
					rtn = ltt->id < rtt->id;
					equ = ltt->id == rtt->id;
					break;
				case LISTSORT_STATUS:
					rtn = ltt->status < rtt->status;
					equ = ltt->status == rtt->status;
					break;
				case LISTSORT_SIZE:
					rtn = ltt->size < rtt->size;
					equ = ltt->size == rtt->size;
					break;
				case LISTSORT_DOWNSPEED:
					rtn = ltt->downspeed < rtt->downspeed;
					equ = ltt->downspeed == rtt->downspeed;
					break;
				case LISTSORT_UPSPEED:
					rtn = ltt->upspeed < rtt->upspeed;
					equ = ltt->upspeed == rtt->upspeed;
					break;
				case LISTSORT_RATIO:
					rtn = ltt->ratio < rtt->ratio;
					equ = ltt->ratio == rtt->ratio;
					break;
				case LISTSORT_LOCATION:
					rtn = wcscmp(ltt->_path, rtt->_path);
					equ = rtn == 0;
					break;

				default:
					equ = true;
					rtn = false;
				}
			}
			else {
				rtn = false;
				equ = false;
			}
		}
		else {
			if (rtt) {
				rtn = true;
				equ = false;
			}
			else {
				rtn = false;
				equ = true;
			}
		}

		rtn = sortasc ? rtn : (equ ? false : !rtn);

		return rtn;
	}
};

int ViewManager::ListSortTorrents(std::vector<ViewNode*>& tns, int sidx, bool asc)
{
	std::sort(tns.begin(), tns.end(), torrentless(sidx, asc));
	return 0;
}

int ViewManager::ListUpdateViewNode(ViewNode* vnupd)
{
	if (currentnode) {
		int cvt = currentnode->GetType();
		switch (cvt) {
		case VNT_GROUP:
			if (currentnode->HasViewNode(vnupd)) {
				if (vnupd->GetType() == VNT_TORRENT) {
					std::vector<ViewNode*>::iterator itvn = std::find(currentnodelist.begin(), currentnodelist.end(), vnupd);
					if (itvn == currentnodelist.end()) { // new vn to display
						currentnodelist.push_back(vnupd);
						ListSortTorrentGroup(listsortdesc);
					}
					else {
						int ipos = itvn - currentnodelist.begin();
						ListView_RedrawItems(hList, ipos, ipos);
					}
				}
			} // current node hasviewnode
			break;
		case VNT_TORRENT:
			if (vnupd->GetType() == VNT_TORRENT) {
				if (currentnode->GetTorrent()->id == vnupd->GetTorrent()->id) {
					if (vnupd->GetTorrent()->state != 0) {
						ListBuildTorrentNodeContent(vnupd);
						long lstcnt = ListView_GetItemCount(hList);
						ListView_RedrawItems(hList, 0, lstcnt);
					}
				}
			} // vnupd type torrent
			break;
		case VNT_CLIPBOARD:
		case VNT_CLIPBOARD_ITEM:
		{
			ViewNode* clipnode = cvt == VNT_CLIPBOARD ? currentnode : currentnode->parent;
			if (clipnode->HasViewNode(vnupd)) {
				std::vector<ViewNode*>::iterator itvn = std::find(currentnodelist.begin(), currentnodelist.end(), vnupd);
				if (itvn == currentnodelist.end()) { // new vn to display
					currentnodelist.push_back(vnupd);
					ListView_SetItemCountEx(hList, currentnodelist.size(), LVSICF_NOINVALIDATEALL);
					//ListView_RedrawItems(hList, 0, currentnodelist.size());
				}
			}
		}
		break;
		}

		UpdateWindow(hList);
	}
	return 0;
}

int ViewManager::ListSortTorrentGroup(int sidx)
{
	ListSortTorrents(currentnodelist, sidx, listsortdesc);
	int ioc = ListView_GetItemCount(hList);
	if (ioc == currentnodelist.size()) {
		ListView_RedrawItems(hList, 0, ioc - 1);
	}
	else {
		ListView_SetItemCount(hList, currentnodelist.size());
	}

	return 0;
}

int ViewManager::ListBuildTorrentNodeContent(ViewNode * vnd)
{
	if (tnpnames.size() <= 0) {
		tnpnames[TNP_ID] = L"ID";
		tnpnames[TNP_NAME] = L"Name";
		tnpnames[TNP_SIZE] = L"Size";
		tnpnames[TNP_ADDDATE] = L"Add Date";
		tnpnames[TNP_STARTDATE] = L"Start Date";
		tnpnames[TNP_DONEDATE] = L"Completed Date";
		tnpnames[TNP_DOWNLOADED] = L"Downloaded Size";
		tnpnames[TNP_UPLOADED] = L"Uploaded Size";
		tnpnames[TNP_LEFT] = L"Left Size";
		tnpnames[TNP_AVIALABLE] = L"Avialable";
		tnpnames[TNP_DOWNSPEED] = L"Download Speed /s";
		tnpnames[TNP_UPSPEED] = L"Upload Speed /s";
		tnpnames[TNP_STATUS] = L"Status";
		tnpnames[TNP_PIECES] = L"Pieces";
		tnpnames[TNP_PIECECOUNT] = L"Pieces Count";
		tnpnames[TNP_PIECESIZE] = L"Piece Size";
		tnpnames[TNP_PATH] = L"Path";
		tnpnames[TNP_ERROR] = L"Error";
	}

	if (vnd) {
		TorrentNode* vtt = vnd->GetTorrent();
		if (vtt) {
			tnpvalues.clear();
			tnpvalues[TNP_ID] = vtt->dispid;
			tnpvalues[TNP_NAME] = vtt->name;
			tnpvalues[TNP_SIZE] = vtt->dispsize;
			tnpvalues[TNP_ADDDATE] = vtt->dispadddate;
			tnpvalues[TNP_STARTDATE] = vtt->dispstartdate;
			tnpvalues[TNP_DONEDATE] = vtt->dispdonedate;
			tnpvalues[TNP_DOWNLOADED] = vtt->dispdownloaded;
			tnpvalues[TNP_UPLOADED] = vtt->dispuploaded;
			tnpvalues[TNP_LEFT] = vtt->displeft;
			tnpvalues[TNP_AVIALABLE] = vtt->dispavialable;
			tnpvalues[TNP_DOWNSPEED] = vtt->dispdownspeed;
			tnpvalues[TNP_UPSPEED] = vtt->dispupspeed;
			tnpvalues[TNP_STATUS] = vtt->dispstatus;
			tnpvalues[TNP_PIECES] = vtt->pieces;
			tnpvalues[TNP_PIECECOUNT] = vtt->disppiececount;
			tnpvalues[TNP_PIECESIZE] = vtt->disppiecesize;
			tnpvalues[TNP_PATH] = vtt->_path;
			tnpvalues[TNP_ERROR] = vtt->_error;
		}
	}
	return 0;
}

int ViewManager::ListBuildClipboardContent(ViewNode * vnd)
{
	ViewNode* bnd = NULL;
	if (vnd) {
		if (vnd->GetType() == VNT_CLIPBOARD_ITEM) {
			bnd = vnd->parent;
		}
		else if (vnd->GetType() == VNT_CLIPBOARD) {
			bnd = vnd;
		}

		if (bnd) {
			currentnodelist.clear();
			for (std::list<ViewNode*>::iterator itvn = bnd->nodes.begin();
				itvn != bnd->nodes.end();
				itvn++) {
				if ((*itvn)->valid) {
					currentnodelist.insert(currentnodelist.end(), *itvn);
				}
			}
		}
	}
	return 0;
}

int ViewManager::ListUpdateViewNodeInvalid(ViewNode * vnd)
{
	bool btn;
	long icnt;
	std::vector<ViewNode*>::iterator itvn;

	if (vnd->valid == false) {
		if (currentnode) {
			if (currentnode == vnd) {
				TreeMoveParenetViewNode();
			}
			else {
				btn = currentnode->HasViewNode(vnd);
				if (btn) {
					itvn = std::find(currentnodelist.begin(), currentnodelist.end(), vnd);
					if (itvn != currentnodelist.end()) {
						currentnodelist.erase(itvn);
						icnt = currentnodelist.size();
						PostMessage(hList, LVM_SETITEMCOUNT, (WPARAM)icnt, 0);
						PostMessage(hList, LVM_REDRAWITEMS, 0, icnt);
					}
				}
			}
		}
	}

	return 0;
}

int ViewManager::ListGetSelectedViewNodes(std::set<ViewNode*>& vds)
{
	bool keepseek = true;
	int currpos = -1;
	while (keepseek) {
		currpos = ListView_GetNextItem(hList, currpos, LVNI_SELECTED);
		if (currpos >= 0) {
			if ((int)currentnodelist.size() > currpos) {
				vds.insert(currentnodelist.at(currpos));
			}
		}
		else {
			keepseek = false;
		}
	}
	return 0;
}

ViewCommand* ViewManager::GetCommand()
{
	ViewCommand* cmd = NULL;
	if (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {
		if (cmds.size() > 0) {
			cmd = *cmds.begin();
			cmds.erase(cmds.begin());

			DWORD ctm = GetTickCount();
			if (ctm < cmd->timetorun) {
				cmds.push_back(cmd);
				cmd = NULL;
			}
		}
		::SetEvent(hEvent);
	}
	return cmd;
}

class VCCBProfileLoaded : public ManagerCommand
{
public:
	ManagerCommand* cmd;
	TransmissionProfile* profile;
	ViewManager* view;
	int Process(TransmissionManager* mgr) {
		view->DoneProfileLoading(profile);

		return 0;
	}

};

VCCBProfileLoaded *cbc = NULL;

int VCInit::Process(ViewManager * mng)
{
	TransmissionProfile* prof = new TransmissionProfile();
	prof->name = L"default";

	wsprintf(wbuf, L"Load [%s] profile...", prof->name.c_str());
	mng->LogToView(VLI_VIEWCHANGE, wbuf);

	CmdLoadProfile* clp = new CmdLoadProfile();
	clp->profile = prof;
	if (cbc == NULL) {
		cbc = new VCCBProfileLoaded();
	}
	cbc->cmd = clp;
	cbc->view = mng;
	cbc->profile = prof;
	clp->callback = cbc;
	mng->transmissionmgr->PutCommand(clp);

	return 0;
}

ViewManager::ViewManager()
	: currentnode(nullptr),
	listsortindex(0),
	listsortdesc(true),
	clipboardRoot(NULL)
{
	log.count = 128;
	log.items = new ViewLogItem[log.count];
}

ViewManager::~ViewManager()
{
	Stop();

	if (log.count) {
		delete[] log.items;
		log.items = NULL;
		log.count = 0;
	}
	for (std::map<std::wstring, TransmissionProfile*>::iterator itpm = profiles.begin()
		; itpm != profiles.end()
		; itpm++) {
		delete itpm->second;
	}
	profiles.clear();

	if (cbc) {
		delete cbc;
		cbc = NULL;
	}
	if (crt) {
		delete crt;
		crt = NULL;
	}
}

int ViewManager::LogToView(int logid, const std::wstring& logmsg)
{
	if (hLog) {
		if (logid < (int)log.count) {
			SYSTEMTIME stm;
			::GetLocalTime(&stm);
			wsprintf(log.items[logid].data, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] : %s"
				, stm.wYear, stm.wMonth, stm.wDay
				, stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds
				, logmsg.c_str());
			log.items[logid].timestamp = GetTickCount();

			int rtn;
			rtn = ListBox_GetCurSel(hLog);
			SendMessage(hLog, LB_SETCOUNT, VLI_FINALCOUNT, 0);
			if (rtn >= 0) {
				ListBox_SetCurSel(hLog, rtn);
			}
		}
	}

	return 0;
}

int ViewManager::TimerRefreshTorrent(TransmissionProfile * prof)
{
	CmdRefreshTorrents *cmd = new CmdRefreshTorrents();
	cmd->profile = prof;
	crt = crt == NULL ? new TMCBRefreshTorrents() : crt;
	crt->profile = prof;
	crt->view = this;
	cmd->callback = crt;
	cmd->timetorun = GetTickCount() + 10000;
	SYSTEMTIME stm;
	::GetLocalTime(&stm);
	double vct;
	SystemTimeToVariantTime(&stm, &vct);
	vct += 10.0 / (24 * 60 * 60);
	VariantTimeToSystemTime(vct, &stm);
	WCHAR lbuf[1024];
	wsprintf(lbuf, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d]"
		, stm.wYear, stm.wMonth, stm.wDay
		, stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds);

	wsprintf(wbuf, L"Timer Refresh torrents %s from [%s]...", lbuf, prof->url.c_str());
	LogToView(VLI_REQUEST, wbuf);

	transmissionmgr->PutCommand(cmd);

	return 0;
}

class VCClipboardEntry : public ViewCommand
{
public:
	wchar_t* entry = NULL;
	int SetEntry(wchar_t* ent) {
		size_t eln = wcslen(ent);
		if (eln > 0) {
			entry = (wchar_t*)malloc((eln + 1) * sizeof(wchar_t));
			wcscpy_s(entry, eln + 1, ent);
		}

		return 0;
	}
	virtual int Process(ViewManager* mng) {
		bool doadd;
		if (entry) {
			std::wstringstream wss(entry);
			std::wstring lns;
			ViewNode* vnurl;
			while (std::getline(wss, lns)) {
				lns.erase(std::remove(lns.begin(), lns.end(), L'\r'), lns.end());
				int len = (int)lns.length() + 1;
				if (len > 1) {
					doadd = false;
					vnurl = ViewNode::NewViewNode(VNT_CLIPBOARD_ITEM);
					vnurl->name = lns;
					//WCHAR* newres = new WCHAR[len];
					//wcscpy_s(newres, len, lns.c_str());
					mng->clipboardRoot->AddNode(vnurl);
					mng->ViewUpdateViewNode(vnurl);
				}
			}

			free(entry);
			entry = NULL;
		}
		return 0;
	}

};
int ViewManager::ProcessClipboardEntry()
{
	BOOL brt = OpenClipboard(hWnd);
	int keeptry = brt == FALSE ? 3 : 0;
	while (keeptry > 0) {
		Sleep(10);
		brt = OpenClipboard(hWnd);
		keeptry -= brt == FALSE ? 1 : keeptry;
	}
	if (brt) {
		HANDLE hgd = GetClipboardData(CF_UNICODETEXT);
		if (hgd) {
			WCHAR* wbuf = (WCHAR*)GlobalLock(hgd);
			size_t bsz = wcslen(wbuf);
			if (bsz > 0) {
				VCClipboardEntry* vcc = new VCClipboardEntry();
				vcc->SetEntry(wbuf);
				PutCommand(vcc);
			}
			GlobalUnlock(hgd);
		}
		brt = CloseClipboard();
	}
	else {
		DWORD lerr = GetLastError();
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, lerr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			wbuf,
			1024,
			NULL);
		LogToView(VLI_VIEWCHANGE, wbuf);
	}
	return 0;
}

int ViewManager::ViewUpdateViewNode(ViewNode * vnd)
{
	bool doneproc = false;
	TorrentNode* vnt = vnd->GetTorrent();
	if (vnd) {
		if (vnd->valid == false) {
			wsprintf(wbuf, L"Delete node [%s]", vnd->name.c_str());
			LogToView(VLI_DELETENODE, wbuf);
			ViewUpdateInvalidViewNode(vnd);
		}
		else {
			TreeUpdateViewNode(vnd);
			ListUpdateViewNode(vnd);
		}
	}


	return 0;
}

int ViewManager::AddLinkByDrop(WCHAR * pth)
{
	VCClipboardEntry* vcc = new VCClipboardEntry();
	vcc->SetEntry(pth);
	PutCommand(vcc);

	return 0;
}

class ViewCommitCmdCB : public CmdCommitTorrentsCB
{
public:
	ViewManager* view = NULL;
	ViewNode* currentcommit;
	virtual int Process(CmdCommitTorrent* cmd, int code)
	{
		if (view) {
			wsprintf(wbuf, L"Commit %s [%s] code [%d]",
				code == 0 ? L"success" : L"error",
				cmd->entry.c_str(), code);
			view->LogToView(VLI_COMMITNODE, wbuf);

			if (code == 0) {
				currentcommit->valid = false;
				view->ViewUpdateViewNode(currentcommit);
			}

			view->ViewCommitTorrents(1000);
		}

		return 0;
	}
} vccb;

class VCCommitTorrent : public ViewCommand
{
public:
	virtual int Process(ViewManager* mgr)
	{
		ViewNode* vnd;
		CmdCommitTorrent* cct;
		if (mgr->clipboardRoot) {
			std::list<ViewNode*>::iterator itvn = mgr->clipboardRoot->nodes.begin();
			bool keepseek = itvn != mgr->clipboardRoot->nodes.end();
			while (keepseek) {
				if ((*itvn)->valid) {
					vnd = *itvn;
					vnd->valid = false;

					cct = new CmdCommitTorrent();
					cct->profile = mgr->defaultprofile;
					cct->entry = vnd->name;
					vccb.view = mgr;
					vccb.currentcommit = vnd;
					cct->callback = &vccb;

					mgr->transmissionmgr->PutCommand(cct);
					keepseek = false;
				}
				itvn++;
				keepseek = itvn != mgr->clipboardRoot->nodes.end() ? keepseek : false;
			}
		}
		return 0;
	}
};

int ViewManager::ViewCommitTorrents(int delay)
{
	VCCommitTorrent* vcc = new VCCommitTorrent();
	if (delay > 0) {
		vcc->timetorun = GetTickCount() + delay;
	}
	PutCommand(vcc);
	return 0;
}

class VCCBDeleteTorrents : public CmdDeleteTorrentsCB
{
public:
	ViewManager* view = NULL;
	virtual int Process(std::set<int>& ids, int code)
	{
		if (view) {
			wsprintf(wbuf, L"Delete [%d] torrents with code [%d]", ids.size(), code);
			view->LogToView(VLI_RESULT, wbuf);
		}
		return 0;
	}
} vcbdt;

#define VCA_DELETE 1

#define VCA_PARAM_WITHFILE 1
#define VCA_PARAM_WITHOUTFILE 0

class VCActionTorrent : public ViewCommand
{
public:
	int action;
	int actionparam;
	virtual int Process(ViewManager* view)
	{
		std::set<ViewNode*> vds;
		view->GetSelectedViewNodes(vds);
		ViewNode* vnd;

		if (vds.size() > 0) {
			CmdActionTorrent* cdt = new CmdActionTorrent();
			for (std::set<ViewNode*>::iterator itvn = vds.begin();
				itvn != vds.end();
				itvn++) {
				vnd = *itvn;
				if (vnd->GetTorrent()) {
					cdt->ids.insert(vnd->GetTorrent()->id);
				}
			}
			vcbdt.view = view;
			cdt->profile = view->defaultprofile;
			cdt->callback = &vcbdt;

			switch (action)
			{

			case VCA_DELETE:
				cdt->action = CATA_DELETE;
				cdt->actionparam = actionparam == VCA_PARAM_WITHFILE ? CATA_PARAM_DELETEFILES : CATA_PARAM_NODELETEFILES;
				break;
			default:
				cdt->action = CATA_NONE;
				break;
			}

			if (cdt->action != CATA_NONE) {
				view->transmissionmgr->PutCommand(cdt);
				wsprintf(wbuf, L"Send torrent action command [%d] param [%d]", cdt->action, cdt->actionparam);
				view->LogToView(VLI_REQUEST, wbuf);
			}
		}
		return 0;
	}
};

int ViewManager::ViewDeleteContent(BOOL withfiles)
{
	VCActionTorrent* vcd = new VCActionTorrent();
	vcd->action = VCA_DELETE;
	vcd->actionparam = withfiles ? VCA_PARAM_WITHFILE : VCA_PARAM_WITHOUTFILE;
	PutCommand(vcd);
	return 0;
}

int ViewManager::GetSelectedViewNodes(std::set<ViewNode*>& vds)
{
	if (currentnode) {
		int vnt = currentnode->GetType();
		switch (vnt) {
		case VNT_GROUP:
			ListGetSelectedViewNodes(vds);
			if (vds.size() <= 0) {
				vds.insert(currentnodelist.begin(), currentnodelist.end());
			}
			for (std::set<ViewNode*>::iterator itvn = vds.begin();
				itvn != vds.end();
				itvn++) {
				if ((*itvn)->GetType() == VNT_TORRENT) {
					vds.insert(*itvn);
				}
			}
			break;
		case VNT_TORRENT:
			vds.insert(currentnode);
			break;
		}
	}
	return 0;
}

int VCRotation::Process(ViewManager * mgr)
{
	mgr->RefreshProfiles();

	VCRotation *vcr = new VCRotation();
	vcr->timetorun = GetTickCount() + 10000;
	mgr->PutCommand(vcr);
	return 0;
}
