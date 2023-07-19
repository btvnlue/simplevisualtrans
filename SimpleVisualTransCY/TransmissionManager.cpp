#include "TransmissionManager.h"
#include "TransmissionProfile.h"
#include "CurlSessionCY.h"

#include <Windows.h>
#include <sstream>
#include <set>
#include <rapidjson/document.h>
#include <rapidjson/encodings.h>
#include <rapidjson/writer.h>

std::set<std::wstring> fields;

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
	if (fields.size() == 0) {
		fields.insert(L"id");
		fields.insert(L"name");
		fields.insert(L"totalSize");
		fields.insert(L"trackers");
		fields.insert(L"status");
		fields.insert(L"downloadDir");
		fields.insert(L"uploadedEver");
		fields.insert(L"downloadedEver");
		fields.insert(L"secondsDownloading");
		fields.insert(L"secondsSeeding");
		fields.insert(L"rateDownload");
		fields.insert(L"errorString");
		fields.insert(L"isPrivate");
		fields.insert(L"isFinished");
		fields.insert(L"isStalled");
		fields.insert(L"rateUpload");
		fields.insert(L"errorString");
		fields.insert(L"uploadRatio");
		fields.insert(L"leftUntilDone");
		fields.insert(L"doneDate");
		fields.insert(L"activityDate");
		fields.insert(L"pieceCount");
		fields.insert(L"pieceSize");
		fields.insert(L"activityDate");
		fields.insert(L"addedDate");
		fields.insert(L"startDate");
		fields.insert(L"desiredAvailable");
		fields.insert(L"pieces");
	}
}

TransmissionManager::~TransmissionManager()
{
	Stop();
	CurlSessionCY::CleanUp();
	fields.clear();
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

	if (worktorrents.count > 0) {
		delete[] worktorrents.items;
		worktorrents.items = NULL;
		worktorrents.count = 0;
	}

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
			profile->url = L"http://192.168.5.158:9091/transmission/rpc";

			profile->state = DONE_FILL;
		}
	}
	if (callback) {
		callback->Process(mgr);
	}

	return 0;
}

int TransmissionManager::LoadDefaultProfile(TransmissionProfile* prof)
{
	CmdLoadProfile* cmd = new CmdLoadProfile();
	cmd->profile = prof;
	PutCommand(cmd);
	return 0;
}

int buildRefreshRequest(_Out_ std::wstring& out)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> fdary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();
	rapidjson::GenericValue<rapidjson::UTF16<>> vvv(rapidjson::kStringType);
	rapidjson::GenericValue<rapidjson::UTF16<>> args(rapidjson::kObjectType);

	for (std::set<std::wstring>::iterator itfs = fields.begin();
		itfs != fields.end();
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

							//////////////////////////////
							Sleep(0);
						}
					}
				}
				free(dff);
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

		if (ttstr.HasMember(L"trackers") && ttstr[L"trackers"].IsArray()) {
			rapidjson::GenericValue<rapidjson::UTF16<>>& trks = ttstr[L"trackers"];
			torrent->CleanTrackers();
			torrent->rawtrackers.count = trks.Size();
			if (torrent->rawtrackers.count > 0) {
				torrent->rawtrackers.items = new TrackerCY[torrent->rawtrackers.count];
				int ii = 0;
				for (rapidjson::GenericValue<rapidjson::UTF16<>>::ValueIterator itk = trks.Begin()
					; itk != trks.End()
					; itk++) {
					rapidjson::GenericValue<rapidjson::UTF16<>>& trka = *itk;

					if (trka.HasMember(L"announce") && trka[L"announce"].IsString()) {
						torrent->rawtrackers.items[ii].url = trka[L"announce"].GetString();
					}
					ii++;
				}
			}
		}
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

int CmdRefreshTorrents::RequestServiceRefresh(ItemArray<TorrentNode*>& wns)
{
	int qrt = 0;
	std::set<long> removed;
	//ULONGLONG ptick = GetTickCount64();
	std::wstring ucoderesp;
	std::wstring xquery;

	buildRefreshRequest(xquery);
	CurlSessionCY* curl = CurlSessionCY::GetInstance();
	std::wstring resp;

	qrt = curl->SendCurlRequest(profile, xquery, resp);
	if (qrt == 0) {
		int csl = resp.length();
		char* cresp = (char*)malloc(csl + 1);
		if (cresp) {
			csl = WideCharToMultiByte(CP_ACP, 0, resp.c_str(), csl, cresp, csl + 1, NULL, NULL);
			cresp[csl] = 0;
			ConvertUTFCodePure(cresp, ucoderesp);
			free(cresp);

			qrt = extractJsonTorrents(ucoderesp, wns);
		}
	}

	returncode = qrt;
	if (qrt == 0) {
		if (callback) {
			callback->Process(&wns, this);
		}
	}

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

int CmdRefreshTorrents::Process(TransmissionManager* mgr)
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
					profile->state = DONE_LOGIN;
					Process(mgr); // state changed
				}
				profile->state = DONE_LOGIN;
				Process(mgr); // state changed
			}
		}
		break;
		case DONE_LOGIN:
			CurlSessionCY* curl = CurlSessionCY::GetInstance();
			std::wstring rawtorrents;
			this->returncode = RequestServiceRefresh(mgr->worktorrents);
			if (this->returncode != 0) { // no error
				if (this->returncode != 5) {	// timeout
					profile->state = DONE_FILL;
				}
			}
			break;
		}

		if (callback) {
			callback->Process(NULL, this);
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
	args.AddMember(L"paused", false, allocator);

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
									buildCommitRequest(b64str, req, true);
								}
								else {
									qrt = -5;
								}
								free(b64str);
							}
							else {
								qrt = -3;
							}
						}
						free(pfb);
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
	return 0;
}

int buildDeleteRequest(ItemArray<int*>* tds, bool deldata, std::wstring& req)
{
	rapidjson::GenericDocument<rapidjson::UTF16<>> rjd(rapidjson::kObjectType);
	rapidjson::GenericValue<rapidjson::UTF16<>> idary(rapidjson::kArrayType);
	rapidjson::GenericDocument<rapidjson::UTF16<>>::AllocatorType& allocator = rjd.GetAllocator();

	for (int ii = 0; ii < (int)tds->count; ii++) {
		idary.PushBack((int)tds->items[ii], allocator);
	}
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

int CmdActionTorrent::Process(TransmissionManager * mng)
{
	int rtn = 0;
	std::wstring req;
	ItemArray<int*> tids;
	int ii;

	tids.count = ids.size();
	if (tids.count > 0) {
		tids.items = new int[tids.count];
		ii = 0;
		for (std::set<int>::iterator itts = ids.begin();
			itts != ids.end();
			itts++) {
			tids.items[ii] = *itts;
			ii++;
		}
		buildDeleteRequest(&tids, true, req);
		delete[] tids.items;
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
		callback->Process(ids, rtn);
	}

	return rtn;
}
