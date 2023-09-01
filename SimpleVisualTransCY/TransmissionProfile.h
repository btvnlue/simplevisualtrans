#pragma once

#include <string>
#include <map>
#include <set>

#include "TorrentNode.h"

class INIFileTool;

enum TransmissionSessionState
{
	NOSTATE = 0,
	DONE_FILL,
	DONE_INITIALIZE,
	DONE_LOGIN,
};

enum ProfileUpdateTorrent
{
	UPN_NOUPDATE=0,
	UPN_UPDATE=1,
	UPN_NEWNODE=2,
	UPN_DELETE=3,
};

struct StatusStatistics
{
	unsigned long long upsize;
	unsigned long long downsize;
	int filesadd;
	int sessioncount;
	int sessionactive;
};
struct ProfileStatus
{
	int downspeed;
	int upspeed;
	int activecount;
	int totalcount;
	int pausecount;
	struct StatusStatistics total;
	struct StatusStatistics current;
};

class TransmissionProfile
{
	wchar_t dispbuf[4096];
public:
	TransmissionProfile();
	virtual ~TransmissionProfile();
	std::wstring name;
	std::wstring url;
	std::wstring username;
	std::wstring passwd;
	std::wstring token;
	TransmissionSessionState state;
	bool inrefresh;
	struct ProfileStatus stat;
	std::map<std::wstring, TrackerCY*> _trackers;

	std::map<long, TorrentNode*> torrents;
	ViewNode* totalview;
	ViewNode* profileview;
	std::map<TorrentNode*, std::set<ViewNode*>> tvmap;
	std::set<TrackerCY*> trackers;
	std::set<ViewNode*> updateviewnodes;
	std::set<TorrentNode*> updatetorrents;
	std::set<TorrentFileNode*> updatefiles;

	TorrentNode* GetTorrent(long tid);
	int AddTorrentTotalView(TorrentNode* trt);
	int GetTorrentViewNodes(long tid, std::set<ViewNode*>& vds);
	int GetTorrentViewNodes(TorrentNode* torrent, std::set<ViewNode*>& vds);
	int GetValidTorrentIds(std::set<long>& tids);
	int GetValidTorrents(std::set<TorrentNode*> tts);
	TrackerCY* GetTracker(std::wstring& tcklink);
	int UpdateTorrent(TorrentNode * torrent);
	int UpdateRawTorrent(int tid, RawTorrentValuePair* rawtt);
	int UpdateTorrentViewNodes(long tid);
	int UpdateTorrentViewNodes(TorrentNode* ttn);
	int MarkTorrentInvalid(long tid);
	//int UpdateProfileTrackers(TorrentNode* torrent);
	int UpdateTorrentTrackerViewNodes(ViewNode* vnd);
	int UpdateTorrentFileViewNodes(ViewNode* vnd);
	int UpdateStatusDispValues();
	bool HasViewNode(ViewNode* vnd);

	int copyRawFileStat(TorrentFileNode* file, RawTorrentValuePair * rawnode);
	int CopyRawTorrent(TorrentNode * ttn, RawTorrentValuePair * rawnode);
	TrackerCY * GetRawTracker(RawTorrentValuePair * rawnode);
	int copyRawFile(TorrentFileNode* root, TorrentFileNode* file, RawTorrentValuePair * rawnode);
	int copyRawTrackers(TorrentNode* ttn, RawTorrentValuePair * rawnode);


	wchar_t* dispupspeed = dispbuf;
	wchar_t* dispdownspeed = dispbuf + 20;
	wchar_t* disptorrentcount = dispbuf + 40;
	wchar_t* dispactivecount = dispbuf + 50;
	wchar_t* disppausecount = dispbuf + 60;
	wchar_t* disptotalupsize = dispbuf + 80;
	wchar_t* disptotaldownsize = dispbuf + 100;
	wchar_t* disptotalfiles = dispbuf + 120;
	wchar_t* disptotalsessions = dispbuf + 140;
	wchar_t* disptotalactive = dispbuf + 160;
	wchar_t* dispcurrentupsize = dispbuf + 180;
	wchar_t* dispcurrentdownsize = dispbuf + 200;
	wchar_t* dispcurrentfiles = dispbuf + 220;
	wchar_t* dispcurrentsessions = dispbuf + 240;
	wchar_t* dispcurrentactive = dispbuf + 260;
};

