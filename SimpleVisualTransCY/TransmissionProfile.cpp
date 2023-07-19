#include "TransmissionProfile.h"
#include "INIFileTool.h"

#include <Windows.h>

TransmissionProfile::TransmissionProfile()
	:inrefresh(false)
{
	state = TransmissionSessionState::NOSTATE;

	totalview = ViewNode::NewViewNode(VNT_GROUP);
	totalview->name = L"Torrents";
	profileview = ViewNode::NewViewNode(VNT_PROFILE);
	profileview->name = L"Profile";

	profileview->AddNode(totalview);
}

TransmissionProfile::~TransmissionProfile()
{
	if (profileview) {
		delete profileview;
		profileview = nullptr;
	}
	for (std::map<long, TorrentNode*>::iterator ittn = torrents.begin();
		ittn != torrents.end();
		ittn++) {
		delete ittn->second;
	}
	torrents.clear();

	tvmap.clear();
}

int TransmissionProfile::GetTorrentViewNodes(long tid, std::set<ViewNode*>& vds)
{
	int rtn = 0;
	std::map<long, TorrentNode*>::iterator ittm = torrents.find(tid);
	if (ittm != torrents.end()) {
		rtn = GetTorrentViewNodes(ittm->second, vds);
	}
	return rtn;
}

int TransmissionProfile::GetTorrentViewNodes(TorrentNode * torrent, std::set<ViewNode*>& vds)
{
	std::map<TorrentNode*, std::set<ViewNode*>>::iterator itvm;

	itvm = tvmap.find(torrent);
	if (itvm != tvmap.end()) {
		vds.insert(itvm->second.begin(), itvm->second.end());
	}

	return vds.size();
}

TrackerCY * TransmissionProfile::GetTracker(std::wstring & tcklink)
{
	bool keepseek;
	std::set<TrackerCY*>::iterator ittk;
	TrackerCY* fck = NULL;

	ittk = trackers.begin();
	keepseek = ittk != trackers.end();
	while (keepseek) {
		if ((*ittk)->url == tcklink) {
			fck = *ittk;
			keepseek = false;
		}

		ittk++;
		keepseek = ittk == trackers.end() ? false : keepseek;
	}

	if (fck == NULL) {
		fck = new TrackerCY();
		fck->url = tcklink;
		fck->name = fck->url;
		size_t fpp = fck->name.find(L"://");
		if (fpp != std::wstring::npos) {
			fck->name = fck->name.substr(fpp + 3); // +3 wsclen(L"://")
		}
		fpp = fck->name.find(L"/");
		if (fpp != std::wstring::npos) {
			fck->name = fck->name.substr(0, fpp);
		}
		fck->id = trackers.size();
		trackers.insert(fck);
	}
	return fck;
}

extern WCHAR wbuf[2048];

int TransmissionProfile::UpdateTorrentViewNodes(long tid)
{
	std::map<long, TorrentNode*>::iterator ittn = torrents.find(tid);
	if (ittn != torrents.end()) {
		TorrentNode* tnt = ittn->second;

		// add torrent into total view
		bool btn = totalview->HasTorrent(tnt);
		if (!btn) {
			ViewNode* vnd = ViewNode::NewViewNode(VNT_TORRENT);
			vnd->SetTorrent(tnt);
			wsprintf(wbuf, L"%04d: %s", tnt->id, tnt->name.c_str());
			vnd->name = wbuf;
			totalview->AddNode(vnd);
			tvmap[tnt].insert(vnd);

			UpdateTorrentTrackerViewNodes(vnd);
		}
	}
	return 0;
}

int TransmissionProfile::UpdateViewNodeInvalid(long tid)
{
	std::map<long, TorrentNode*>::iterator ittn = torrents.find(tid);
	if (ittn != torrents.end()) {
		ittn->second->valid = false;
	}
	return 0;
}

int TransmissionProfile::UpdateProfileTrackers(TorrentNode * torrent)
{
	std::set<TrackerCY*> tks;
	TrackerCY* ctk;

	for (int ii = 0; ii < (int)torrent->rawtrackers.count; ii++) {
		ctk = GetTracker(torrent->rawtrackers.items[ii].url);
		tks.insert(ctk);
	}

	if (tks.size() > 0) {
		if (torrent->_trackers.count > 0) {
			delete[] torrent->_trackers.items;
		}
		torrent->_trackers.count = tks.size();
		torrent->_trackers.items = new TrackerCY*[torrent->_trackers.count];
		int ii = 0;
		for (std::set<TrackerCY*>::iterator ittk = tks.begin();
			ittk != tks.end();
			ittk++) {
			torrent->_trackers.items[ii] = *ittk;
			ii++;
		}
	}
	return 0;
}

int TransmissionProfile::UpdateTorrentTrackerViewNodes(ViewNode * vnd)
{
	if (vnd->GetType() == VNT_TORRENT) {
		TorrentNode* trt = vnd->GetTorrent();
		if (trt->_trackers.count > 0) {
			ViewNode* tkg = vnd->GetTrackerGroup();
			ViewNode* tkn;
			TrackerCY* tkr;
			if (tkg == NULL) {
				tkg = ViewNode::NewViewNode(VNT_TRACKERS);
				tkg->name = L"Trackers";
				vnd->AddNode(tkg);
			}
			for (int ii = 0; ii < (int)trt->_trackers.count; ii++) {
				tkn = tkg->GetTracker(trt->_trackers.items[ii]->id);
				if (tkn == NULL) {
					tkn = ViewNode::NewViewNode(VNT_TRACKER);
					tkr = trt->_trackers.items[ii];
					tkn->SetTracker(tkr);
					wsprintf(wbuf, L"%04d: %s", tkr->id, tkr->name.c_str());
					tkn->name = wbuf;
					tkg->AddNode(tkn);
				}
			}
		}
	}
	return 0;
}

int TransmissionProfile::UpdateTorrent(TorrentNode * torrent)
{
	long nid = torrent->id;
	int rtn = UPN_NOUPDATE;

	if (torrent->rawtrackers.count > 0) {
		UpdateProfileTrackers(torrent);
	}

	std::map<long, TorrentNode*>::iterator ittn = torrents.find(nid);
	TorrentNode* nnd = NULL;
	if (ittn == torrents.end()) {
		nnd = new TorrentNode();
		torrents[nid] = nnd;
		rtn = UPN_NEWNODE;
	}
	else {
		nnd = ittn->second;
	}
	nnd->Copy(torrent);
	nnd->DispRefresh();
	if (rtn == UPN_NOUPDATE) {
		rtn = nnd->state ? UPN_UPDATE : UPN_NOUPDATE;
	}

	UpdateTorrentViewNodes(torrent->id);
	return rtn;
}
