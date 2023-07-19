#pragma once

#include <string>
#include <list>
#include <set>
#include <vector>

template <typename T> struct ItemArray {
	T items = NULL;
	unsigned long count = 0;
};

struct TrackerCY {
	int id = -1;
	std::wstring name;
	std::wstring url;
};

//#define TTSTATE_NAME 0x0001
#define TTSTATE_BASEINFO 0x0002
#define TTSTATE_TRANINFO 0x0004
#define TTSTATE_SPEED 0x0008

#define TTS_PAUSED 0
#define TTS_PENDING 1
#define TTS_VERIFY 2
#define TTS_QUEUED 3
#define TTS_DOWNLOADING 4
#define TTS_SEEDING 6

class TorrentNode
{
	int ClearState();
	int SetState(unsigned long sst);
	wchar_t charbuf[2048];
	wchar_t *datbuf = charbuf;
	wchar_t *dispbuf = charbuf + 1024;
public:
	TorrentNode();
	virtual ~TorrentNode();

	unsigned long state;
	unsigned long id;
	std::wstring name;
	unsigned long long size = 0;
	unsigned long long downloaded = 0;
	unsigned long long uploaded = 0;
	unsigned long long leftsize = 0;
	unsigned long long corrupt = 0;
	ItemArray<struct TrackerCY*> rawtrackers;
	ItemArray<struct TrackerCY**> _trackers;
	long downspeed = 0;
	long upspeed = 0;
	unsigned long long updatetick = 0;
	unsigned long long readtick = 0;
	long status = 0;
	long downloadTime = 0;
	long seedTime = 0;
	//TorrentNodeFile path;
	wchar_t * _path = datbuf + 0;
	wchar_t * _error = datbuf + 256;
	bool errorflag = false;
	double ratio = 0;
	bool privacy = false;
	bool done = false;
	bool stalled = false;
	long donedate = 0;
	long activedate = 0;
	long adddate = 0;
	long startdate = 0;
	unsigned long piececount = 0;
	long piecesize = 0;
	std::wstring pieces;
	unsigned long long desired = 0;
	ItemArray<unsigned char*> piecesdata;
	bool valid = true;
	ItemArray<struct TorrentNodeFile*> files;

	wchar_t *dispid = dispbuf;
	wchar_t *dispsize = dispbuf + 10;
	wchar_t *dispdownloaded = dispbuf + 30;
	wchar_t *dispuploaded = dispbuf + 50;
	wchar_t *displeft = dispbuf + 70;
	wchar_t *dispdownspeed = dispbuf + 105;
	wchar_t *dispupspeed = dispbuf + 120;
	wchar_t *dispstatus = dispbuf + 135;
	wchar_t *dispratio = dispbuf + 155;
	wchar_t *dispdonedate = dispbuf + 185;
	wchar_t *dispadddate = dispbuf + 215;
	wchar_t *dispstartdate = dispbuf + 245;
	wchar_t *disppiececount = dispbuf + 275;
	wchar_t *disppiecesize = dispbuf + 295;
	wchar_t *dispavialable = dispbuf + 315;

	int Copy(TorrentNode* node);
	int DispRefresh();
	int CleanTrackers();
};

#define VNT_GROUP 0
#define VNT_TORRENT 1
#define VNT_CLIPBOARD 2
#define VNT_CLIPBOARD_ITEM 3
#define VNT_PROFILE 4
#define VNT_TRACKERS 5
#define VNT_TRACKER 6

class ViewNode
{
	static long vnodeidx;
	ViewNode();
	wchar_t dispbuf[1024];
	TorrentNode* torrent;
	TrackerCY* tracker;
public:
	virtual ~ViewNode();

	static ViewNode* NewViewNode(int vnt);
	ViewNode* parent;
	std::list<ViewNode*> nodes;
	std::wstring name;
	long id;
	int type;
	bool valid;

	int AddNode(ViewNode* node);
	bool HasTorrent(TorrentNode* trt);
	bool HasViewNode(ViewNode* vnd);
	int GetAllTorrentsViewNode(std::set<ViewNode*>& tts);
	int GetType();
	int SetTorrent(TorrentNode* trt);
	int SetTracker(TrackerCY* tkr);
	TorrentNode* GetTorrent();
	ViewNode* GetTrackerGroup();
	ViewNode* GetTracker(int id);
};