#pragma once

#include <string>
#include <map>
#include <list>
#include <set>

#include <rapidjson/document.h>
#include <rapidjson/encodings.h>
#include <rapidjson/writer.h>

class CurlSession;

struct TorrentTracker
{
	std::wstring announce;
	unsigned long id;
};

struct TorrentNode;

struct SessionInfo
{
	long torrentcount = 0;
	long downloadspeed = 0;
	long uploadspeed = 0;
	unsigned long long downloaded = 0;
	unsigned long long uploaded = 0;
};

struct TorrentNodeFile {
	std::wstring name;
	unsigned long long size;
	unsigned long long donesize;
	bool check;
	int priority;
};

struct TorrentNodeFileNode {
	int id;
	std::wstring name;
	std::wstring path;
	unsigned long long done;
	TorrentNode* node;
	TorrentNodeFile* file;
	TorrentNodeFileNode* parent;
	bool check;
	int priority = 0;
	std::set<TorrentNodeFileNode*> sub;
	enum NODETYPE {
		FILE
		, DIR
	} type;
	unsigned long readtick;
	unsigned long updatetick;
};

class TorrentNodeHelper
{
public:
	static std::wstring DumpTorrentNode(TorrentNode* node);
	static int ClearTorrentNodeFile(struct TorrentNodeFile& node);
	static int DeleteTorrentNode(TorrentNode* node);
	static TorrentNodeFileNode* GetPathNode(TorrentNodeFileNode* node, std::wstring path);
	static int GetNodeFileSet(TorrentNodeFileNode* file, std::set<TorrentNodeFileNode*>& fileset);
	static std::wstring GetFileNodePath(TorrentNodeFileNode* file);
};


struct TorrentNode
{
	unsigned long id;
	std::wstring name;
	unsigned long long size = 0;
	unsigned long long downloaded = 0;
	unsigned long long uploaded = 0;
	unsigned long long leftsize = 0;
	unsigned long long corrupt = 0;
	//TODO: allocate trackers
	std::wstring trackers[128];
	unsigned long trackerscount = 0;
	long downspeed = 0;
	long upspeed = 0;
	unsigned long updatetick = 0;
	unsigned long readtick = 0;
	//unsigned long treereadtick = 0;
	long status = 0;
	long downloadTime = 0;
	long seedTime = 0;
	std::wstring path;
	std::wstring error;
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
	long piecesdatasize = 0;
	unsigned long long desired = 0;
	unsigned char* piecesdata = NULL;
	struct {
		int count = 0;
		struct TorrentNodeFile* file = NULL;
		TorrentNodeFileNode* node = NULL;
	} files;
};

class TorrentGroup
{
public:
	std::map<unsigned long, TorrentNode*> torrents;
	std::map<std::wstring, TorrentGroup*> subs;
	std::wstring name;
	TorrentGroup* parent;
	unsigned long long size;
	unsigned long updatetick;
	unsigned long readtick;
	long downspeed;
	long upspeed;

	TorrentGroup();
	virtual ~TorrentGroup();
	int addTorrents(TorrentNode* trt);
	int addGroup(TorrentGroup* grp);
	int updateSize();
	int GetNodes(std::set<TorrentNode*>& nds);
	TorrentNode* getTorrent(unsigned long tid);
	TorrentGroup* getGroup(const std::wstring& gname);
	int removeTorrent(TorrentNode* trt);
};

struct ReqParm {
	long torrentid;
	bool reqfiles = false;
	bool reqfilestat = false;
	bool reqpieces = false;
};

struct FileReqParm {
	long torrentid;
	std::set<long> fileIds;
	bool setcheck;
	bool check;
	bool setpriority;
	int priority;
};

class TransAnalyzer
{
	CurlSession* curlSession;
	int cleanupTorrents();
	TorrentGroup* fulllist;
	TorrentGroup* trackertorrents;
	std::set<std::wstring> refreshtorrentfields;
public:
	TransAnalyzer();
	int setupRefreshFields(std::set<std::wstring>& fst);
	virtual ~TransAnalyzer();
	int buildStopRequest(std::wstring& req, int tid, bool dopause);
	int buildFileRequest(std::wstring& req, FileReqParm& frp);
	int buildDeleteRequest(std::wstring& req, int rid, bool rld);
	std::wstring analysisRemoteResult(std::wstring& rss);
	int PerformRemoteDelete(int tid, bool bcont);
	int PerformRemoteStop(int tid, bool dopause);
	int PerformRemoteFileCheck(FileReqParm& frp);
	std::wstring GetErrorString(int eid);
	TorrentNode* GetTorrentNodeById(unsigned long tid);
	TorrentGroup* GetTrackerGroup(const std::wstring& tracker);
	std::wstring hosturl;
	std::wstring username;
	std::wstring password;
	std::map<std::wstring, TorrentTracker*> trackers;
	std::list<int> removed;
	TorrentGroup* groupRoot;
	SessionInfo session;

	int RemoveTorrent(unsigned long tid);
	int buildRefreshRequestTorrent(std::wstring& req, ReqParm parm);
	int PerformRemoteRefreshTorrent(ReqParm rpm);
	int buildSessionStat(std::wstring& req);
	int PerformRemoteSessionStat();
	int PerformRemoteRefresh();

	int StartClient(const wchar_t* url, const wchar_t* user, const wchar_t* passwd);

	int extractSessionStat(std::wstring& jsonstr);

	int buildGetAllRequest(std::wstring& req);
	int extractTorrentsGet(std::wstring& jsonstr);
	int buildRefreshRequest(std::wstring& req);
	int buildAddTorrentRequest(const std::wstring& turl, bool ismedia, std::wstring& req);
	int convertPiecesData(TorrentNode* node);
	void extractTorrentObject(rapidjson::GenericValue<rapidjson::UTF16<>>& trtn);
	int extractFile(rapidjson::GenericValue<rapidjson::UTF16<>>& trfa, TorrentNodeFile* file);
	void extractTracker(rapidjson::GenericValue<rapidjson::UTF16<>>& trka, TorrentNode* torrent);
	int groupTorrentTrackers();
	std::wstring getTrackerName(const std::wstring& tck);
	std::wstring PerformRemoteAddTorrent(const std::wstring turl);
};

