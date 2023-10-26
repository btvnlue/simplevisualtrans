#pragma once

#include <string>
#include <list>
#include <set>
#include <vector>
#include <map>

template <typename T> struct ItemArray {
	T items = NULL;
	unsigned long count = 0;
};

#define RTT_OBJECT 1
#define RTT_ARRAY 2
#define RTT_STRING 3
#define RTT_DOUBLE 4
#define RTT_INT64 5
#define RTT_INT 6
#define RTT_BOOL 7

struct RawTorrentValuePair;
typedef ItemArray<RawTorrentValuePair*> RawTorrentValues;

struct RawTorrentValuePair
{
	RawTorrentValuePair() {};
	wchar_t* key = nullptr;
	int valuetype;
	union {
		wchar_t* vvstr;
		RawTorrentValues vvobject;
		double vvdouble;
		__int64 vvint64;
		int vvint;
		bool vvbool;
	};
};

struct TrackerCY {
	int groupid;
	std::wstring name;
	std::wstring url;
	int rawid;
};

class TorrentNode;

struct TorrentFileNode {
	static wchar_t pathch;
	int id;
	long pathid = -1;
	std::wstring name;
	TorrentNode* pnode = NULL;
	TorrentFileNode* parent = NULL;
	std::map<long, TorrentFileNode*> paths;
	std::map<long, TorrentFileNode*> files;
	unsigned long long size = 0;
	long count = 0;
	long pathcount = 0;
	wchar_t dispbuf[1024] = { 0 };
	bool wanted = true;
	int priority = 0;
	unsigned long long completed = 0;
	
	wchar_t* disppath = dispbuf;
	wchar_t* dispname = dispbuf + 384;
	wchar_t* dispsize = dispbuf + 512;
	wchar_t* dispid = dispbuf + 542;
	wchar_t* dispbkmg = dispbuf + 555;
	wchar_t* disphas = dispbuf + 590;
	wchar_t* disppr = dispbuf + 600;

	int Add(TorrentFileNode * fnd);
	TorrentFileNode * GetPathNode(wchar_t * path);
	int Path(wchar_t* pbuf);
	int UpdateDisp();
	int UpdateStat();
	bool IsPath() {
		return count >= 0;
	}
	TorrentFileNode* GetFile(long fid);
	int GetFiles(std::set<TorrentFileNode*>& files);
	long GetPathId();
	TorrentNode* GetTorrent();
};

typedef std::set<TorrentFileNode*> TorrentFileSet;

struct TorrentPeerNode
{
	std::wstring address;
	std::wstring name;
	long upspeed;
	long downspeed;
	double progress;
	long port;
	bool valid = false;

	wchar_t dispbuf[1024] = { 0 };
	wchar_t * dispup = dispbuf;
	wchar_t * dispdown = dispbuf + 15;
	wchar_t * dispprogress = dispbuf + 30;
	wchar_t * dispport = dispbuf + 40;
	int UpdateDisp();
};

#define TTSTATE_NONE 0x0000
//#define TTSTATE_NAME 0x0001
#define TTSTATE_BASEINFO 0x0002
#define TTSTATE_TRANINFO 0x0004
#define TTSTATE_SPEED 0x0008
//#define TTSTATE_EXIST 0x0010

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
	wchar_t charbuf[4096];
	wchar_t *datbuf = charbuf;
	wchar_t *dispbuf = charbuf + 1024;
	unsigned char * _disppieces = (unsigned char *)(charbuf + 2048);
public:
	TorrentNode();
	virtual ~TorrentNode();

	unsigned long state;
	bool exist;
	unsigned long id;
	std::wstring name;
	unsigned long long size = 0;
	unsigned long long downloaded = 0;
	unsigned long long uploaded = 0;
	unsigned long long leftsize = 0;
	unsigned long long corrupt = 0;
	std::set<TrackerCY*> __trackers;
	long downspeed = 0;
	long upspeed = 0;
	long downloadlimit = 0;
	long uploadlimit = 0;
	bool downloadlimited = false;
	bool uploadlimited = false;
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
	double recheck = 0;
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
	std::wstring magnet;
	TorrentFileNode* files = nullptr;
	std::vector<TorrentPeerNode*> peers;

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
	wchar_t *dispdownlimit = dispbuf + 335;
	wchar_t *dispuplimit = dispbuf + 355;
	wchar_t *disptracker = dispbuf + 375;

	//int Copy(TorrentNode* node);
	//int CopyRaw(RawTorrentValuePair* rawnode);
	//int copyRawTrackers(RawTorrentValuePair * rawnode);
	//int copyRawFile(long fid, RawTorrentValuePair * rawnode);
	//int copyRawFileStat(TorrentFileNode* file, RawTorrentValuePair * rawnode);
	//TrackerCY* GetRawTracker(RawTorrentValuePair * rawnode);


	int DispRefresh();
	int CleanTrackers();
};

enum _ViewNodeType {
	VNT_GROUP = 0,
	VNT_TORRENT = 1,
	VNT_CLIPBOARD = 2,
	VNT_CLIPBOARD_ITEM = 3,
	VNT_PROFILE = 4,
	VNT_TRACKERS = 5,
	VNT_TRACKER = 6,
	VNT_FILEPATH = 7,
	VNT_FILE = 8,
};

typedef enum _ViewNodeType ViewNodeType;

int FormatNumberView(wchar_t* buf, size_t bufsz, unsigned __int64 size);
int FormatNumberBKMG(wchar_t* buf, size_t bufsz, unsigned __int64 size);
int FormatViewStatus(wchar_t* wbuf, size_t bsz, long status);
int FormatViewStatus(wchar_t* wbuf, size_t bsz, long status);
int FormatDoublePercentage(wchar_t* wbuf, size_t bsz, double pval);
int FormatIntegerDate(wchar_t* wbuf, size_t bsz, long mtt);

class ViewNode
{
	static long vnodeidx;
	ViewNode();
	wchar_t dispbuf[1024];
	TorrentNode* torrent;
	TrackerCY* tracker;
public:
	virtual ~ViewNode();

	static ViewNode* NewViewNode(ViewNodeType vnt);
	ViewNode* parent;
	std::list<ViewNode*> nodes;
	TorrentFileNode* file;
	std::wstring name;
	long id;
	ViewNodeType type;
	bool valid;

	int AddNode(ViewNode* node);
	bool HasTorrent(TorrentNode* trt);
	bool HasViewNode(ViewNode* vnd);
	int GetAllTorrentsViewNode(std::set<ViewNode*>& tts);
	//int GetAllFileViewNode(std::set<ViewNode*>& fns);
	int GetAllTypeNode(ViewNodeType vnt, std::set<ViewNode*>& vns);
	ViewNodeType GetType();
	int SetTorrent(TorrentNode* trt);
	int SetTracker(TrackerCY* tkr);
	TorrentNode* GetTorrent();
	ViewNode* GetTrackerGroup();
	ViewNode* GetTracker(int id);
	ViewNode* GetFilesNode();
	ViewNode* GetRoot();
};

typedef std::set<ViewNode*> ViewNodeSet;