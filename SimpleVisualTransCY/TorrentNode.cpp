#include "TorrentNode.h"
#include <Windows.h>
#include <time.h>
#include "Utilities.h"

int FormatNumberView(wchar_t* buf, size_t bufsz, unsigned __int64 size)
{
	unsigned int ccn;
	wchar_t tbf[128];
	int rtn = 0;

	buf[0] = 0;
	tbf[0] = 0;
	if (size > 0) {
		while (size >= 1000) {
			ccn = size % 1000;
			wsprintf(tbf, L",%03d%s", ccn, buf);
			rtn = wsprintf(buf, L"%s", tbf);
			size /= 1000;
		}
		if (size > 0) {
			ccn = size % 1000;
			rtn = wsprintf(buf, L"%d%s", ccn, tbf);
		}
	}
	else {
		rtn = wsprintf(buf, L"0");
	}
	return rtn;
}

int obsoleted_FormatNumberBKMG(wchar_t* buf, size_t bufsz, unsigned __int64 size)
{
	unsigned int rst = size & 0x3FF;
	wchar_t smark[] = L"BKMGTPEZY";
	int smarkp = 0;
	int rtn = 0;

	if (size > 768) {
		if (size < (1 << 10)) {
			size = 0;
		}
		while (size > ((1 << 10) - 1)) {
			smarkp++;
			rst = size & 0x3FF;
			size >>= 10;
		}

		rst = (rst * 1000) >> 10;
		if (rst == 0) {
			rtn = wsprintf(buf, L"%d %c", (unsigned int)size, rst, smark[smarkp]);
		}
		else {
			rtn = wsprintf(buf, L"%d.%03d %c", (unsigned int)size, rst, smark[smarkp]);
		}
	}
	else {
		rtn = wsprintf(buf, L"%d %c", rst, smark[smarkp]);
	}
	return rtn;
}

int FormatNumberBKMG(wchar_t* buf, size_t bufsz, unsigned __int64 size)
{
	unsigned int rst = 0;
	wchar_t smark[] = L"BKMGTPEZY";
	int smarkp = 0;
	int rtn = 0;

	while (size > 768) {
		rst = size & 0x3FF;
		size >>= 10;
		smarkp++;
	}

	rst = (rst * 1000) >> 10;

	if (rst == 0) {
		rtn = wsprintf(buf, L"%d %c", (unsigned int)size, smark[smarkp]);
	}
	else {
		rtn = wsprintf(buf, L"%d.%03d %c", (unsigned int)size, rst, smark[smarkp]);
	}

	return rtn;
}

int FormatViewStatus(wchar_t* wbuf, size_t bsz, long status)
{
	int rtn = 0;

	switch (status) {
	case TTS_DOWNLOADING:
		rtn = wsprintf(wbuf, L"Downloading");
		break;
	case TTS_SEEDING:
		rtn = wsprintf(wbuf, L"Seeding");
		break;
	case TTS_PAUSED:
		rtn = wsprintf(wbuf, L"Paused");
		break;
	case TTS_VERIFY:
		rtn = wsprintf(wbuf, L"Verify");
		break;
	case TTS_PENDING:
		rtn = wsprintf(wbuf, L"Pending");
		break;
	case TTS_QUEUED:
		rtn = wsprintf(wbuf, L"Queued");
		break;
	default:
		rtn = wsprintf(wbuf, L"%d", status);
	}
	return rtn;
}

int FormatDoublePercentage(wchar_t* wbuf, size_t bsz, double pval)
{
	int rtn = 0;

	long lft = (long)((pval - (int)pval) * 100);
	rtn = wsprintf(wbuf, L"%d.%02d %%", (long)pval, lft);
	return rtn;
}

int FormatIntegerDate(wchar_t* wbuf, size_t bsz, long mtt)
{
	int rtn = 0;

	if (mtt > 0) {
		tm ttm;
		//_gmtime32_s(&ttm, &mtt);
		_localtime32_s(&ttm, &mtt);
		rtn = wsprintf(wbuf, L"%4d-%02d-%02d %02d:%02d:%02d", ttm.tm_year + 1900, ttm.tm_mon + 1, ttm.tm_mday, ttm.tm_hour, ttm.tm_min, ttm.tm_sec);
	}
	else {
		rtn = wsprintf(wbuf, L"");
	}
	return rtn;
}


int TorrentNode::ClearState()
{
	state = 0;
	return 0;
}

int TorrentNode::SetState(unsigned long sst)
{
	state |= sst;
	return 0;
}

TorrentNode::TorrentNode()
	:state(0)
{
}

TorrentNode::~TorrentNode()
{
	CleanTrackers();
}

int convertPiecesData(std::wstring& pcs, unsigned char* buf, long bufsz, long piececnt)
{
	if (pcs.length() > 0) {
		DWORD datsize;
		DWORD dfg;
		int sbt = 0;
		CryptStringToBinary(pcs.c_str(), pcs.length(), CRYPT_STRING_BASE64, NULL, &datsize, NULL, &dfg);
		if (datsize > 0) {
			//unsigned char* dff = (unsigned char*)malloc(datsize);
			unsigned char* dff = (unsigned char*)BufferedJsonAllocator::GetInstance()->Malloc(datsize);
			if (dff) {
				BOOL btn = CryptStringToBinary(pcs.c_str(), pcs.length(), CRYPT_STRING_BASE64, dff, &datsize, NULL, &dfg);
				long bitcnt = datsize * 8;
				if ((piececnt > bitcnt - 8) && (piececnt <= bitcnt)) {
					long bitfct = 0;
					while (piececnt < bufsz) {
						piececnt <<= 1;
						bitfct++;
					}

					long bitidx;
					long nbitidx;
					//long bitmod;
					long bitidxcnt;
					long pctvvv;
					for (int ii = 0; ii < bufsz; ii++) {
						pctvvv = 0;
						bitidxcnt = 0;
						bitidx = ii * piececnt / bufsz;
						nbitidx = (ii + 1)*piececnt / bufsz;
						nbitidx += bitidx == nbitidx ? 1 : 0;
						while (bitidx < nbitidx) {
							bitcnt = bitidx >> bitfct;
							pctvvv += (dff[bitcnt >> 3] << (bitcnt % 8)) & (0x80) ? 1 : 0;
							bitidxcnt++;
							bitidx++;
						}
						buf[ii] = (unsigned char)(pctvvv * 0xFF / bitidxcnt);
					}
				}
				//free(dff);
			}
		}
	}
	return 0;
}

//#define STATED_COPY(_ori, _var, _sst) if (!(this->_var == _ori->_var)) {this->_var = _ori->_var;SetState(_sst);}
//#define STATED_STR_COPY(_ori, _var, _ssz, _sst) if (wcscmp(_ori->_var, _var)!=0) {wcscpy_s(_var, _ssz, _ori->_var); SetState(_sst);}

//int TorrentNode::Copy(TorrentNode * node)
//{
//	this->id = node->id;
//	ClearState();
//
//	STATED_COPY(node, name, TTSTATE_BASEINFO);
//	STATED_COPY(node, size, TTSTATE_BASEINFO);
//	STATED_COPY(node, privacy, TTSTATE_BASEINFO);
//	STATED_COPY(node, adddate, TTSTATE_BASEINFO);
//	STATED_COPY(node, startdate, TTSTATE_BASEINFO);
//	STATED_COPY(node, piececount, TTSTATE_BASEINFO);
//	STATED_COPY(node, piecesize, TTSTATE_BASEINFO);
//	STATED_STR_COPY(node, _path, 256, TTSTATE_BASEINFO);
//	STATED_COPY(node, magnet, TTSTATE_BASEINFO);
//
//	STATED_COPY(node, downloaded, TTSTATE_TRANINFO);
//	STATED_COPY(node, uploaded, TTSTATE_TRANINFO);
//	STATED_COPY(node, leftsize, TTSTATE_TRANINFO);
//	STATED_COPY(node, desired, TTSTATE_TRANINFO);
//	STATED_COPY(node, corrupt, TTSTATE_TRANINFO);
//	STATED_COPY(node, ratio, TTSTATE_TRANINFO);
//	STATED_COPY(node, done, TTSTATE_TRANINFO);
//	STATED_COPY(node, pieces, TTSTATE_TRANINFO);
//	STATED_COPY(node, stalled, TTSTATE_TRANINFO);
//	STATED_COPY(node, valid, TTSTATE_TRANINFO);
//	STATED_COPY(node, status, TTSTATE_TRANINFO);
//	STATED_COPY(node, recheck, TTSTATE_TRANINFO);
//	STATED_STR_COPY(node, _error, 1024, TTSTATE_TRANINFO);
//
//	STATED_COPY(node, downspeed, TTSTATE_SPEED);
//	STATED_COPY(node, upspeed, TTSTATE_SPEED);
//
//	//bool keepseek;
//	//int ii = 0;
//	//bool trackermatch = true;
//	//bool trackerfound;
//
//	//if (_trackers.count != node->_trackers.count) {
//	//	trackermatch = false;
//	//	keepseek = false;
//	//}
//	//keepseek = ii < (int)node->_trackers.count ? keepseek : false;
//
//	//while (keepseek) { // find every tracker in update
//	//	trackerfound = false;
//	//	for (int kk = 0; kk < (int)_trackers.count; kk++) {
//	//		if (node->_trackers.items[ii] == _trackers.items[kk]) {
//	//			trackerfound = true;
//	//		}
//	//	}
//
//	//	if (trackerfound == false) {
//	//		trackermatch = false;
//	//		keepseek = false;
//	//	}
//
//	//	ii++;
//	//	keepseek = ii < (int)node->_trackers.count ? keepseek : false;
//	//}
//
//	//if (trackermatch == false) {
//	//	SetState(TTSTATE_BASEINFO);
//	//	if (_trackers.count > 0) {
//	//		delete[] _trackers.items;
//	//	}
//	//	_trackers.count = node->_trackers.count;
//	//	_trackers.items = new TrackerCY*[_trackers.count];
//	//	for (int ii = 0; ii < (int)node->_trackers.count; ii++) {
//	//		_trackers.items[ii] = node->_trackers.items[ii];
//	//	}
//	//}
//	return 0;
//}


WCHAR ttdispbuf[1024];

int TorrentNode::DispRefresh()
{
	wsprintf(dispid, L"%04d", id);
	FormatNumberView(dispsize, 1024, size);
	FormatNumberBKMG(ttdispbuf, 1024, downspeed);
	wsprintf(dispdownspeed, L"%s/s", ttdispbuf);
	FormatNumberBKMG(ttdispbuf, 1024, upspeed);
	wsprintf(dispupspeed, L"%s/s", ttdispbuf);
	FormatNumberBKMG(ttdispbuf, 1024, downloadlimit * 1024);
	wsprintf(dispdownlimit, L"%s/s", ttdispbuf);
	FormatNumberBKMG(ttdispbuf, 1024, uploadlimit * 1024);
	wsprintf(dispuplimit, L"%s/s", ttdispbuf);
	FormatViewStatus(dispstatus, 1024, status);

	if ((status == TTS_VERIFY) || (status == TTS_PENDING)) {
		FormatDoublePercentage(dispratio, 1024, recheck * 100);
	}
	else {
		FormatDoublePercentage(dispratio, 1024, ratio * 100);
	}
	FormatIntegerDate(dispadddate, 1024, adddate);
	FormatIntegerDate(dispstartdate, 1024, startdate);
	FormatNumberView(dispdownloaded, 1024, downloaded);
	FormatNumberView(dispuploaded, 1024, uploaded);
	FormatIntegerDate(dispdonedate, 1024, donedate);
	FormatNumberBKMG(disppiecesize, 1024, piecesize);

	FormatNumberView(ttdispbuf, 1024, leftsize);
	FormatDoublePercentage(ttdispbuf + 512, 1024, size > 0 ? 100.0 * leftsize / size : 0);
	wsprintf(displeft, L"%s (%s)", ttdispbuf, ttdispbuf + 512);

	FormatNumberView(dispavialable, 1024, desired);
	FormatNumberView(disppiececount, 1024, piececount);
	errorflag = wcslen(_error) > 0;

	////////////////////////////////////
	// refresh pieces data
	if (piecesdata.items == NULL) {
		piecesdata.count = sizeof(_disppieces);
		piecesdata.items = _disppieces;
	}
	convertPiecesData(pieces, piecesdata.items, piecesdata.count, piececount);

	if (__trackers.size() > 0) {
		wsprintf(disptracker, L"%04d: %s", (*__trackers.begin())->groupid, (*__trackers.begin())->name.c_str());
	}
	return 0;
}

int TorrentNode::CleanTrackers()
{
	//if (rawtrackers.count > 0) {
	//	delete[] rawtrackers.items;
	//	rawtrackers.count = 0;
	//}
	//if (_trackers.count > 0) {
	//	delete[] _trackers.items;
	//	_trackers.count = 0;
	//}

	return 0;
}

ViewNode::ViewNode()
	: parent(NULL)
	, torrent(NULL)
	, valid(true)
	, tracker(NULL)
	, file(NULL)
{
}

ViewNode::~ViewNode()
{
	for (std::list<ViewNode*>::iterator itvn = nodes.begin();
		itvn != nodes.end();
		itvn++) {
		delete *itvn;
	}
	nodes.clear();
}

long ViewNode::vnodeidx = 0;

ViewNode * ViewNode::NewViewNode(ViewNodeType vnt)
{
	ViewNode* vnd = new ViewNode();
	vnd->id = vnodeidx++;
	vnd->type = vnt;
	return vnd;
}

int ViewNode::AddNode(ViewNode * node)
{
	nodes.insert(nodes.end(), node);
	node->parent = this;
	return 0;
}

bool ViewNode::HasTorrent(TorrentNode * trt)
{
	std::list<ViewNode*>::iterator itvn = nodes.begin();
	bool found = torrent == trt;
	bool keepseek = found ? false : itvn != nodes.end();

	while (keepseek) {
		found = (*itvn)->HasTorrent(trt);
		itvn++;
		keepseek = found ? false : itvn != nodes.end();
	}
	return found;
}

bool ViewNode::HasViewNode(ViewNode * vnd)
{
	std::list<ViewNode*>::iterator itvn = nodes.begin();
	bool found = this == vnd;
	bool keepseek = found ? false : itvn != nodes.end();

	while (keepseek) {
		found = (*itvn)->HasViewNode(vnd);
		itvn++;
		keepseek = found ? false : itvn != nodes.end();
	}
	return found;
}

int ViewNode::GetAllTorrentsViewNode(std::set<ViewNode*>& tts)
{
	for (std::list<ViewNode*>::iterator itvn = nodes.begin();
		itvn != nodes.end();
		itvn++) {
		(*itvn)->GetAllTorrentsViewNode(tts);
	}
	if (torrent) {
		if (torrent->valid) {
			tts.insert(tts.end(), this);
		}
	}
	return 0;
}

int ViewNode::GetAllTypeNode(ViewNodeType vnt, std::set<ViewNode*>& vns)
{
	if (type == vnt) {
		vns.insert(this);
	}
	for (std::list<ViewNode*>::iterator itvn = nodes.begin();
		itvn != nodes.end();
		itvn++) {
		(*itvn)->GetAllTypeNode(vnt, vns);
	}

	return 0;
}

ViewNodeType ViewNode::GetType()
{
	return type;
}

int ViewNode::SetTorrent(TorrentNode * trt)
{
	torrent = trt;
	return 0;
}

int ViewNode::SetTracker(TrackerCY * tkr)
{
	tracker = tkr;
	return 0;
}

TorrentNode * ViewNode::GetTorrent()
{
	TorrentNode* ttn = NULL;
	if (torrent) {
		ttn = torrent;
	}
	else {
		if (parent) {
			ttn = parent->GetTorrent();
		}
	}
	return ttn;
}

ViewNode * ViewNode::GetTrackerGroup()
{
	ViewNode* tkv = NULL;
	for (std::list<ViewNode*>::iterator itvn = nodes.begin();
		itvn != nodes.end();
		itvn++) {
		if ((*itvn)->GetType() == VNT_TRACKERS) {
			tkv = *itvn;
		}
	}

	return tkv;
}

//ViewNode * ViewNode::GetTracker(int id)
//{
//	ViewNode* tkn = NULL;
//	for (std::list<ViewNode*>::iterator itvn = nodes.begin();
//		itvn != nodes.end();
//		itvn++) {
//		if ((*itvn)->GetType() == VNT_TRACKER) {
//			if ((*itvn)->tracker->id == id) {
//				tkn = *itvn;
//			}
//		}
//	}
//
//	return tkn;
//}

ViewNode * ViewNode::GetFilesNode()
{
	ViewNode* tkn = NULL;
	for (std::list<ViewNode*>::iterator itvn = nodes.begin();
		itvn != nodes.end();
		itvn++) {
		if ((*itvn)->GetType() == VNT_FILEPATH) {
			tkn = *itvn;
		}
	}

	return tkn;
}

ViewNode * ViewNode::GetRoot()
{
	ViewNode *rtn = parent ? parent->GetRoot() : this;
	return rtn;
}


wchar_t TorrentFileNode::pathch = L'/';

int TorrentFileNode::Add(TorrentFileNode * fnd)
{
	if (fnd) {
		if (fnd->IsPath()) {
			paths[fnd->id] = fnd;
		}
		else {
			files[fnd->id] = fnd;
		}
		fnd->parent = this;
	}
	return 0;
}

TorrentFileNode * TorrentFileNode::GetPathNode(wchar_t * path)
{
	wchar_t sepch = L'/';
	std::wstring fpt;
	std::wstring ppt;
	wchar_t * pch = wcschr(path, sepch);

	// ppt := first part of path, before 1st '/'
	// fpt := last part of path
	if (pch) {
		fpt = pch + 1;
		*pch = 0;
		ppt = path;
	}
	else {
		ppt = path;
	}


	TorrentFileNode* tpn = nullptr;
	if (ppt.length() > 0) {
		for (std::map<long, TorrentFileNode*>::iterator itfn = paths.begin();
			itfn != paths.end();
			itfn++) {
			if (itfn->second->name.compare(ppt) == 0) {
				tpn = itfn->second;
			}
		}
		if (tpn == nullptr) {
			tpn = new TorrentFileNode();
			tpn->id = GetPathId();
			tpn->name = ppt;
			this->Add(tpn);
		}
	}
	else {
		tpn = this;
	}


	if (tpn) {
		if (fpt.length() > 0) {
			wchar_t * tpw = new wchar_t[fpt.length() + 1];
			wcscpy_s(tpw, fpt.length() + 1, fpt.c_str());
			tpn = tpn->GetPathNode(tpw);
			delete tpw;
		}
	}
	return tpn;
}

int TorrentFileNode::Path(wchar_t * pbuf)
{
	if (parent) {
		parent->Path(pbuf);
	}
	if (IsPath()) {
		wchar_t pbb[2] = { pathch, 0 };
		wcscat_s(pbuf, 1024, pbb);
		wcscat_s(pbuf, 1024, name.c_str());
	}
	return 0;
}

int TorrentFileNode::UpdateDisp()
{
	if (disppath[0] == 0) {
		Path(disppath);
	}
	FormatNumberView(dispsize, 30, size);
	FormatNumberBKMG(dispbkmg, 20, size);
	wcscpy_s(dispname, 128, name.c_str());
	wsprintf(dispid, L"%06d", id);

	wsprintf(disphas, L"%c", wanted == 0 ? L'X' : L'O');
	wsprintf(disppr, L"%c", priority > 0 ? L'H' : (priority < 0 ? L'L' : L'-'));

	return 0;
}

int TorrentFileNode::UpdateStat()
{
	if (count >= 0) {
		count = 0;
		size = 0;
		pathcount = 0;
		for (std::map<long, TorrentFileNode*>::iterator itfn = paths.begin();
			itfn != paths.end();
			itfn++) {
			itfn->second->UpdateStat();
			size += itfn->second->size;
			count += itfn->second->count;
			pathcount += 1;
		}
		for (std::map<long, TorrentFileNode*>::iterator itfn = files.begin();
			itfn != files.end();
			itfn++) {
			size += itfn->second->size;
			count += 1;
		}
	}
	UpdateDisp();
	return 0;
}

TorrentFileNode * TorrentFileNode::GetFile(long fid)
{
	TorrentFileNode* tfn = NULL;
	bool keepseek;
	std::map<long, TorrentFileNode*>::iterator itff = files.find(fid);
	if (itff == files.end()) {
		itff = paths.begin();
		keepseek = itff != paths.end();
		while (keepseek) {
			tfn = itff->second->GetFile(fid);
			keepseek = tfn == NULL;
			itff++;
			keepseek = itff == paths.end() ? false : keepseek;
		}
	}
	else {
		tfn = itff->second;
	}
	return tfn;
}

int TorrentFileNode::GetFiles(std::set<TorrentFileNode*>& fls)
{
	for (std::map<long, TorrentFileNode*>::iterator itfp = files.begin(); itfp != files.end(); itfp++) {
		fls.insert(itfp->second);
	}
	for (std::map<long, TorrentFileNode*>::iterator itfp = paths.begin(); itfp != paths.end(); itfp++) {
		itfp->second->GetFiles(fls);
	}
	return 0;
}

long TorrentFileNode::GetPathId()
{
	long pid = pathid;
	if (parent) {
		pid = parent->GetPathId();
	}
	else {
		pid = pathid--;
	}
	return pid;
}

TorrentNode * TorrentFileNode::GetTorrent()
{
	TorrentNode * trn = NULL;
	if (pnode) {
		trn = pnode;
	} else {
		if (parent) {
			trn = parent->GetTorrent();
		}
	}
	return trn;
}

int TorrentPeerNode::UpdateDisp()
{
	FormatDoublePercentage(dispprogress, 30, progress * 100);
	FormatNumberBKMG(dispup, 30, upspeed);
	FormatNumberBKMG(dispdown, 30, downspeed);
	wsprintf(dispport, L"%d", port);
	return 0;
}
