#include "ViewCommons.h"

#include <list>
#include <algorithm>
#include <strsafe.h>

WCHAR wbuf[4096];

int FormatViewPercentage(wchar_t* wbuf, size_t bsz, double pval)
{
	int rtn = 0;

	long lft = (long)((pval - (int)pval) * 100);
	rtn = wsprintf(wbuf, L"%d.%02d %%", (long)pval, lft);
	return rtn;
}

int FormatViewDouble(wchar_t* wbuf, size_t bsz, double dval)
{
	int rtn = 0;

	int ivv = (int)dval;
	int ipp = (int)((dval - ivv) * 100);
	rtn = wsprintf(wbuf, L"%d.%02d", ivv, ipp);
	return rtn;
}

int FormatNumberView(wchar_t* buf, size_t bufsz, unsigned __int64 size)
{
	unsigned int ccn;
	wchar_t tbf[128];
	int rtn = 0;

	buf[0] = 0;
	tbf[0] = 0;
	if (size > 0) {
		while (size >= 1000) {
			ccn = size % 1000;
			wsprintf(tbf, L",%03d%s", ccn, buf);
			rtn = wsprintf(buf, L"%s", tbf);
			size /= 1000;
		}
		if (size > 0) {
			ccn = size % 1000;
			rtn = wsprintf(buf, L"%d%s", ccn, tbf);
		}
	}
	else {
		rtn = wsprintf(buf, L"0");
	}
	return rtn;
}

int FormatViewStatus(wchar_t* wbuf, size_t bsz, long status)
{
	int rtn = 0;

	switch (status) {
	case 4:
		rtn = wsprintf(wbuf, L"Downloading");
		break;
	case 6:
		rtn = wsprintf(wbuf, L"Seeding");
		break;
	case 0:
		rtn = wsprintf(wbuf, L"Paused");
		break;
	case 2:
		rtn = wsprintf(wbuf, L"Verify");
		break;
	case 1:
		rtn = wsprintf(wbuf, L"Pending");
		break;
	default:
		rtn = wsprintf(wbuf, L"Unknown");
	}
	return rtn;
}

int FormatNumberByteView(wchar_t* buf, size_t bufsz, unsigned __int64 size)
{
	unsigned int rst = size & 0x3FF;
	wchar_t smark[] = L"BKMGTPEZY";
	int smarkp = 0;
	int rtn = 0;

	if (size > 768) {
		if (size < (1 << 10)) {
			size = 0;
		}
		while (size > ((1 << 10) - 1)) {
			smarkp++;
			rst = size & 0x3FF;
			size >>= 10;
		}

		rst = (rst * 1000) >> 10;
		rtn = wsprintf(buf, L"%d.%03d %c", (unsigned int)size, rst, smark[smarkp]);
	}
	else {
		rtn = wsprintf(buf, L"%d %c", rst, smark[smarkp]);
	}
	return rtn;
}

int FormatTimeSeconds(wchar_t* wbuf, size_t bsz, long secs)
{
	int rtn = 0;

	int tss = secs % 60;
	secs /= 60;
	int tmm = secs % 60;
	secs /= 60;
	int thh = secs % 24;
	secs /= 24;

	if (secs > 0) {
		rtn = wsprintf(wbuf, L"%d d %02d:%02d:%02d", secs, thh, tmm, tss);
	}
	else {
		rtn = wsprintf(wbuf, L"%02d:%02d:%02d", thh, tmm, tss);
	}
	return rtn;
}

int FormatDualByteView(wchar_t* wbuf, int bsz, unsigned __int64 size)
{
	int rtn = 0;

	wchar_t dbuf[1024];
	FormatNumberByteView(dbuf, 1024, size);
	FormatNumberView(dbuf + 512, 512, size);
	rtn = wsprintf(wbuf, L"%s (%s)", dbuf + 512, dbuf);
	return rtn;
}

int FormatIntegerDate(wchar_t* wbuf, size_t bsz, long mtt)
{
	int rtn = 0;

	if (mtt > 0) {
		tm ttm;
		//_gmtime32_s(&ttm, &mtt);
		_localtime32_s(&ttm, &mtt);
		rtn = wsprintf(wbuf, L"%4d-%02d-%02d %02d:%02d:%02d", ttm.tm_year + 1900, ttm.tm_mon + 1, ttm.tm_mday, ttm.tm_hour, ttm.tm_min, ttm.tm_sec);
	}
	else {
		rtn = wsprintf(wbuf, L"");
	}
	return rtn;
}

int FormatNormalNumber(wchar_t* buf, size_t bufsz, long num)
{
	int rtn = 0;

	rtn = wsprintf(buf, L"%d", num);
	return rtn;
}

ViewNode::ViewNode(VG_TYPE typ)
	: type(typ)
	, torrent{ 0 }
	, parent(NULL)
	, viewcontent(NULL)
{
}

ViewNode::~ViewNode()
{

}

int ViewNode::AddNode_(ViewNode* node)
{
	subnodes.insert(node);
	node->parent = this;
	return 0;
}

int ViewNode::RemoveNode(ViewNode* vnode)
{
	subnodes.erase(vnode);
	return 0;
}

ViewNode* ViewNode::GetViewNode(TorrentNodeVT* node)
{
	std::set<ViewNode*>::iterator itvn = std::find_if(subnodes.begin(), subnodes.end()
		, [node](ViewNode* vnode) {
		return vnode->torrent.node == node;
	});
	ViewNode* cvn = itvn == subnodes.end() ? NULL : *itvn;
	return cvn;
}

ViewNode* ViewNode::AddFileViewNode(TorrentNodeFile* fnode)
{
	torrent.file = fnode;
	TorrentNodeFile* tnf;
	ViewNode* nfn;
	for (unsigned int ii = 0; ii < fnode->subfiles.count; ii++) {
		tnf = fnode->subfiles.items[ii];
		nfn = NULL;
		std::for_each(subnodes.begin(), subnodes.end(), [fnode, tnf, &nfn](ViewNode* vnode) {
			if (vnode->name.compare(tnf->name) == 0) {
				nfn = vnode;
			}
		});
		if (nfn == NULL) {
			if (tnf->type == TNF_TYPE::dir) {
				nfn = new ViewNode(VG_TYPE::FILE);
				nfn->torrent.node = torrent.node;
				nfn->name = tnf->name;
				AddNode_(nfn);
			}
		}
		if (nfn) {
			nfn->AddFileViewNode(tnf);
		}
	}
	return this;
}

int FileNodeGetFiles(TorrentNodeFile* fnode, std::vector<TorrentNodeFile*>& vnlist)
{
	if (fnode->type == TNF_TYPE::file) {
		vnlist.push_back(fnode);
	}
	else if (fnode->type == TNF_TYPE::dir) {
		for (unsigned int ii = 0; ii < fnode->subfiles.count; ii++) {
			FileNodeGetFiles(fnode->subfiles.items[ii], vnlist);
		}
	}

	return 0;
}
int ViewNode::GetFileNodes(std::vector<TorrentNodeFile*>& vnlist)
{
	if (torrent.file) {
		FileNodeGetFiles(torrent.file, vnlist);
	}
	return 0;
}

int ViewNode::GetTorrentNodes(std::set<TorrentNodeVT*>& tnodes)
{
	switch (type) {
	case VG_TYPE::GROUP:
	case VG_TYPE::GROUPGROUP:
		std::for_each(subnodes.begin(), subnodes.end(), [&tnodes](ViewNode* vnd) {
			vnd->GetTorrentNodes(tnodes);
		});
		break;
	case VG_TYPE::TORRENT:
		tnodes.insert(torrent.node);
		break;
	}
	return 0;
}

int ViewNode::GetViewNodeSet(TorrentNodeVT* seeknode, std::set<ViewNode*>& vnodes)
{
	if (type == VG_TYPE::TORRENT) {
		if (seeknode == torrent.node) {
			vnodes.insert(this);
		}
	}
	else {
		std::for_each(subnodes.begin(), subnodes.end(), [seeknode, &vnodes](ViewNode* vnode)	{
			vnode->GetViewNodeSet(seeknode, vnodes);
		}
		);
	}
	return 0;
}

int ViewNode::GetGroupNodes(std::set<ViewNode*>& groups)
{
	if (type == VG_TYPE::GROUPGROUP) {
		std::for_each(subnodes.begin(), subnodes.end(), [&groups](ViewNode* vnode) {
			vnode->GetGroupNodes(groups);
		});
	}
	else if (type == VG_TYPE::GROUP) {
		groups.insert(this);
		}
	return 0;
}

int ViewNode::GetTorrentViewNodes(std::set<ViewNode*>& torrents)
{
	if (type == VG_TYPE::GROUP) {
		std::for_each(subnodes.begin(), subnodes.end(), [&torrents](ViewNode* vnode) {
			vnode->GetTorrentViewNodes(torrents);
		});
	}
	else if (type == VG_TYPE::TORRENT) {
		torrents.insert(this);
	}
	return 0;
}

TorrentNodeVT * ViewNode::GetTorrentNode()
{
	TorrentNodeVT* tnd = NULL;
	if (type == VG_TYPE::TORRENT) {
		tnd = torrent.node;
	}
	else if (parent) {
		tnd = parent->GetTorrentNode();
	}
	return tnd;
}

int ViewNode::FormatGroupTextContent()
{
	size_t ssize = 0;
	group.viewgroupname = wbuf;
	ssize += wsprintf(wbuf + ssize, L"%s", group.groupnode->name.c_str());
	ssize++;
	group.viewgroupsize = wbuf + ssize;
	ssize += FormatNumberView(wbuf + ssize, 128, group.groupnode->size);
	ssize++;
	group.viewgroupvsize = wbuf + ssize;
	ssize += FormatNumberByteView(wbuf + ssize, 128, group.groupnode->size);
	ssize++;
	group.viewgroupcount = wbuf + ssize;
	ssize += wsprintf(wbuf + ssize, L"%d", group.groupnode->nodecount);
	ssize++;
	group.viewgroupdownload = wbuf + ssize;
	ssize+= FormatNumberByteView(wbuf + ssize, 128, group.groupnode->downspeed);
	ssize++;
	group.viewgroupup = wbuf + ssize;
	ssize += FormatNumberByteView(wbuf + ssize, 128, group.groupnode->upspeed);
	ssize++;

	if (viewcontent) {
		delete viewcontent;
	}
	viewcontent = new WCHAR[ssize];
	memcpy_s(viewcontent, ssize * sizeof(WCHAR), wbuf, ssize * sizeof(WCHAR));
	group.viewgroupname = viewcontent + (group.viewgroupname - wbuf);
	group.viewgroupsize = viewcontent + (group.viewgroupsize - wbuf);
	group.viewgroupvsize = viewcontent + (group.viewgroupvsize - wbuf);
	group.viewgroupcount = viewcontent + (group.viewgroupcount - wbuf);
	group.viewgroupdownload = viewcontent + (group.viewgroupdownload - wbuf);
	group.viewgroupup = viewcontent + (group.viewgroupup - wbuf);

	return 0;
}

int ViewNode::FormatTorrentTextContent()
{
	WCHAR tbuf[128];

	size_t ssize = 0;
	torrent.viewnodeid = wbuf + ssize;
	ssize += wsprintf(wbuf + ssize, L"%d", torrent.node->id);
	ssize++;

	torrent.viewnodename = wbuf + ssize;
	ssize += wsprintf(wbuf + ssize, L"%s", torrent.node->name.c_str());
	ssize++;

	torrent.viewnodesize = wbuf + ssize;
	ssize += FormatNumberView(wbuf + ssize, 128, torrent.node->size);
	ssize++;

	torrent.viewnodevsize = wbuf + ssize;
	ssize += FormatNumberByteView(wbuf + ssize, 128, torrent.node->size);
	ssize++;

	torrent.viewnodetracker = wbuf + ssize;
	if (torrent.node->trackers.count > 0) {
		ssize += wsprintf(wbuf + ssize, L"%s", torrent.node->trackers.items[0].name.c_str());
	}
	else {
		wbuf[ssize] = 0;
	}
	ssize++;

	torrent.viewnodedownload = wbuf + ssize;
	FormatNumberByteView(tbuf, 128, torrent.node->downspeed);
	ssize += wsprintf(wbuf + ssize, L"%s/s", tbuf);
	ssize++;

	torrent.viewnodeupload = wbuf + ssize;
	FormatNumberByteView(tbuf, 128, torrent.node->upspeed);
	ssize += wsprintf(wbuf + ssize, L"%s/s", tbuf);
	ssize++;

	torrent.viewnodestatus = wbuf + ssize;
	ssize += FormatViewStatus(wbuf + ssize, 128, torrent.node->status);
	ssize++;

	torrent.viewnoderation = wbuf + ssize;
	ssize += FormatViewDouble(wbuf + ssize, 128, torrent.node->ratio);
	ssize++;

	torrent.viewnodelocation = wbuf + ssize;
	ssize += wsprintf(wbuf + ssize, L"%s", torrent.node->path.fullpath.c_str());
	ssize++;

	if (viewcontent) {
		delete viewcontent;
	}
	viewcontent = new WCHAR[ssize];
	memcpy_s(viewcontent, ssize * sizeof(WCHAR), wbuf, ssize * sizeof(WCHAR));
	torrent.viewnodeid = torrent.viewnodeid - wbuf + viewcontent;
	torrent.viewnodename = torrent.viewnodename - wbuf + viewcontent;
	torrent.viewnodesize = torrent.viewnodesize - wbuf + viewcontent;
	torrent.viewnodevsize = torrent.viewnodevsize - wbuf + viewcontent;
	torrent.viewnodetracker = torrent.viewnodetracker - wbuf + viewcontent;
	torrent.viewnodedownload = torrent.viewnodedownload - wbuf + viewcontent;
	torrent.viewnodeupload = torrent.viewnodeupload - wbuf + viewcontent;
	torrent.viewnodestatus = torrent.viewnodestatus - wbuf + viewcontent;
	torrent.viewnoderation = torrent.viewnoderation - wbuf + viewcontent;
	torrent.viewnodelocation = torrent.viewnodelocation - wbuf + viewcontent;

	return 0;
}
