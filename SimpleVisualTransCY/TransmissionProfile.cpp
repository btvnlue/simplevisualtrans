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
	tvmap.clear();
	for (std::map<long, TorrentNode*>::iterator ittn = torrents.begin();
		ittn != torrents.end();
		ittn++) {
		delete ittn->second;
	}
	torrents.clear();

}

TorrentNode * TransmissionProfile::GetTorrent(long tid)
{
	std::map<long, TorrentNode*>::iterator ittn = torrents.find(tid);
	TorrentNode* nnd = nullptr;
	if (ittn != torrents.end()) {
		nnd = ittn->second;
	}

	return nnd;
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

int TransmissionProfile::GetValidTorrentIds(std::set<long>& tids)
{
	for (std::map<long, TorrentNode*>::iterator ittn = torrents.begin();
		ittn != torrents.end();
		ittn++) {
		if (ittn->second->valid) {
			tids.insert(ittn->first);
		}
	}

	return 0;
}

int TransmissionProfile::GetValidTorrents(std::set<TorrentNode*> tts)
{
	std::set<long> tds;
	GetValidTorrentIds(tds);
	TorrentNode* trt;
	for (std::set<long>::iterator itds = tds.begin();
		itds != tds.end();
		itds++) {
		trt = GetTorrent(*itds);
		if (trt) {
			tts.insert(trt);
		}
	}
	return 0;
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

int TransmissionProfile::AddTorrentTotalView(TorrentNode* trt)
{
	// add torrent into total view
	bool btn = totalview->HasTorrent(trt);
	if (!btn) {
		ViewNode* vnd = ViewNode::NewViewNode(VNT_TORRENT);
		vnd->SetTorrent(trt);
		wsprintf(wbuf, L"%04d: %s", trt->id, trt->name.c_str());
		vnd->name = wbuf;
		totalview->AddNode(vnd);
		tvmap[trt].insert(vnd);
		updateviewnodes.insert(vnd);
	}

	return 0;
}

int TransmissionProfile::UpdateTorrentViewNodes(long tid)
{
	std::map<long, TorrentNode*>::iterator ittn = torrents.find(tid);
	if (ittn != torrents.end()) {
		TorrentNode* tnt = ittn->second;
		UpdateTorrentViewNodes(tnt);
	}
	return 0;
}

int TransmissionProfile::UpdateTorrentViewNodes(TorrentNode * ttn)
{
	if (ttn) {
		AddTorrentTotalView(ttn);

		std::set<ViewNode*> vns;
		GetTorrentViewNodes(ttn, vns);
		for (std::set<ViewNode*>::iterator itvn = vns.begin();
			itvn != vns.end();
			itvn++) {
			if (ttn->valid) {
				UpdateTorrentTrackerViewNodes(*itvn);
				UpdateTorrentFileViewNodes(*itvn);
			}
			else {
				(*itvn)->valid = false;
			}
			updateviewnodes.insert(*itvn);
		}
	}
	return 0;
}

int TransmissionProfile::MarkTorrentInvalid(long tid)
{
	int rtn = 1;
	TorrentNode* ttn = GetTorrent(tid);
	if (ttn) {
		ttn->valid = false;
		rtn = 0;
	}
	return rtn;
}

//int TransmissionProfile::UpdateProfileTrackers(TorrentNode * torrent)
//{
	//std::set<TrackerCY*> tks;
	//TrackerCY* ctk;

	//for (int ii = 0; ii < (int)torrent->rawtrackers.count; ii++) {
	//	ctk = GetTracker(torrent->rawtrackers.items[ii].url);
	//	tks.insert(ctk);
	//}

	//if (tks.size() > 0) {
	//	if (torrent->_trackers.count > 0) {
	//		delete[] torrent->_trackers.items;
	//	}
	//	torrent->_trackers.count = tks.size();
	//	torrent->_trackers.items = new TrackerCY*[torrent->_trackers.count];
	//	int ii = 0;
	//	for (std::set<TrackerCY*>::iterator ittk = tks.begin();
	//		ittk != tks.end();
	//		ittk++) {
	//		torrent->_trackers.items[ii] = *ittk;
	//		ii++;
	//	}
	//}
//	return 0;
//}

int TransmissionProfile::UpdateTorrentFileViewNodes(ViewNode* vnd)
{
	ViewNodeType vnt = vnd->GetType();

	switch (vnt) {
	case VNT_TORRENT:
	{
		TorrentNode* trt = vnd->GetTorrent();
		if (trt->files) {
			ViewNode* fnd = vnd->GetFilesNode();
			if (fnd == NULL) {
				ViewNode * nfn = ViewNode::NewViewNode(VNT_FILEPATH);
				nfn->file = trt->files;
				wsprintf(wbuf, L"Files: %s Count: %d Size: %s", nfn->file->name.c_str(), nfn->file->count, nfn->file->dispbkmg);
				nfn->name = wbuf;
				vnd->AddNode(nfn);
				UpdateTorrentFileViewNodes(nfn);
			}
		}
	}
	break;
	case VNT_FILEPATH:
		if (vnd->file) {
			TorrentFileNode * tff;
			ViewNode * nfn;
			for (std::map<long, TorrentFileNode*>::iterator ittf = vnd->file->nodes.begin();
				ittf != vnd->file->nodes.end();
				ittf++) {
				tff = ittf->second;
				nfn = ViewNode::NewViewNode(tff->IsPath() ? VNT_FILEPATH : VNT_FILE);
				nfn->file = tff;
				wsprintf(wbuf, L"%s [ %s / %d ]", nfn->file->name.c_str(), nfn->file->dispbkmg, nfn->file->count);
				nfn->name = wbuf;
				vnd->AddNode(nfn);
				if (tff->IsPath()) {
					UpdateTorrentFileViewNodes(nfn);
				}
			}
			updateviewnodes.insert(vnd);
		}
	}
	return 0;
}

int TransmissionProfile::UpdateTorrentTrackerViewNodes(ViewNode * vnd)
{
	if (vnd->GetType() == VNT_TORRENT) {
		TorrentNode* trt = vnd->GetTorrent();
		if (trt->__trackers.size() > 0) {
			ViewNode* tkg = vnd->GetTrackerGroup();
			ViewNode* tkn;
			TrackerCY* tkr;
			if (tkg == NULL) {
				tkg = ViewNode::NewViewNode(VNT_TRACKERS);
				tkg->name = L"Trackers";
				vnd->AddNode(tkg);
			}
			for (std::set<TrackerCY*>::iterator ittc = trt->__trackers.begin();
				ittc != trt->__trackers.end();
				ittc++) {
				tkn = tkg->GetTracker((*ittc)->id);
				if (tkn == NULL) {
					tkn = ViewNode::NewViewNode(VNT_TRACKER);
					tkr = *ittc;
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

int TransmissionProfile::UpdateStatusDispValues()
{
	FormatNumberBKMG(wbuf, 20, stat.downspeed);
	wsprintf(dispdownspeed, L"%s/s", wbuf);
	FormatNumberBKMG(wbuf, 20, stat.upspeed);
	wsprintf(dispupspeed, L"%s/s", wbuf);
	FormatNumberView(disptorrentcount, 20, stat.totalcount);
	FormatNumberView(dispactivecount, 20, stat.activecount);
	FormatNumberView(disppausecount, 20, stat.pausecount);

	FormatNumberView(disptotaldownsize, 20, stat.total.downsize);
	FormatNumberView(disptotalupsize, 20, stat.total.upsize);
	FormatNumberView(disptotalfiles, 20, stat.total.filesadd);
	FormatNumberView(disptotalactive, 20, stat.total.sessionactive);
	FormatNumberView(disptotalsessions, 20, stat.total.sessioncount);

	FormatNumberView(dispcurrentdownsize, 20, stat.current.downsize);
	FormatNumberView(dispcurrentupsize, 20, stat.current.upsize);
	FormatNumberView(dispcurrentfiles, 20, stat.current.filesadd);
	FormatNumberView(dispcurrentactive, 20, stat.current.sessionactive);
	FormatNumberView(dispcurrentsessions, 20, stat.current.sessioncount);

	return 0;
}

bool TransmissionProfile::HasViewNode(ViewNode * vnd)
{
	ViewNode* rvn = vnd->GetRoot();
	bool rtn = (rvn == profileview);
	return rtn;
}

int TransmissionProfile::UpdateTorrent(TorrentNode * torrent)
{
	long nid = torrent->id;
	int rtn = UPN_NOUPDATE;

	//if (torrent->rawtrackers.count > 0) {
	//	UpdateProfileTrackers(torrent);
	//}

	TorrentNode* nnd = GetTorrent(nid);
	if (nnd == nullptr) {
		nnd = new TorrentNode();
		nnd->id = nid;
		torrents[nid] = nnd;
		rtn = UPN_NEWNODE;
	}

	nnd->Copy(torrent);
	nnd->DispRefresh();
	if (rtn == UPN_NOUPDATE) {
		rtn = nnd->state ? UPN_UPDATE : UPN_NOUPDATE;
	}

	UpdateTorrentViewNodes(torrent->id);
	return rtn;
}

int TransmissionProfile::UpdateRawTorrent(int tid, RawTorrentValuePair * rawtt)
{
	int rtn = UPN_NOUPDATE;

	TorrentNode* ttn = GetTorrent(tid);
	if (ttn == nullptr) {
		ttn = new TorrentNode();
		ttn->id = tid;
		torrents[tid] = ttn;
		rtn = UPN_NEWNODE;
	}
	CopyRawTorrent(ttn, rawtt);
	ttn->DispRefresh();
	if (rtn == UPN_NOUPDATE) {
		rtn = ttn->state ? UPN_UPDATE : UPN_NOUPDATE;
	}

	if (rtn != UPN_NOUPDATE) {
		UpdateTorrentViewNodes(ttn->id);
		updatetorrents.insert(ttn);
	}
	return rtn;
}

int TransmissionProfile::copyRawFileStat(TorrentFileNode* file, RawTorrentValuePair * rawnode)
{
	int rtn = 0;
	wchar_t* fullpath = nullptr;
	unsigned long long size = 0;
	wchar_t sepch = L'/';

	if (file) {
		if (rawnode->valuetype == RTT_OBJECT) {
			for (unsigned long ii = 0; ii < rawnode->vvobject.count; ii++) {
				if (wcscmp(L"wanted", rawnode->vvobject.items[ii].key) == 0) {
					file->wanted = rawnode->vvobject.items[ii].vvint;
				}
				if (wcscmp(L"priority", rawnode->vvobject.items[ii].key) == 0) {
					file->priority = rawnode->vvobject.items[ii].vvint;
				}
				if (wcscmp(L"bytesCompleted", rawnode->vvobject.items[ii].key) == 0) {
					file->completed = rawnode->vvobject.items[ii].vvint64;
				}
			}
		}
	}

	return rtn;
}

int TransmissionProfile::CopyRawTorrent(TorrentNode * ttn, RawTorrentValuePair * rawnode)
{
	int rtn = 0;
	int state = TTSTATE_NONE;

	if (rawnode->valuetype == RTT_OBJECT) {
		ttn->exist = true;

		for (unsigned long ii = 0; ii < rawnode->vvobject.count; ii++) {
			if (wcscmp(L"id", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->id != rawnode->vvobject.items[ii].vvint) {
					ttn->id = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_BASEINFO;
				}
			}
			if (wcscmp(L"secondsDownloading", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->downloadTime != rawnode->vvobject.items[ii].vvint) {
					ttn->downloadTime = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"secondsDownloading", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->seedTime != rawnode->vvobject.items[ii].vvint) {
					ttn->seedTime = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"rateDownload", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->downspeed != rawnode->vvobject.items[ii].vvint) {
					ttn->downspeed = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"rateUpload", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->upspeed != rawnode->vvobject.items[ii].vvint) {
					ttn->upspeed = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"magnetLink", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->magnet.compare(rawnode->vvobject.items[ii].vvstr) != 0) {
					ttn->magnet = rawnode->vvobject.items[ii].vvstr;
					state |= TTSTATE_BASEINFO;
				}
			}
			if (wcscmp(L"name", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->name.compare(rawnode->vvobject.items[ii].vvstr) != 0) {
					ttn->name = rawnode->vvobject.items[ii].vvstr;
					state |= TTSTATE_BASEINFO;
				}
			}
			if (wcscmp(L"totalSize", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->size != rawnode->vvobject.items[ii].vvint64) {
					ttn->size = rawnode->vvobject.items[ii].vvint64;
					state |= TTSTATE_BASEINFO;
				}
			}
			if (wcscmp(L"isPrivate", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->privacy != rawnode->vvobject.items[ii].vvbool) {
					ttn->privacy = rawnode->vvobject.items[ii].vvbool;
					state |= TTSTATE_BASEINFO;
				}
			}
			if (wcscmp(L"addedDate", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->adddate != rawnode->vvobject.items[ii].vvint) {
					ttn->adddate = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"startDate", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->startdate != rawnode->vvobject.items[ii].vvint) {
					ttn->startdate = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"activityDate", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->activedate != rawnode->vvobject.items[ii].vvint) {
					ttn->activedate = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"doneDate", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->donedate != rawnode->vvobject.items[ii].vvint) {
					ttn->donedate = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"pieceCount", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->piececount != rawnode->vvobject.items[ii].vvint) {
					ttn->piececount = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_BASEINFO;
				}
			}
			if (wcscmp(L"pieceSize", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->piecesize != rawnode->vvobject.items[ii].vvint) {
					ttn->piecesize = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_BASEINFO;
				}
			}
			if (wcscmp(L"downloadDir", rawnode->vvobject.items[ii].key) == 0) {
				if (wcscmp(ttn->_path, rawnode->vvobject.items[ii].vvstr) != 0) {
					wcscpy_s(ttn->_path, 1024, rawnode->vvobject.items[ii].vvstr);
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"downloadedEver", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->downloaded != rawnode->vvobject.items[ii].vvint64) {
					ttn->downloaded = rawnode->vvobject.items[ii].vvint64;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"uploadedEver", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->uploaded != rawnode->vvobject.items[ii].vvint64) {
					ttn->uploaded = rawnode->vvobject.items[ii].vvint64;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"leftUntilDone", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->leftsize != rawnode->vvobject.items[ii].vvint64) {
					ttn->leftsize = rawnode->vvobject.items[ii].vvint64;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"desiredAvailable", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->desired != rawnode->vvobject.items[ii].vvint64) {
					ttn->desired = rawnode->vvobject.items[ii].vvint64;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"corrupt", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->corrupt != rawnode->vvobject.items[ii].vvint64) {
					ttn->corrupt = rawnode->vvobject.items[ii].vvint64;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"uploadRatio", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->ratio != rawnode->vvobject.items[ii].vvdouble) {
					ttn->ratio = rawnode->vvobject.items[ii].vvdouble;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"isFinished", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->done != rawnode->vvobject.items[ii].vvbool) {
					ttn->done = rawnode->vvobject.items[ii].vvbool;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"pieces", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->pieces.compare(rawnode->vvobject.items[ii].vvstr) != 0) {
					ttn->pieces = rawnode->vvobject.items[ii].vvstr;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"isStalled", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->stalled != rawnode->vvobject.items[ii].vvbool) {
					ttn->stalled = rawnode->vvobject.items[ii].vvbool;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"status", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->status != rawnode->vvobject.items[ii].vvint) {
					ttn->status = rawnode->vvobject.items[ii].vvint;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"recheckProgress", rawnode->vvobject.items[ii].key) == 0) {
				if (ttn->recheck != rawnode->vvobject.items[ii].vvdouble) {
					ttn->recheck = rawnode->vvobject.items[ii].vvdouble;
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"errorString", rawnode->vvobject.items[ii].key) == 0) {
				if (wcscmp(ttn->_error, rawnode->vvobject.items[ii].vvstr) != 0) {
					wcscpy_s(ttn->_error, 256, rawnode->vvobject.items[ii].vvstr);
					state |= TTSTATE_TRANINFO;
				}
			}
			if (wcscmp(L"trackers", rawnode->vvobject.items[ii].key) == 0) {
				if (rawnode->vvobject.items[ii].valuetype == RTT_ARRAY) {
					int tst = 0;
					for (unsigned long tt = 0; tt < rawnode->vvobject.items[ii].vvobject.count; tt++) {
						tst += copyRawTrackers(ttn, &rawnode->vvobject.items[ii].vvobject.items[tt]);
					}
					state |= tst > 0 ? TTSTATE_BASEINFO : state;
				}
			}
			if (wcscmp(L"files", rawnode->vvobject.items[ii].key) == 0) {
				if (rawnode->vvobject.items[ii].valuetype == RTT_ARRAY) {
					if (ttn->files == nullptr) {
						ttn->files = new TorrentFileNode();
						ttn->files->name = ttn->_path;
						TorrentFileNode * nfn;
						for (unsigned long tt = 0; tt < rawnode->vvobject.items[ii].vvobject.count; tt++) {
							nfn = new TorrentFileNode();
							nfn->id = tt;
							nfn->count = -1;
							copyRawFile(ttn->files, nfn, &rawnode->vvobject.items[ii].vvobject.items[tt]);
						}
						ttn->files->UpdateStat();
						state |= TTSTATE_BASEINFO;

						updatefiles.insert(ttn->files);
					}
				}
			}
			if (wcscmp(L"fileStats", rawnode->vvobject.items[ii].key) == 0) {
				if (rawnode->vvobject.items[ii].valuetype == RTT_ARRAY) {
					if (ttn->files) {
						TorrentFileNode* sdf;
						int fst = 0;
						for (unsigned long tt = 0; tt < rawnode->vvobject.items[ii].vvobject.count; tt++) {
							sdf = ttn->files->GetFile(tt);
							if (sdf) {
								fst += copyRawFileStat(sdf, &rawnode->vvobject.items[ii].vvobject.items[tt]);
							}
						}
						state |= fst > 0 ? TTSTATE_BASEINFO : state;
						ttn->files->UpdateStat();
					}
				}
			}

		}
	}
	
	if (state != TTSTATE_NONE) {
		updatetorrents.insert(ttn);
	}
	return 0;
}

TrackerCY * TransmissionProfile::GetRawTracker(RawTorrentValuePair * rawnode)
{
	//wchar_t* annc = nullptr;
	std::wstring annc;
	TrackerCY* tracker = nullptr;
	if (rawnode->valuetype == RTT_OBJECT) {
		for (unsigned long ii = 0; ii < rawnode->vvobject.count; ii++) {
			if (wcscmp(L"announce", rawnode->vvobject.items[ii].key) == 0) {
				annc = rawnode->vvobject.items[ii].vvstr;
			}
		}

		if (annc.length() >= 0) {
			std::map<std::wstring, TrackerCY*>::iterator ittc = _trackers.find(annc);
			if (ittc == _trackers.end()) {
				tracker = new TrackerCY();
				tracker->id = _trackers.size();
				_trackers[annc] = tracker;

				for (unsigned long ii = 0; ii < rawnode->vvobject.count; ii++) {
					if (wcscmp(L"announce", rawnode->vvobject.items[ii].key) == 0) {
						tracker->url = rawnode->vvobject.items[ii].vvstr;
					}
					if (wcscmp(L"sitename", rawnode->vvobject.items[ii].key) == 0) {
						tracker->name = rawnode->vvobject.items[ii].vvstr;
					}
					if (wcscmp(L"id", rawnode->vvobject.items[ii].key) == 0) {
						tracker->rawid = rawnode->vvobject.items[ii].vvint;
					}
				}
			}
		}
	}

	return tracker;
}

int TransmissionProfile::copyRawFile(TorrentFileNode* root, TorrentFileNode* file, RawTorrentValuePair * rawnode)
{
	int rtn = 0;
	wchar_t* fullpath = nullptr;
	unsigned long long size = 0;
	wchar_t sepch = L'/';

	if (rawnode->valuetype == RTT_OBJECT) {
		for (unsigned long ii = 0; ii < rawnode->vvobject.count; ii++) {
			if (wcscmp(L"name", rawnode->vvobject.items[ii].key) == 0) {
				fullpath = rawnode->vvobject.items[ii].vvstr;
			}
			if (wcscmp(L"length", rawnode->vvobject.items[ii].key) == 0) {
				size = rawnode->vvobject.items[ii].vvint64;
			}
		}

		if (root) {
			if (fullpath) {
				file->size = size;
				wchar_t * pch = wcsrchr(fullpath, sepch);
				if (pch) {
					file->name = pch + 1;
					*pch = 0;
					TorrentFileNode* tfp = root->GetPathNode(fullpath);
					tfp->Add(file);
				}
				else {
					// no path, under root dir
					file->name = fullpath;
					root->Add(file);
				}
			}
		}
	}

	return rtn;
}

int TransmissionProfile::copyRawTrackers(TorrentNode * ttn, RawTorrentValuePair * rawnode)
{
	int rtn = 0;
	if (rawnode->valuetype == RTT_OBJECT) {
		TrackerCY* ctc = GetRawTracker(rawnode);
		if (ctc) {
			std::set<TrackerCY*>::iterator ittc = ttn->__trackers.find(ctc);
			if (ittc == ttn->__trackers.end()) {
				ttn->__trackers.insert(ctc);
			}
		}
	}

	return rtn;
}
