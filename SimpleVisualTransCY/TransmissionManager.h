#pragma once

#include <Windows.h>
#include <string>
#include <list>
#include <set>

#include "TorrentNode.h"

class TransmissionProfile;
class TransmissionManager;
class CmdFullRefreshTorrents;
class CmdCommitTorrent;

class ManagerCommand
{
public:
	virtual ~ManagerCommand(){}
	std::wstring name;
	int code = 0;
	DWORD timetorun = 0;
	virtual int Process(TransmissionManager* mng) = 0;
};

class CmdFullRefreshTorrents;
class CmdFullRefreshTorrentsCB
{
public:
	//virtual int Process(ItemArray<TorrentNode*>* nodes, CmdFullRefreshTorrents *cmd) = 0;
	virtual int ProcessRawObjects(RawTorrentValuePair* node, CmdFullRefreshTorrents *cmd, bool scaninvalid) = 0;
};

#define CRT_E_TIMEOUT 5

class CmdFullRefreshTorrents : public ManagerCommand
{
	int RequestServiceRefresh(ItemArray<TorrentNode*>& wns);
	int RequestServiceRefreshReduced();
public:
	int returncode;
	std::set<long> torrentids;
	bool dofullyrefresh = true;
	TransmissionProfile* profile = NULL;
	int Process(TransmissionManager* mng);
	CmdFullRefreshTorrentsCB* callback = NULL;
};

//class CmdRefreshTorrentsDetail;
//class CmdRefreshTorrentsDetailCB
//{
//public:
//	virtual int Process(ItemArray<TorrentNode*>* nodes, CmdRefreshTorrentsDetail *cmd) = 0;
//};

//class CmdRefreshTorrentsDetail : public ManagerCommand
//{
//	int RequestTorrentDetailRefresh(std::set<int>& tss, ItemArray<TorrentNode*>& tws);
//public:
//	int returncode;
//	std::set<int> ids;
//	TransmissionProfile* profile = NULL;
//	int Process(TransmissionManager* mng);
//	CmdFullRefreshTorrentsCB* callback = NULL;
//};

class CmdSetLocation;
class CmdSetLocationCB
{
public:
	virtual int Process(CmdSetLocation* cmd) = 0;
};

class CmdSetLocation : public ManagerCommand
{
public:
	int returncode;
	std::set<int> idset;
	std::wstring location;
	TransmissionProfile* profile = NULL;
	int Process(TransmissionManager* mgr);
	CmdSetLocationCB* callback = NULL;
};

class CmdLoadProfile;
class CmdLoadProfileCB
{
public:
	virtual int Process(TransmissionManager* mgr, CmdLoadProfile* cmd) = 0;
};

class CmdLoadProfile : public ManagerCommand
{
public:
	TransmissionProfile* profile = NULL;
	int Process(TransmissionManager* mng);
	CmdLoadProfileCB* callback;
};

class CBCmdRefreshSession;
class CmdRefreshSession : public ManagerCommand
{
public:
	TransmissionProfile* profile = NULL;
	int Process(TransmissionManager* mng);
	CBCmdRefreshSession* callback = NULL;
};
class CBCmdRefreshSession
{
public:
	virtual int Process(TransmissionManager* mng, CmdRefreshSession* cmd) = 0;
};

class CmdCommitTorrentsCB
{
public:
	virtual int Process(CmdCommitTorrent* cmd, int code) = 0;
};

class CmdCommitTorrent : public ManagerCommand
{
public:
	std::wstring entry;
	TransmissionProfile* profile;
	int Process(TransmissionManager* mng);
	CmdCommitTorrentsCB* callback = NULL;
};

class CmdActionTorrent;
class CmdActionTorrentsCB
{
public:
	virtual int Process(CmdActionTorrent* cmd, std::set<int>& ids, int code) = 0;
};

#define CATA_NONE 0
#define CATA_DELETE 1
#define CATA_PAUSE 2
#define CATA_UNPAUSE 3
#define CATA_VERIFY 4

#define CATA_PARAM_DELETEFILES 1
#define CATA_PARAM_NODELETEFILES 0

class CmdActionTorrent : public ManagerCommand
{
public:
	std::set<int> ids;
	int action = CATA_NONE;
	int actionparam;
	TransmissionProfile* profile;
	int Process(TransmissionManager* mng);
	CmdActionTorrentsCB* callback = NULL;
};

class BufferAllocator
{
	long totalsize;
	long currpos;
	long currindex;
	long currsize;
	unsigned char** buffer;
	long index;

	int _alloccurrindex(long cidx, long csz);
public:
	BufferAllocator();
	virtual ~BufferAllocator();
	unsigned char* Allcate(long ssz);
	int FreeAll();
	int Reset();
};

class TransmissionManager
{
	static DWORD WINAPI ThManagerProc(LPVOID lpParam);
	DWORD tid;
	HANDLE hTh;
	BOOL keeprun;
	std::list<ManagerCommand*> cmds;
	HANDLE hEvent;
public:
	//ItemArray<TorrentNode*> worktorrents;
	RawTorrentValues rawtorrent;

	TransmissionManager();
	virtual ~TransmissionManager();

	int Start();
	int Stop();
	int PutCommand(ManagerCommand* cmd);
	ManagerCommand* GetCommand();
};
