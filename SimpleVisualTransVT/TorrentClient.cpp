#include "TorrentClient.h"
#include <rapidjson/document.h>
#include <rapidjson/encodings.h>
#include <rapidjson/writer.h>
#include <sstream>
#include <Windows.h>
#include <algorithm>

#pragma comment(lib, "Crypt32.lib")

#define REFRESHTORRENTFIELDS L"id,name,totalSize,trackers,status,downloadDir,\
uploadedEver,downloadedEver,secondsDownloading,secondsSeeding,rateDownload,\
errorString,isPrivate,isFinished,isStalled,rateUpload,errorString,uploadRatio,\
leftUntilDone,doneDate,activityDate,pieceCount,pieceSize,doneDate,activityDate,\
addedDate,startDate,desiredAvailable"
WCHAR pathsepchar = L'/';

int splitStringList(const std::wstring& str, std::list<std::wstring>& lst)
{
	size_t dfs = str.find(L",");
	size_t ffs = 0;
	while (dfs != std::wstring::npos) {
		lst.push_back(str.substr(ffs, dfs - ffs));
		ffs = dfs + 1;
		dfs = str.find(L",", ffs);
	}
	lst.push_back(str.substr(ffs));
	return 0;
}

TorrentClient::TorrentClient()
	: grouproot(NULL)
	, curl(NULL)
	, profiledataloader(NULL)
	, profile(NULL)
	, localprofile(NULL)
{
	curl = new CurlSessionVT();
	std::list<std::wstring> ilst;
	splitStringList(REFRESHTORRENTFIELDS, ilst);
	refreshtorrentfields.insert(ilst.begin(), ilst.end());
	grouproot = new TorrentGroupVT();
	allnodes = new TorrentGroupVT();
	trackers = new TorrentTrackerGroup();
	profiledataloader = new CProfileData();
	profile = new CurlProfile();
	localprofile = new CurlProfile();

	grouproot->name = L"Torrent View";
	allnodes->name = L"Torrents";
	trackers->name = L"Trackers";

	grouproot->AddGroup(allnodes);
	grouproot->AddGroup(trackers);

	WCHAR pbf[1024];
	DWORD bsz = GetCurrentDirectory(512, pbf + 512);
	if (bsz > 0) {
		wsprintf(pbf, L"%s\\%s", pbf + 512, L"visualtransvt.ini");
		profiledataloader->LoadProfile(pbf);
		LoadProfileSite();
	}
}

TorrentClient::~TorrentClient()
{
	if (curl) {
		delete curl;
		curl = NULL;
	}
	if (allnodes) {
		allnodes->DeleteTorrents();
	}
	if (grouproot) {
		delete grouproot;
		grouproot = NULL;
	}
	if (profiledataloader) {
		delete profiledataloader;
		profiledataloader = NULL;
	}
	if (profile) {
		delete profile;
		profile = NULL;
	}
	if (localprofile) {
		delete localprofile;
		localprofile = NULL;
	}
}

int buildRefreshRequest(TorrentClientRequest* req, std::set<std::wstring>& rts, std::wstring& out)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);

	for (std::set<std::wstring>::iterator itsf = rts.begin()
		; itsf != rts.end()
		; itsf++) {
		vvv.SetString((*itsf).c_str(), allocator);
		fdary.PushBack(vvv, allocator);
	}

	bool donefiles = req->refresh.reqfiles;
	if (req->refresh.reqfiles) {
		fdary.PushBack(L"files", allocator);
	}
	if (req->refresh.reqfilestat) {
		fdary.PushBack(L"fileStats", allocator);
	}
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	if (req->refresh.tid < 0) {
		if (req->refresh.recent) {
			args.AddMember(L"ids", L"recently-active", allocator);
		}
		else if (!donefiles) {
			//fdary.PushBack(L"files", allocator);
		}
	}
	else {
		args.AddMember(L"ids"
			, rapidjson::GenericValue<rapidjson::UTF16<>>(rapidjson::Type::kNumberType).SetInt(req->refresh.tid)
			, allocator);
		fdary.PushBack(L"pieces", allocator);
	}
	args.AddMember(L"fields", fdary, allocator);
	//rapidjson::GenericValue<rapidjson::UTF16<>> ags(rapidjson::kObjectType);
	//ags.AddMember(L"arguments", fds, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-get", allocator);
	rjd.AddMember(L"tag", 4, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	out = sbuf.GetString();
	return 0;
}

int ConvertUTFCodePure(const char* instr, std::wstring& wstr)
{
	size_t insz = strlen(instr);
	std::wstringstream owstr;

	for (unsigned int ii = 0; ii < insz; ii++) {
		owstr << (wchar_t)instr[ii];
	}
	wstr = owstr.str();
	return 0;
}

std::wstring getTrackerName(const std::wstring& tck)
{
	size_t sps;
	size_t cps;
	std::wstring ttn(tck);

	sps = tck.find(L"://", 0);
	if (sps != std::wstring::npos) {
		sps += 3;
		cps = tck.find(L'/', sps);
		cps = cps == std::wstring::npos ? cps = tck.length() : cps;
		ttn = tck.substr(sps, cps - sps);
	}

	return ttn;
}

void extractTracker(rapidjson::GenericValue<rapidjson::UTF16<>>& trka, TorrentNodeVT* torrent, int tidx)
{
	if (trka.HasMember(L"announce") && trka[L"announce"].IsString()) {
		std::wstring anc = trka[L"announce"].GetString();
		torrent->trackers.items[tidx].url = anc;
		torrent->trackers.items[tidx].name = getTrackerName(anc);
	}
}

int convertPiecesData(TorrentNodeVT* node)
{
	if (node->pieces.length() > 0) {
		DWORD dsz;
		DWORD dfg;
		unsigned char cbt;
		int sbt = 0;
		CryptStringToBinary(node->pieces.c_str(), node->pieces.length(), CRYPT_STRING_BASE64, NULL, &dsz, NULL, &dfg);
		if (dsz > 0) {
			unsigned char* dff = (unsigned char*)malloc(dsz);
			if (dff) {
				BOOL btn = CryptStringToBinary(node->pieces.c_str(), node->pieces.length(), CRYPT_STRING_BASE64, dff, &dsz, NULL, &dfg);
				if (node->piecesdata.count == 0) {
					node->piecesdata.items = new unsigned char[dsz * 8];
					node->piecesdata.count = dsz * 8;
				}

				if (node->piecesdata.count == dsz * 8) {
					sbt = 0;
					for (DWORD ii = 0; ii < dsz; ii++) {
						cbt = dff[ii];
						for (int jj = 0; jj < 8; jj++) {
							node->piecesdata.items[sbt] = cbt & 0x80 ? 1 : 0;
							cbt <<= 1;
							sbt++;
						}
					}
				}
				free(dff);
			}
		}
	}
	return 0;
}

TorrentNodeFile* getPathNode(TorrentNodeFile* root, const std::wstring& path)
{
	TorrentNodeFile* pnode = NULL;
	size_t psp = path.find_last_of(pathsepchar);
	std::wstring spath;
	std::wstring sname;
	TorrentNodeFile* ppnode = NULL;
	if (psp == std::wstring::npos) {
		sname = path;
		ppnode = root;
	}
	else {
		spath = path.substr(0, psp);
		sname = path.substr(psp + 1);
		ppnode = getPathNode(root, spath);
	}

	if (ppnode) {
		for (unsigned int ii = 0; ii < ppnode->subfiles.count; ii++) {
			if (ppnode->subfiles.items[ii]->name.compare(sname) == 0) {
				pnode = ppnode->subfiles.items[ii];
			}
		}

		if (pnode == NULL) {
			pnode = new TorrentNodeFile();
			pnode->type = TNF_TYPE::dir;
			pnode->name = sname;
			pnode->path = spath;
			pnode->fullpath = path;
			pnode->parent = ppnode;

			TorrentNodeFile** nfa = new TorrentNodeFile*[ppnode->subfiles.count + 1];
			for (unsigned int ii = 0; ii < ppnode->subfiles.count; ii++) {
				nfa[ii] = ppnode->subfiles.items[ii];
			}
			nfa[ppnode->subfiles.count] = pnode;
			ppnode->subfiles.count++;
			TorrentNodeFile** rfa = ppnode->subfiles.items;
			ppnode->subfiles.items = nfa;
			if (rfa) {
				delete rfa;
			}
		}
	}
	return pnode;
}

int addFileNode(TorrentNodeFile* root, TorrentNodeFile* nfile)
{
	TorrentNodeFile* pnode;
	size_t psp = nfile->fullpath.find_last_of(pathsepchar);

	if (psp == std::wstring::npos) {
		pnode = root;
	}
	else {
		std::wstring spath = nfile->fullpath.substr(0, psp);
		std::wstring sname = nfile->fullpath.substr(psp + 1);
		pnode = getPathNode(root, spath);
	}

	if (pnode) {
		TorrentNodeFile* cfile = NULL;
		for (unsigned int ii = 0; ii < pnode->subfiles.count; ii++) {
			if (pnode->subfiles.items[ii] == nfile) {
				cfile = nfile;
			}
		}
		if (cfile == NULL) {
			TorrentNodeFile** nfa = new TorrentNodeFile*[pnode->subfiles.count + 1];
			for (unsigned int ii = 0; ii < pnode->subfiles.count; ii++) {
				nfa[ii] = pnode->subfiles.items[ii];
			}
			nfa[pnode->subfiles.count] = nfile;
			nfile->parent = pnode;
			pnode->subfiles.count++;
			TorrentNodeFile** rfa = pnode->subfiles.items;
			pnode->subfiles.items = nfa;
			if (rfa) {
				delete rfa;
			}
		}

	}
	return 0;
}

int updateFileNodeTotalCount(TorrentNodeFile* fnode)
{
	fnode->totalcount = 0;
	if (fnode->type == TNF_TYPE::file) {
		fnode->totalcount = 1;
	}
	else if (fnode->type == TNF_TYPE::dir) {
		for (unsigned int ii = 0; ii < fnode->subfiles.count; ii++) {
			fnode->totalcount += updateFileNodeTotalCount(fnode->subfiles.items[ii]);
		}
	}
	return fnode->totalcount;
}

int arrangeTorrentFiles(TorrentNodeVT* trt)
{
	TorrentNodeFile* cfile;
	for (unsigned int ii = 0; ii < trt->files.count;ii++) {
		cfile = &trt->files.items[ii];
		addFileNode(&trt->path, cfile);
	}
	updateFileNodeTotalCount(&trt->path);
	return 0;
}

#define JSTRYSTRING(TOVAL, VALNAME, JSVAL) \
	if (JSVAL.HasMember(VALNAME) && JSVAL[VALNAME].IsString()) { \
		TOVAL = JSVAL[VALNAME].GetString(); \
	}

#define JSTRYINT(TOVAL, VALNAME, JSVAL) \
	if (JSVAL.HasMember(VALNAME) && JSVAL[VALNAME].IsInt()) { \
		TOVAL = JSVAL[VALNAME].GetInt(); \
	}

#define JSTRYINT64(TOVAL, VALNAME, JSVAL) \
	if (JSVAL.HasMember(VALNAME) && JSVAL[VALNAME].IsInt64()) { \
		TOVAL = JSVAL[VALNAME].GetInt64(); \
	}

#define JSTRYDOUBLE(TOVAL, VALNAME, JSVAL) \
	if (JSVAL.HasMember(VALNAME) && JSVAL[VALNAME].IsDouble()) { \
		TOVAL = JSVAL[VALNAME].GetDouble(); \
	}

#define JSTRYBOOL(TOVAL, VALNAME, JSVAL) \
	if (JSVAL.HasMember(VALNAME) && JSVAL[VALNAME].IsBool()) { \
		TOVAL = JSVAL[VALNAME].GetBool(); \
	}

void extractTorrentObject(rapidjson::GenericValue<rapidjson::UTF16<>>& trtn, std::set<TorrentNodeVT*>& nodes)
{
	unsigned long iid;
	TorrentNodeVT* torrent = NULL;
	std::wstring wname;
	size_t wpos;
	long long tll;
	int tii;
	bool tbb;
	unsigned long long utick;

	if (trtn.HasMember(L"id") && trtn[L"id"].IsInt()) {
		iid = trtn[L"id"].GetUint();
		
		for (std::set<TorrentNodeVT*>::iterator ittn = nodes.begin()
			; ittn != nodes.end()
			; ittn++) {
			if ((*ittn)->id == iid) {
				torrent = *ittn;
			}
		}
		if (torrent == NULL) {
			torrent = new TorrentNodeVT();
			torrent->id = iid;
			torrent->path.type = TNF_TYPE::dir;
			nodes.insert(torrent);
		}

		JSTRYSTRING(torrent->name, L"name", trtn);
		JSTRYINT64(torrent->size, L"totalSize", trtn);
		JSTRYINT64(torrent->uploaded, L"uploadedEver", trtn);
		JSTRYINT64(torrent->downloaded, L"downloadedEver", trtn);
		JSTRYINT64(torrent->corrupt, L"corruptEver", trtn);
		JSTRYINT(torrent->downspeed, L"rateDownload", trtn);
		JSTRYINT(torrent->upspeed, L"rateUpload", trtn);
		JSTRYINT(torrent->status, L"status", trtn);
		JSTRYSTRING(torrent->path.fullpath, L"downloadDir", trtn);
		JSTRYINT(torrent->downloadTime, L"secondsDownloading", trtn);
		JSTRYINT(torrent->seedTime, L"secondsSeeding", trtn);
		JSTRYSTRING(torrent->error, L"errorString", trtn);
		JSTRYDOUBLE(torrent->ratio, L"uploadRatio", trtn);
		JSTRYINT64(torrent->leftsize, L"leftUntilDone", trtn);
		JSTRYBOOL(torrent->privacy, L"isPrivate", trtn);
		JSTRYBOOL(torrent->done, L"isFinished", trtn);
		JSTRYBOOL(torrent->stalled, L"isStalled", trtn);
		JSTRYINT(torrent->donedate, L"doneDate", trtn);
		JSTRYINT(torrent->activedate, L"activityDate", trtn);
		JSTRYINT64(torrent->desired, L"desiredAvailable", trtn);
		JSTRYINT(torrent->adddate, L"addedDate", trtn);
		JSTRYINT(torrent->startdate, L"startDate", trtn);
		JSTRYINT(torrent->piececount, L"pieceCount", trtn);
		JSTRYINT(torrent->piecesize, L"pieceSize", trtn);
		JSTRYSTRING(torrent->pieces, L"pieces", trtn);

		if (trtn.HasMember(L"trackers") && trtn[L"trackers"].IsArray()) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& trks = trtn[L"trackers"];
			if (trks.Size() != torrent->trackers.count) {
				if (torrent->trackers.count > 0) {
					delete torrent->trackers.items;
				}
				torrent->trackers.items = new TrackerCY[trks.Size()];
				torrent->trackers.count = 0;
			}
			int ii = 0;
			for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itk = trks.Begin()
				; itk != trks.End()
				; itk++) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trka = *itk;
				extractTracker(trka, torrent, ii);
				torrent->trackers.count = ii + 1;
				ii++;
			}
		}
		if (trtn.HasMember(L"files") && trtn[L"files"].IsArray()) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& trfs = trtn[L"files"];
			if (torrent->files.count == 0) {
				torrent->files.count = trfs.GetArray().Size();
				//int mls = sizeof(struct TorrentNodeFile) * torrent->files.count;
				torrent->files.items = new struct TorrentNodeFile[torrent->files.count];
				if (torrent->files.items) {
					//memset(torrent->files.file, 0, mls);
					int fix = 0;
					for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itf = trfs.Begin()
						; itf != trfs.End()
						; itf++) {
						rapidjson::GenericValue<rapidjson::UTF16<>>& trfl = *itf;
						torrent->files.items[fix].id = fix;
						JSTRYSTRING(wname, L"name", trfl);
						wpos = wname.find_last_of(pathsepchar);
						if (wpos == std::wstring::npos) {
							torrent->files.items[fix].name = wname;
							torrent->files.items[fix].path = L"";
						}
						else {
							torrent->files.items[fix].name = wname.substr(wpos + 1);
							torrent->files.items[fix].path = wname.substr(0, wpos);
						}
						torrent->files.items[fix].fullpath = wname;
						JSTRYINT64(torrent->files.items[fix].size, L"length", trfl);
						JSTRYINT64(torrent->files.items[fix].done, L"bytesCompleted", trfl);
						torrent->files.items[fix].check = true;
						torrent->files.items[fix].priority = 0;
						torrent->files.items[fix].updatetick = GetTickCount64();
						torrent->files.items[fix].type = TNF_TYPE::file;

						fix++;
					}
				}
			}
			arrangeTorrentFiles(torrent);
		}
		if (trtn.HasMember(L"fileStats") && trtn[L"fileStats"].IsArray()) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& trfs = trtn[L"fileStats"];
			unsigned int uix = 0;
			for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itf = trfs.Begin()
				; itf != trfs.End()
				; itf++) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trfl = *itf;
				if (torrent->files.count > uix) {
					JSTRYINT64(tll, L"bytesCompleted", trfl);
					JSTRYBOOL(tbb, L"wanted", trfl);
					JSTRYINT(tii, L"priority", trfl);
					utick = GetTickCount64();
					if (tll != torrent->files.items[uix].done) {
						torrent->files.items[uix].done = tll;
						torrent->files.items[uix].updatetick = utick;
					}
					if (tbb != torrent->files.items[uix].check) {
						torrent->files.items[uix].check = tbb;
						torrent->files.items[uix].updatetick = utick;
					}
					if (tii != torrent->files.items[uix].priority) {
						torrent->files.items[uix].priority = tii;
						torrent->files.items[uix].updatetick = utick;
					}
				}
				uix++;
			}
		}

		convertPiecesData(torrent);

		torrent->updatetick = GetTickCount64();
	}
}

int extractJsonTorrents(std::wstring& jsonstr, std::set<TorrentNodeVT*>& nodes, std::set<long>& removed)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd;

	rapidjson::GenericStringStream<rapidjson::UTF16<>> gssu16((wchar_t*)jsonstr.c_str());
	rjd.ParseStream(gssu16);
	if (!rjd.HasParseError()) {
		if (rjd.HasMember(L"arguments")) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& args = rjd[L"arguments"];
			if (args.HasMember(L"torrents")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trns = args[L"torrents"];
				if (trns.IsArray()) {
					for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator ittr = trns.Begin()
						; ittr != trns.End()
						; ittr++) {
						rapidjson::GenericValue<rapidjson::UTF16<>>& trtn = *ittr;
						if (trtn.IsObject()) {
							extractTorrentObject(trtn, nodes);
						}
					}
				}
			}
			if (args.HasMember(L"removed")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& rmvs = args[L"removed"];
				if (rmvs.IsArray()) {
					for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itrm = rmvs.Begin()
						; itrm != rmvs.End()
						; itrm++) {
						rapidjson::GenericValue<rapidjson::UTF16<>>& trtn = *itrm;
						if (trtn.IsInt()) {
							removed.insert(trtn.GetInt());
						}
					}
				}
			}
		}
	}
	return 0;
}

int TorrentClient::RequestTaskProc(TorrentClientRequest* req)
{
	GeneralRequestTask* task = new GeneralRequestTask(this, req);
	PutTask(task);
	return 0;
}

int TorrentClient::QueryGroupTorrents(TorrentClientRequest* req)
{
	TorrentNodeVT* node;
	for (std::set<TorrentNodeVT*>::iterator ittn = req->getgroupnodes.group->nodes.begin()
		; ittn != req->getgroupnodes.group->nodes.end()
		; ittn++) {
		node = *ittn;
		if (node->valid) {
			(*req->getgroupnodes.cbquery)(req, *ittn);
		}
	}
	(*req->getgroupnodes.cbquery)(req, NULL);
	return 0;
}

int TorrentClient::QueryGroupGroups(TorrentClientRequest* req)
{
	for (std::set<TorrentGroupVT*>::iterator ittn = req->getgroupnodes.group->subs.begin()
		; ittn != req->getgroupnodes.group->subs.end()
		; ittn++) {
		(*req->getgroupgroups.cbquery)(req, *ittn);
	}
	(*req->getgroupnodes.cbquery)(req, NULL);
	return 0;
}

int buildAddTorrentRequest(const std::wstring& turl, bool ismedia, std::wstring& req)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	//rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	vvv.SetString(turl.c_str(), allocator);
	if (ismedia) {
		args.AddMember(L"metainfo", vvv, allocator);
	}
	else {
		args.AddMember(L"filename", vvv, allocator);
	}
	args.AddMember(L"paused", false, allocator);

	rjd.AddMember(L"method", L"torrent-add", allocator);
	rjd.AddMember(L"arguments", args, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

int buildFileRequest(TorrentClientRequest* trq, std::wstring& req)
{
	if (trq->req == TorrentClientRequest::REQ::TORRENTFILEREQUEST) {
		rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
		rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
		rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

		rapidjson::GenericValue<rapidjson::UTF16<>> gvv((int)trq->filerequest.torrentid);
		idary.PushBack(gvv, allocator);

		rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
		args.AddMember(L"ids", idary, allocator);

		rapidjson::GenericValue<rapidjson::UTF16<>> fileary(rapidjson::kArrayType);
		for (int ii = 0; ii < trq->filerequest.fileidscount; ii++) {
			fileary.PushBack((int)trq->filerequest.fileids[ii], allocator);
		}

		if (trq->filerequest.hascheck) {
			if (trq->filerequest.ischeck) {
				args.AddMember(L"files-wanted", fileary, allocator);
			}
			else {
				args.AddMember(L"files-unwanted", fileary, allocator);
			}
		}
		if (trq->filerequest.haspriority) {
			if (trq->filerequest.priority > 0) {
				args.AddMember(L"priority-high", fileary, allocator);
			}
			else if (trq->filerequest.priority < 0) {
				args.AddMember(L"priority-low", fileary, allocator);
			}
			else {
				args.AddMember(L"priority-normal", fileary, allocator);
			}
		}
		rjd.AddMember(L"arguments", args, allocator);
		rjd.AddMember(L"method", L"torrent-set", allocator);

		rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
		rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
		rjd.Accept(writer);

		req = sbuf.GetString();
	}
	return 0;
}

int buildLocationRequest(TorrentClientRequest* trq, std::wstring& req)
{
	if (trq->req == TorrentClientRequest::REQ::SETLOCATION) {
		rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
		rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
		rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

		rapidjson::GenericValue<rapidjson::UTF16<>> gvv((int)trq->location.torrentid);
		idary.PushBack(gvv, allocator);

		rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
		args.AddMember(L"ids", idary, allocator);

		rapidjson::GenericValue<rapidjson::UTF16<>> lcv(rapidjson::kStringType);
		lcv.SetString(trq->location.location, allocator);

		args.AddMember(L"location", lcv, allocator);
		args.AddMember(L"move", true, allocator);

		rjd.AddMember(L"arguments", args, allocator);
		rjd.AddMember(L"method", L"torrent-set-location", allocator);

		rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
		rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
		rjd.Accept(writer);

		req = sbuf.GetString();
	}
	return 0;
}

int buildDeleteRequest(long torrentid, bool deldata, std::wstring& req)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	idary.PushBack((int)torrentid, allocator);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"ids", idary, allocator);
	args.AddMember(L"delete-local-data", deldata, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-remove", allocator);
	rjd.AddMember(L"tag", 5, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

int buildPauseRequest(int tid, bool dopause, std::wstring& req)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	idary.PushBack(tid, allocator);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"ids", idary, allocator);

	//rapidjson::GenericValue<rapidjson::UTF16<>> ags(rapidjson::kObjectType);
	//ags.AddMember(L"arguments", fds, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	if (dopause) {
		rjd.AddMember(L"method", L"torrent-stop", allocator);
	}
	else {
		rjd.AddMember(L"method", L"torrent-start", allocator);
	}
	rjd.AddMember(L"tag", 11, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

int buildVerifyRequest(int tid, std::wstring& req)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	idary.PushBack(tid, allocator);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"ids", idary, allocator);

	//rapidjson::GenericValue<rapidjson::UTF16<>> ags(rapidjson::kObjectType);
	//ags.AddMember(L"arguments", fds, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-verify", allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

int buildSessionRequest(std::wstring& req)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	rjd.AddMember(L"method", L"session-stats", allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

std::wstring analysisRemoteResult(std::wstring& rss)
{
	std::wstring wrt;
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd;

	rapidjson::GenericStringStream<rapidjson::UTF16<>> gssu16((wchar_t*)rss.c_str());
	rjd.ParseStream(gssu16);
	if (!rjd.HasParseError()) {
		if (rjd.HasMember(L"result")) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& rsts = rjd[L"result"];
			if (rsts.IsString()) {
				wrt = rsts.GetString();
			}
		}
	}
	return wrt;
}

int TorrentClient::analysisSession(std::wstring& rss)
{
	int rtn = 0;
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd;

	rapidjson::GenericStringStream<rapidjson::UTF16<>> gssu16((wchar_t*)rss.c_str());
	rjd.ParseStream(gssu16);
	if (!rjd.HasParseError()) {
		if (rjd.HasMember(L"arguments")) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& args = rjd[L"arguments"];
			JSTRYINT(session.downspeed, L"downloadSpeed", args);
			JSTRYINT(session.upspeed, L"uploadSpeed", args);
		}
		else {
			rtn = 1;
		}
	}
	else {
		rtn = 1;
	}
	return rtn;
}

int TorrentClient::AddNewTorrent(TorrentClientRequest* req)
{
	std::wstring rms;
	std::wstring wrn;
	bool ism = false;

	HANDLE htf = CreateFile(
		req->addtorrent.torrentlink
		, GENERIC_READ
		, FILE_SHARE_READ | FILE_SHARE_WRITE
		, NULL
		, OPEN_EXISTING
		, FILE_ATTRIBUTE_NORMAL
		, NULL
	);
	if (htf != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER fsz;
		DWORD dsz;
		if (GetFileSizeEx(htf, &fsz)) {
			if (fsz.HighPart == 0) {
				unsigned char* pfb = (unsigned char*)malloc(fsz.LowPart);
				if (pfb) {
					ReadFile(htf, pfb, fsz.LowPart, &dsz, NULL);
					DWORD sbz = 0;
					BOOL btn = CryptBinaryToString(pfb, dsz, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &sbz);
					if (sbz > 0) {
						TCHAR* b64str = (TCHAR*)malloc(sbz * sizeof(TCHAR));
						if (b64str) {
							btn = CryptBinaryToString(pfb, dsz, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64str, &sbz);
							if (btn) {
								wrn = b64str;
								ism = true;
							}
							free(b64str);
						}
						else {
							rms = L"Out of memory";
						}
					}
					free(pfb);
				}
				else {
					rms = L"Out of memory";
				}
			}
		}
		else {
			wrn = req->addtorrent.torrentlink;
		}
		CloseHandle(htf);
	}
	else {
		wrn = req->addtorrent.torrentlink;
	}

	int qrt;
	if (wrn.length() > 0) {
		std::wstring rstr;
		buildAddTorrentRequest(wrn, ism, rstr);

		qrt = curl->PerformQuery(profile, rstr);
		if (qrt == 0) {
			std::wstring urlresp = curl->GetResponse(profile);
			int csl = urlresp.length();
			char* cresp = (char*)malloc(csl + 1);
			if (cresp) {
				csl = WideCharToMultiByte(CP_ACP, 0, urlresp.c_str(), csl, cresp, csl + 1, NULL, NULL);
				cresp[csl] = 0;
				ConvertUTFCodePure(cresp, rms);
				free(cresp);
				rms = analysisRemoteResult(rms);

				if (rms.length() > 0) {
					int rsl = rms.length() + 1;
					req->addtorrent.result = new WCHAR[rsl];
					wcscpy_s(req->addtorrent.result, rsl, rms.c_str());
				}
			}
		}
		else {
			qrt = 2;
		}
	}
	else {
		qrt = 5;
	}

	req->addtorrent.cbaddresult(req, qrt);
	return 0;
}

int TorrentClient::PauseTorrent(TorrentClientRequest* req)
{
	std::wstring rstr;
	std::wstring rms;
	std::wstring wrn;

	buildPauseRequest(req->pausetorrent.torrentid, req->pausetorrent.dopause, rstr);

	int qrt = curl->PerformQuery(profile, rstr);
	if (qrt == 0) {
		std::wstring urlresp = curl->GetResponse(profile);
		int csl = urlresp.length();
		char* cresp = (char*)malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, urlresp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			ConvertUTFCodePure(cresp, rms);
			free(cresp);
			rms = analysisRemoteResult(rms);

			if (rms.length() > 0) {
				int rsl = rms.length() + 1;
				req->pausetorrent.result = new WCHAR[rsl];
				wcscpy_s(req->pausetorrent.result, rsl, rms.c_str());
			}
		}
	}
	else {
		qrt = 2;
	}
	req->pausetorrent.cbpauseresult(req, qrt);
	return qrt;
}

int TorrentClient::VerifyTorrent(TorrentClientRequest* req)
{
	std::wstring rstr;
	std::wstring rms;
	std::wstring wrn;

	buildVerifyRequest(req->verifytorrent.torrentid, rstr);

	int qrt = curl->PerformQuery(profile, rstr);
	if (qrt == 0) {
		std::wstring urlresp = curl->GetResponse(profile);
		int csl = urlresp.length();
		char* cresp = (char*)malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, urlresp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			ConvertUTFCodePure(cresp, rms);
			free(cresp);
			rms = analysisRemoteResult(rms);

			if (rms.length() > 0) {
				int rsl = rms.length() + 1;
				req->verifytorrent.result = new WCHAR[rsl];
				wcscpy_s(req->verifytorrent.result, rsl, rms.c_str());
			}
		}
	}
	else {
		qrt = 2;
	}
	req->verifytorrent.cbverifyresult(req, qrt);
	return qrt;
}

int TorrentClient::RefreshSession(TorrentClientRequest* req)
{
	std::wstring rstr;
	std::wstring rms;

	buildSessionRequest(rstr);

	int qrt = curl->PerformQuery(profile, rstr);
	if (qrt == 0) {
		std::wstring urlresp = curl->GetResponse(profile);
		int csl = urlresp.length();
		char* cresp = (char*)malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, urlresp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			ConvertUTFCodePure(cresp, rms);
			free(cresp);
			int rst = analysisSession(rms);

			qrt = rst == 0 ? qrt : 1;
		}
	}
	else {
		qrt = 2;
	}

	if (qrt == 0) {
		req->session.cbsessionresult(req, &session);
	}
	else {
		req->session.cbsessionresult(req, NULL);
	}
	return qrt;
}

int TorrentClient::SetTorrentLocation(TorrentClientRequest* req)
{
	std::wstring rstr;
	std::wstring rms;
	std::wstring wrn;

	buildLocationRequest(req, rstr);

	int qrt = curl->PerformQuery(profile, rstr);
	if (qrt == 0) {
		std::wstring urlresp = curl->GetResponse(profile);
		int csl = urlresp.length();
		char* cresp = (char*)malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, urlresp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			ConvertUTFCodePure(cresp, rms);
			free(cresp);
			rms = analysisRemoteResult(rms);

			if (rms.length() > 0) {
				int rsl = rms.length() + 1;
				req->location.result = new WCHAR[rsl];
				wcscpy_s(req->location.result, rsl, rms.c_str());
			}
		}
	}
	else {
		qrt = 2;
	}
	req->location.cblocationresult(req, qrt);
	return qrt;
}


int TorrentClient::DeleteTorrent(TorrentClientRequest* req)
{
	std::wstring rstr;
	std::wstring rms;
	std::wstring wrn;

	buildDeleteRequest(req->deletetorrent.torrentid, req->deletetorrent.deletedata, rstr);

	int qrt = curl->PerformQuery(profile, rstr);
	if (qrt == 0) {
		std::wstring urlresp = curl->GetResponse(profile);
		int csl = urlresp.length();
		char* cresp = (char*)malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, urlresp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			ConvertUTFCodePure(cresp, rms);
			free(cresp);
			rms = analysisRemoteResult(rms);

			if (rms.length() > 0) {
				int rsl = rms.length() + 1;
				req->deletetorrent.result = new WCHAR[rsl];
				wcscpy_s(req->deletetorrent.result, rsl, rms.c_str());
			}
		}
	}
	else {
		qrt = 2;
	}
	req->deletetorrent.cbdelresult(req, qrt);
	return qrt;
}

int TorrentClient::RequestTorrentFiles(TorrentClientRequest* req)
{
	std::wstring rstr;
	std::wstring rms;
	std::wstring wrn;

	buildFileRequest(req, rstr);

	int qrt = curl->PerformQuery(profile, rstr);
	if (qrt == 0) {
		std::wstring urlresp = curl->GetResponse(profile);
		int csl = urlresp.length();
		char* cresp = (char*)malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, urlresp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			ConvertUTFCodePure(cresp, rms);
			free(cresp);
			rms = analysisRemoteResult(rms);

			if (rms.length() > 0) {
				int rsl = rms.length() + 1;
				req->filerequest.result = new WCHAR[rsl];
				wcscpy_s(req->filerequest.result, rsl, rms.c_str());
			}
		}
	}
	else {
		qrt = 2;
	}
	req->filerequest.cbfileresult(req, qrt);
	return qrt;
}

int TorrentClient::QueryNodeFiles(TorrentClientRequest* req)
{
	if (req->getnodefiles.node) {
		if (req->getnodefiles.node->files.count > 0) {
			(*req->getnodefiles.cbnodefile)(req, req->getnodefiles.node);
		}
	}
	(*req->getnodefiles.cbnodefile)(req, NULL);
	return 0;
}

int TorrentClient::LoadProfileSite()
{
	profiledataloader->GetDefaultValue(L"RemoteUrl", profile->siteUrl);
	profiledataloader->GetDefaultValue(L"RemoteUser", profile->siteUser);
	profiledataloader->GetDefaultValue(L"RemotePass", profile->sitePasswd);
	return 0;
}

std::wstring TorrentClient::GetErrorString(int eid)
{
	std::wstring wrt;

	switch (eid) {
	case 0:
		wrt = L"success";
		break;
	case 1:
		wrt = L"connection fail";
		break;
	case 2:
		wrt = L"service unavailable";
		break;
	case 5:
		wrt = L"out of memory";
		break;
	default:
		wrt = L"unknown error";
		break;
	}

	return wrt;
}

int TorrentClient::RefreshTorrents(TorrentClientRequest* req)
{
	int qrt = 0;
	std::set<long> removed;
	ULONGLONG ptick = GetTickCount64();
	std::wstring ucoderesp;

	if (curl->ValidateSession(profile)) {
		std::wstring xquery;
		buildRefreshRequest(req, refreshtorrentfields, xquery);
		qrt = curl->PerformQuery(profile, xquery);
		if (qrt == 0) {
			std::wstring urlresp = curl->GetResponse(profile);
			int csl = urlresp.length();
			char* cresp = (char*)malloc(csl + 1);
			if (cresp) {
				csl = WideCharToMultiByte(CP_ACP, 0, urlresp.c_str(), csl, cresp, csl + 1, NULL, NULL);
				cresp[csl] = 0;
				ConvertUTFCodePure(cresp, ucoderesp);
				free(cresp);

				extractJsonTorrents(ucoderesp, allnodes->nodes, removed);
			}
		}
		else {
			qrt = 2;
		}
	}
	else {
		qrt = 1;
	}

	if (qrt == 0) {
		TorrentNodeVT* ctt;
		std::set<TorrentNodeVT*> tuts;
		std::for_each(allnodes->nodes.begin(), allnodes->nodes.end(), [&tuts, ptick](TorrentNodeVT* node) {
			if (node->updatetick >= ptick) {
				tuts.insert(node);
			}
			});
		std::for_each(tuts.begin(), tuts.end(), [ptick, this, req](TorrentNodeVT* node) {
			if (node->valid) {
				UpdateTrackerGroups(node);
				std::list<TorrentGroupVT*> grps;
				grps.clear();
				grouproot->GetTorrentGroups(node, grps);
				std::for_each(grps.begin(), grps.end(), [this, node, req](TorrentGroupVT*& grp) {
					req->refresh.cbgroupnode(req, node, grp);
					});
			}
			if (req->refresh.reqfiles) {
				req->refresh.cbnodefile(req, node);
			}
			if (req->refresh.reqfilestat) {
				for (unsigned long ii = 0; ii < node->files.count; ii++) {
					if (node->files.items[ii].updatetick >= ptick) {
						req->refresh.cbnodefilestat(req, node, node->files.items + ii);
					}
				}
				//req->refresh.cbnodefilestat(req, NULL, NULL);
			}
			});
		std::for_each(removed.begin(), removed.end(), [&, this, req](const long tid) {
			ctt = allnodes->GetIdTorrent(tid);
			if (ctt) {
				ctt->valid = false;
				std::list<TorrentGroupVT*> groups;
				grouproot->GetTorrentGroups(ctt, groups);
				std::for_each(groups.begin(), groups.end(), [req, ctt](TorrentGroupVT* group) {
					req->refresh.cbremovenode(req, ctt, group);
					});
			}
			});

		UpdateTorrentGroups(grouproot);
		std::set<TorrentGroupVT*> gps;
		grouproot->GetGroups(gps);
		std::for_each(gps.begin(), gps.end(), [ptick, req](TorrentGroupVT* grp) {
			if (ptick < grp->updatetick) {
				(*req->refresh.cbgroupgroup)(req, grp, grp->parent);
			}
			});
		req->refresh.cbgroupnode(req, NULL, NULL);
	}
	else {
		std::wstring estr = GetErrorString(qrt);
		(*req->refresh.cbfailed)(req, estr, qrt);
	}

	return qrt;
}

int TorrentClient::UpdateTorrentGroups(TorrentGroupVT* group)
{
	if (group->subs.size() > 0) {
		std::for_each(group->subs.begin(), group->subs.end(), [this](TorrentGroupVT* ttg) {
			UpdateTorrentGroups(ttg);
			});
	}
	std::set<TorrentNodeVT*> nds;
	group->GetNodes(nds);
	unsigned long long szz = 0;
	long dsp = 0;
	long usp = 0;
	std::for_each(nds.begin(), nds.end(), [&szz, &dsp, &usp](TorrentNodeVT* tdd) {
		szz += tdd->size;
		dsp += tdd->downspeed;
		usp += tdd->upspeed;
		});
	bool omd = false;

	omd = nds.size()==group->nodecount ? omd : true;
	omd = szz == group->size ? omd : true;
	omd = dsp == group->downspeed ? omd : true;
	omd = usp == group->upspeed ? omd : true;
	group->size = szz;
	group->upspeed = usp;
	group->downspeed = dsp;
	group->nodecount = nds.size();

	if (dsp != group->upspeed) {
		omd = true;
	}

	if (omd) {
		group->updatetick = ::GetTickCount64();
	}

	return 0;
}

int TorrentGroupVT::GetNodes(std::set<TorrentNodeVT*>& nds)
{
	if (subs.size() > 0) {
		std::for_each(subs.begin(), subs.end(), [&nds](TorrentGroupVT* ttg) {
			ttg->GetNodes(nds);
			});
	}
	else {
		std::for_each(nodes.begin(), nodes.end(), [&nds](TorrentNodeVT* ndd) {
			nds.insert(ndd);
			});
	}
	return 0;
}


int TorrentClient::UpdateTrackerGroups(TorrentNodeVT* torrent)
{
	TorrentGroupVT* group;
	TorrentNodeVT* etn;
	for (unsigned long ii = 0; ii < torrent->trackers.count; ii++) {
		group = trackers->GetTrackerGroup(torrent->trackers.items[ii].name.c_str());
		if (group) {
			etn = group->GetIdTorrent(torrent->id);
			if (etn == NULL) {
				group->AddTorrent(torrent);
			}
		}
	}
	return 0;
}

TorrentClient::RefreshTorrentTask::RefreshTorrentTask(TorrentClient* clt, TorrentClientRequest* req)
	: client(clt)
	, request(req)
{
}

int TorrentClient::RefreshTorrentTask::Process(TaskEngine* eng)
{
	if (client) {
		client->RefreshTorrents(request);
	}
	return 0;
}

int TorrentNodeHelper::Delete(TorrentNodeVT* node)
{
	if (node) {
		if (node->files.count > 0) {
			delete node->files.items;
			node->files.count = 0;
		}
		if (node->piecesdata.count > 0) {
			delete node->piecesdata.items;
			node->piecesdata.count = 0;
		}
		if (node->trackers.count > 0) {
			delete node->trackers.items;
			node->trackers.count = 0;
		}
		delete node;
	}
	return 0;
}

int TorrentGroupVT::AddGroup(TorrentGroupVT* grp)
{
	subs.insert(grp);
	grp->parent = this;
	return 0;
}

int TorrentGroupVT::AddTorrent(TorrentNodeVT* node)
{
	nodes.insert(node);
	return 0;
}

TorrentGroupVT::~TorrentGroupVT()
{
	for (std::set<TorrentGroupVT*>::iterator ittg = subs.begin()
		; ittg != subs.end()
		; ittg++) {
		delete* ittg;
	}
}

int TorrentGroupVT::DeleteTorrents()
{
	for (std::set<TorrentGroupVT*>::iterator ittg = subs.begin()
		; ittg != subs.end()
		; ittg++) {
		(*ittg)->DeleteTorrents();
	}
	
	for (std::set<TorrentNodeVT*>::iterator ittn = nodes.begin()
		; ittn != nodes.end()
		; ittn++) {
		delete* ittn;
	}
	nodes.clear();
	return 0;
}

int TorrentGroupVT::GetTorrentGroups(TorrentNodeVT* torrent, std::list<TorrentGroupVT*>& groups)
{
	std::set<TorrentNodeVT*>::iterator itnd = nodes.find(torrent);
	
	if (itnd != nodes.end()) {
		groups.push_back(this);
	}
	for (std::set<TorrentGroupVT*>::iterator ittg = subs.begin()
		; ittg != subs.end()
		; ittg++) {
		(*ittg)->GetTorrentGroups(torrent, groups);
	}

	return 0;
}

TorrentNodeVT* TorrentGroupVT::GetIdTorrent(long tid)
{
	std::set<TorrentNodeVT*>::iterator ittn = nodes.begin();
	bool keepseek = ittn != nodes.end();
	TorrentNodeVT* trt = NULL;

	while (keepseek) {
		if ((*ittn)->id == tid) {
			keepseek = false;
			trt = *ittn;
		}
		ittn++;
		keepseek = ittn == nodes.end() ? false : keepseek;
	}
	return trt;
}

int TorrentGroupVT::GetGroups(std::set<TorrentGroupVT*>& groups)
{
	if (subs.size() > 0) {
		std::for_each(subs.begin(), subs.end(), [&groups](TorrentGroupVT* grp) {
			grp->GetGroups(groups);
			groups.insert(grp);
			});
	}
	return 0;
}


TorrentClient::GeneralRequestTask::GeneralRequestTask(TorrentClient* clt, TorrentClientRequest* req)
	: client(clt)
	, request(req)
{
}

int TorrentClient::GeneralRequestTask::Process(TaskEngine* eng)
{
	switch (request->req) {
	case TorrentClientRequest::REQ::QUERYGROUPTORRENTS:
		client->QueryGroupTorrents(request);
		break;
	case TorrentClientRequest::REQ::QUERYGROUPGROUPS:
		client->QueryGroupGroups(request);
		break;
	case TorrentClientRequest::REQ::QUERYTORRENTFILES:
		client->QueryNodeFiles(request);
		break;
	case TorrentClientRequest::REQ::ADDNEWTORRENT:
		client->AddNewTorrent(request);
		break;
	case TorrentClientRequest::REQ::TORRENTFILEREQUEST:
		client->RequestTorrentFiles(request);
		break;
	case TorrentClientRequest::REQ::DELETETORRENT:
		client->DeleteTorrent(request);
		break;
	case TorrentClientRequest::REQ::PAUSETORRENT:
		client->PauseTorrent(request);
		break;
	case TorrentClientRequest::REQ::SETLOCATION:
		client->SetTorrentLocation(request);
		break;
	case TorrentClientRequest::REQ::VERIFYTORRENT:
		client->VerifyTorrent(request);
		break;
	case TorrentClientRequest::REQ::REFRESHSESSION:
		client->RefreshSession(request);
		break;
	}
	return 0;
}

TorrentGroupVT* TorrentTrackerGroup::GetTrackerGroup(const wchar_t* name)
{
	TorrentGroupVT* tgt = NULL;
	std::map<std::wstring, TorrentGroupVT*>::iterator ittk = trackers.find(name);
	if (ittk == trackers.end()) {
		tgt = new TorrentGroupVT();
		tgt->name = name;
		AddGroup(tgt);
	}
	else {
		tgt = ittk->second;
	}
	return tgt;
}

int TorrentTrackerGroup::AddGroup(TorrentGroupVT* group)
{
	TorrentGroupVT::AddGroup(group);
	trackers[group->name] = group;
	return 0;
}
