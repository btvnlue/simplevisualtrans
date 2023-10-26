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
	int downlimit;
	int uplimit;
	bool downlimitenable;
	bool uplimitenable;
	std::wstring version;
	int rpc;
	std::wstring session;
};

typedef std::set<ViewNode*> ViewSet;
typedef std::map<TorrentFileNode *, ViewNode *> FileViewMap;
typedef std::map<TorrentNode *, ViewSet> TorrentViewMap;
typedef std::map<TorrentFileNode *, ViewSet> PathViewMap;
typedef std::map<std::wstring, long> TrackerGroupMap;
typedef std::map<long, TrackerCY*> TrackerMap;
typedef std::map<long, TorrentNode*> TorrentMap;

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
	TrackerMap _trackers;
	TrackerGroupMap trackergroupmap;

	TorrentMap torrents;
	ViewNode* totalview;
	ViewNode* profileview;
	
	TorrentViewMap tvmap;
	FileViewMap fvmap;
	PathViewMap pvmap;

	std::set<TrackerCY*> trackers;
	ViewSet updateviewnodes;
	std::set<TorrentNode*> updatetorrents;
	std::set<TorrentFileNode*> updatefiles;

	TorrentNode* GetTorrent(long tid);
	int AddTorrentTotalView(TorrentNode* trt);
	int GetTorrentViewNodes(TorrentNode* torrent, std::set<ViewNode*>& vds);
	ViewNode* GetFileViewNode(TorrentFileNode* file);
	int GetValidTorrentIds(std::set<long>& tids);
	int GetValidTorrents(std::set<TorrentNode*> tts);
	int UpdateRawTorrent(int tid, RawTorrentValuePair* rawtt);
	int UpdateTorrentView(TorrentNode* ttn);
	int MarkTorrentInvalid(long tid);
	int UpdateTorrentTrackerViewNodes(ViewNode* vnd);
	int UpdateTorrentFileView(TorrentFileNode* vnd);
	int UpdateStatusDispValues();
	bool HasViewNode(ViewNode* vnd);
	TrackerCY * GetRawTracker(RawTorrentValuePair * rawnode);

	int CopyRawTorrentData(TorrentNode * ttn, RawTorrentValuePair * rawnode);
	int copyRawTorrent(TorrentNode * ttn, RawTorrentValuePair * rawnode);
	int copyRawFileStat(TorrentFileNode* file, RawTorrentValuePair * rawnode);
	int copyRawFile(TorrentFileNode* root, TorrentFileNode* file, RawTorrentValuePair * rawnode);
	int copyRawTrackers(TorrentNode* ttn, RawTorrentValuePair * rawnode);
	int copyRawPeers(int tt, TorrentNode * ttn, RawTorrentValuePair * rawnode);
	int copyRawSessionInfo(RawTorrentValuePair * rawnode, int level);

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
	wchar_t* dispdownlimit = dispbuf + 275;
	wchar_t* dispuplimit = dispbuf + 290;
	wchar_t* dispupenable = dispbuf + 305;
	wchar_t* dispdownenable = dispbuf + 310;
	wchar_t* disprpc = dispbuf + 315;
};

