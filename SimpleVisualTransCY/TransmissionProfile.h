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
	UPN_NEWNODE=2
};

class TransmissionProfile
{
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

	std::map<long, TorrentNode*> torrents;
	ViewNode* totalview;
	ViewNode* profileview;
	std::map<TorrentNode*, std::set<ViewNode*>> tvmap;
	std::set<TrackerCY*> trackers;

	int GetTorrentViewNodes(long tid, std::set<ViewNode*>& vds);
	int GetTorrentViewNodes(TorrentNode* torrent, std::set<ViewNode*>& vds);
	TrackerCY* GetTracker(std::wstring& tcklink);
	int UpdateTorrent(TorrentNode * torrent);
	int UpdateTorrentViewNodes(long tid);
	int UpdateViewNodeInvalid(long tid);
	int UpdateProfileTrackers(TorrentNode* torrent);
	int UpdateTorrentTrackerViewNodes(ViewNode* vnd);
};

