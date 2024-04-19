#pragma once

#include <Windows.h>
#include <set>
#include <vector>

#include "TorrentClient.h"

int FormatViewPercentage(wchar_t* wbuf, size_t bsz, double pval);
int FormatViewDouble(wchar_t* wbuf, size_t bsz, double dval);
int FormatNumberView(wchar_t* buf, size_t bufsz, unsigned __int64 size);
int FormatViewStatus(wchar_t* wbuf, size_t bsz, long status);
int FormatNumberByteView(wchar_t* buf, size_t bufsz, unsigned __int64 size);
int FormatTimeSeconds(wchar_t* wbuf, size_t bsz, long secs);
int FormatDualByteView(wchar_t* wbuf, int bsz, unsigned __int64 size);
int FormatIntegerDate(wchar_t* wbuf, size_t bsz, long mtt);
int FormatNormalNumber(wchar_t* buf, size_t bufsz, long num);

enum class VG_TYPE {
	TORRENT
	, GROUP
	, GROUPGROUP
	, ATTRIBUTE
	, SESSION
	, FILE
};

class ViewNode
{
public:
	VG_TYPE type;
	union {
		struct {
			TorrentNodeVT* node;
			ViewNode* sizenode;
			ViewNode* idnode;
			ViewNode* filenode;
			TorrentNodeFile* file;
			WCHAR* viewnodeid;
			WCHAR* viewnodename;
			WCHAR* viewnodesize;
			WCHAR* viewnodevsize;
			WCHAR* viewnodetracker;
			WCHAR* viewnodedownload;
			WCHAR* viewnodeupload;
			WCHAR* viewnodestatus;
			WCHAR* viewnoderation;
			WCHAR* viewnodelocation;
		} torrent;
		struct {
			TorrentGroupVT* groupnode;
			WCHAR* viewgroupname;
			WCHAR* viewgroupsize;
			WCHAR* viewgroupvsize;
			WCHAR* viewgroupcount;
			WCHAR* viewgroupdownload;
			WCHAR* viewgroupup;
		} group;
	};
	std::set<ViewNode*> subnodes;
	ViewNode* parent;
	std::wstring name;
	WCHAR* viewcontent;

	ViewNode(VG_TYPE typ);
	virtual ~ViewNode();
	int AddNode_(ViewNode* node);
	int RemoveNode(ViewNode* vnode);
	ViewNode* GetViewNode(TorrentNodeVT* node);
	ViewNode * AddFileViewNode(TorrentNodeFile * fnode);
	int GetFileNodes(std::vector<TorrentNodeFile*>& vnlist);
	int GetTorrentNodes(std::set<TorrentNodeVT*>& tnodes);
	int GetViewNodeSet(TorrentNodeVT* seeknode, std::set<ViewNode*>& vnodes);
	int GetGroupNodes(std::set<ViewNode*>& groups);
	int GetTorrentViewNodes(std::set<ViewNode*>& torrents);
	TorrentNodeVT* GetTorrentNode();
	int FormatGroupTextContent();
	int FormatTorrentTextContent();
};
