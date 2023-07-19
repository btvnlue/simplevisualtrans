#pragma once

#include <Windows.h>
#include <string>
#include <list>
#include <set>

#include "TorrentNode.h"

class TransmissionProfile;
class TransmissionManager;
class CmdRefreshTorrents;
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

class CmdRefreshTorrentsCB
{
public:
	virtual int Process(ItemArray<TorrentNode*>* nodes, CmdRefreshTorrents *cmd) = 0;
};

#define CRT_E_TIMEOUT 5

class CmdRefreshTorrents : public ManagerCommand
{
	int RequestServiceRefresh(ItemArray<TorrentNode*>& wns);
public:
	int returncode;
	TransmissionProfile* profile = NULL;
	int Process(TransmissionManager* mng);
	CmdRefreshTorrentsCB* callback = NULL;
};

class CmdLoadProfile : public ManagerCommand
{
public:
	TransmissionProfile* profile = NULL;
	int Process(TransmissionManager* mng);
	ManagerCommand* callback;
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

class CmdDeleteTorrentsCB
{
public:
	virtual int Process(std::set<int>& ids, int code) = 0;
};

#define CATA_NONE 0
#define CATA_DELETE 1

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
	CmdDeleteTorrentsCB* callback = NULL;
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
	ItemArray<TorrentNode*> worktorrents;
	TransmissionManager();
	virtual ~TransmissionManager();

	int Start();
	int Stop();
	int PutCommand(ManagerCommand* cmd);
	ManagerCommand* GetCommand();

	int LoadDefaultProfile(TransmissionProfile* prof);
};

