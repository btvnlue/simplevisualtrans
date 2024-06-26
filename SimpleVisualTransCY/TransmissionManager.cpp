#include "TransmissionManager.h"
#include "TransmissionProfile.h"
#include "CurlSessionCY.h"
#include "Utilities.h"

#include <Windows.h>
#include <sstream>
#include <set>
#include <fstream>

#ifndef RAPIDJSON_DEFAULT_ALLOCATOR
#define RAPIDJSON_DEFAULT_ALLOCATOR BufferedJsonAllocator
#endif

//#ifndef RAPIDJSON_NEW
/////! customization point for global \c new
//#define RAPIDJSON_NEW(TypeName) TypeName::GetInstance();
//#endif
//#ifndef RAPIDJSON_DELETE
/////! customization point for global \c delete
//#define RAPIDJSON_DELETE(x) x->Free();
//#endif


#include <rapidjson/document.h>
#include <rapidjson/encodings.h>
#include <rapidjson/writer.h>
#include <rapidjson/istreamwrapper.h>

std::set<std::wstring> listfields;
std::set<std::wstring> detailfields;

int ConvertUTFCodePure(const char* instr, std::wstring& wstr)
{
	size_t insz = strlen(instr);
	std::wstringstream owstr;

	for (unsigned int ii = 0; ii < insz; ii++) {
		owstr << (wchar_t)instr[ii];
	}
	wstr = owstr.str();
	owstr.clear();
	return 0;
}

TransmissionManager::TransmissionManager()
{
	if (listfields.size() == 0) {
		listfields.insert(L"id");
		listfields.insert(L"name");
		listfields.insert(L"totalSize");
		listfields.insert(L"leftUntilDone");
		listfields.insert(L"trackers");
		listfields.insert(L"status");
		listfields.insert(L"downloadDir");
		listfields.insert(L"rateDownload");
		listfields.insert(L"errorString");
		listfields.insert(L"rateUpload");
		listfields.insert(L"uploadRatio");
		listfields.insert(L"recheckProgress");
	}
	if (detailfields.size() == 0) {
		detailfields.insert(listfields.begin(), listfields.end());
		detailfields.insert(L"uploadedEver");
		detailfields.insert(L"downloadedEver");
		detailfields.insert(L"secondsDownloading");
		detailfields.insert(L"secondsSeeding");
		detailfields.insert(L"isPrivate");
		detailfields.insert(L"isFinished");
		detailfields.insert(L"isStalled");
		detailfields.insert(L"doneDate");
		detailfields.insert(L"activityDate");
		detailfields.insert(L"pieceCount");
		detailfields.insert(L"pieceSize");
		detailfields.insert(L"addedDate");
		detailfields.insert(L"startDate");
		detailfields.insert(L"desiredAvailable");
		detailfields.insert(L"pieces");
		detailfields.insert(L"magnetLink");
		detailfields.insert(L"files");
		detailfields.insert(L"fileStats");
		detailfields.insert(L"peers");
		detailfields.insert(L"peersFrom");
		detailfields.insert(L"downloadLimit");
		detailfields.insert(L"downloadLimited");
		detailfields.insert(L"uploadLimit");
		detailfields.insert(L"uploadLimited");
	}
}

TransmissionManager::~TransmissionManager()
{
	Stop();
	CurlSessionCY::CleanUp();
	listfields.clear();
	detailfields.clear();
	BufferedJsonAllocator::GetInstance()->FreeAll();
}

DWORD WINAPI TransmissionManager::ThManagerProc(LPVOID lpParam)
{
	TransmissionManager* self = reinterpret_cast<TransmissionManager*>(lpParam);
	ManagerCommand* cmd;

	while (self->keeprun) {
		cmd = self->GetCommand();
		if (cmd == NULL) {
			::Sleep(100);
		}
		else {
			cmd->Process(self);
			delete cmd;
		}
	}

	return 0;
}

int TransmissionManager::Start()
{
	if (hEvent == NULL) {
		hEvent = ::CreateEvent(NULL, FALSE, TRUE, NULL);
	}
	keeprun = TRUE;
	hTh = ::CreateThread(NULL, 0, ThManagerProc, this, 0, &tid);
	return 0;
}

int TransmissionManager::Stop()
{
	DWORD dcd = 0;
	bool keepwait = true;
	int waitidx = 0;

	if (hTh) {
		keeprun = FALSE;

		while (keepwait) {
			::Sleep(100);
			::GetExitCodeThread(hTh, &dcd);
			keepwait = dcd == STILL_ACTIVE ? keepwait : false;
			keepwait = waitidx++ < 3 ? keepwait : false;
		}
		if (dcd == STILL_ACTIVE) {
			TerminateThread(hTh, 0x999);
		}
		hTh = nullptr;
	}

	//if (worktorrents.count > 0) {
	//	delete[] worktorrents.items;
	//	worktorrents.items = NULL;
	//	worktorrents.count = 0;
	//}

	if (hEvent) {
		CloseHandle(hEvent);
		hEvent = NULL;
	}
	return 0;
}

int TransmissionManager::PutCommand(ManagerCommand * cmd)
{
	if (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {
		cmds.push_back(cmd);
		::SetEvent(hEvent);
	}
	return 0;
}

ManagerCommand * TransmissionManager::GetCommand()
{
	ManagerCommand* cmd = NULL;
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

int CmdLoadProfile::Process(TransmissionManager* mgr) {
	if (profile) {
		if (profile->name == L"default") {
			profile->username = L"transmission";
			profile->passwd = L"transmission";
			profile->url = L"http://192.168.5.159:9091/transmission/rpc";

			profile->state = DONE_FILL;
		}
	}
	if (callback) {
		callback->Process(mgr, this);
	}

	return 0;
}

//int TransmissionManager::LoadDefaultProfile(TransmissionProfile* prof)
//{
//	CmdLoadProfile* cmd = new CmdLoadProfile();
//	cmd->profile = prof;
//	PutCommand(cmd);
//	return 0;
//}

int buildRefreshRequest(_Out_ std::wstring& out)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);

	for (std::set<std::wstring>::iterator itfs = listfields.begin();
		itfs != listfields.end();
		itfs++) {
		vvv.SetString((*itfs).c_str(), allocator);
		fdary.PushBack(vvv, allocator);
	}

	args.AddMember(L"fields", fdary, allocator);

	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-get", allocator);
	rjd.AddMember(L"tag", 4, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	out = sbuf.GetString();
	return 0;
}

int convertPiecesData(TorrentNode* node)
{
	if (node->pieces.length() > 0) {
		DWORD dsz;
		DWORD dfg;
		unsigned char cbt;
		int sbt = 0;
		CryptStringToBinary(node->pieces.c_str(), node->pieces.length(), CRYPT_STRING_BASE64, NULL, &dsz, NULL, &dfg);
		if (dsz > 0) {
			//unsigned char* dff = (unsigned char*)malloc(dsz);
			unsigned char* dff = (unsigned char*)BufferedJsonAllocator::GetInstance()->Malloc(dsz);
			if (dff) {
				BOOL btn = CryptStringToBinary(node->pieces.c_str(), node->pieces.length(), CRYPT_STRING_BASE64, dff, &dsz, NULL, &dfg);
				if (node->piecesdata.count == 0) {
					//node->piecesdata.items = new unsigned char[dsz * 8];
					node->piecesdata.items = (unsigned char*)BufferedJsonAllocator::GetInstance()->Malloc(dsz * 8);
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

							//////////////////////////////
							Sleep(0);
						}
					}
				}
				//free(dff);
			}
		}
	}
	return 0;
}


#define JSTRYSTRING(TOVAL, VALNAME, JSVAL) \
	if (JSVAL.HasMember(VALNAME) && JSVAL[VALNAME].IsString()) { \
		TOVAL = JSVAL[VALNAME].GetString(); \
	}
#define JSTRYSTRING_CY(TOVAL, SSZ, VALNAME, JSVAL) \
	if (JSVAL.HasMember(VALNAME) && JSVAL[VALNAME].IsString()) { \
		wcscpy_s(TOVAL, SSZ, JSVAL[VALNAME].GetString()); \
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

int extractTorrentObject(rapidjson::GenericValue<rapidjson::UTF16<>>& ttstr, TorrentNode* torrent)
{
	unsigned long iid;

	if (ttstr.HasMember(L"id") && ttstr[L"id"].IsInt()) {
		iid = ttstr[L"id"].GetUint();
		torrent->id = iid;
		JSTRYSTRING(torrent->name, L"name", ttstr);
		JSTRYINT64(torrent->size, L"totalSize", ttstr);
		JSTRYINT64(torrent->uploaded, L"uploadedEver", ttstr);
		JSTRYINT64(torrent->downloaded, L"downloadedEver", ttstr);
		JSTRYINT64(torrent->corrupt, L"corruptEver", ttstr);
		JSTRYINT(torrent->downspeed, L"rateDownload", ttstr);
		JSTRYINT(torrent->upspeed, L"rateUpload", ttstr);
		JSTRYINT(torrent->status, L"status", ttstr);
		//JSTRYSTRING(torrent->path.fullpath, L"downloadDir", ttstr);
		JSTRYSTRING_CY(torrent->_path, 256, L"downloadDir", ttstr);
		JSTRYINT(torrent->downloadTime, L"secondsDownloading", ttstr);
		JSTRYINT(torrent->seedTime, L"secondsSeeding", ttstr);
		JSTRYSTRING_CY(torrent->_error, 1024, L"errorString", ttstr);
		JSTRYDOUBLE(torrent->ratio, L"uploadRatio", ttstr);
		JSTRYINT64(torrent->leftsize, L"leftUntilDone", ttstr);
		JSTRYBOOL(torrent->privacy, L"isPrivate", ttstr);
		JSTRYBOOL(torrent->done, L"isFinished", ttstr);
		JSTRYBOOL(torrent->stalled, L"isStalled", ttstr);
		JSTRYINT(torrent->donedate, L"doneDate", ttstr);
		JSTRYINT(torrent->activedate, L"activityDate", ttstr);
		JSTRYINT64(torrent->desired, L"desiredAvailable", ttstr);
		JSTRYINT(torrent->adddate, L"addedDate", ttstr);
		JSTRYINT(torrent->startdate, L"startDate", ttstr);
		JSTRYINT(torrent->piececount, L"pieceCount", ttstr);
		JSTRYINT(torrent->piecesize, L"pieceSize", ttstr);
		JSTRYSTRING(torrent->pieces, L"pieces", ttstr);
		JSTRYDOUBLE(torrent->recheck, L"recheckProgress", ttstr);
		JSTRYSTRING(torrent->magnet, L"magnetLink", ttstr);

		//if (ttstr.HasMember(L"trackers") && ttstr[L"trackers"].IsArray()) {
		//	rapidjson::GenericValue<rapidjson::UTF16<>>& trks = ttstr[L"trackers"];
		//	torrent->CleanTrackers();
		//	torrent->rawtrackers.count = trks.Size();
		//	if (torrent->rawtrackers.count > 0) {
		//		torrent->rawtrackers.items = new TrackerCY[torrent->rawtrackers.count];
		//		//torrent->rawtrackers.items = (TrackerCY*)BufferedJsonAllocator::GetInstance()->Malloc(sizeof(TrackerCY) * torrent->rawtrackers.count);
		//		int ii = 0;
		//		for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itk = trks.Begin()
		//			; itk != trks.End()
		//			; itk++) {
		//			rapidjson::GenericValue<rapidjson::UTF16<>>& trka = *itk;

		//			if (trka.HasMember(L"announce") && trka[L"announce"].IsString()) {
		//				torrent->rawtrackers.items[ii].url = trka[L"announce"].GetString();
		//			}
		//			ii++;
		//		}
		//	}
		//}
		//if (trtn.HasMember(L"files") && trtn[L"files"].IsArray()) {
		//	rapidjson::GenericValue<rapidjson::UTF16<>>& trfs = trtn[L"files"];
		//	if (torrent->files.count == 0) {
		//		torrent->files.count = trfs.GetArray().Size();
		//		//int mls = sizeof(struct TorrentNodeFile) * torrent->files.count;
		//		torrent->files.items = new struct TorrentNodeFile[torrent->files.count];
		//		if (torrent->files.items) {
		//			//memset(torrent->files.file, 0, mls);
		//			int fix = 0;
		//			for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itf = trfs.Begin()
		//				; itf != trfs.End()
		//				; itf++) {
		//				rapidjson::GenericValue<rapidjson::UTF16<>>& trfl = *itf;
		//				torrent->files.items[fix].id = fix;
		//				JSTRYSTRING(wname, L"name", trfl);
		//				wpos = wname.find_last_of(pathsepchar);
		//				if (wpos == std::wstring::npos) {
		//					torrent->files.items[fix].name = wname;
		//					torrent->files.items[fix].path = L"";
		//				}
		//				else {
		//					torrent->files.items[fix].name = wname.substr(wpos + 1);
		//					torrent->files.items[fix].path = wname.substr(0, wpos);
		//				}
		//				torrent->files.items[fix].fullpath = wname;
		//				JSTRYINT64(torrent->files.items[fix].size, L"length", trfl);
		//				JSTRYINT64(torrent->files.items[fix].done, L"bytesCompleted", trfl);
		//				torrent->files.items[fix].check = true;
		//				torrent->files.items[fix].priority = 0;
		//				torrent->files.items[fix].updatetick = GetTickCount64();
		//				torrent->files.items[fix].type = TNF_TYPE::file;

		//				fix++;
		//			}
		//		}
		//	}
		//	arrangeTorrentFiles(torrent);
		//}
		//if (trtn.HasMember(L"fileStats") && trtn[L"fileStats"].IsArray()) {
		//	rapidjson::GenericValue<rapidjson::UTF16<>>& trfs = trtn[L"fileStats"];
		//	unsigned int uix = 0;
		//	for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itf = trfs.Begin()
		//		; itf != trfs.End()
		//		; itf++) {
		//		rapidjson::GenericValue<rapidjson::UTF16<>>& trfl = *itf;
		//		if (torrent->files.count > uix) {
		//			JSTRYINT64(tll, L"bytesCompleted", trfl);
		//			JSTRYBOOL(tbb, L"wanted", trfl);
		//			JSTRYINT(tii, L"priority", trfl);
		//			utick = GetTickCount64();
		//			if (tll != torrent->files.items[uix].done) {
		//				torrent->files.items[uix].done = tll;
		//				torrent->files.items[uix].updatetick = utick;
		//			}
		//			if (tbb != torrent->files.items[uix].check) {
		//				torrent->files.items[uix].check = tbb;
		//				torrent->files.items[uix].updatetick = utick;
		//			}
		//			if (tii != torrent->files.items[uix].priority) {
		//				torrent->files.items[uix].priority = tii;
		//				torrent->files.items[uix].updatetick = utick;
		//			}
		//		}
		//		uix++;
		//	}
		//}

		if (torrent->pieces.length() > 0) {
			convertPiecesData(torrent);
		}
	}

	return 0;

}

int extractJsonObjectBuffer(rapidjson::GenericValue<rapidjson::UTF16<>>& ttstr, RawTorrentValuePair* object)
{
	unsigned long svs;
	long vss;
	BufferedJsonAllocator* allc = BufferedJsonAllocator::GetInstance();

	if (ttstr.IsObject()) {
		int mmc = ttstr.MemberCount();
		object->valuetype = RTT_OBJECT;
		object->vvobject.count = mmc;
		object->vvobject.items = (RawTorrentValuePair*)allc->Malloc(sizeof(RawTorrentValuePair) * object->vvobject.count);

		int ii = 0;
		rapidjson::GenericValue<rapidjson::UTF16<>>::MemberIterator itms = ttstr.MemberBegin();
		while (itms != ttstr.MemberEnd())
		{
			svs = itms->name.GetStringLength() + 1;
			object->vvobject.items[ii].key = (wchar_t*)allc->Malloc(sizeof(wchar_t) * svs);
			wcscpy_s(object->vvobject.items[ii].key, svs, itms->name.GetString());

			rapidjson::GenericValue<rapidjson::UTF16<>> & value = itms->value;
			extractJsonObjectBuffer(value, &object->vvobject.items[ii]);
			itms++;
			ii++;
		}
	}
	else if (ttstr.IsArray()) {
		vss = ttstr.Size();
		object->valuetype = RTT_ARRAY;
		object->vvobject.count = vss;
		object->vvobject.items = (RawTorrentValuePair*)allc->Malloc(sizeof(RawTorrentValuePair) * object->vvobject.count);
		int ii = 0;

		for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itvi = ttstr.Begin();
			itvi != ttstr.End();
			itvi++) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& ttvv = *itvi;
			extractJsonObjectBuffer(ttvv, &object->vvobject.items[ii]);

			ii++;
		}
	}
	else if (ttstr.IsString()) {
		object->valuetype = RTT_STRING;
		svs = ttstr.GetStringLength() + 1;
		object->vvstr = (wchar_t*)allc->Malloc(sizeof(wchar_t) * svs);
		wcscpy_s(object->vvstr, svs, ttstr.GetString());
	}
	else if (ttstr.IsDouble()) {
		object->valuetype = RTT_DOUBLE;
		object->vvdouble = ttstr.GetDouble();
	}
	else if (ttstr.IsInt()) {
		object->valuetype = RTT_INT;
		object->vvint64 = ttstr.GetInt();
	}
	else if (ttstr.IsInt64()) {
		object->valuetype = RTT_INT64;
		object->vvint64 = ttstr.GetInt64();
	}
	else if (ttstr.IsBool()) {
		object->valuetype = RTT_BOOL;
		object->vvbool = ttstr.GetBool();
	}
	else {
		assert(false);
	}

	return 0;
}


int extractJsonTorrents(std::wstring& torrentstr, _Out_ ItemArray<TorrentNode*>& nodes)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd;

	rapidjson::GenericStringStream<rapidjson::UTF16<>> gssu16((wchar_t*)torrentstr.c_str());
	rjd.ParseStream(gssu16);
	if (!rjd.HasParseError()) {
		if (rjd.HasMember(L"arguments")) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& args = rjd[L"arguments"];
			if (args.HasMember(L"torrents")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& trns = args[L"torrents"];
				if (trns.IsArray()) {
					int ttc = trns.GetArray().Capacity();
					if (ttc > 0) {

						// re-init worknodes
						if (ttc > (int)nodes.count) {
							if (nodes.items) {
								delete[] nodes.items;
							}
							nodes.count = ttc;
							nodes.items = new TorrentNode[ttc];
						}
						else {
							nodes.count = ttc;
						}
						int ii = 0;
						for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator ittr = trns.Begin()
							; ittr != trns.End()
							; ittr++) {
							rapidjson::GenericValue<rapidjson::UTF16<>>& trtn = *ittr;
							if (trtn.IsObject()) {

								extractTorrentObject(trtn, &nodes.items[ii]);
								ii++;
							}

							//////////////////////////////////
							Sleep(0);
						}
					}
				}
			}
			//if (args.HasMember(L"removed")) {
			//	rapidjson::GenericValue<rapidjson::UTF16<>>& rmvs = args[L"removed"];
			//	if (rmvs.IsArray()) {
			//		for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itrm = rmvs.Begin()
			//			; itrm != rmvs.End()
			//			; itrm++) {
			//			rapidjson::GenericValue<rapidjson::UTF16<>>& trtn = *itrm;
			//			if (trtn.IsInt()) {
			//				removed.insert(trtn.GetInt());
			//			}
			//		}
			//	}
			//}
		}
	}
	return 0;
}

int CmdFullRefreshTorrents::RequestServiceRefresh(ItemArray<TorrentNode*>& wns)
{
	int qrt = 0;
	std::set<long> removed;
	//ULONGLONG ptick = GetTickCount64();
	std::wstring ucoderesp;
	std::wstring xquery;
	RawTorrentValuePair rtvp;

	buildRefreshRequest(xquery);
	CurlSessionCY* curl = CurlSessionCY::GetInstance();
	std::wstring resp;

	qrt = curl->SendCurlRequest(profile, xquery, resp);
	if (qrt == 0) {
		int csl = resp.length();
		//char* cresp = (char*)malloc(csl + 1);
		char* cresp = (char*)BufferedJsonAllocator::GetInstance()->Malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, resp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			ConvertUTFCodePure(cresp, ucoderesp);
			//free(cresp);

			//qrt = extractJsonTorrents(ucoderesp, wns);
			rapidjson::GenericDocument<rapidjson::UTF16<>> jdoc;
			rapidjson::GenericStringStream<rapidjson::UTF16<>> jss((wchar_t*)ucoderesp.c_str());
			jdoc.ParseStream(jss);
			if (!jdoc.HasParseError()) {
				extractJsonObjectBuffer(jdoc, &rtvp);
			}
		}
	}

	returncode = qrt;
	if (qrt == 0) {
		if (callback) {
			callback->ProcessRawObjects(&rtvp, this, true);
		}
	}

	BufferedJsonAllocator::GetInstance()->Reset();

	//if (qrt == 0) {
	//	TorrentNodeVT* ctt;
	//	std::set<TorrentNodeVT*> tuts;
	//	std::for_each(allnodes->nodes.begin(), allnodes->nodes.end(), [&tuts, ptick](TorrentNodeVT* node) {
	//		if (node->updatetick >= ptick) {
	//			tuts.insert(node);
	//		}
	//	});
	//	std::for_each(tuts.begin(), tuts.end(), [ptick, this, req](TorrentNodeVT* node) {
	//		if (node->valid) {
	//			UpdateTrackerGroups(node);
	//			std::list<TorrentGroupVT*> grps;
	//			grps.clear();
	//			grouproot->GetTorrentGroups(node, grps);
	//			std::for_each(grps.begin(), grps.end(), [this, node, req](TorrentGroupVT*& grp) {
	//				req->refresh.cbgroupnode(req, node, grp);
	//			});
	//		}
	//		if (req->refresh.reqfiles) {
	//			req->refresh.cbnodefile(req, node);
	//		}
	//		if (req->refresh.reqfilestat) {
	//			for (unsigned long ii = 0; ii < node->files.count; ii++) {
	//				if (node->files.items[ii].updatetick >= ptick) {
	//					req->refresh.cbnodefilestat(req, node, node->files.items + ii);
	//				}
	//			}
	//			//req->refresh.cbnodefilestat(req, NULL, NULL);
	//		}
	//	});
	//	std::for_each(removed.begin(), removed.end(), [&, this, req](const long tid) {
	//		ctt = allnodes->GetIdTorrent(tid);
	//		if (ctt) {
	//			ctt->valid = false;
	//			std::list<TorrentGroupVT*> groups;
	//			grouproot->GetTorrentGroups(ctt, groups);
	//			std::for_each(groups.begin(), groups.end(), [req, ctt](TorrentGroupVT* group) {
	//				req->refresh.cbremovenode(req, ctt, group);
	//			});
	//		}
	//	});

	//	UpdateTorrentGroups(grouproot);
	//	std::set<TorrentGroupVT*> gps;
	//	grouproot->GetGroups(gps);
	//	std::for_each(gps.begin(), gps.end(), [ptick, req](TorrentGroupVT* grp) {
	//		if (ptick < grp->updatetick) {
	//			(*req->refresh.cbgroupgroup)(req, grp, grp->parent);
	//		}
	//	});
	//	req->refresh.cbgroupnode(req, NULL, NULL);
	//}
	//else {
	//	std::wstring estr = GetErrorString(qrt);
	//	(*req->refresh.cbfailed)(req, estr, qrt);
	//}

	return qrt;

}

int buildRefreshDetailRequest(_In_ std::set<long>& tss, _Out_ std::wstring& out)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);

	for (std::set<std::wstring>::iterator itfs = detailfields.begin();
		itfs != detailfields.end();
		itfs++) {
		vvv.SetString((*itfs).c_str(), allocator);
		fdary.PushBack(vvv, allocator);
	}
	args.AddMember(L"fields", fdary, allocator);

	for (std::set<long>::iterator itid = tss.begin();
		itid != tss.end();
		itid++) {
		vvv.SetInt(*itid);
		idary.PushBack(vvv, allocator);
	}
	args.AddMember(L"ids", idary, allocator);
	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-get", allocator);
	rjd.AddMember(L"tag", 8, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	out = sbuf.GetString();
	return 0;
}

#define WFS_BUF_SIZE 4096

struct WideFileStream {
	typedef typename wchar_t Ch;

	WideFileStream(const Ch *fname) :
		loadedfilepos_(0)
		, curbufpos_(0)
		, total_(0)
		, curbufsize_(0)
		, doneall(false)
	{
		int fnl = wcslen(fname);
		filename_ = (Ch*)BufferedJsonAllocator::GetInstance()->Malloc((fnl + 1) * sizeof(Ch));
		wcscpy_s(filename_, fnl + 1, fname);
		filename_[fnl] = 0;
	}

	int LoadBuffer() {
		HANDLE hrr = CreateFile(filename_, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hrr != INVALID_HANDLE_VALUE) {
			DWORD dwrr;
			BOOL brr = SetFilePointer(hrr, loadedfilepos_, NULL, FILE_BEGIN);
			brr = ReadFile(hrr, (LPVOID)datbuf_, WFS_BUF_SIZE * sizeof(Ch), &dwrr, NULL);
			CloseHandle(hrr);
			if (dwrr > 0) {
				curbufsize_ = dwrr / sizeof(Ch);
				loadedfilepos_ += dwrr;
				curbufpos_ = 0;
			}
			else {
				doneall = true;
			}
		}
		return 0;
	}

	Ch Peek() {
		Ch cvv = 0;
		if (doneall) {
			cvv = 0;
		}
		else {
			if (curbufsize_ == 0) {
				LoadBuffer();
			}
			cvv = datbuf_[curbufpos_];
		}
		return cvv;
	}

	Ch Take() {
		Ch cvv = 0;
		if (doneall) {
			cvv = 0;
		}
		else {
			cvv = Peek();
			curbufpos_++;
			if (curbufpos_ >= curbufsize_) {
				curbufsize_ = 0;
			}
		}
		return cvv;
	}
	size_t Tell() const {
		return (loadedfilepos_ / sizeof(Ch)) - curbufsize_ + curbufpos_;
	}

	Ch* PutBegin() { RAPIDJSON_ASSERT(false); return 0; }
	void Put(Ch) { RAPIDJSON_ASSERT(false); }
	void Flush() { RAPIDJSON_ASSERT(false); }
	size_t PutEnd(Ch*) { RAPIDJSON_ASSERT(false); return 0; }

	Ch* filename_;     //!< Current read position.
	size_t loadedfilepos_;
	size_t total_;
	size_t curbufpos_;
	size_t curbufsize_;
	bool doneall;
	Ch datbuf_[WFS_BUF_SIZE];
};

struct WideFileHandleStream {
	typedef typename wchar_t Ch;

	WideFileHandleStream(const HANDLE hfstream) :
		loadedfilepos_(0)
		, curbufpos_(0)
		, total_(0)
		, curbufsize_(0)
		, doneall(false)
		, hfstream_(hfstream)
	{
	}

	int LoadBuffer() {
		//HANDLE hrr = CreateFile(filename_, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hfstream_ != INVALID_HANDLE_VALUE) {
			DWORD dwrr;
			BOOL brr = SetFilePointer(hfstream_, loadedfilepos_, NULL, FILE_BEGIN);
			brr = ReadFile(hfstream_, (LPVOID)datbuf_, WFS_BUF_SIZE * sizeof(Ch), &dwrr, NULL);
			//CloseHandle(hrr);
			if (dwrr > 0) {
				curbufsize_ = dwrr / sizeof(Ch);
				loadedfilepos_ += dwrr;
				curbufpos_ = 0;
			}
			else {
				doneall = true;
			}
		}
		return 0;
	}

	Ch Peek() {
		Ch cvv = 0;
		if (doneall) {
			cvv = 0;
		}
		else {
			if (curbufsize_ == 0) {
				LoadBuffer();
			}
			cvv = datbuf_[curbufpos_];
		}
		return cvv;
	}

	Ch Take() {
		Ch cvv = 0;
		if (doneall) {
			cvv = 0;
		}
		else {
			cvv = Peek();
			curbufpos_++;
			if (curbufpos_ >= curbufsize_) {
				curbufsize_ = 0;
			}
		}
		return cvv;
	}
	size_t Tell() const {
		return (loadedfilepos_ / sizeof(Ch)) - curbufsize_ + curbufpos_;
	}

	Ch* PutBegin() { RAPIDJSON_ASSERT(false); return 0; }
	void Put(Ch) { RAPIDJSON_ASSERT(false); }
	void Flush() { RAPIDJSON_ASSERT(false); }
	size_t PutEnd(Ch*) { RAPIDJSON_ASSERT(false); return 0; }

	HANDLE hfstream_;
	size_t loadedfilepos_;
	size_t total_;
	size_t curbufpos_;
	size_t curbufsize_;
	bool doneall;
	Ch datbuf_[WFS_BUF_SIZE];
};


wchar_t tmbuf[2048];

int CmdFullRefreshTorrents::RequestServiceRefreshReduced()
{
	int qrt = 0;
	std::set<long> removed;
	//ULONGLONG ptick = GetTickCount64();
	//std::wstring ucoderesp;
	std::wstring xquery;
	RawTorrentValuePair rtvp;

	if (dofullyrefresh) {
		qrt = buildRefreshRequest(xquery);
	}
	else {
		if (torrentids.size() > 0) {
			qrt = buildRefreshDetailRequest(torrentids, xquery);
		}
		else {
			qrt = -1;
		}
	}

	bool domemreq;
	//dofilereq = dofullyrefresh;
	domemreq = false;

	std::wstring resp;
	HANDLE hrdat = NULL;
	if (qrt == 0) {
		CurlSessionCY* curl = CurlSessionCY::GetInstance();
		if (domemreq) {
			qrt = curl->SendCurlRequest(profile, xquery, resp);
		}
		else {
			UINT urt = GetTempFileName(L".", L"_sp", 0, tmbuf);
			hrdat = CreateFile(tmbuf, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW | OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);

			qrt = curl->SendCurlRequestFileHandle(profile, xquery, hrdat);
		}
	}

	if (qrt == 0) {
		rapidjson::GenericDocument<rapidjson::UTF16<>> jdoc;

		if (domemreq) {
			rapidjson::GenericStringStream<rapidjson::UTF16<>> jss((wchar_t*)resp.c_str());
			jdoc.ParseStream(jss);
		}
		else {
			WideFileHandleStream wfs(hrdat);
			jdoc.ParseStream<WideFileHandleStream>(wfs);
		}

		if (!jdoc.HasParseError()) {
			qrt = extractJsonObjectBuffer(jdoc, &rtvp);
		}
		else {
			qrt = -2;
		}
	}

	if (!domemreq) {
		if (hrdat != INVALID_HANDLE_VALUE) {
			CloseHandle(hrdat);
		}
	}

	returncode = qrt;
	if (qrt == 0) {
		if (callback) {
			callback->ProcessRawObjects(&rtvp, this, dofullyrefresh ? true : false);
		}
	}

	BufferedJsonAllocator::GetInstance()->Reset();

	return qrt;
}

int CmdFullRefreshTorrents::Process(TransmissionManager* mgr)
{
	if (profile) {
		profile->inrefresh = true;

		switch (profile->state)
		{
		case DONE_FILL:
		{
			CurlSessionCY* curl = CurlSessionCY::GetInstance();
			std::wstring token;
			this->returncode = curl->GetSessionToken(profile->url, profile->username, profile->passwd, token);
			if (this->returncode == 0) {
				if (token.length() > 0) {
					profile->token = token;
				}
				profile->state = DONE_LOGIN;
				Process(mgr); // state changed
			}
		}
		break;
		case DONE_LOGIN:
			CurlSessionCY* curl = CurlSessionCY::GetInstance();
			std::wstring rawtorrents;
			this->returncode = RequestServiceRefreshReduced();
			if (this->returncode != 0) { // no error
				if (this->returncode != 5) {	// timeout
					profile->state = DONE_FILL;
				}
			}
			break;
		}

		if (callback) {
			callback->ProcessRawObjects(NULL, this, false);
		}

		profile->inrefresh = false;
	}

	return this->returncode;
}

int buildCommitRequest(_In_ const std::wstring& ent, _Out_ std::wstring& req, bool ismedia)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	//rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);

	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	vvv.SetString(ent.c_str(), allocator);
	if (ismedia) {
		args.AddMember(L"metainfo", vvv, allocator);
	}
	else {
		args.AddMember(L"filename", vvv, allocator);
	}
	args.AddMember(L"paused", true, allocator);

	rjd.AddMember(L"method", L"torrent-add", allocator);
	rjd.AddMember(L"arguments", args, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	req = sbuf.GetString();
	return 0;
}

int CmdCommitTorrent::Process(TransmissionManager * mng)
{
	int qrt = 0;
	if (entry.length() > 0) {
		std::wstring req;

		HANDLE htf = CreateFile(
			entry.c_str()
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
					//unsigned char* pfb = (unsigned char*)malloc(fsz.LowPart);
					unsigned char* pfb = (unsigned char*)BufferedJsonAllocator::GetInstance()->Malloc(fsz.LowPart);
					if (pfb) {
						ReadFile(htf, pfb, fsz.LowPart, &dsz, NULL);
						DWORD sbz = 0;
						BOOL btn = CryptBinaryToString(pfb, dsz, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &sbz);
						if (sbz > 0) {
							//TCHAR* b64str = (TCHAR*)malloc(sbz * sizeof(TCHAR));
							TCHAR* b64str = (TCHAR*)BufferedJsonAllocator::GetInstance()->Malloc(sbz * sizeof(TCHAR));
							if (b64str) {
								btn = CryptBinaryToString(pfb, dsz, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64str, &sbz);
								if (btn) {
									buildCommitRequest(b64str, req, true);
								}
								else {
									qrt = -5;
								}
								//free(b64str);
							}
							else {
								qrt = -3;
							}
						}
						//free(pfb);
					}
					else {
						qrt = -4;
					}
				}
				else {
					qrt = -6;
				}
			}
			else {
				buildCommitRequest(entry, req, false);
			}

			CloseHandle(htf);
		} // invalid file handle
		else {
			buildCommitRequest(entry, req, false);
		}

		if (req.length() > 0) {
			CurlSessionCY* curl = CurlSessionCY::GetInstance();
			std::wstring resp;

			qrt = curl->SendCurlRequest(profile, req, resp);
			//if (qrt == 0) {
			//	int csl = resp.length();
			//	char* cresp = (char*)malloc(csl + 1);
			//	if (cresp) {
			//		csl = WideCharToMultiByte(CP_ACP, 0, resp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			//		cresp[csl] = 0;
			//		ConvertUTFCodePure(cresp, ucoderesp);
			//		free(cresp);

			//		qrt = extractJsonTorrents(ucoderesp, wns);
			//	}
			//}

			//returncode = qrt;
			//if (qrt == 0) {
			//	if (callback) {
			//		callback->Process(&wns, this);
			//	}
			//}
		}
		else { // req length 0
			qrt = -2;
		}
	}
	else { // entry length 0
		qrt = -1;
	}

	if (callback) {
		callback->Process(this, qrt);
	}

	BufferedJsonAllocator::GetInstance()->Reset();

	return 0;
}

//int buildDeleteRequest(ItemArray<int*>* tds, bool deldata, std::wstring& req)
//{
//	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
//	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
//	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
//
//	for (int ii = 0; ii < (int)tds->count; ii++) {
//		idary.PushBack((int)tds->items[ii], allocator);
//	}
//	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
//	args.AddMember(L"ids", idary, allocator);
//	args.AddMember(L"delete-local-data", deldata, allocator);
//
//	rjd.AddMember(L"arguments", args, allocator);
//	rjd.AddMember(L"method", L"torrent-remove", allocator);
//	rjd.AddMember(L"tag", 5, allocator);
//
//	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
//	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
//	rjd.Accept(writer);
//
//	req = sbuf.GetString();
//	return 0;
//}

int buildActionRequest(ItemArray<int*>* tds, int action, int param, std::wstring& req)
{
	int rtn = 0;

	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	for (int ii = 0; ii < (int)tds->count; ii++) {
		idary.PushBack((int)tds->items[ii], allocator);
	}
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	args.AddMember(L"ids", idary, allocator);

	switch (action)
	{
	case CATA_DELETE:
		args.AddMember(L"delete-local-data", param == CATA_PARAM_DELETEFILES, allocator);
		rjd.AddMember(L"method", L"torrent-remove", allocator);
		break;
	case CATA_PAUSE:
		rjd.AddMember(L"method", L"torrent-stop", allocator);
		break;
	case CATA_UNPAUSE:
		rjd.AddMember(L"method", L"torrent-start", allocator);
		break;
	case CATA_VERIFY:
		rjd.AddMember(L"method", L"torrent-verify", allocator);
		break;
	default:
		rtn = 100;
		break;
	}

	if (rtn == 0) {
		rjd.AddMember(L"arguments", args, allocator);
		rjd.AddMember(L"tag", 5, allocator);

		rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
		rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
		rjd.Accept(writer);

		req = sbuf.GetString();
	}
	return rtn;
}

int CmdActionTorrent::Process(TransmissionManager * mng)
{
	int rtn = 0;
	std::wstring req;
	ItemArray<int*> tids;
	int ii;

	tids.count = ids.size();
	if (tids.count > 0) {
		tids.items = new int[tids.count];
		tids.items = (int*)BufferedJsonAllocator::GetInstance()->Malloc(sizeof(int) * tids.count);
		ii = 0;
		for (std::set<int>::iterator itts = ids.begin();
			itts != ids.end();
			itts++) {
			tids.items[ii] = *itts;
			ii++;
		}
		rtn = buildActionRequest(&tids, action, actionparam, req);
		//delete[] tids.items;
	}
	else {
		rtn = -2;
	}

	if (req.length() > 0) {
		CurlSessionCY* curl = CurlSessionCY::GetInstance();
		std::wstring resp;

		rtn = curl->SendCurlRequest(profile, req, resp);
	}
	else {
		rtn = -1;
	}

	if (callback) {
		callback->Process(this, ids, rtn);
	}

	BufferedJsonAllocator::GetInstance()->Reset();

	return rtn;
}

int buildSessionStatusRequest(std::wstring& req)
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

int extractSessionStatus(std::wstring& sss, TransmissionProfile* prof)
{
	int rtn = 0;

	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd;

	rapidjson::GenericStringStream<rapidjson::UTF16<>> gssu16((wchar_t*)sss.c_str());
	rjd.ParseStream(gssu16);
	if (!rjd.HasParseError()) {
		if (rjd.HasMember(L"arguments")) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& args = rjd[L"arguments"];
			JSTRYINT(prof->stat.downspeed, L"downloadSpeed", args);
			JSTRYINT(prof->stat.upspeed, L"uploadSpeed", args);
			JSTRYINT(prof->stat.activecount, L"activeTorrentCount", args);
			JSTRYINT(prof->stat.pausecount, L"pausedTorrentCount", args);
			JSTRYINT(prof->stat.totalcount, L"torrentCount", args);

			if (args.HasMember(L"cumulative-stats")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& cstt = args[L"cumulative-stats"];
				JSTRYINT64(prof->stat.total.upsize, L"uploadedBytes", cstt);
				JSTRYINT64(prof->stat.total.downsize, L"downloadedBytes", cstt);
				JSTRYINT(prof->stat.total.filesadd, L"filesAdded", cstt);
				JSTRYINT(prof->stat.total.sessioncount, L"sessionCount", cstt);
				JSTRYINT(prof->stat.total.sessionactive, L"secondsActive", cstt);
			}
			if (args.HasMember(L"current-stats")) {
				rapidjson::GenericValue<rapidjson::UTF16<>>& ustt = args[L"current-stats"];
				JSTRYINT64(prof->stat.current.upsize, L"uploadedBytes", ustt);
				JSTRYINT64(prof->stat.current.downsize, L"downloadedBytes", ustt);
				JSTRYINT(prof->stat.current.filesadd, L"filesAdded", ustt);
				JSTRYINT(prof->stat.current.sessioncount, L"sessionCount", ustt);
				JSTRYINT(prof->stat.current.sessionactive, L"secondsActive", ustt);
			}
		}
		else {
			rtn = 2;
		}
	}
	else {
		rtn = 1;
	}

	return rtn;
}

int CmdRefreshSession::Process(TransmissionManager * mng)
{
	int rtn = 0;

	if (profile) {
		profile->inrefresh = true;
		ProcessSessionStatus(mng);
		ProcessSessionInfo(mng);
		BufferedJsonAllocator::GetInstance()->Reset();
		profile->inrefresh = false;
	}
	else {
		rtn = -1;
	}

	return rtn;
}

int CmdRefreshSession::ProcessSessionStatus(TransmissionManager * mng)
{
	std::wstring req;
	int rtn = buildSessionStatusRequest(req);

	std::wstring resp;

	if (rtn == 0) {
		CurlSessionCY* curl = CurlSessionCY::GetInstance();

		rtn = curl->SendCurlRequest(profile, req, resp);
	}

	if (rtn == 0) {
		int csl = resp.length();
		char* cresp = (char*)BufferedJsonAllocator::GetInstance()->Malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, resp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			std::wstring ucoderesp;

			ConvertUTFCodePure(cresp, ucoderesp);
			rtn = extractSessionStatus(ucoderesp, profile);
		}
	}

	if (callback) {
		callback->ProcessStatus(mng, this);
	}

	return rtn;
}

int buildRefreshSessionInfo(_Out_ std::wstring& out)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	//rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fields(rapidjson::kObjectType);

	fdary.PushBack(L"session-id", allocator);
	fdary.PushBack(L"rpc-version", allocator);
	fdary.PushBack(L"version", allocator);
	fdary.PushBack(L"config-dir", allocator);
	fdary.PushBack(L"speed-limit-down", allocator);
	fdary.PushBack(L"speed-limit-down-enabled", allocator);
	fdary.PushBack(L"speed-limit-up-enabled", allocator);
	fdary.PushBack(L"speed-limit-up", allocator);
	fdary.PushBack(L"alt-speed-enabled", allocator);
	fdary.PushBack(L"alt-speed-down", allocator);
	fdary.PushBack(L"alt-speed-up", allocator);
	fdary.PushBack(L"download-dir", allocator);

	fields.AddMember(L"fields", fdary, allocator);
	rjd.AddMember(L"arguments", fields, allocator);
	rjd.AddMember(L"method", L"session-get", allocator);
	rjd.AddMember(L"tag", 8, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	out = sbuf.GetString();
	return 0;
}

int CmdRefreshSession::ProcessSessionInfo(TransmissionManager * mng)
{
	int qrt = 0;
	std::set<long> removed;
	//ULONGLONG ptick = GetTickCount64();
	//std::wstring ucoderesp;
	std::wstring xquery;
	RawTorrentValuePair rtvp;

	qrt = buildRefreshSessionInfo(xquery);

	std::wstring resp;
	HANDLE hrdat = NULL;
	if (qrt == 0) {
		CurlSessionCY* curl = CurlSessionCY::GetInstance();
		qrt = curl->SendCurlRequest(profile, xquery, resp);
	}

	if (qrt == 0) {
		rapidjson::GenericDocument<rapidjson::UTF16<>> jdoc;

		rapidjson::GenericStringStream<rapidjson::UTF16<>> jss((wchar_t*)resp.c_str());
		jdoc.ParseStream(jss);

		if (!jdoc.HasParseError()) {
			qrt = extractJsonObjectBuffer(jdoc, &rtvp);
		}
		else {
			qrt = -2;
		}
	}

	if (qrt == 0) {
		if (callback) {
			callback->ProcessInfo(&rtvp, this);
		}
	}

	BufferedJsonAllocator::GetInstance()->Reset();

	return qrt;
}

//int CmdRefreshTorrentsDetail::RequestTorrentDetailRefresh(std::set<int>& tss, ItemArray<TorrentNode*>& tws)
//{
//	int qrt = 0;
//	std::set<long> removed;
//	RawTorrentValuePair rtvp;
//	//ULONGLONG ptick = GetTickCount64();
//	std::wstring ucoderesp;
//	std::wstring xquery;
//
//	buildRefreshDetailRequest(tss, xquery);
//	CurlSessionCY* curl = CurlSessionCY::GetInstance();
//	std::wstring resp;
//
//	qrt = curl->SendCurlRequest(profile, xquery, resp);
//	if (qrt == 0) {
//		//int csl = resp.length();
//		////char* cresp = (char*)malloc(csl + 1);
//		//char* cresp = (char*)BufferedJsonAllocator::GetInstance()->Malloc(csl + 1);
//		//if (cresp) {
//		//	csl = WideCharToMultiByte(CP_ACP, 0, resp.c_str(), csl, cresp, csl + 1, NULL, NULL);
//		//	cresp[csl] = 0;
//		//	ConvertUTFCodePure(cresp, ucoderesp);
//		//	//free(cresp);
//
//		//qrt = extractJsonTorrents(ucoderesp, tws);
//		rapidjson::GenericDocument<rapidjson::UTF16<>> jdoc;
//		rapidjson::GenericStringStream<rapidjson::UTF16<>> jss((wchar_t*)resp.c_str());
//		jdoc.ParseStream(jss);
//		if (!jdoc.HasParseError()) {
//			extractJsonObjectBuffer(jdoc, &rtvp);
//		}
//		//}
//	}
//
//	returncode = qrt;
//	if (qrt == 0) {
//		if (callback) {
//			callback->ProcessRawObjects(&rtvp, this);
//		}
//	}
//
//	BufferedJsonAllocator::GetInstance()->Reset();
//
//	return qrt;
//}
//
//int CmdRefreshTorrentsDetail::Process(TransmissionManager *mgr)
//{
//	if (profile) {
//		//profile->inrefresh = true;
//		switch (profile->state)
//		{
//		case DONE_FILL:
//		{
//			CurlSessionCY* curl = CurlSessionCY::GetInstance();
//			std::wstring token;
//			this->returncode = curl->GetSessionToken(profile->url, profile->username, profile->passwd, token);
//			if (this->returncode == 0) {
//				if (token.length() > 0) {
//					profile->token = token;
//				}
//				profile->state = DONE_LOGIN;
//				Process(mgr); // state changed
//			}
//		}
//		break;
//		case DONE_LOGIN:
//			if (ids.size() > 0) {
//				CurlSessionCY* curl = CurlSessionCY::GetInstance();
//				std::wstring rawtorrents;
//				this->returncode = RequestTorrentDetailRefresh(ids, mgr->worktorrents);
//				if (this->returncode != 0) { // no error
//					if (this->returncode != 5) {	// timeout
//						profile->state = DONE_FILL;
//					}
//				}
//			}
//			break;
//
//		}
//	}
//
//	if (callback) {
//		callback->Process(NULL, this);
//	}
//	profile->inrefresh = false;
//
//	return this->returncode;
//}

int buildSetLocationRequest(_In_ std::set<int>& tss, _In_ std::wstring& loc, _Out_ std::wstring& out)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);

	for (std::set<int>::iterator itid = tss.begin();
		itid != tss.end();
		itid++) {
		vvv.SetInt(*itid);
		idary.PushBack(vvv, allocator);
	}
	args.AddMember(L"ids", idary, allocator);
	vvv.SetString(loc.c_str(), allocator);
	args.AddMember(L"location", vvv, allocator);
	vvv.SetBool(false);
	args.AddMember(L"move", vvv, allocator);
	rjd.AddMember(L"arguments", args, allocator);
	rjd.AddMember(L"method", L"torrent-set-location", allocator);
	rjd.AddMember(L"tag", 8, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	out = sbuf.GetString();
	return 0;
}


int CmdSetLocation::Process(TransmissionManager * mgr)
{
	int qrt = 0;

	if (profile) {
		std::wstring xquery;

		buildSetLocationRequest(idset, location, xquery);
		CurlSessionCY* curl = CurlSessionCY::GetInstance();
		std::wstring resp;

		qrt = curl->SendCurlRequest(profile, xquery, resp);

		returncode = qrt;
	}
	else {
		qrt = -1;
	}

	if (callback) {
		callback->Process(this);
	}

	BufferedJsonAllocator::GetInstance()->Reset();

	return qrt;
}

int BufferAllocator::_alloccurrindex(long cidx, long csz)
{
	buffer[cidx] = (unsigned char*)malloc(csz);
	totalsize += currsize;
	currpos = 0;

	return 0;
}

BufferAllocator::BufferAllocator()
	: totalsize(0)
	, index(0)
	, currpos(0)
{
	buffer = (unsigned char**)malloc(sizeof(unsigned char*) * 1024);
	currsize = 4096;
	currindex = 0;
	_alloccurrindex(currindex, currsize);
}

BufferAllocator::~BufferAllocator()
{
	FreeAll();
	free(buffer);
	buffer = NULL;
}

// #define __max(x, y) ((x > y) ? (x) : (y))
unsigned char * BufferAllocator::Allcate(long ssz)
{
	unsigned char * rtn = nullptr;

	if (currpos + ssz > currsize) {
		currindex++;
		currsize = max(currsize * 2, ssz);
		_alloccurrindex(currindex, currsize);
	}
	rtn = buffer[currindex] + currpos;
	currpos += ssz;
	if (currpos >= currsize) {
		currindex++;
		currsize *= 2;
		_alloccurrindex(currindex, currsize);
	}

	return rtn;
}

int BufferAllocator::FreeAll()
{
	if (index > 0) {
		for (int ii = 0; ii < index; ii++) {
			free(buffer[ii]);
		}
		index = 0;
	}
	return 0;
}

int buildFilesRequest(int tid, std::set<int> fids, int action, int param, std::wstring& req)
{
	int rtn = 0;

	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericValue<rapidjson::UTF16<>> tidary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	for (std::set<int>::iterator itfd = fids.begin(); itfd != fids.end(); itfd++) {
		idary.PushBack((int)*itfd, allocator);
	}
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);
	tidary.PushBack((int)tid, allocator);
	args.AddMember(L"ids", tidary, allocator);

	switch (action)
	{
	case CAF_ENABLE:
		if (param) {
			args.AddMember(L"files-wanted", idary, allocator);
		}
		else {
			args.AddMember(L"files-unwanted", idary, allocator);
		}
		rjd.AddMember(L"method", L"torrent-set", allocator);
		break;
	case CAF_PRIORITY:
		if (param > 0) {
			args.AddMember(L"priority-high", idary, allocator);
		}
		else if (param < 0) {
			args.AddMember(L"priority-low", idary, allocator);
		}
		else {
			args.AddMember(L"priority-normal", idary, allocator);
		}
		rjd.AddMember(L"method", L"torrent-set", allocator);
		break;
	default:
		rtn = 100;
		break;
	}

	if (rtn == 0) {
		rjd.AddMember(L"arguments", args, allocator);
		rjd.AddMember(L"tag", 6, allocator);

		rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
		rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
		rjd.Accept(writer);

		req = sbuf.GetString();
	}
	return rtn;
}

int CmdActionFile::Process(TransmissionManager * mng)
{
	int qrt = 0;

	if (profile) {
		std::wstring xquery;

		buildFilesRequest(torrentid, fileids, action, actionparam, xquery);
		CurlSessionCY* curl = CurlSessionCY::GetInstance();
		std::wstring resp;

		qrt = curl->SendCurlRequest(profile, xquery, resp);
	}
	else {
		qrt = -1;
	}

	if (callback) {
		callback->Process(this);
	}

	BufferedJsonAllocator::GetInstance()->Reset();

	return qrt;
}

int buildSetLimitRequest(int act, int lmt, _Out_ std::wstring& out)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fields(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);

	switch (act) {
	case CSLA_DOWN:
		fields.AddMember(L"speed-limit-down", (int)lmt, allocator);
		break;
	case CSLA_UP:
		fields.AddMember(L"speed-limit-up", (int)lmt, allocator);
		break;
	case CSLA_ENABLE_DOWN:
		fields.AddMember(L"speed-limit-down-enabled", (bool)true, allocator);
		break;
	case CSLA_ENABLE_UP:
		fields.AddMember(L"speed-limit-up-enabled", (bool)true, allocator);
		break;
	case CSLA_DISABLE_DOWN:
		fields.AddMember(L"speed-limit-down-enabled", (bool)false, allocator);
		break;
	case CSLA_DISABLE_UP:
		fields.AddMember(L"speed-limit-up-enabled", (bool)false, allocator);
		break;

	default:
		break;
	}
	rjd.AddMember(L"arguments", fields, allocator);
	rjd.AddMember(L"method", L"session-set", allocator);
	rjd.AddMember(L"tag", 9, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	out = sbuf.GetString();
	return 0;
}

int buildSetTorrentLimitRequest(int act, int lmt, std::set<int> & ids, _Out_ std::wstring& out)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fields(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);

	switch (act) {
	case CSLA_DOWN:
		fields.AddMember(L"downloadLimit", (int)lmt, allocator);
		break;
	case CSLA_UP:
		fields.AddMember(L"uploadLimit", (int)lmt, allocator);
		break;
	case CSLA_ENABLE_DOWN:
		fields.AddMember(L"downloadLimited", (bool)true, allocator);
		break;
	case CSLA_ENABLE_UP:
		fields.AddMember(L"uploadLimited", (bool)true, allocator);
		break;
	case CSLA_DISABLE_DOWN:
		fields.AddMember(L"downloadLimited", (bool)false, allocator);
		break;
	case CSLA_DISABLE_UP:
		fields.AddMember(L"uploadLimited", (bool)false, allocator);
		break;

	default:
		break;
	}
	for (std::set<int>::iterator itis = ids.begin(); itis != ids.end(); itis++) {
		idary.PushBack(*itis, allocator);
	}
	fields.AddMember(L"ids", idary, allocator);
	rjd.AddMember(L"arguments", fields, allocator);
	rjd.AddMember(L"method", L"torrent-set", allocator);
	rjd.AddMember(L"tag", 9, allocator);

	rapidjson::GenericStringBuffer<rapidjson::UTF16<>> sbuf;
	rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF16<>>, rapidjson::UTF16<>> writer(sbuf);
	rjd.Accept(writer);

	out = sbuf.GetString();
	return 0;
}


int CmdSetLimit::Process(TransmissionManager * mgr)
{
	int qrt = 0;

	if (profile) {
		std::wstring xquery;

		if (tidset.size() > 0) {
			buildSetTorrentLimitRequest(limitaction, limitspeed, tidset, xquery);
		}
		else {
			buildSetLimitRequest(limitaction, limitspeed, xquery);
		}
		CurlSessionCY* curl = CurlSessionCY::GetInstance();
		std::wstring resp;

		qrt = curl->SendCurlRequest(profile, xquery, resp);
	}
	else {
		qrt = -1;
	}

	BufferedJsonAllocator::GetInstance()->Reset();

	return qrt;
}
