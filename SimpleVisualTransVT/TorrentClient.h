#pragma once

#include "TaskEngine.h"
#include "CurlSessionVT.h"
#include "CProfileData.h"
#include <string>
#include <set>

struct TorrentNodeVT;

template <typename T> struct ItemArray {
	T items = NULL;
	unsigned long count = 0;
};

enum class TNF_TYPE {
	dir = 1,
	file = 2,
	none = 0
};

struct TorrentNodeFile {
	int id = 0;
	std::wstring name;
	std::wstring path;
	std::wstring fullpath;
	unsigned long long done = 0;
	unsigned long long size = 0;
	bool check = true;
	int priority = 0;
	unsigned long long updatetick = 0;
	ItemArray<struct TorrentNodeFile**> subfiles;
	TorrentNodeFile* parent;
	TNF_TYPE type;
	int totalcount = 0;
};

struct TrackerCY {
	std::wstring name;
	std::wstring url;
};

struct TorrentNodeVT
{
	unsigned long id;
	std::wstring name;
	unsigned long long size = 0;
	unsigned long long downloaded = 0;
	unsigned long long uploaded = 0;
	unsigned long long leftsize = 0;
	unsigned long long corrupt = 0;
	ItemArray<struct TrackerCY*> trackers;
	long downspeed = 0;
	long upspeed = 0;
	unsigned long long updatetick = 0;
	unsigned long long readtick = 0;
	long status = 0;
	long downloadTime = 0;
	long seedTime = 0;
	TorrentNodeFile path;
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
	unsigned long long desired = 0;
	ItemArray<unsigned char*> piecesdata;
	bool valid = true;
	ItemArray<struct TorrentNodeFile*> files;
};

class TorrentNodeHelper {
public:
	static int Delete(TorrentNodeVT* node);
};

class TorrentGroupVT
{
public:
	std::set<TorrentGroupVT*> subs;
	std::set<TorrentNodeVT*> nodes;
	std::wstring name;
	TorrentGroupVT* parent;
	unsigned long long size;
	long downspeed;
	long upspeed;
	long nodecount;
	long groupcount;
	unsigned long long updatetick = 0;

	virtual ~TorrentGroupVT();

	virtual int AddGroup(TorrentGroupVT* subgroup);
	virtual int AddTorrent(TorrentNodeVT* node);
	virtual int DeleteTorrents();
	int GetTorrentGroups(TorrentNodeVT* torrent, std::list<TorrentGroupVT*>& groups);
	TorrentNodeVT* GetIdTorrent(long tid);
	int GetNodes(std::set<TorrentNodeVT*>& nodes);
	int GetGroups(std::set<TorrentGroupVT*>& groups);
};

class TorrentTrackerGroup : public TorrentGroupVT
{
	std::map<std::wstring, TorrentGroupVT*> trackers;
public:
	TorrentGroupVT* GetTrackerGroup(const wchar_t* name);
	virtual int AddGroup(TorrentGroupVT* subgroup);
};

struct SessionInfo {
	long downspeed;
	long upspeed;
};

//class ITorrentClientCallback
//{
//public:
//	virtual int OnTorrentUpdate(TorrentNodeVT* node, TorrentGroupVT* group) = 0;
//};

struct TorrentClientRequest {
	enum class REQ {
		REFRESHTORRENTS
		, QUERYGROUPTORRENTS
		, QUERYGROUPGROUPS
		, QUERYTORRENTFILES
		, ADDNEWTORRENT
		, TORRENTFILEREQUEST
		, DELETETORRENT
		, PAUSETORRENT
		, SETLOCATION
		, VERIFYTORRENT
		, REFRESHSESSION
	} req;
	union {
		struct {
			bool recent;
			bool specid;
			long tid;
			bool reqfiles;
			bool reqfilestat;
			int(*cbgroupnode)(TorrentClientRequest* req, TorrentNodeVT* node, TorrentGroupVT* group);
			int(*cbgroupgroup)(TorrentClientRequest* req, TorrentGroupVT* group, TorrentGroupVT* pgroup);
			int(*cbremovenode)(TorrentClientRequest* req, TorrentNodeVT* node, TorrentGroupVT* group);
			int(*cbfailed)(TorrentClientRequest* req, std::wstring error, long ecode);
			int(*cbnodefile)(TorrentClientRequest* req, TorrentNodeVT* node);
			int(*cbnodefilestat)(TorrentClientRequest* req, TorrentNodeVT* node, TorrentNodeFile* fnode);
			HWND hwnd;
		} refresh;
		struct {
			TorrentGroupVT* group;
			HWND hwnd;
			int(*cbquery)(TorrentClientRequest* oreq, TorrentNodeVT* node);
		} getgroupnodes;
		struct {
			TorrentGroupVT* group;
			HWND hwnd;
			int(*cbquery)(TorrentClientRequest* oreq, TorrentGroupVT* sgroup);
		} getgroupgroups;
		struct {
			TorrentNodeVT* node;
			HWND hwnd;
			int(*cbnodefile)(TorrentClientRequest* req, TorrentNodeVT* cbnode);
		} getnodefiles;
		struct {
			HWND hwnd;
			WCHAR* torrentlink;
			int(*cbaddresult)(TorrentClientRequest* req, int rcode);
			WCHAR* result;
		} addtorrent;
		struct {
			long torrentid;
			TorrentNodeVT* node;
			long* fileids;
			long fileidscount;
			bool hascheck;
			bool ischeck;
			bool haspriority;
			int priority;
			WCHAR* result;
			HWND hwnd;
			int(*cbfileresult)(TorrentClientRequest* req, int rcode);
		} filerequest;
		struct {
			long torrentid;
			bool deletedata;
			TorrentNodeVT* node;
			WCHAR* result;
			HWND hwnd;
			int(*cbdelresult)(TorrentClientRequest* req, int rcode);
		} deletetorrent;
		struct {
			HWND hwnd;
			long torrentid;
			bool dopause;
			TorrentNodeVT* node;
			WCHAR* result;
			int(*cbpauseresult)(TorrentClientRequest* req, int rcode);
		} pausetorrent;
		struct {
			HWND hwnd;
			long torrentid;
			TorrentNodeVT* node;
			WCHAR* result;
			int(*cbverifyresult)(TorrentClientRequest* req, int rcode);
		} verifytorrent;
		struct {
			HWND hwnd;
			long torrentid;
			WCHAR* location;
			WCHAR* result;
			TorrentNodeVT* node;
			int(*cblocationresult)(TorrentClientRequest* req, int rcode);
		} location;
		struct {
			HWND hwnd;
			int(*cbsessionresult)(TorrentClientRequest* req, SessionInfo* session);
		} session;
	};
	bool done = false;
	unsigned long sequence;
};

class TorrentClient : public TaskEngine
{
	CurlSessionVT* curl;
	CurlProfile* profile;
	CurlProfile* localprofile;
	SessionInfo session;
	std::set<std::wstring> refreshtorrentfields;
	TorrentGroupVT* grouproot;
	TorrentGroupVT* allnodes;
	TorrentTrackerGroup* trackers;
	int QueryGroupTorrents(TorrentClientRequest* req);
	int QueryGroupGroups(TorrentClientRequest* req);
	int QueryNodeFiles(TorrentClientRequest* req);
public:
	CProfileData* profiledataloader;

	class RefreshTorrentTask : public TaskItem
	{
		TorrentClient* client;
		TorrentClientRequest* request;
	public:
		RefreshTorrentTask(TorrentClient* clt, TorrentClientRequest* req);
		virtual int Process(TaskEngine* eng);
	};
	class GeneralRequestTask : public TaskItem
	{
		TorrentClient* client;
		TorrentClientRequest* request;
	public:
		GeneralRequestTask(TorrentClient* clt, TorrentClientRequest* req);
		virtual int Process(TaskEngine* eng);
	};
	int LoadProfileSite();

	//class ITorrentClientCallback
	//{
	//public:
	//	virtual int UpdateTorrent(TorrentNodeVT* torrent, TorrentGroupVT* group) = 0;
	//	virtual int UpdateRemoveTorrent(TorrentNodeVT* torrent) = 0;
	//}* callback;
	TorrentClient();
	virtual ~TorrentClient();
	int RefreshTorrents(TorrentClientRequest* req);
	int UpdateTorrentGroups(TorrentGroupVT* group);
	int UpdateTrackerGroups(TorrentNodeVT* torrent);
	int RequestTaskProc(TorrentClientRequest* req);
	int analysisSession(std::wstring & rss);
	int AddNewTorrent(TorrentClientRequest* req);
	int RequestTorrentFiles(TorrentClientRequest* req);
	int DeleteTorrent(TorrentClientRequest* req);
	int PauseTorrent(TorrentClientRequest* req);
	int VerifyTorrent(TorrentClientRequest * req);
	int RefreshSession(TorrentClientRequest * req);
	int SetTorrentLocation(TorrentClientRequest * req);

	std::wstring GetErrorString(int eid);
};

