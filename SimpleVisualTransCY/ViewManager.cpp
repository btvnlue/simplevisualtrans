#include "ViewManager.h"
#include "TransmissionProfile.h"
#include "TransmissionManager.h"
#include "Resource.h"

#include <set>
#include <vector>
#include <windowsx.h>
#include <algorithm>
#include <sstream>
#include <string>

// TODO: clipboard, remove sub-nodes in tree

WCHAR wbuf[2048];

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

	cmd = self->GetCommand();
	while (cmd) {
		delete cmd;
		cmd = self->GetCommand();
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
	keeprun = TRUE;
	VCCreateClipboardRoot* ccc = new VCCreateClipboardRoot();
	PutCommand(ccc);
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

	tree_node_handle_map.clear();

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

class VCUpdateTorrentNodes : public ViewCommand
{
public:
	TransmissionProfile* profile = NULL;
	//std::set<ViewNode*> viewnodes;
	std::set<TorrentNode*> torrents;
	virtual int Process(ViewManager* view)
	{
		if (profile) {
			for (std::set<TorrentNode*>::iterator ittn = torrents.begin();
				ittn != torrents.end();
				ittn++) {
				profile->UpdateTorrentView(*ittn);
			}
			wsprintf(wbuf, L"[%d] torrents updated", torrents.size());
			view->LogToView(LTV_RESULT_TORRENT, wbuf);
		}
		return 0;
	}
};

class VCUpdateFileNodes : public ViewCommand
{
public:
	TransmissionProfile* profile = NULL;
	//std::set<ViewNode*> viewnodes;
	std::set<TorrentFileNode*> files;
	virtual int Process(ViewManager* view)
	{
		if (profile) {
			for (std::set<TorrentFileNode*>::iterator ittn = files.begin();
				ittn != files.end();
				ittn++) {
				profile->UpdateTorrentFileView(*ittn);
			}
			wsprintf(wbuf, L"[%d] files updated", files.size());
			view->LogToView(LTV_RESULT_FILE, wbuf);
		}
		return 0;
	}
};

class VCUpdateViewNodes : public ViewCommand
{
public:
	TransmissionProfile* profile = NULL;
	virtual int Process(ViewManager* view)
	{
		if (profile) {
			for (std::set<ViewNode*>::iterator ittn = profile->updateviewnodes.begin();
				ittn != profile->updateviewnodes.end();
				ittn++) {
				view->ViewUpdateViewNode(*ittn);
			}
			wsprintf(wbuf, L"[%d] view nodes updated", profile->updateviewnodes.size());
			view->LogToView(LTV_RESULT_VIEWNODE, wbuf);

			profile->updateviewnodes.clear();
		}
		return 0;
	}
};

class TMCBRefreshTorrents : public CmdFullRefreshTorrentsCB
{
public:
	ViewManager* view;
	TransmissionProfile* profile;
	//std::set<long> newids;
	//std::set<long> updids;
	//std::set<long> delids;

	//int Process(ItemArray<TorrentNode*>* nodes, CmdFullRefreshTorrents *cmd) {
	//	if (cmd->returncode == 0) {
	//		if (nodes) {
	//			if (cmd->profile) {
	//				if (nodes->count > 0) {
	//					std::set<long> nts;
	//					std::set<long> uts;
	//					int rtn;
	//					std::set<long> ids;

	//					wsprintf(wbuf, L"Found [%d] torrents from [%s]...", nodes->count, cmd->profile->url.c_str());
	//					view->LogToView(LTV_RESULT, wbuf);

	//					for (std::map<long, TorrentNode*>::iterator ittn = cmd->profile->torrents.begin();
	//						ittn != cmd->profile->torrents.end();
	//						ittn++) {
	//						if (ittn->second->valid) {
	//							ids.insert(ittn->first);
	//						}
	//					}
	//					for (unsigned int ii = 0; ii < nodes->count; ii++) {
	//						rtn = cmd->profile->UpdateTorrent(&nodes->items[ii]);
	//						if (rtn == UPN_NEWNODE) {
	//							nts.insert(nodes->items[ii].id);
	//						}
	//						else {
	//							ids.erase(nodes->items[ii].id);
	//							if (rtn == UPN_UPDATE) {
	//								uts.insert(nodes->items[ii].id);
	//							}
	//						}
	//					}

	//					// new nodes, update in view, TreeView, ListView
	//					for (std::set<long>::iterator itid = nts.begin();
	//						itid != nts.end();
	//						itid++) {
	//						view->ViewUpdateProfileTorrent(cmd->profile, *itid);
	//					}
	//					for (std::set<long>::iterator itud = uts.begin();
	//						itud != uts.end();
	//						itud++) {
	//						view->ViewUpdateProfileTorrent(cmd->profile, *itud);
	//					}

	//					std::map<long, TorrentNode*>::iterator ittn;
	//					for (std::set<long>::iterator itdl = ids.begin();
	//						itdl != ids.end();
	//						itdl++) {
	//						ittn = cmd->profile->torrents.find(*itdl);
	//						if (ittn != cmd->profile->torrents.end()) {
	//							ittn->second->valid = false;
	//						}
	//						cmd->profile->MarkTorrentInvalid(*itdl);
	//						view->ViewUpdateProfileTorrent(cmd->profile, *itdl);
	//					}
	//					UpdateWindow(view->hList);
	//				}
	//			}
	//		}
	//		else {
	//			wsprintf(wbuf, L"Done refreshing torrents from [%s]...", cmd->profile->url.c_str());
	//			view->LogToView(LTV_VIEWCHANGE, wbuf);

	//			//view->TimerRefreshTorrent(profile);
	//		}
	//	} // cmd->return = 0
	//	else {
	//		switch (cmd->returncode) {
	//		case CRT_E_TIMEOUT:
	//			wsprintf(wbuf, L"Error in refreshing torrents: [TIME_OUT]...");
	//			break;
	//		default:
	//			wsprintf(wbuf, L"Error in refreshing torrents: [%d]...", cmd->returncode);
	//		}

	//		view->LogToView(LTV_VIEWCHANGE, wbuf);
	//	}
	//	return 0;
	//}

	int getRawTorrentId(RawTorrentValuePair* node) {
		int tid = -1;
		bool keepseek;
		if (node->valuetype == RTT_OBJECT) {
			unsigned long ii = 0;
			keepseek = ii < node->vvobject.count;
			while (keepseek) {
				if (wcscmp(L"id", node->vvobject.items[ii].key) == 0) {
					tid = node->vvobject.items[ii].vvint;
					keepseek = false;
				}

				ii++;
				keepseek = ii < node->vvobject.count ? keepseek : false;
			}
		}
		return tid;
	}

	int processRawTorrent(RawTorrentValuePair* node) {
		int rtn = UPN_NOUPDATE;
		if (node->valuetype == RTT_OBJECT) {
			int tid = getRawTorrentId(node);
			if (tid >= 0) {
				rtn = profile->UpdateRawTorrent(tid, node);
			}
		}

		return rtn;
	}

	int processRawTorrents(RawTorrentValuePair* node) {
		int rtn = UPN_NOUPDATE;
		if (node->valuetype == RTT_ARRAY) {
			for (unsigned long ii = 0; ii < node->vvobject.count; ii++) {
				rtn = processRawTorrent(&node->vvobject.items[ii]);
			}
		}

		return rtn;
	}

	int processRawArguments(RawTorrentValuePair* node) {
		int rtn = UPN_NOUPDATE;
		if (node->valuetype == RTT_OBJECT) {
			for (unsigned long ii = 0; ii < node->vvobject.count; ii++) {
				if (wcscmp(L"torrents", node->vvobject.items[ii].key) == 0) {
					rtn = processRawTorrents(&node->vvobject.items[ii]);
				}
			}
		}

		return rtn;
	}
	virtual int ProcessRawObjects(RawTorrentValuePair* node, CmdFullRefreshTorrents *cmd, bool scaninvalid) {
		int nodestate;
		int rtn = 0;
		//TorrentNode* ttn;

		if (node) {
			int nnn = 0, uuu = 0, ddd = 0; // vars for output message

			profile = cmd->profile;
			if (node->valuetype == RTT_OBJECT) {
				std::set<TorrentNode*> alltorrent;
				if (scaninvalid) {
					profile->GetValidTorrents(alltorrent);
					for (std::set<TorrentNode*>::iterator itat = alltorrent.begin();
						itat != alltorrent.end();
						itat++) {
						(*itat)->exist = false;
					}
				}

				for (unsigned long ii = 0; ii < node->vvobject.count; ii++) {
					if (wcscmp(L"arguments", node->vvobject.items[ii].key) == 0) {
						nodestate = processRawArguments(&node->vvobject.items[ii]);
					}
				}

				if (scaninvalid) {
					alltorrent.clear();
					profile->GetValidTorrents(alltorrent);
					for (std::set<TorrentNode*>::iterator itat = alltorrent.begin();
						itat != alltorrent.end();
						itat++) {
						(*itat)->valid = (*itat)->exist ? (*itat)->valid : false;
					}
				}

				//// new nodes, update in view, TreeView, ListView
				//for (std::set<long>::iterator itid = newids.begin();
				//	itid != newids.end();
				//	itid++) {
				//	view->ViewUpdateProfileTorrent(cmd->profile, *itid);
				//	delids.erase(*itid);
				//	nnn++;
				//}

				//// content updated nodes
				//for (std::set<long>::iterator itid = updids.begin();
				//	itid != updids.end();
				//	itid++) {
				//	ttn = profile->GetTorrent(*itid);
				//	if (ttn) {
				//		if (ttn->state != TTSTATE_EXIST) {
				//			view->ViewUpdateProfileTorrent(cmd->profile, *itid);
				//			uuu++;
				//		}
				//	}
				//	delids.erase(*itid);
				//}

				//// deleted nodes
				//for (std::set<long>::iterator itid = delids.begin();
				//	itid != delids.end();
				//	itid++) {
				//	profile->MarkTorrentInvalid(*itid);
				//	view->ViewUpdateProfileTorrent(cmd->profile, *itid);
				//	ddd++;
				//}

				//UpdateWindow(view->hList);
			}

			//wsprintf(wbuf, L"Process raw torrents new [%d] update [%d] delete [%d]", nnn, uuu, ddd);
			//view->LogToView(LTV_RESULT, wbuf);

			VCUpdateTorrentNodes* vcuvn = new VCUpdateTorrentNodes();
			vcuvn->profile = cmd->profile;
			vcuvn->torrents.insert(profile->updatetorrents.begin(), profile->updatetorrents.end());

			//vcuvn->viewnodes.insert(profile->updateviewnodes.begin(), profile->updateviewnodes.end());
			//profile->updateviewnodes.clear();
			profile->updatetorrents.clear();
			view->PutCommand(vcuvn);

			VCUpdateFileNodes* vcuf = new VCUpdateFileNodes();
			vcuf->profile = cmd->profile;
			vcuf->files.insert(profile->updatefiles.begin(), profile->updatefiles.end());
			view->PutCommand(vcuf);
			profile->updatefiles.clear();

			VCUpdateViewNodes* vcuv = new VCUpdateViewNodes();
			vcuv->profile = cmd->profile;
			view->PutCommand(vcuv);
		}
		else {
			wsprintf(wbuf, L"Done refreshing raw torrents from [%s]...", cmd->profile->url.c_str());
			view->LogToView(LTV_VIEWCHANGE, wbuf);
		}

		return rtn;
	}
} crt;

int ViewManager::DoneProfileLoading(TransmissionProfile * prof)
{
	profiles[prof->name] = prof;
	prof->profileview->name = prof->url;
	defaultprofile = prof;

	wsprintf(wbuf, L"Done loading [%s] profile.", prof->name.c_str());
	LogToView(LTV_REQUEST, wbuf);

	CmdFullRefreshTorrents *cmd = new CmdFullRefreshTorrents();
	cmd->profile = prof;
	crt.view = this;
	cmd->callback = &crt;

	wsprintf(wbuf, L"Refresh torrents from [%s]...", prof->url.c_str());
	LogToView(LTV_REFRESH, wbuf);

	transmissionmgr->PutCommand(cmd);

	return 0;
}

int ViewManager::PutCommand(ViewCommand * cmd)
{
	if (keeprun) {
		if (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {
			cmds.push_back(cmd);
			::SetEvent(hEvent);
		}
	}
	else {
		delete cmd;
	}
	return 0;
}

//int ViewManager::ViewUpdateProfileTorrent(TransmissionProfile * prof, long tid)
//{
//	std::set<ViewNode*> vns;
//	ViewNode* vnd;
//	prof->GetTorrentViewNodes(tid, vns);
//	for (std::set<ViewNode*>::iterator itvn = vns.begin();
//		itvn != vns.end();
//		itvn++) {
//		vnd = *itvn;
//		if (vnd->GetTorrent()) {
//			if (vnd->GetTorrent()->valid == false) {
//				vnd->valid = false;
//			}
//		}
//		ViewUpdateViewNode(*itvn);
//	}
//	return 0;
//}

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

int ViewManager::FullRefreshProfileNodes(TransmissionProfile* prof)
{
	CmdFullRefreshTorrents* cmd;

	if (prof->inrefresh == false) {
		cmd = new CmdFullRefreshTorrents();
		cmd->profile = prof;
		crt.view = this;
		cmd->callback = &crt;

		wsprintf(wbuf, L"Refresh torrents from [%s]...", cmd->profile->url.c_str());
		LogToView(LTV_REFRESH, wbuf);

		transmissionmgr->PutCommand(cmd);
	}

	return 0;
}

int ViewManager::RefreshProfileNodeDetail(TransmissionProfile * prof, long tid)
{
	CmdFullRefreshTorrents* cmd;

	if (prof->inrefresh == false) {
		cmd = new CmdFullRefreshTorrents();
		cmd->profile = prof;
		cmd->dofullyrefresh = false;

		//std::set<ViewNode*> vns;
		//GetSelectedViewNodes(vns);
		//for (std::set<ViewNode*>::iterator itvn = vns.begin();
		//	itvn != vns.end();
		//	itvn++) {
		//	if ((*itvn)->GetType() == VNT_TORRENT) {
		cmd->torrentids.insert(tid);
		//	}
		//}
		crt.view = this;
		cmd->callback = &crt;

		if (cmd->torrentids.size() > 0) {
			wsprintf(wbuf, L"Refresh torrents from [%s]...", cmd->profile->url.c_str());
			LogToView(LTV_REFRESH, wbuf);
			transmissionmgr->PutCommand(cmd);
		}
		else {
			wsprintf(wbuf, L"Refresh torrent detail error: No torrent selected...");
			LogToView(LTV_REFRESH, wbuf);

			delete cmd;
		}
	}

	return 0;
}

class TMCBRefreshSession : public CBCmdRefreshSession
{
public:
	ViewManager* view;
	virtual int ProcessStatus(TransmissionManager* mng, CmdRefreshSession* cmd)
	{
		if (cmd) {
			if (cmd->profile) {
				view->ListUpdateViewNode(view->currentnode);

				wsprintf(wbuf, L"Done refreshing session from [%s]...", cmd->profile->url.c_str());
				view->LogToView(LTV_VIEWCHANGE, wbuf);
			}
		}
		return 0;
	}
	virtual int ProcessInfo(RawTorrentValuePair * rawinfo, CmdRefreshSession* cmd)
	{
		if (cmd->profile) {
			cmd->profile->copyRawSessionInfo(rawinfo, 0);
		}
		return 0;
	}
} tmcbrs;

int ViewManager::RefreshProfileStatus(TransmissionProfile * prof)
{
	CmdRefreshSession* cmd;

	if (prof->inrefresh == false) {

		tmcbrs.view = this;
		cmd = new CmdRefreshSession();
		cmd->profile = prof;
		cmd->callback = &tmcbrs;

		wsprintf(wbuf, L"Refresh profile session for [%s]...", cmd->profile->url.c_str());
		LogToView(LTV_REFRESH, wbuf);

		transmissionmgr->PutCommand(cmd);
	}

	return 0;
}

TransmissionProfile * ViewManager::GetViewNodeProfile(ViewNode * vnd)
{
	bool btn;
	TransmissionProfile* prof = NULL;
	for (std::map<std::wstring, TransmissionProfile*>::iterator itpf = profiles.begin();
		itpf != profiles.end();
		itpf++) {
		btn = itpf->second->HasViewNode(vnd);
		if (btn) {
			prof = itpf->second;
		}
	}
	return prof;
}

TorrentNode * ViewManager::GetCurrentTorrent()
{
	TorrentNode* ttn = NULL;
	if (currentnode) {
		ttn = currentnode->GetTorrent();
	}
	return ttn;
}

int ViewManager::ShowContextMenu(int xx, int yy)
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

int ViewManager::RefreshCurrentNode()
{
	TransmissionProfile* prof = NULL;
	bool refreshfull = false;

	if (currentnode) {
		switch (currentnode->GetType()) {
		case VNT_PROFILE:
			prof = GetViewNodeProfile(currentnode);
			RefreshProfileStatus(prof);
			break;
		case VNT_GROUP:
			prof = GetViewNodeProfile(currentnode);
			if (prof) {
				FullRefreshProfileNodes(prof);
			}
			break;
		case VNT_TORRENT:
		case VNT_FILEPATH:
		case VNT_TRACKER:
			prof = GetViewNodeProfile(currentnode);
			if (prof) {
				TorrentNode* ttn = currentnode->GetTorrent();
				if (ttn) {
					RefreshProfileNodeDetail(prof, ttn->id);
				}
			}
			break;
		default:
			refreshfull = true;
			break;
		}
	}
	else
	{
		refreshfull = true;
	}

	if (refreshfull) {
		for (std::map<std::wstring, TransmissionProfile*>::iterator itpf = profiles.begin();
			itpf != profiles.end();
			itpf++) {
			FullRefreshProfileNodes(itpf->second);
		}
	}
	return 0;
}

int ViewManager::ChangeSelectedViewNode(ViewNode * vnd)
{
	if (currentnode != vnd) {
		currentnode = vnd;
		ListSwitchContent();
		RefreshCurrentNode();
	}
	return 0;
}

HTREEITEM ViewManager::TreeUpdateViewNode(ViewNode * vnd)
{
	HTREEITEM rtn = NULL;

	BOOL procnode = TRUE;

	procnode = vnd->GetType() == VNT_FILE ? FALSE : procnode;

	if (procnode) {
		std::map<ViewNode*, HTREEITEM>::iterator itvn = tree_node_handle_map.find(vnd);
		if (itvn == tree_node_handle_map.end()) {
			HTREEITEM hpv = NULL;
			if (vnd->parent) {
				hpv = TreeUpdateViewNode(vnd->parent);
			}

			TVINSERTSTRUCT tis = { 0 };
			tis.hParent = hpv;
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
	//std::map<ViewNode*, HTREEITEM>::iterator ittm;
	if (vnd) {
		for (std::list<ViewNode*>::iterator itvn = vnd->nodes.begin();
			itvn != vnd->nodes.end();
			itvn++) {
			snd = *itvn;
			TreeUpdateViewNode(snd);
		}
		if (vnd->GetType() == VNT_GROUP) {
			TreeSortTorrentGroup(hitem);
		}
	}
	return 0;
}

int CALLBACK compareTreeViewNode(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	ViewNode* vnl = (ViewNode*)lParam1;
	ViewNode* vnr = (ViewNode*)lParam2;

	int rtn = 0;
	if (vnl) {
		if (vnr) {
			if (vnl->GetType() == VNT_TORRENT) {
				if (vnr->GetType() == VNT_TORRENT) {
					rtn = (vnl->GetTorrent()->id < vnr->GetTorrent()->id) ? -1 : ((vnl->GetTorrent()->id > vnr->GetTorrent()->id ? 1 : 0));
				}
			}
		}
	}

	return rtn;
}

int ViewManager::TreeSortTorrentGroup(HTREEITEM hitem)
{
	BOOL btn;
	TV_SORTCB tscb;
	tscb.hParent = hitem;
	tscb.lParam = (LPARAM)this;
	tscb.lpfnCompare = compareTreeViewNode;
	btn = SendMessage(hTree, TVM_SORTCHILDRENCB, 0, (LPARAM)&tscb);

	return 0;
}

int ViewManager::ViewSortProfileTotal(TransmissionProfile * prof)
{
	if (prof) {
		std::map<ViewNode*, HTREEITEM>::iterator itth = tree_node_handle_map.find(prof->totalview);
		if (itth != tree_node_handle_map.end()) {
			TreeSortTorrentGroup(itth->second);
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

int ViewManager::ListSwitchContent()
{
	ListClearContent();

	if (currentnode) {
		int vnt = currentnode->GetType();
		switch (vnt) {
		case VNT_GROUP:
		{
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

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Tracker";
			lvc.fmt = LVCFMT_LEFT;
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.cx = 150;
			cols = ListView_InsertColumn(hList, cols, &lvc);
			cols++;

			lvc.iSubItem = cols;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.pszText = (LPWSTR)L"Error";
			lvc.fmt = LVCFMT_LEFT;
			//lvc.cchTextMax = wcslen(lvc.pszText);
			lvc.cx = 150;
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
		case VNT_PROFILE:
		{
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

			ListBuildTorrentContent(currentnode);
			ListView_SetItemCountEx(hList, tnpdisp.size(), LVSICF_NOINVALIDATEALL);

		}
		break;
		case VNT_CLIPBOARD:
		case VNT_CLIPBOARD_ITEM:
			if (currentnode) {
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
		case VNT_FILEPATH:
			if (currentnode->file) {
				int cols = 0;
				LVCOLUMN lvc;
				lvc.iSubItem = 0;
				lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
				lvc.pszText = (LPWSTR)L"Id";
				lvc.cx = 50;
				lvc.fmt = LVCFMT_LEFT;
				cols = ListView_InsertColumn(hList, cols, &lvc);
				cols++;

				lvc.iSubItem = 0;
				lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
				lvc.pszText = (LPWSTR)L"Has";
				lvc.cx = 20;
				lvc.fmt = LVCFMT_LEFT;
				cols = ListView_InsertColumn(hList, cols, &lvc);
				cols++;

				lvc.iSubItem = 0;
				lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
				lvc.pszText = (LPWSTR)L"Pr";
				lvc.cx = 20;
				lvc.fmt = LVCFMT_LEFT;
				cols = ListView_InsertColumn(hList, cols, &lvc);
				cols++;

				lvc.iSubItem = 0;
				lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
				lvc.pszText = (LPWSTR)L"Fileame";
				lvc.cx = 250;
				lvc.fmt = LVCFMT_LEFT;
				cols = ListView_InsertColumn(hList, cols, &lvc);
				cols++;

				lvc.iSubItem = 0;
				lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
				lvc.pszText = (LPWSTR)L"Path";
				lvc.cx = 200;
				lvc.fmt = LVCFMT_LEFT;
				cols = ListView_InsertColumn(hList, cols, &lvc);
				cols++;

				lvc.iSubItem = 0;
				lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
				lvc.pszText = (LPWSTR)L"Size";
				lvc.cx = 70;
				lvc.fmt = LVCFMT_RIGHT;
				cols = ListView_InsertColumn(hList, cols, &lvc);
				cols++;

				ListBuildFileContent(currentnode);
				listsortindex = listsortindex < cols ? listsortindex : 0;
				ListSortFiles(currentnodelist, listsortindex, listsortdesc);
				ListView_SetItemCountEx(hList, currentnodelist.size(), LVSICF_NOINVALIDATEALL);
			}
			break;
		}
	}
	else {	//viewnode vnd : null
		//ListClearContent();
		LogToView(LTV_COMMENTMESSAGE, L"No node selected in view");
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
					icc = wcscmp(ltt->_path, rtt->_path);
					rtn = icc < 0;
					equ = icc == 0;
					break;
				case LISTSORT_TRACKER:
					icc = wcscmp(ltt->disptracker, rtt->disptracker);
					rtn = icc < 0;
					equ = icc == 0;
					break;
				case LISTSORT_ERROR:
					icc = wcscmp(ltt->_error, rtt->_error);
					rtn = icc < 0;
					equ = icc == 0;
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

class fileless
{
	int colindex;
	bool sortasc;
public:
	fileless(int cid, bool asc) : colindex(cid), sortasc(asc) {}
	bool operator()(ViewNode* lt, ViewNode* rt) {
		bool rtn = false;
		int icc;
		bool equ = false;
		TorrentFileNode* ltt = lt->file;
		TorrentFileNode* rtt = rt->file;
		if (ltt) {
			if (rtt) {
				switch (colindex) {
				case LISTFILESORT_ID:
					rtn = ltt->id < rtt->id;
					equ = ltt->id == rtt->id;
					break;
				case LISTFILESORT_NAME:
					icc = wcscmp(ltt->dispname, rtt->dispname);
					rtn = icc < 0;
					equ = icc == 0;
					break;
				case LISTFILESORT_PATH:
					icc = wcscmp(ltt->disppath, rtt->disppath);
					rtn = icc < 0;
					equ = icc == 0;
					break;
				case LISTFILESORT_SIZE:
					rtn = ltt->size < rtt->size;
					equ = ltt->size == rtt->size;
					break;

				}
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

int ViewManager::ListSortFiles(std::vector<ViewNode*>& tns, int sidx, bool asc)
{
	std::sort(tns.begin(), tns.end(), fileless(sidx, asc));
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
						ListSortTorrentGroup(listsortindex);
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
					ListBuildTorrentContent(vnupd);
					long lstcnt = tnpdisp.size();
					ListView_SetItemCountEx(hList, lstcnt, LVSICF_NOINVALIDATEALL);
					ListView_RedrawItems(hList, 0, lstcnt);
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
		case VNT_PROFILE:
			if (currentnode == vnupd) {
				TransmissionProfile* prof = GetViewNodeProfile(vnupd);
				prof->UpdateStatusDispValues();
				ListBuildTorrentContent(vnupd);
				long lstcnt = ListView_GetItemCount(hList);
				ListView_RedrawItems(hList, 0, lstcnt);
			}
			break;
		case VNT_FILEPATH:
			if (vnupd->GetType() == VNT_FILE) {
				std::vector<ViewNode*>::iterator itvn = std::find(currentnodelist.begin(), currentnodelist.end(), vnupd);
				if (itvn != currentnodelist.end()) {
					int upi = itvn - currentnodelist.begin();
					long lstcnt = ListView_GetItemCount(hList);
					if (upi < lstcnt) {
						ListView_RedrawItems(hList, upi, upi);
					}
				}
			}
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

int ViewManager::ListBuildTorrentContent(ViewNode * vnd)
{
	if (vnd) {
		tnpdisp.clear();
		int vdt = vnd->GetType();
		switch (vdt) {
		case VNT_TORRENT:
		{
			TorrentNode* vtt = vnd->GetTorrent();
			if (vtt) {
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"ID", vtt->dispid));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Name", vtt->name.c_str()));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Size", vtt->dispsize));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Add Date", vtt->dispadddate));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Start Date", vtt->dispstartdate));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Completed Date", vtt->dispdonedate));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Downloaded Size", vtt->dispdownloaded));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Uploaded Size", vtt->dispuploaded));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Left Size", vtt->displeft));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Avialable", vtt->dispavialable));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Download Speed /s", vtt->dispdownspeed));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Upload Speed /s", vtt->dispupspeed));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Download Limit /s", vtt->dispdownlimit));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Upload Limit /s", vtt->dispuplimit));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Download Limit Enabled", vtt->downloadlimited ? L"Y" : L"N"));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Upload Limit Enabled", vtt->uploadlimited? L"Y" : L"N"));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Status", vtt->dispstatus));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Pieces", vtt->pieces.c_str()));
				pieceItemIndex = tnpdisp.size() - 1;
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Pieces Count", vtt->disppiececount));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Piece Size", vtt->disppiecesize));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Path", vtt->_path));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Error", vtt->_error));

				int ii = 0;
				for (std::set<TrackerCY*>::iterator ittc = vtt->__trackers.begin();
					ittc != vtt->__trackers.end();
					ittc++) {
					wsprintf(wbuf, L"Tracker:%d", ii);
					wsprintf(wbuf + 128, L"%04d: %s", (*ittc)->groupid, (*ittc)->url.c_str());
					tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(wbuf, wbuf + 128));
					ii++;
				}
				for (std::vector<TorrentPeerNode*>::iterator itpr = vtt->peers.begin();
					itpr != vtt->peers.end();
					itpr++) {
					if ((*itpr)->valid) {
						wsprintf(wbuf, L"Peer: %s", (*itpr)->name.c_str());
						wsprintf(wbuf + 128, L"%s:%d Down:%s/s Up:%s/s Have:%s", (*itpr)->address.c_str(), (*itpr)->port, (*itpr)->dispdown, (*itpr)->dispup, (*itpr)->dispprogress);
						tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(wbuf, wbuf + 128));
						ii++;
					}
				}
			}
		}
		break;
		case VNT_PROFILE:
		{
			TransmissionProfile *prof = GetViewNodeProfile(vnd);
			if (prof) {
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Download Speed", prof->dispdownspeed));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Upload Speed", prof->dispupspeed));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Torrent Count", prof->disptorrentcount));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Active Torrents", prof->dispactivecount));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Paused Torrents", prof->disppausecount));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Total: Downloaded Size", prof->disptotaldownsize));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Total: Uploaded Size", prof->disptotalupsize));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Total: Files", prof->disptotalfiles));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Total: Sessions", prof->disptotalsessions));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Total: Active Sessions", prof->disptotalactive));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Current: Downloaded Size", prof->dispcurrentdownsize));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Current: Uploaded Size", prof->dispcurrentupsize));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Current: Files", prof->dispcurrentfiles));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Current: Sessions", prof->dispcurrentsessions));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Current: Active Sessions", prof->dispcurrentactive));

				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Session ID", prof->stat.session.c_str()));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Version", prof->stat.version.c_str()));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"RPC Version", prof->disprpc));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Download Limit", prof->dispdownlimit));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Upload Limit", prof->dispuplimit));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Download Limit Enabled", prof->dispdownenable));
				tnpdisp.insert(tnpdisp.end(), std::pair<std::wstring, std::wstring>(L"Upload Limit Enabled", prof->dispupenable));
			}
		}
		break;
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

int ViewManager::ListBuildFileContent(ViewNode * vnd)
{
	if (vnd) {
		currentnodelist.clear();
		if (vnd->file) {
			TransmissionProfile* cpf = GetViewNodeProfile(vnd);
			if (cpf) {
				TorrentFileNode * cpn = vnd->file;
				TorrentFileSet tfs;
				cpn->GetFiles(tfs);
				ViewNode* fvd;
				for (TorrentFileSet::iterator itfs = tfs.begin(); itfs != tfs.end(); itfs++) {
					fvd = cpf->GetFileViewNode(*itfs);
					if (fvd) {
						currentnodelist.insert(currentnodelist.end(), fvd);
					}
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

class TMCBLoadProfile : public CmdLoadProfileCB
{
public:
	ManagerCommand* cmd;
	ViewManager* view;
	int Process(TransmissionManager* mgr, CmdLoadProfile * cmd) {
		view->DoneProfileLoading(cmd->profile);

		return 0;
	}

} cbc;

int VCInit::Process(ViewManager * mng)
{
	TransmissionProfile* prof = new TransmissionProfile();
	prof->name = L"default";

	wsprintf(wbuf, L"Load [%s] profile...", prof->name.c_str());
	mng->LogToView(LTV_VIEWCHANGE, wbuf);

	CmdLoadProfile* clp = new CmdLoadProfile();
	clp->profile = prof;
	cbc.view = mng;
	clp->callback = &cbc;
	mng->transmissionmgr->PutCommand(clp);

	return 0;
}

ViewManager::ViewManager()
	: currentnode(nullptr),
	listsortindex(0),
	listsortdesc(true),
	clipboardRoot(NULL),
	pieceItemIndex(-1)
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
			SendMessage(hLog, LB_SETCOUNT, LTV_FINALCOUNT, 0);
			if (rtn >= 0) {
				ListBox_SetCurSel(hLog, rtn);
			}
		}
	}

	return 0;
}

//int ViewManager::TimerRefreshTorrent(TransmissionProfile * prof)
//{
//	CmdFullRefreshTorrents *cmd = new CmdFullRefreshTorrents();
//	cmd->profile = prof;
//	crt = crt == NULL ? new TMCBRefreshTorrents() : crt;
//	crt->profile = prof;
//	crt->view = this;
//	cmd->callback = crt;
//	cmd->timetorun = GetTickCount() + 10000;
//	SYSTEMTIME stm;
//	::GetLocalTime(&stm);
//	double vct;
//	SystemTimeToVariantTime(&stm, &vct);
//	vct += 10.0 / (24 * 60 * 60);
//	VariantTimeToSystemTime(vct, &stm);
//	WCHAR lbuf[1024];
//	wsprintf(lbuf, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d]"
//		, stm.wYear, stm.wMonth, stm.wDay
//		, stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds);
//
//	wsprintf(wbuf, L"Timer Refresh torrents %s from [%s]...", lbuf, prof->url.c_str());
//	LogToView(LTV_REQUEST, wbuf);
//
//	transmissionmgr->PutCommand(cmd);
//
//	return 0;
//}

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

		SelectClipboardRoot();
	}
	else {
		DWORD lerr = GetLastError();
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, lerr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			wbuf,
			1024,
			NULL);
		LogToView(LTV_VIEWCHANGE, wbuf);
	}
	return 0;
}

int ViewManager::SelectClipboardRoot()
{
	if (clipboardRoot) {
		std::map<ViewNode*, HTREEITEM>::iterator itht = tree_node_handle_map.find(clipboardRoot);
		if (itht != tree_node_handle_map.end()) {
			HTREEITEM hti = itht->second;
			TreeView_Select(hTree, hti, TVGN_CARET);
		}

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
			LogToView(LTV_DELETENODE, wbuf);
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
			view->LogToView(LTV_COMMITNODE, wbuf);

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
	virtual int Process(ViewManager* view)
	{
		ViewNode* vnd;
		CmdCommitTorrent* cct;
		if (view->clipboardRoot) {
			std::list<ViewNode*>::iterator itvn = view->clipboardRoot->nodes.begin();
			bool keepseek = itvn != view->clipboardRoot->nodes.end();
			while (keepseek) {
				if ((*itvn)->valid) {
					vnd = *itvn;
					vnd->valid = false;

					cct = new CmdCommitTorrent();
					cct->profile = view->defaultprofile;
					cct->entry = vnd->name;
					vccb.view = view;
					vccb.currentcommit = vnd;
					cct->callback = &vccb;

					view->transmissionmgr->PutCommand(cct);

					wsprintf(wbuf, L"Commit torrent [%s]", vnd->name.c_str());
					view->LogToView(LTV_REQUEST, wbuf);

					keepseek = false;

				}
				itvn++;
				keepseek = itvn != view->clipboardRoot->nodes.end() ? keepseek : false;
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

class VCCBActionTorrents : public CmdActionTorrentsCB
{
public:
	ViewManager* view = NULL;
	virtual int Process(CmdActionTorrent* cmd, std::set<int>& ids, int code)
	{
		if (view) {
			wsprintf(wbuf, L"Action [%d] completed [%d] torrents with code [%d]", cmd->action, ids.size(), code);
			view->LogToView(LTV_RESULT, wbuf);
		}
		return 0;
	}
} vcbdt;

#define VCAF_NONE 0
#define VCAF_ENABLE 1
#define VCAF_PRIORITY 2

class VCActionFiles : public ViewCommand
{
public:
	int action = VCAF_NONE;
	int parameter = 0;

	virtual int Process(ViewManager * view) {
		if (action != VCAF_NONE) {
			ViewNodeSet vds;
			TorrentNode* ttn = view->GetCurrentTorrent();
			if (ttn) {
				view->GetSelectedViewNodes(vds);
				TorrentFileSet fns;
				TransmissionProfile* prof = NULL;

				for (ViewNodeSet::iterator itvn = vds.begin(); itvn != vds.end(); itvn++) {
					if (prof == NULL) {
						prof = view->GetViewNodeProfile(*itvn);
					}
					switch ((*itvn)->GetType()) {
					case ViewNodeType::VNT_FILE:
						if ((*itvn)->file) {
							fns.insert((*itvn)->file);
						}
						break;
					case ViewNodeType::VNT_FILEPATH:
						if ((*itvn)->file) {
							(*itvn)->file->GetFiles(fns);
						}
						break;
					default:
						break;
					}
				}

				if (fns.size() > 0) {
					CmdActionFile* caf = new CmdActionFile();
					caf->torrentid = ttn->id;
					switch (action) {
					case VCAF_ENABLE:
						caf->action = CAF_ENABLE;
						caf->actionparam = parameter;
						break;
					case VCAF_PRIORITY:
						caf->action = CAF_PRIORITY;
						caf->actionparam = parameter;
						break;
					}
					for (TorrentFileSet::iterator ittf = fns.begin(); ittf != fns.end();ittf++) {
						caf->fileids.insert((*ittf)->id);
					}
					caf->profile = prof;
					view->transmissionmgr->PutCommand(caf);
				}
			}
		}
		return 0;
	}
};

#define VCA_DELETE 1
#define VCA_PAUSE 2
#define VCA_UNPAUSE 3
#define VCA_VERIFY 4

#define VCA_PARAM_WITHFILE 1
#define VCA_PARAM_WITHOUTFILE 0

class VCActionTorrent : public ViewCommand
{
public:
	int action;
	int actionparam;
	int ProcessGroupTorrent(ViewManager* view) {
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
					cdt->profile = view->GetViewNodeProfile(vnd);
				}
			}
			vcbdt.view = view;
			cdt->profile = cdt->profile ? cdt->profile : view->defaultprofile;
			cdt->callback = &vcbdt;

			switch (action)
			{

			case VCA_DELETE:
				cdt->action = CATA_DELETE;
				cdt->actionparam = actionparam == VCA_PARAM_WITHFILE ? CATA_PARAM_DELETEFILES : CATA_PARAM_NODELETEFILES;
				break;
			case VCA_PAUSE:
			case VCA_UNPAUSE:
				cdt->action = action == VCA_PAUSE ? CATA_PAUSE : CATA_UNPAUSE;
				break;
			case VCA_VERIFY:
				cdt->action = CATA_VERIFY;
				break;
			default:
				cdt->action = CATA_NONE;
				break;
			}

			if (cdt->action != CATA_NONE) {
				view->transmissionmgr->PutCommand(cdt);
				wsprintf(wbuf, L"Send torrent action command [%d] param [%d]", cdt->action, cdt->actionparam);
				view->LogToView(LTV_REQUEST, wbuf);
			}
			else
			{
				delete cdt;
				wsprintf(wbuf, L"Unknown action command [%d] param [%d]", action, actionparam);
				view->LogToView(LTV_REQUEST, wbuf);
			}
		}
		return 0;
	}
	int ProcessClipboard(ViewManager* view)
	{
		std::set<ViewNode*> vds;
		view->GetSelectedViewNodes(vds);
		ViewNode* vnd;

		switch (action) {
		case VCA_DELETE:
			for (std::set<ViewNode*>::iterator itvn = vds.begin();
				itvn != vds.end();
				itvn++) {
				vnd = *itvn;
				if (vnd->GetType() == VNT_CLIPBOARD_ITEM) {
					vnd->valid = false;
					view->ViewUpdateViewNode(vnd);
				}
			}
			break;
		}
		return 0;
	}
	virtual int Process(ViewManager* view)
	{
		if (view->currentnode) {
			switch (view->currentnode->GetType())
			{
			case VNT_GROUP:
			case VNT_TORRENT:
				ProcessGroupTorrent(view);
				break;  // case group/torrent
			case VNT_CLIPBOARD:
			case VNT_CLIPBOARD_ITEM:
				ProcessClipboard(view);
				break;
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

int ViewManager::ViewPauseTorrent(BOOL dopause)
{
	VCActionTorrent* vcd = new VCActionTorrent();
	vcd->action = dopause ? VCA_PAUSE : VCA_UNPAUSE;
	PutCommand(vcd);

	return 0;
}

int ViewManager::ViewEnableFile(BOOL enable)
{
	VCActionFiles* vcf = new VCActionFiles();
	vcf->action = VCAF_ENABLE;
	vcf->parameter = enable ? 1 : 0;
	PutCommand(vcf);
	return 0;
}

int ViewManager::ViewPriorityFile(int spr)
{
	VCActionFiles* vcf = new VCActionFiles();
	vcf->action = VCAF_PRIORITY;
	vcf->parameter = spr;
	PutCommand(vcf);
	return 0;
}

int ViewManager::ViewVerifyTorrent()
{
	VCActionTorrent* vcd = new VCActionTorrent();
	vcd->action = VCA_VERIFY;
	PutCommand(vcd);

	return 0;
}

int ViewManager::ViewSetLocation()
{
	std::set<ViewNode*> vds;
	GetSelectedViewNodes(vds);
	WCHAR * wwb = wbuf + 1024;
	WCHAR * locwb = wbuf + 1500;
	if (vds.size() > 0) {
		wbuf[0] = 0;
		locwb[0] = 0;
		ViewNode* vnd;
		for (std::set<ViewNode*>::iterator itvn = vds.begin();
			itvn != vds.end();
			itvn++) {
			vnd = *itvn;
			if (vnd->GetType() == VNT_TORRENT) {
				if (wbuf[0] == 0) {
					wsprintf(wwb, L"%d", vnd->GetTorrent()->id);
				}
				else
				{
					wsprintf(wwb, L"%s,%d", wbuf, vnd->GetTorrent()->id);
				}
				wcscpy_s(wbuf, 1024, wwb);

				if (locwb[0] == 0) {
					wcscpy_s(locwb, 512, vnd->GetTorrent()->_path);
				}
			}
		}
		if (wcslen(wbuf) > 0) {
			wsprintf(wbuf + 1024, L"setpath %s %s", wbuf, locwb);
			Edit_SetText(hInput, wbuf + 1024);
			SetFocus(hInput);
		}
	}
	return 0;
}

int ViewManager::ViewSetLimit(int vslup)
{
	TransmissionProfile* prof = NULL;
	std::set<ViewNode*> vns;

	if (currentnode) {
		prof = GetViewNodeProfile(currentnode);
		if ((currentnode->GetType() == VNT_GROUP) || (currentnode->GetType() == VNT_TORRENT)) {
			GetSelectedViewNodes(vns);
		}
	}
	if (prof == NULL) {
		prof = defaultprofile;
	}

	switch (vslup)
	{
	case CSLA_DOWN:
	case CSLA_UP:
		if (prof) {
			const wchar_t * updowns = vslup == CSLA_UP ? L"up" : L"down";

			if (wcslen(wbuf) > 0) {
				wsprintf(wbuf, L"setlimit %s %d", updowns, vslup ? prof->stat.uplimit : prof->stat.downlimit);
				SetFocus(hInput);
			}

			if (vns.size() > 0) {
				wcscat_s(wbuf, L" ");
				wchar_t* wtch = wbuf + 1024;
				for (std::set<ViewNode*>::iterator itvn = vns.begin(); itvn != vns.end(); itvn++) {
					if ((*itvn)->GetType() == VNT_TORRENT) {
						wsprintf(wtch, L"%d", (*itvn)->GetTorrent()->id);
						wcscat_s(wbuf, 1024, wtch);
						wcscat_s(wbuf, 1024, L",");
					}
				}
			}

			Edit_SetText(hInput, wbuf);
		}
		break;
	case CSLA_ENABLE_UP:
	case CSLA_ENABLE_DOWN:
		if (prof) {
			const wchar_t * updowns = vslup == CSLA_ENABLE_UP ? L"up" : L"down";

			wsprintf(wbuf, L"enablelimit %s", updowns);
			if (vns.size() > 0) {
				wcscat_s(wbuf, L" ");
				wchar_t* wtch = wbuf + 1024;
				for (std::set<ViewNode*>::iterator itvn = vns.begin(); itvn != vns.end(); itvn++) {
					if ((*itvn)->GetType() == VNT_TORRENT) {
						wsprintf(wtch, L"%d", (*itvn)->GetTorrent()->id);
						wcscat_s(wbuf, 1024, wtch);
						wcscat_s(wbuf, 1024, L",");
					}
				}
			}

			Edit_SetText(hInput, wbuf);
			SetFocus(hInput);
			ProcessInputCommand();
		}


		break;
	case CSLA_DISABLE_UP:
	case CSLA_DISABLE_DOWN:
		if (prof) {
			const wchar_t * updowns = vslup == CSLA_DISABLE_UP ? L"up" : L"down";

			wsprintf(wbuf, L"disablelimit %s", updowns);
			if (vns.size() > 0) {
				wcscat_s(wbuf, L" ");
				wchar_t* wtch = wbuf + 1024;
				for (std::set<ViewNode*>::iterator itvn = vns.begin(); itvn != vns.end(); itvn++) {
					if ((*itvn)->GetType() == VNT_TORRENT) {
						wsprintf(wtch, L"%d", (*itvn)->GetTorrent()->id);
						wcscat_s(wbuf, 1024, wtch);
						wcscat_s(wbuf, 1024, L",");
					}
				}
			}
			Edit_SetText(hInput, wbuf);
			SetFocus(hInput);
			ProcessInputCommand();
		}
		break;
	}
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
		case VNT_CLIPBOARD:
			ListGetSelectedViewNodes(vds);
			if (vds.size() <= 0) {
				vds.insert(currentnodelist.begin(), currentnodelist.end());
			}
			for (std::set<ViewNode*>::iterator itvn = vds.begin();
				itvn != vds.end();
				itvn++) {
				if ((*itvn)->GetType() == VNT_CLIPBOARD_ITEM) {
					vds.insert(*itvn);
				}
			}
			break;
		case VNT_CLIPBOARD_ITEM:
			vds.insert(currentnode);
			break;
		case VNT_FILEPATH:
			ListGetSelectedViewNodes(vds);
			if (vds.size() <= 0) {
				vds.insert(currentnodelist.begin(), currentnodelist.end());
			}
			break;
		}
	}
	return 0;
}

int cmdIdentify(const wchar_t* cmd, const wchar_t* input)
{
	int rtn = 0;
	wchar_t workcmd[2048];
	int spos;
	const wchar_t* fsp = wcsstr(input, L" ");
	if (fsp) {
		spos = fsp - input;
		wcsncpy_s(workcmd, 1024, input, spos);
	}
	else {
		wcscpy_s(workcmd, 1024, input);
	}

	int cmdl = wcslen(cmd);
	int inpl = wcslen(workcmd);
	if (cmdl == inpl) {
		fsp = wcsstr(workcmd, cmd);
		if (fsp == NULL) {
			rtn = -1;
		}
	}
	else {
		rtn = -1;
	}

	return rtn;
}

int splitString(const wchar_t* str, std::vector<std::wstring>& strset, const wchar_t* delim) {
	wchar_t workstr[2048];
	wchar_t * wws = workstr + 1024;
	int dlml = wcslen(delim);

	wcscpy_s(workstr, 1024, str);
	const wchar_t * fsp = wcsstr(workstr, delim);
	while (fsp) {
		wcsncpy_s(wws, 1024, workstr, fsp - workstr);
		strset.insert(strset.end(), wws);
		wcscpy_s(wws, 1024, workstr + (fsp - workstr + dlml));
		wcscpy_s(workstr, 1024, wws);
		fsp = wcsstr(workstr, delim);
	}

	if (wcslen(workstr) > 0) {
		strset.insert(strset.end(), workstr);
	}
	return 0;
}

//TODO: cmds compare
int ViewManager::ProcessInputCommand()
{
	int rtn = Edit_GetText(hInput, wbuf, 1024);
	if (rtn > 0) {
		wsprintf(wbuf + 1024, L"Get command [%s]", wbuf);
		LogToView(LTV_COMMENTMESSAGE, wbuf + 1024);
		Edit_SetText(hInput, L"");

		std::vector<std::wstring> cmds;
		splitString(wbuf, cmds, L" ");

		if (cmds.size() > 0) {
			if (cmdIdentify(L"setpath", cmds.at(0).c_str()) == 0) {
				ProcessInputCommandSetLocation(cmds);
			}
			if (cmdIdentify(L"setlimit", cmds.at(0).c_str()) == 0) {
				ProcessInputCommandSetLimit(cmds);
			}
			if (cmdIdentify(L"enablelimit", cmds.at(0).c_str()) == 0) {
				ProcessInputCommandSetLimit(cmds);
			}
			if (cmdIdentify(L"disablelimit", cmds.at(0).c_str()) == 0) {
				ProcessInputCommandSetLimit(cmds);
			}
			if (cmdIdentify(L"settorrentlimit", cmds.at(0).c_str()) == 0) {
				ProcessInputCommandSetTorrentLimit(cmds);
			}
		}
	}
	return 0;
}

class TMCBCmdSetLocation : public CmdSetLocationCB {
public:
	ViewManager* view;
	virtual int Process(CmdSetLocation* cmd) {
		wsprintf(wbuf, L"Set location [%d] to [%s] completed [%d]", cmd->idset.size(), cmd->location.c_str(), cmd->returncode);
		view->LogToView(LTV_COMMENTMESSAGE, wbuf);
		return 0;
	}
} tmcbcsl;

int ViewManager::ProcessInputCommandSetLocation(std::vector<std::wstring>& cmds)
{
	std::vector<std::wstring> ids;
	std::set<int> iids;
	int iid;
	if (cmds.size() >= 3) {
		splitString(cmds.at(1).c_str(), ids, L",");
		for (std::vector<std::wstring>::iterator itds = ids.begin();
			itds != ids.end();
			itds++) {
			iid = _wtoi((*itds).c_str());
			iids.insert(iid);
		}

		if (iids.size() > 0) {
			CmdSetLocation* cmd = new CmdSetLocation();
			cmd->idset.insert(iids.begin(), iids.end());
			cmd->location = cmds.at(2);
			cmd->profile = defaultprofile;
			tmcbcsl.view = this;
			cmd->callback = &tmcbcsl;

			transmissionmgr->PutCommand(cmd);
		}
	}
	return 0;
}

int ViewManager::ProcessInputCommandSetLimit(std::vector<std::wstring>& cmds)
{
	int ltt = 0;
	int lmtspeed = 0;
	TransmissionProfile * prof = NULL;

	if (currentnode) {
		prof = GetViewNodeProfile(currentnode);
	}
	if (prof == NULL) {
		prof = defaultprofile;
	}
	if (prof) {
		if (cmds.at(0).compare(L"setlimit") == 0) {
			if (cmds.size() >= 3) {
				if (cmds.at(1).compare(L"up") == 0) {
					ltt = CSLA_UP;
				}
				else {
					ltt = CSLA_DOWN;
				}

				lmtspeed = _wtoi(cmds.at(2).c_str());
				CmdSetLimit* cmd = new CmdSetLimit();
				cmd->profile = prof;
				cmd->limitspeed = lmtspeed;
				cmd->limitaction = ltt;

				std::vector<std::wstring> tidss;
				if (cmds.size() > 3) {
					splitString(cmds.at(3).c_str(), tidss, L",");
					for (std::vector<std::wstring>::iterator itvs = tidss.begin(); itvs != tidss.end(); itvs++) {
						cmd->tidset.insert(_wtoi((*itvs).c_str()));
					}
				}

				transmissionmgr->PutCommand(cmd);
			}
		}
		if (cmds.at(0).compare(L"enablelimit") == 0) {
			if (cmds.size() >= 2) {
				if (cmds.at(1).compare(L"up") == 0) {
					ltt = CSLA_ENABLE_UP;
				}
				else {
					ltt = CSLA_ENABLE_DOWN;
				}

				CmdSetLimit* cmd = new CmdSetLimit();
				cmd->profile = prof;
				cmd->limitaction = ltt;

				std::vector<std::wstring> tidss;
				if (cmds.size() > 2) {
					splitString(cmds.at(2).c_str(), tidss, L",");
					for (std::vector<std::wstring>::iterator itvs = tidss.begin(); itvs != tidss.end(); itvs++) {
						cmd->tidset.insert(_wtoi((*itvs).c_str()));
					}
				}

				transmissionmgr->PutCommand(cmd);
			}
		}
		if (cmds.at(0).compare(L"disablelimit") == 0) {
			if (cmds.size() >= 2) {
				if (cmds.at(1).compare(L"up") == 0) {
					ltt = CSLA_DISABLE_UP;
				}
				else {
					ltt = CSLA_DISABLE_DOWN;
				}

				CmdSetLimit* cmd = new CmdSetLimit();
				cmd->profile = prof;
				cmd->limitaction = ltt;
				std::vector<std::wstring> tidss;
				if (cmds.size() > 2) {
					splitString(cmds.at(2).c_str(), tidss, L",");
					for (std::vector<std::wstring>::iterator itvs = tidss.begin(); itvs != tidss.end(); itvs++) {
						cmd->tidset.insert(_wtoi((*itvs).c_str()));
					}
				}

				transmissionmgr->PutCommand(cmd);
			}
		}
	}
	return 0;
}

int ViewManager::ProcessInputCommandSetTorrentLimit(std::vector<std::wstring>& cmds)
{
	int ltt = 0;
	int lmtspeed = 0;
	TransmissionProfile * prof = NULL;

	if (currentnode) {
		prof = GetViewNodeProfile(currentnode);
	}
	if (prof == NULL) {
		prof = defaultprofile;
	}
	if (prof) {
		if (cmds.at(0).compare(L"settorrentlimit") == 0) {
			if (cmds.size() >= 3) {
				if (cmds.at(1).compare(L"up") == 0) {
					ltt = CSLA_UP;
				}
				else {
					ltt = CSLA_DOWN;
				}

				lmtspeed = _wtoi(cmds.at(3).c_str());
				std::vector<std::wstring> tidss;
				splitString(cmds.at(2).c_str(), tidss, L",");

				if (tidss.size() > 0) {
					CmdSetLimit* cmd = new CmdSetLimit();
					cmd->profile = prof;
					cmd->limitspeed = lmtspeed;
					cmd->limitaction = ltt;

					for (std::vector<std::wstring>::iterator itvs = tidss.begin(); itvs != tidss.end(); itvs++) {
						cmd->tidset.insert(_wtoi((*itvs).c_str()));
					}

					transmissionmgr->PutCommand(cmd);
				}
			}
		}
		if (cmds.at(0).compare(L"enablelimit") == 0) {
			if (cmds.size() >= 2) {
				if (cmds.at(1).compare(L"up") == 0) {
					ltt = CSLA_ENABLE_UP;
				}
				else {
					ltt = CSLA_ENABLE_DOWN;
				}

				CmdSetLimit* cmd = new CmdSetLimit();
				cmd->profile = prof;
				cmd->limitaction = ltt;

				transmissionmgr->PutCommand(cmd);
			}
		}
		if (cmds.at(0).compare(L"disablelimit") == 0) {
			if (cmds.size() >= 2) {
				if (cmds.at(1).compare(L"up") == 0) {
					ltt = CSLA_DISABLE_UP;
				}
				else {
					ltt = CSLA_DISABLE_DOWN;
				}

				CmdSetLimit* cmd = new CmdSetLimit();
				cmd->profile = prof;
				cmd->limitaction = ltt;

				transmissionmgr->PutCommand(cmd);
			}
		}
	}
	return 0;
}
int ViewManager::ProcessInputCommandEnableLimit(std::vector<std::wstring>& cmds)
{
	int ltt = 0;
	TransmissionProfile * prof = NULL;

	if (currentnode) {
		prof = GetViewNodeProfile(currentnode);
	}
	if (prof == NULL) {
		prof = defaultprofile;
	}
	if (prof) {
		if (cmds.size() >= 2) {
			if (cmds.at(1).compare(L"up") == 0) {
				ltt = CSLA_ENABLE_UP;
			}
			else {
				ltt = CSLA_ENABLE_DOWN;
			}

			CmdSetLimit* cmd = new CmdSetLimit();
			cmd->profile = prof;
			cmd->limitaction = ltt;

			transmissionmgr->PutCommand(cmd);
		}
	}
	return 0;
}

TransmissionProfile * ViewManager::GetCurrentProfile()
{
	TransmissionProfile* prof = NULL;
	if (currentnode) {
		std::map<std::wstring, TransmissionProfile*>::iterator itpf = profiles.begin();
		bool keepseek = itpf != profiles.end();
		bool found = false;
		while (keepseek) {
			found = itpf->second->HasViewNode(currentnode);
			keepseek = found ? false : keepseek;
			itpf++;
			keepseek = itpf != profiles.end() ? keepseek : false;
		}
		prof = found ? itpf->second : prof;
	}
	else
	{
		prof = defaultprofile;
	}
	return prof;
}

int VCRotation::Process(ViewManager * mgr)
{
	mgr->RefreshCurrentNode();

	VCRotation *vcr = new VCRotation();
	vcr->timetorun = GetTickCount() + 10000;
	mgr->PutCommand(vcr);
	return 0;
}
