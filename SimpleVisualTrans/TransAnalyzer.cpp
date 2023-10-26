#include "TransAnalyzer.h"
#include "CurlSession.h"

#include <sstream>

//#define TT_URL L"http://192.168.5.158:9091/transmission/rpc"
//#define TT_USER L"transmission"
//#define TT_PASSWD L"transmission"

#pragma comment(lib, "Crypt32.lib")

//int Log(const wchar_t* msg);

int ConvertUTFCodeNative(const char* instr, std::wstring& wstr)
{
	size_t insz = strlen(instr);
	wchar_t* wcc = (wchar_t*)malloc((insz + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, instr, insz, wcc, insz + 1);
	wstr = wcc;

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

int ConvertUTFCode(const char* instr, std::wstring& wstr)
{
	size_t insz = strlen(instr);
	std::wstringstream owstr;

	size_t ipos = 0;
	size_t opos = 0;
	wchar_t code;
	wchar_t inchar;
	bool keeprun = insz > 0;
	bool cconv;
	while (keeprun) {
		if (instr[ipos] == '\\') {
			if ((insz - ipos) > 1) {
				cconv = true;
				switch (instr[ipos + 1]) {
				case 'r':
					owstr << (wchar_t)'\r';
					break;
				case 'n':
					owstr << (wchar_t)'\n';
					break;
				case '\\':
					owstr << (wchar_t)'\\';
					break;
				case 'u':
					if ((insz - ipos) > 5) {
						inchar = 0;
						for (int ii = 0; ii < 4; ii++) {
							code = instr[ipos + 2 + ii];
							if (code >= '0' && code <= '9') {
								code -= '0';
							}
							else if (code >= 'A' && code <= 'F') {
								code -= 'A';
								code += 10;
							}
							else if (code >= 'a' && code <= 'f') {
								code -= 'a';
								code += 10;
							}
							inchar <<= 4;
							inchar |= code;
						}
						owstr << inchar;
					}
					ipos += 6;
					ipos -= 2;
					break;
				default:
					cconv = false;
				}
				if (cconv) {
					ipos += 2;
					ipos--;
				}
				else {
					owstr << (wchar_t)'\\';
				}
			}
		}
		else {
			owstr << (wchar_t)instr[ipos];
		}

		ipos++;
		opos++;
		keeprun = ipos < insz ? keeprun : false;
	}
	
	/*
	int imcsz = WideCharToMultiByte(CP_UTF8, 0, ostr, opos, NULL, 0, NULL, NULL);
	if (imcsz > 0) {
		resputfbuf = (unsigned char*)malloc(imcsz + 1);
		imcsz = WideCharToMultiByte(CP_UTF8, 0, ostr, opos, (LPSTR)resputfbuf, imcsz, NULL, NULL);
	}

	free(ostr);
	*/
	wstr = owstr.str();

	return 0;
}


int TransAnalyzer::cleanupTorrents()
{
	TorrentNode* node;
	for (std::map<unsigned long, TorrentNode*>::iterator ittn = fulllist->torrents_.begin()
		; ittn != fulllist->torrents_.end()
		; ittn++) {
		node = ittn->second;
		TorrentNodeHelper::DeleteTorrentNode(node);
	}
	
	return 0;
}

TransAnalyzer::TransAnalyzer()
{
	curlSession = new CurlSession();

	groupRoot = new TorrentGroup();
	groupRoot->name = L"Torrent Groups";

	fulllist = new TorrentGroup();
	trackertorrents = new TorrentGroup();
	fulllist->name = L"Torrents";
	trackertorrents->name = L"Trackers";

	groupRoot->addGroup(fulllist);
	groupRoot->addGroup(trackertorrents);

	setupRefreshFields(refreshtorrentfields);
}

int TransAnalyzer::setupRefreshFields(std::set<std::wstring>& fst)
{
	fst.insert(L"id");
	fst.insert(L"name");
	fst.insert(L"totalSize");
	fst.insert(L"trackers");
	fst.insert(L"status");
	fst.insert(L"downloadDir");
	fst.insert(L"uploadedEver");
	fst.insert(L"downloadedEver");
	fst.insert(L"secondsDownloading");
	fst.insert(L"secondsSeeding");
	fst.insert(L"rateDownload");
	fst.insert(L"errorString");
	fst.insert(L"isPrivate");
	fst.insert(L"isFinished");
	fst.insert(L"isStalled");
	fst.insert(L"rateUpload");
	fst.insert(L"errorString");
	fst.insert(L"uploadRatio");
	fst.insert(L"leftUntilDone");
	fst.insert(L"doneDate");
	fst.insert(L"activityDate");
	fst.insert(L"pieceCount");
	fst.insert(L"pieceSize");
	fst.insert(L"doneDate");
	fst.insert(L"activityDate");
	fst.insert(L"addedDate");
	fst.insert(L"startDate");
	fst.insert(L"desiredAvailable");

	return 0;
}

TransAnalyzer::~TransAnalyzer()
{
	cleanupTorrents();
	delete groupRoot;
}

int TransAnalyzer::buildStopRequest(std::wstring& req, int tid, bool dopause)
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

int TransAnalyzer::buildFileRequest(std::wstring& req, FileReqParm& frp)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	rapidjson::GenericValue<rapidjson::UTF16<>> gvv((int)frp.torrentid);
	idary.PushBack(gvv, allocator);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"ids", idary, allocator);

	rapidjson::GenericValue<rapidjson::UTF16<>> fileary(rapidjson::kArrayType);
	for (std::set<long>::iterator itfi = frp.fileIds.begin()
		; itfi != frp.fileIds.end()
		; itfi++) {
		fileary.PushBack((int)*itfi, allocator);
	}

	if (frp.setcheck) {
		if (frp.check) {
			args.AddMember(L"files-wanted", fileary, allocator);
		}
		else {
			args.AddMember(L"files-unwanted", fileary, allocator);
		}
	}
	if (frp.setpriority) {
		if (frp.priority > 0) {
			args.AddMember(L"priority-high", fileary, allocator);
		}
		else if (frp.priority < 0) {
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
	return 0;
}

int TransAnalyzer::buildDeleteRequest(std::wstring& req, int rid, bool rld)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	idary.PushBack(rid, allocator);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"ids", idary, allocator);
	args.AddMember(L"delete-local-data", rld, allocator);

	//rapidjson::GenericValue<rapidjson::UTF16<>> ags(rapidjson::kObjectType);
	//ags.AddMember(L"arguments", fds, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-remove", allocator);
	rjd.AddMember(L"tag", 5, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

std::wstring TransAnalyzer::analysisRemoteResult(std::wstring& rss)
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

/**
Name:
    PerformRemoteDelete
Description:
    send delete torrent request to Transmission RPC service
Input:
    tid: int, torrent Id for the torrent to be deleted
	bcont: bool, 'true' to delete file content on disk
Return:
    int, 0: success, 1: service error, 2: connection error
*/
int TransAnalyzer::PerformRemoteDelete(int tid, bool bcont)
{
	std::wstring req;
	std::wstring ucstr;
	buildDeleteRequest(req, tid, bcont);
	int rtn = 0;
	int qrt = curlSession->performQuery(req);
	if (qrt == 0) {
		int csl = curlSession->response.length();
		char* cstr = (char*)malloc(csl + 1);
		if (cstr) {
			csl = WideCharToMultiByte(CP_ACP, 0, curlSession->response.c_str(), csl, cstr, csl + 1, NULL, NULL);
			cstr[csl] = 0;
			ConvertUTFCodePure(cstr, ucstr);
			ucstr = analysisRemoteResult(ucstr);
			free(cstr);
			rtn = ucstr.compare(L"success") == 0 ? rtn : 1;
		}
	}
	else {
		rtn = 2;
	}
	return rtn;
}
/**
Name: PerformRemoteStop
Description: Pause or Resume a torrent
Input: tid, int, torrent Id of torrent to be paused/resumed
Input: dopause, bool, 'true' do pause, 'false' do resume
Return: 0: success, 1: service fail, 2: connection fail
*/
int TransAnalyzer::PerformRemoteStop(int tid, bool dopause)
{
	std::wstring req;
	int rtn = 0;
	buildStopRequest(req, tid, dopause);
	int qrt = curlSession->performQuery(req);
	if (qrt == 0) {
		std::wstring ucstr;
		int csl = curlSession->response.length();
		char* cstr = (char*)malloc(csl + 1);
		if (cstr) {
			csl = WideCharToMultiByte(CP_ACP, 0, curlSession->response.c_str(), csl, cstr, csl + 1, NULL, NULL);
			cstr[csl] = 0;
			ConvertUTFCodePure(cstr, ucstr);
			ucstr = analysisRemoteResult(ucstr);
			free(cstr);
			if (ucstr.compare(L"success") != 0) {
				rtn = 1;
			}
		}
	}
	else {
		rtn = 2;
	}
	return rtn;
}

/**
Name: PerformRemoteFileCheck
Description: Files operation on a torrent
Input: tid, int, torrent Id of torrent to be paused/resumed
Input: dopause, bool, 'true' do pause, 'false' do resume
Return: 0: success, 1: service fail, 2: connection fail
*/
int TransAnalyzer::PerformRemoteFileCheck(FileReqParm& frp)
{
	std::wstring req;
	int rtn = 0;
	buildFileRequest(req, frp);
	int qrt = curlSession->performQuery(req);
	if (qrt == 0) {
		std::wstring ucstr;
		int csl = curlSession->response.length();
		char* cstr = (char*)malloc(csl + 1);
		if (cstr) {
			csl = WideCharToMultiByte(CP_ACP, 0, curlSession->response.c_str(), csl, cstr, csl + 1, NULL, NULL);
			cstr[csl] = 0;
			ConvertUTFCodePure(cstr, ucstr);
			ucstr = analysisRemoteResult(ucstr);
			free(cstr);
			if (ucstr.compare(L"success") != 0) {
				rtn = 1;
			}
		}
	}
	else {
		rtn = 2;
	}
	return rtn;
}

std::wstring TransAnalyzer::GetErrorString(int eid)
{
	std::wstring wrt;

	switch (eid) {
	case 0:
		wrt = L"success";
		break;
	case 1:
		wrt = L"fail";
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
TorrentNode* TransAnalyzer::GetTorrentNodeById(unsigned long tid)
{
	TorrentNode* ttt = fulllist->getTorrent(tid);
	return ttt;
}

TorrentGroup* TransAnalyzer::GetTrackerGroup(const std::wstring& tracker)
{
	std::map<std::wstring, TrackerCY*>::iterator ittt;
	TorrentGroup* rtg = NULL;

	std::wstring tgn = getTrackerName(tracker);
	if (tgn.length() > 0) {
		rtg = trackertorrents->getGroup(tgn);
	}
	return rtg;
}

int TransAnalyzer::RemoveTorrent(unsigned long tid)
{
	TorrentNode* rtt = fulllist->getTorrent(tid);
	if (rtt) {
		fulllist->removeTorrent(rtt);
		trackertorrents->removeTorrent(rtt);
		//TorrentNodeHelper::DeleteTorrentNode(rtt);
	}
	return 0;
}

int TransAnalyzer::buildRefreshRequestTorrent(std::wstring& req, ReqParm parm)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv;

	for (std::set<std::wstring>::iterator itsf = refreshtorrentfields.begin()
		; itsf != refreshtorrentfields.end()
		; itsf++) {
		vvv.SetString((*itsf).c_str(), allocator);
		fdary.PushBack(vvv, allocator);
	}
	if (parm.reqpieces) {
		fdary.PushBack(L"pieces", allocator);
	}
	if (parm.reqfiles) {
		fdary.PushBack(L"files", allocator);
	}
	if (parm.reqfilestat) {
		fdary.PushBack(L"fileStats", allocator);
	}
	//fdary.PushBack(L"fileStats", allocator);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"fields", fdary, allocator);
	rapidjson::GenericValue<rapidjson::UTF16<>> uuv; 
	uuv.SetInt(parm.torrentid);
	args.AddMember(L"ids", uuv, allocator);

	//rapidjson::GenericValue<rapidjson::UTF16<>> ags(rapidjson::kObjectType);
	//ags.AddMember(L"arguments", fds, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-get", allocator);
	rjd.AddMember(L"tag", 4, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

int TransAnalyzer::PerformRemoteRefreshTorrent(ReqParm rpm)
{
	curlSession->setSiteInfo(hosturl.c_str(), username.c_str(), password.c_str());
	std::wstring req;
	
	buildRefreshRequestTorrent(req, rpm);
	int qrt = curlSession->performQuery(req);
	if (qrt == 0) {
		std::wstring ucstr;
		int csl = curlSession->response.length();
		char* cstr = (char*)malloc(csl + 1);
		if (cstr) {
			csl = WideCharToMultiByte(CP_ACP, 0, curlSession->response.c_str(), csl, cstr, csl + 1, NULL, NULL);
			cstr[csl] = 0;
			ConvertUTFCodePure(cstr, ucstr);
			free(cstr);
			extractTorrentsGet(ucstr);
			groupTorrentTrackers();
		}
	}
	else {
		qrt = 2;
	}
	return qrt;
}

int TransAnalyzer::buildSessionStat(std::wstring& req)
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

int TransAnalyzer::PerformRemoteSessionStat()
{
	curlSession->setSiteInfo(hosturl.c_str(), username.c_str(), password.c_str());
	std::wstring req;
	buildSessionStat(req);
	int qrt = curlSession->performQuery(req);
	if (qrt == 0) {
		std::wstring ucstr;
		int csl = curlSession->response.length();
		char* cstr = (char*)malloc(csl + 1);
		if (cstr) {
			csl = WideCharToMultiByte(CP_ACP, 0, curlSession->response.c_str(), csl, cstr, csl + 1, NULL, NULL);
			cstr[csl] = 0;
			ConvertUTFCodePure(cstr, ucstr);
			free(cstr);
			extractSessionStat(ucstr);
		}
	}
	else {
		qrt = 2;
	}
	return qrt;
}
/**
Name: PerformRemoteRefresh
Input: None
Return: 0: success, 2: service unavailable 
*/
int TransAnalyzer::PerformRemoteRefresh()
{
	curlSession->setSiteInfo(hosturl.c_str(), username.c_str(), password.c_str());
	std::wstring req;
	buildRefreshRequest(req);
	int qrt = curlSession->performQuery(req);
	if (qrt == 0) {
		std::wstring ucstr;
		int csl = curlSession->response.length();
		char* cstr = (char*)malloc(csl + 1);
		if (cstr) {
			csl = WideCharToMultiByte(CP_ACP, 0, curlSession->response.c_str(), csl, cstr, csl + 1, NULL, NULL);
			cstr[csl] = 0;
			ConvertUTFCodePure(cstr, ucstr);
			free(cstr);
			extractTorrentsGet(ucstr);
			groupTorrentTrackers();
		}
	}
	else {
		qrt = 2;
	}
	return qrt;
}

int TransAnalyzer::StartClient(const wchar_t* url, const wchar_t* user, const wchar_t* passwd)
{
	hosturl = url;
	username = user;
	password = passwd;
	curlSession->setSiteInfo(hosturl.c_str(), username.c_str(), password.c_str());
	std::wstring req;
	buildGetAllRequest(req);
	int qrt = curlSession->performQuery(req);
	std::wstring ucstr;
	int csl = curlSession->response.length();
	char* cstr = (char*)malloc(csl + 1);
	if (cstr) {
		csl = WideCharToMultiByte(CP_ACP, 0, curlSession->response.c_str(), csl, cstr, csl + 1, NULL, NULL);
		cstr[csl] = 0;
		ConvertUTFCodePure(cstr, ucstr);
		free(cstr);
		extractTorrentsGet(ucstr);
		groupTorrentTrackers();
	}
	return 0;
}

int TransAnalyzer::extractSessionStat(std::wstring& jsonstr)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd;

	rapidjson::GenericStringStream<rapidjson::UTF16<>> gssu16((wchar_t*)jsonstr.c_str());
	rjd.ParseStream(gssu16);
	if (!rjd.HasParseError()) {
		if (rjd.HasMember(L"arguments")) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& args = rjd[L"arguments"];
			if (args.HasMember(L"activeTorrentCount")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trns = args[L"activeTorrentCount"];
				if (trns.IsInt()) {
					session.torrentcount = trns.GetInt();
				}
			}
			if (args.HasMember(L"downloadSpeed")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trns = args[L"downloadSpeed"];
				if (trns.IsInt()) {
					session.downloadspeed = trns.GetInt();
				}
			}
			if (args.HasMember(L"uploadSpeed")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trns = args[L"uploadSpeed"];
				if (trns.IsInt()) {
					session.uploadspeed = trns.GetInt();
				}
			}
			if (args.HasMember(L"cumulative-stats")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& rmvs = args[L"cumulative-stats"];
				if (rmvs.IsObject()) {
					if (rmvs.HasMember(L"downloadedBytes") && rmvs[L"downloadedBytes"].IsInt64()) {
						session.downloaded = rmvs[L"downloadedBytes"].GetInt64();
					}
					if (rmvs.HasMember(L"uploadedBytes") && rmvs[L"uploadedBytes"].IsInt64()) {
						session.uploaded = rmvs[L"uploadedBytes"].GetInt64();
					}
				}
			}
		}
		fulllist->updateSize();
	}
	return 0;
}

int TransAnalyzer::extractTorrentsGet(std::wstring& jsonstr)
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
							extractTorrentObject(trtn);
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
							removed.push_back(trtn.GetInt());
						}
					}
				}
			}
		}
		fulllist->updateSize();
	}
	return 0;
}

int TransAnalyzer::buildRefreshRequest(std::wstring& req)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);

	for (std::set<std::wstring>::iterator itsf = refreshtorrentfields.begin()
		; itsf != refreshtorrentfields.end()
		; itsf++) {
		vvv.SetString((*itsf).c_str(), allocator);
		fdary.PushBack(vvv, allocator);
	}

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"fields", fdary, allocator);
	args.AddMember(L"ids", L"recently-active", allocator);

	//rapidjson::GenericValue<rapidjson::UTF16<>> ags(rapidjson::kObjectType);
	//ags.AddMember(L"arguments", fds, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-get", allocator);
	rjd.AddMember(L"tag", 4, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

int TransAnalyzer::buildAddTorrentRequest(const std::wstring& turl, bool ismedia, std::wstring& req)
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


int TransAnalyzer::buildGetAllRequest(std::wstring& req)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);

	for (std::set<std::wstring>::iterator itsf = refreshtorrentfields.begin()
		; itsf != refreshtorrentfields.end()
		; itsf++) {
		vvv.SetString((*itsf).c_str(), allocator);
		fdary.PushBack(vvv, allocator);
	}
	//fdary.PushBack(L"files", allocator);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"fields", fdary, allocator);

	//rapidjson::GenericValue<rapidjson::UTF16<>> ags(rapidjson::kObjectType);
	//ags.AddMember(L"arguments", fds, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-get", allocator);
	rjd.AddMember(L"tag", 2, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
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

int TransAnalyzer::convertPiecesData(TorrentNode* node)
{
	if (node->pieces.size() > 0) {
		if (node->piecesdata) {
			size_t usz = _msize(node->piecesdata);
			if (usz > 0) {
				free(node->piecesdata);
				node->piecesdata = NULL;
			}
		}
		DWORD dsz;
		DWORD dfg;
		int sbt = 0;
		unsigned char cbt;

		CryptStringToBinary(node->pieces.c_str(), node->pieces.length(), CRYPT_STRING_BASE64, NULL, &dsz, NULL, &dfg);
		if (dsz > 0) {
			unsigned char* dff = (unsigned char*)malloc(dsz);
			if (dff) {
				BOOL btn = CryptStringToBinary(node->pieces.c_str(), node->pieces.length(), CRYPT_STRING_BASE64, dff, &dsz, NULL, &dfg);
				if (dsz * 8 >= node->piececount) {
					node->piecesdata = (unsigned char*)malloc(dsz * 8);
					for (DWORD ii = 0; ii < dsz; ii++) {
						cbt = dff[ii];
						for (int jj = 0; jj < 8; jj++) {
							node->piecesdata[sbt] = cbt & 0x80 ? 1 : 0;
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

void TransAnalyzer::extractTorrentObject(rapidjson::GenericValue<rapidjson::UTF16<>>& trtn)
{
	unsigned long iid;
	TorrentNode* torrent;

	if (trtn.HasMember(L"id") && trtn[L"id"].IsInt()) {
		iid = trtn[L"id"].GetUint();
		if (fulllist->torrents_.find(iid) != fulllist->torrents_.end()) {
			torrent = fulllist->torrents_[iid];
		}
		else {
			torrent = new TorrentNode();
			torrent->id = iid;
			fulllist->addTorrents(torrent);
		}

		JSTRYSTRING(torrent->name, L"name", trtn);
		JSTRYINT64(torrent->size, L"totalSize", trtn);
		JSTRYINT64(torrent->uploaded, L"uploadedEver", trtn);
		JSTRYINT64(torrent->downloaded, L"downloadedEver", trtn);
		JSTRYINT64(torrent->corrupt, L"corruptEver", trtn);
		JSTRYINT(torrent->downspeed, L"rateDownload", trtn);
		JSTRYINT(torrent->upspeed, L"rateUpload", trtn);
		JSTRYINT(torrent->status, L"status", trtn);
		JSTRYSTRING(torrent->path, L"downloadDir", trtn);
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
			torrent->trackerscount = 0;
			rapidjson::GenericValue<rapidjson::UTF16<>>& trks = trtn[L"trackers"];
			for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itk = trks.Begin()
				; itk != trks.End()
				; itk++) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trka = *itk;
				extractTracker(trka, torrent);
			}
		}
		ULARGE_INTEGER ltt;
		if (trtn.HasMember(L"files") && trtn[L"files"].IsArray()) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& trfs = trtn[L"files"];
			if (torrent->files.count == 0) {
				torrent->files.count = trfs.GetArray().Size();
				//int mls = sizeof(struct TorrentNodeFile) * torrent->files.count;
				torrent->files.file = new struct TorrentNodeFile[torrent->files.count];
				if (torrent->files.file) {
					//memset(torrent->files.file, 0, mls);

					int fix = 0;
					for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itf = trfs.Begin()
						; itf != trfs.End()
						; itf++) {
						rapidjson::GenericValue<rapidjson::UTF16<>>& trfl = *itf;
						//extractFile(trfl, &torrent->files.file[fix]);
						JSTRYSTRING(torrent->files.file[fix].name, L"name", trfl);
						JSTRYINT64(torrent->files.file[fix].size, L"length", trfl);
						JSTRYINT64(torrent->files.file[fix].donesize, L"bytesCompleted", trfl);

						fix++;
					}
				}
			}
		}
		if (trtn.HasMember(L"fileStats") && trtn[L"fileStats"].IsArray()) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& trfs = trtn[L"fileStats"];
			int uix = 0;
			for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itf = trfs.Begin()
				; itf != trfs.End()
				; itf++) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trfl = *itf;
				//extractFile(trfl, &torrent->files.file[fix]);
				JSTRYINT64(torrent->files.file[uix].donesize, L"bytesCompleted", trfl);
				JSTRYBOOL(torrent->files.file[uix].check, L"wanted", trfl);
				JSTRYINT(torrent->files.file[uix].priority, L"priority", trfl);

				uix++;
			}

		}
		if (torrent->files.count > 0) {
			if (torrent->files.node == NULL) {
				std::wstring fpp, fnn;
				size_t nps;
				torrent->files.node = new TorrentNodeFileNode();
				torrent->files.node->name = torrent->path;
				torrent->files.node->type = TorrentNodeFileNode::NODETYPE::DIR;
				torrent->files.node->node = torrent;
				torrent->files.node->priority = 0;
				torrent->files.node->check = true;
				TorrentNodeFileNode* npn;
				TorrentNodeFileNode* nfn;
				for (int ii = 0; ii < torrent->files.count; ii++) {
					nps = torrent->files.file[ii].name.find_last_of(L'/');
					if (nps == std::wstring::npos) {
						npn = torrent->files.node;
						fnn = torrent->files.file[ii].name;
					}
					else {
						fpp = torrent->files.file[ii].name.substr(0, nps);
						fnn = torrent->files.file[ii].name.substr(nps + 1);
						npn = TorrentNodeHelper::GetPathNode(torrent->files.node, fpp);
					}
					if (npn) {
						nfn = new TorrentNodeFileNode();
						*nfn = { 0 };
						nfn->id = ii;
						nfn->name = fnn;
						nfn->parent = npn;
						npn->sub.insert(nfn);
						nfn->type = TorrentNodeFileNode::NODETYPE::FILE;
						nfn->path = TorrentNodeHelper::GetFileNodePath(nfn);
						nfn->file = &torrent->files.file[ii];
						ltt.QuadPart = GetTickCount64();
						nfn->updatetick = ltt.LowPart;
						nfn->node = torrent;
					}
				}
			}
			else {
				std::set<TorrentNodeFileNode*> nfs;
				TorrentNodeHelper::GetNodeFileSet(torrent->files.node, nfs);
				ltt.QuadPart = GetTickCount64();

				for (std::set<TorrentNodeFileNode*>::iterator itfs = nfs.begin()
					; itfs != nfs.end()
					; itfs++) {
					TorrentNodeFileNode* nfn = *itfs;
					if (nfn->check != nfn->file->check) {
						nfn->check = nfn->file->check;
						nfn->updatetick = ltt.LowPart;
					}
					if (nfn->priority != nfn->file->priority) {
						nfn->priority = nfn->file->priority;
						nfn->updatetick = ltt.LowPart;
					}
					if (nfn->done != nfn->file->donesize) {
						nfn->done = nfn->file->donesize;
						nfn->updatetick = ltt.LowPart;
					}
				}
			}
		}
		convertPiecesData(torrent);

		LARGE_INTEGER ttk;
		ttk.QuadPart = GetTickCount64();
		torrent->updatetick = ttk.LowPart;
	}
}

int TransAnalyzer::extractFile(rapidjson::GenericValue<rapidjson::UTF16<>>& trfa, TorrentNodeFile* file)
{
	std::wstring tnm;
	JSTRYSTRING(tnm, L"name", trfa);
	if (tnm.length() > 0) {
		int nbs = (tnm.length() + 1) * sizeof(wchar_t);
		file->name = (wchar_t*)malloc(nbs);
		//wcscpy_s(file->name, tnm.length(), tnm.c_str());
	}
	JSTRYINT64(file->size, L"length", trfa);
	JSTRYINT64(file->donesize, L"bytesCompleted", trfa);
	return 0;
}

void TransAnalyzer::extractTracker(rapidjson::GenericValue<rapidjson::UTF16<>>& trka, TorrentNode* torrent)
{
	if (trka.HasMember(L"announce") && trka[L"announce"].IsString()) {
		std::wstring anc = trka[L"announce"].GetString();
		
		if (trackers.find(anc) == trackers.end()) {
			TrackerCY* ntk = new TrackerCY();
			
			ntk->announce = anc;
			trackers[anc] = ntk;
			ntk->id = trackers.size();
		}
		torrent->trackers[torrent->trackerscount] = anc;
		torrent->trackerscount++;
	}
}

int TransAnalyzer::groupTorrentTrackers()
{
	TorrentGroup* ntg;
	std::wstring gtname;
	for (std::map<unsigned long, TorrentNode*>::iterator ittn = fulllist->torrents_.begin()
		; ittn != fulllist->torrents_.end()
		; ittn++) {
		for (unsigned int ii = 0; ii < ittn->second->trackerscount; ii++) {
			std::wstring& tck = ittn->second->trackers[ii];
			gtname = getTrackerName(tck);
			if (trackertorrents->subs.find(gtname) == trackertorrents->subs.end()) {
				ntg = new TorrentGroup();
				ntg->name = gtname;
				trackertorrents->addGroup(ntg);
			}
			ntg = trackertorrents->subs[gtname];
			ntg->addTorrents(ittn->second);
		}
	}
	trackertorrents->updateSize();
	return 0;
}

std::wstring TransAnalyzer::getTrackerName(const std::wstring& tck)
{
	size_t sps;
	size_t cps;
	std::wstring ttn(tck);

	sps = tck.find(L"://", 0);
	if (sps != std::wstring::npos) {
		sps += 3;
		cps = tck.find(L'/', sps);
		cps = cps == std::wstring::npos?cps = tck.length():cps;
		ttn = tck.substr(sps, cps - sps);
	}

	return ttn;
}

std::wstring TransAnalyzer::PerformRemoteAddTorrent(const std::wstring turl)
{
	std::wstring rms;
	std::wstring wrn;
	bool ism = false;

	HANDLE htf = CreateFile(
		turl.c_str()
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
			wrn = turl;
		}
		CloseHandle(htf);
	}
	else {
		wrn = turl;
	}

	if (wrn.length() > 0) {
		std::wstring req;
		buildAddTorrentRequest(wrn, ism, req);

		int qrt = curlSession->performQuery(req);
		int csl = curlSession->response.length();
		char* cstr = (char*)malloc(csl + 1);
		if (cstr) {
			csl = WideCharToMultiByte(CP_ACP, 0, curlSession->response.c_str(), csl, cstr, csl + 1, NULL, NULL);
			cstr[csl] = 0;
			ConvertUTFCodePure(cstr, rms);
			rms = analysisRemoteResult(rms);
			free(cstr);
		}
	}

	return rms;
}

int TransAnalyzer::ItorateGroupTorrents(TorrentGroup* grp, int(*func)(bool, void*, TorrentGroup*, TorrentNode*), void* parm)
{
	for (std::map<unsigned long, TorrentNode*>::iterator ittn = grp->torrents_.begin()
		; ittn != grp->torrents_.end()
		; ittn++) {
		(*func)(false, parm, grp, ittn->second);
	}
	(*func)(true, parm, grp, NULL);
	return 0;
}

int TransAnalyzer::AsyncGetGroupAllNodes(TorrentGroup* grp, int(*func)(bool, void*, TorrentGroup*, TorrentNode*), void* parm, int level = 0)
{
	for (std::map<unsigned long, TorrentNode*>::iterator ittn = grp->torrents_.begin()
		; ittn != grp->torrents_.end()
		; ittn++) {
		(*func)(false, parm, grp, ittn->second);
	}

	for (std::map<std::wstring, TorrentGroup*>::iterator itsb = grp->subs.begin()
		; itsb != grp->subs.end()
		; itsb++) {
		AsyncGetGroupAllNodes(itsb->second, func, parm, level + 1);
	}

	if (level == 0) {
		(*func)(true, parm, grp, NULL);
	}
	return 0;
}

TorrentGroup::TorrentGroup()
	: size(0)
	, parent(NULL)
	, updatetick(0)
	, readtick(0)
	, downspeed(0)
	, upspeed(0)
{
}

TorrentGroup::~TorrentGroup()
{
	/*
	for (std::map<unsigned long, TorrentNode*>::iterator ittn = torrents.begin()
		; ittn != torrents.end()
		; ittn++) {
		delete ittn->second;
	}
	torrents.clear();
	*/
	for (std::map<std::wstring, TorrentGroup*>::iterator ittg = subs.begin()
		; ittg != subs.end()
		; ittg++) {
		delete ittg->second;
	}
	subs.clear();
}

int TorrentGroup::addTorrents(TorrentNode* trt)
{
	if (torrents_.find(trt->id) == torrents_.end()) {
		torrents_[trt->id] = trt;
		size += trt->size;
	}
	return 0;
}

int TorrentGroup::addGroup(TorrentGroup* grp)
{
	subs[grp->name] = grp;
	size += grp->size;
	grp->parent = this;
	return 0;
}

int TorrentGroup::updateSize()
{
	unsigned long long osz = size;
	long ous = upspeed;
	long ods = downspeed;

	size = 0;
	upspeed = 0;
	downspeed = 0;
	for (std::map<unsigned long, TorrentNode*>::iterator itts = torrents_.begin()
		; itts != torrents_.end()
		; itts++) {
		size += itts->second->size;
		upspeed += itts->second->upspeed;
		downspeed += itts->second->downspeed;
	}
	for (std::map<std::wstring, TorrentGroup*>::iterator ittg = subs.begin()
		; ittg != subs.end()
		; ittg++) {
		ittg->second->updateSize();
		size += ittg->second->size;
		upspeed += ittg->second->upspeed;
		downspeed += ittg->second->downspeed;
	}

	if ((osz != size) || (ous != upspeed) || (ods != downspeed)) {
		LARGE_INTEGER ttk;
		ttk.QuadPart = GetTickCount64();
		updatetick = ttk.LowPart;
	}
	return 0;
}

int TorrentGroup::GetNodes(std::set<TorrentNode*>& nds)
{
	for (std::map<unsigned long, TorrentNode*>::iterator ittn = torrents_.begin()
		; ittn != torrents_.end()
		; ittn++) {
		nds.insert(ittn->second);
	}
	for (std::map<std::wstring, TorrentGroup*>::iterator ittg = subs.begin()
		; ittg != subs.end()
		; ittg++) {
		ittg->second->GetNodes(nds);
	}
	return 0;
}

int TorrentGroup::GetTorrentsCount() const
{
	return torrents_.size();
}

TorrentNode* TorrentGroup::getTorrent(unsigned long tid)
{
	TorrentNode* fnd = NULL;
	std::map<unsigned long, TorrentNode*>::iterator ittt = torrents_.begin();
	BOOL keepseek = ittt != torrents_.end();

	while (keepseek) {
		if (ittt->second->id == tid) {
			fnd = ittt->second;
			keepseek = FALSE;
		}
		else {
			ittt++;
			keepseek = ittt != torrents_.end() ? keepseek : FALSE;
		}
	}

	if (fnd == NULL) {
		std::map<std::wstring, TorrentGroup*>::iterator itsb = subs.begin();
		keepseek = itsb != subs.end();
		while (keepseek) {
			fnd = itsb->second->getTorrent(tid);
			if (fnd == NULL) {
				itsb++;
				keepseek = itsb == subs.end() ? FALSE : keepseek;
			}
			else {
				keepseek = FALSE;
			}
		}
	}
	return fnd;
}

TorrentGroup* TorrentGroup::getGroup(const std::wstring& gname)
{
	std::map<std::wstring, TorrentGroup*>::iterator itsb;
	TorrentGroup* grp = NULL;
	bool keepseek;

	itsb = subs.find(gname);
	if (itsb != subs.end()) {
		grp = itsb->second;
	}
	else {
		itsb = subs.begin();
		keepseek = itsb != subs.end();
		while (keepseek) {
			grp = itsb->second->getGroup(gname);
			keepseek = grp ? FALSE : keepseek;
			itsb++;
			keepseek = itsb == subs.end() ? FALSE : keepseek;
		}
	}
	return grp;
}

int TorrentGroup::removeTorrent(TorrentNode* trt)
{
	std::map<unsigned long, TorrentNode*>::iterator ittt = torrents_.begin();
	BOOL keepseek = ittt != torrents_.end();

	while (keepseek) {
		if (ittt->second == trt) {
			ittt->second->valid = false;
			//keepseek = FALSE;
		}

		ittt++;
		keepseek = ittt != torrents_.end() ? keepseek : FALSE;
	}

	for (std::map<std::wstring, TorrentGroup*>::iterator itsb = subs.begin()
		; itsb != subs.end()
		; itsb++) {
		itsb->second->removeTorrent(trt);
	}
	return 0;
}

std::wstring TorrentNodeHelper::DumpTorrentNode(TorrentNode* node)
{
	std::wstringstream wss;
	wss << L"ID: " << node->id << L'\r' << std::endl;
	wss << L"Name: " << node->name << L'\r' << std::endl;
	wss << L"Size: " << node->size << L'\r' << std::endl;
	wss << L"Pieces: " << node->piececount << L'\r' << std::endl;
	wss << L"Error: " << node->error << L'\r' << std::endl;
	wss << L"Status: " << node->status << L'\r' << std::endl;
	wss << L"DownloadTime: " << node->downloadTime << L'\r' << std::endl;
	wss << L"SeedTime: " << node->seedTime << L'\r' << std::endl;
	wss << L"Ratio: " << node->ratio << L'\r' << std::endl;
	wss << L"PieceSize: " << node->piecesdatasize << L'\r' << std::endl;
	wss << L"AddedDate: " << node->adddate << L'\r' << std::endl;
	for (unsigned int ii = 0; ii < node->trackerscount; ii++) {
		wss << L"Tracker: " << node->trackers[ii] << L'\r' << std::endl;
	}
	return wss.str();
}

int TorrentNodeHelper::ClearTorrentNodeFile(struct TorrentNodeFile& file)
{
	return 0;
}

int TorrentNodeHelper::DeleteTorrentNode(TorrentNode* node)
{
	if (node->piecesdatasize > 0) {
		free(node->piecesdata);
		node->piecesdatasize = 0;
	}
	if (node->files.count > 0) {
		for (int ii = 0; ii < node->files.count; ii++) {
			TorrentNodeHelper::ClearTorrentNodeFile(node->files.file[ii]);
		}
		delete[] node->files.file;
		node->files.count = 0;
	}
	delete node;
	return 0;
}

TorrentNodeFileNode* TorrentNodeHelper::GetPathNode(TorrentNodeFileNode* node, std::wstring path)
{
	TorrentNodeFileNode* rtn = NULL;
	TorrentNodeFileNode* ptn = NULL;
	
	if (path.length() > 0) {
		size_t nps = path.find_last_of(L'/');
		std::wstring pps;
		if (nps != std::wstring::npos) {
			pps = path.substr(0, nps);
			path = path.substr(nps + 1);
		}
		ptn = GetPathNode(node, pps);

		if (ptn) {
			for (std::set<TorrentNodeFileNode*>::iterator itsp = ptn->sub.begin()
				; itsp != ptn->sub.end()
				; itsp++) {
				if ((*itsp)->name.compare(path) == 0) {
					rtn = *itsp;
				}
			}
			if (rtn == NULL) {
				rtn = new TorrentNodeFileNode();
				rtn->parent = ptn;
				ptn->sub.insert(rtn);
				rtn->type = TorrentNodeFileNode::NODETYPE::DIR;
				rtn->name = path;
				rtn->node = node->node;
			}
		}
	}
	else {
		rtn = node;
	}
	return rtn;
}

int TorrentNodeHelper::GetNodeFileSet(TorrentNodeFileNode* file, std::set<TorrentNodeFileNode*>& fileset)
{
	if (file->type == TorrentNodeFileNode::NODETYPE::FILE) {
		fileset.insert(file);
	}
	for (std::set<TorrentNodeFileNode*>::iterator itfn = file->sub.begin()
		; itfn != file->sub.end()
		; itfn++) {
		GetNodeFileSet(*itfn, fileset);
	}

	return 0;
}

std::wstring TorrentNodeHelper::GetFileNodePath(TorrentNodeFileNode* file)
{
	std::wstringstream wss;
	if (file->parent) {
		wss << GetFileNodePath(file->parent);
	}
	if (file->type == TorrentNodeFileNode::NODETYPE::DIR) {
		if (wss.str().length() > 0) {
			wss << L'/';
		}
		wss << file->name;
	}
	return wss.str();
}
