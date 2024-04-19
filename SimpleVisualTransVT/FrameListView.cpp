#include "FrameListView.h"
#include "TorrentClient.h"

#include <algorithm>
#include <strsafe.h>

FrameListView::FrameListView(HWND hlist)
	: hList(hlist)
	, gradualpenvt{ 0 }
{
}

FrameListView::~FrameListView()
{
	if (hbRed) {
		DeleteObject(hbRed);
	}
	if (hbGreen) {
		DeleteObject(hbGreen);
	}
	for (unsigned int ii = 0; ii < 256; ii++) {
		DeleteObject(gradualpenvt[ii]);
	}
}

int FrameListView::UpdateTorrentNode(TorrentNodeVT* node, ViewNode* vgroup)
{
	switch (listviewconst.content) {
	case LISTCONTENT::TORRENTLIST:
	{
		if (listviewconst.currentListNode == vgroup) {
			std::set<ViewNode*> vnodes;
			vgroup->GetViewNodeSet(node, vnodes);
			if (vnodes.size() > 0) {
				std::for_each(vnodes.begin(), vnodes.end(), [this](ViewNode* vnode) {
					std::vector<ViewNode*>::iterator itfv = std::find(ownernodes.begin(), ownernodes.end(), vnode);
					int fvi = -1;
					if (itfv == ownernodes.end()) {
						fvi = ownernodes.size();
						ownernodes.push_back(vnode);
					}
					else {
						fvi = itfv - ownernodes.begin();
					}
					if (fvi >= 0 && fvi < (int)ownernodes.size()) {
						ListView_RedrawItems(hList, fvi, fvi);
					}
				});
			}
		}
	}
	break;
	case LISTCONTENT::TORRENTDETAIL:
		UpdateTorrentContentData(node);
		break;
	}
	return 0;
}

double seeksqrt(double low, double high, double sqr)
{
	double mid = (low + high) / 2;
	double rtn = mid;
	if ((mid <= low) || (mid >= high)) {
		rtn = mid;
	}
	else {
		double csqr = mid * mid;
		if (csqr > sqr) {
			rtn = seeksqrt(low, mid, sqr);
		}
		else if (csqr < sqr) {
			rtn = seeksqrt(mid, high, sqr);
		}
		else {
			rtn = mid;
		}
	}
	return rtn;
}

#define COLORLEVELS 256
int FrameListView::Initialize()
{
	listviewconst.content = LISTCONTENT::NONE;

	hbRed = ::CreateSolidBrush(RGB(0xFF, 0xCF, 0xCF));
	hbGreen = ::CreateSolidBrush(RGB(0xCF, 0xFF, 0xCF));
	double biq;
	unsigned int cix;

	for (unsigned int ii = 0; ii < COLORLEVELS; ii++) {
		biq = ii;
		biq /= COLORLEVELS - 1;
		biq *= biq;
		cix = (unsigned int)(biq * (COLORLEVELS - 1));
		if (ii < 128) {
			gradualpenvt[ii] = CreatePen(PS_SOLID, 1, RGB(0xFF, 0x7F + cix, 0x7F));
		}
		else {
			gradualpenvt[ii] = CreatePen(PS_SOLID, 1, RGB(0xFF - (cix - 0x80), 0xFF, 0x7F));
		}
	}

	return 0;
}

int FrameListView::UpdateFileStat(TorrentNodeVT* node, TorrentNodeFile* fnode)
{
	if (listviewconst.currentListNode) {
		if (listviewconst.currentListNode->type == VG_TYPE::FILE) {
			std::vector<TorrentNodeFile*>::iterator itfn = std::find(files.begin(), files.end(), fnode);
			if (itfn != files.end()) {
				int fni = itfn - files.begin();
				if (fni >= 0 && fni < (int)files.size()) {
					ListView_RedrawItems(hList, fni, fni);
				}
			}
		}
	}
	return 0;
}

int FrameListView::UpdateItemGroup(int iix, TorrentGroupVT* group)
{
	int itt = 0;
	WCHAR wbuf[1024];

	if (listviewconst.content == LISTCONTENT::GROUPLIST) {
		if (listviewconst.currentListNode) {
			if (listviewconst.currentListNode->type == VG_TYPE::GROUPGROUP) {
				TorrentGroupVT* pgroup = group->parent;
				if (listviewconst.currentListNode->group.groupnode == pgroup) {
					LVFINDINFO fif;
					fif.flags = LVFI_PARAM;
					fif.lParam = (LPARAM)group;

					int fix = ListView_FindItem(hList, -1, &fif);
					if (fix >= 0) {
						ListView_SetItemText(hList, iix, itt, (LPWSTR)group->name.c_str());
						itt++;

						FormatNumberView(wbuf, 128, group->size);
						ListView_SetItemText(hList, iix, itt, wbuf);
						itt++;

						FormatNumberByteView(wbuf, 128, group->size);
						ListView_SetItemText(hList, iix, itt, wbuf);
						itt++;

						wsprintf(wbuf, L"%d", group->nodecount);
						ListView_SetItemText(hList, iix, itt, wbuf);
						itt++;

						FormatNumberByteView(wbuf + 64, 64, group->downspeed);
						wsprintf(wbuf, L"%s/s", wbuf + 64);
						ListView_SetItemText(hList, iix, itt, wbuf);
						itt++;

						FormatNumberByteView(wbuf + 64, 64, group->upspeed);
						wsprintf(wbuf, L"%s/s", wbuf + 64);
						ListView_SetItemText(hList, iix, itt, wbuf);
						itt++;
					}
				}
			}
		}
	}


	return 0;
}

//int FrameListView::UpdateItemNode(long iit, TorrentNodeVT* node) {
//	wchar_t wbuf[128];
//	int isub = 0;
//
//	wsprintf(wbuf, L"%d", node->id);
//	ListView_SetItemText(hList, iit, isub, wbuf);
//	isub++;
//
//	ListView_SetItemText(hList, iit, isub, (LPWSTR)node->name.c_str());
//	isub++;
//
//	FormatNumberView(wbuf, 128, node->size);
//	ListView_SetItemText(hList, iit, isub, wbuf);
//	isub++;
//
//	FormatNumberByteView(wbuf, 128, node->size);
//	ListView_SetItemText(hList, iit, isub, wbuf);
//	isub++;
//
//	if (node->trackers.count > 0) {
//		ListView_SetItemText(hList, iit, isub, (LPWSTR)node->trackers.items[0].name.c_str());
//	}
//	isub++;
//
//	FormatNumberByteView(wbuf, 128, node->downspeed);
//	ListView_SetItemText(hList, iit, isub, wbuf);
//	isub++;
//
//	FormatNumberByteView(wbuf, 128, node->upspeed);
//	ListView_SetItemText(hList, iit, isub, wbuf);
//	isub++;
//
//	FormatViewStatus(wbuf, 128, node->status);
//	ListView_SetItemText(hList, iit, isub, wbuf);
//	isub++;
//
//	FormatViewDouble(wbuf, 128, node->ratio);
//	ListView_SetItemText(hList, iit, isub, wbuf);
//	isub++;
//
//	ListView_SetItemText(hList, iit, isub, (LPWSTR)node->path.fullpath.c_str());
//	isub++;
//
//	return 0;
//}

int FrameListView::InsertGroupItem(TorrentGroupVT* group)
{
	LVITEM lvi = { 0 };
	long lvc = 0;
	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	lvi.pszText = (LPWSTR)L"";
	lvi.cchTextMax = 0;
	lvi.iItem = lvc;
	lvi.lParam = (LPARAM)group;
	lvi.iSubItem = 0;
	lvc = ListView_InsertItem(hList, &lvi);

	UpdateItemGroup(lvc, group);

	return 0;
}

int FrameListView::AddGroupGroup(TorrentClientRequest* req, TorrentGroupVT* group)
{
	if (group == NULL) {
		delete req;
	}
	else {
		InsertGroupItem(group);
	}
	return 0;
}

//int FrameListView::AddTorrentListNode(TorrentClientRequest* req, TorrentNodeVT* node)
//{
//	if (node) {
//		if (listviewconst.listchangesequence == req->sequence) {
//			if (listviewconst.content == LISTCONTENT::TORRENTLIST) {
//				LVFINDINFO fif;
//				fif.flags = LVFI_PARAM;
//				fif.lParam = (LPARAM)node;
//				int fix = ListView_FindItem(hList, -1, &fif);
//				if (fix < 0) {
//					InsertNodeItem(node);
//				}
//			}
//		}
//	}
//	else {
//		if (listviewconst.content == LISTCONTENT::TORRENTLIST) {
//			std::map<LISTCONTENT, LISTSORTCOL>::iterator itct = listviewconst.sortcontentmap.find(listviewconst.content);
//			if (itct != listviewconst.sortcontentmap.end()) {
//				SortTorrentList(itct->second);
//			}
//		}
//		delete req;
//	}
//	return 0;
//}

//int FrameListView::InsertNodeItem(TorrentNodeVT* node)
//{
//	LVITEM lvi = { 0 };
//	long lvc = 0;
//	lvi.mask = LVIF_TEXT | LVIF_PARAM;
//	lvi.pszText = (LPWSTR)L"";
//	lvi.cchTextMax = 0;
//	lvi.iItem = lvc;
//	lvi.lParam = (LPARAM)node;
//	lvi.iSubItem = 0;
//	lvc = ListView_InsertItem(hList, &lvi);
//
//	UpdateItemNode(lvc, node);
//
//	return lvc;
//}

//int FrameListView::AddNodeFile(TorrentClientRequest* req, std::vector<TorrentNodeFile*>* filelist)
//{
//	if (filelist == NULL) {
//		if (req) {
//			delete req;
//		}
//	}
//	else {
//		std::for_each(filelist->begin(), filelist->end(), [this](TorrentNodeFile* file) {
//			InsertNodeFileStat(file);
//		}
//		);
//		delete filelist;
//	}
//	return 0;
//}

//int FrameListView::InsertNodeFileStat(TorrentNodeFile* file)
//{
//	wchar_t tbuf[1024];
//
//	wsprintf(tbuf, L"%d", file->id);
//	LVITEM lvi = { 0 };
//	long lvc = 0;
//	lvi.mask = LVIF_TEXT | LVIF_PARAM;
//	lvi.pszText = (LPWSTR)tbuf;
//	lvi.cchTextMax = (int)wcslen(tbuf);
//	lvi.iItem = lvc;
//	lvi.lParam = (LPARAM)file;
//	lvi.iSubItem = 0;
//	lvc = ListView_InsertItem(hList, &lvi);
//
//	UpdateNodeFileStat(file, lvc);
//
//	return lvc;
//}

//void FrameListView::UpdateNodeFileStat(TorrentNodeFile* file, long lvc)
//{
//	int isub = 1;
//	wchar_t tbuf[1024];
//
//	//include
//	ListView_SetItemText(hList, lvc, isub, (LPWSTR)(file->check ? L"Yes" : L"No"));
//	//ListView_SetColumnWidth(hList, isub, LVSCW_AUTOSIZE);
//	isub++;
//
//	//priotity
//	wsprintf(tbuf, L"%d", file->priority);
//	ListView_SetItemText(hList, lvc, isub, (LPWSTR)tbuf);
//	//ListView_SetColumnWidth(hList, isub, LVSCW_AUTOSIZE);
//	isub++;
//
//	//name
//	ListView_SetItemText(hList, lvc, isub, (LPWSTR)file->path.c_str());
//	//ListView_SetColumnWidth(hList, isub, 200);
//	isub++;
//
//	//path
//	ListView_SetItemText(hList, lvc, isub, (LPWSTR)file->name.c_str());
//	//ListView_SetColumnWidth(hList, isub, 200);
//	isub++;
//
//	//size
//	FormatNumberByteView(tbuf, 1024, file->size);
//	ListView_SetItemText(hList, lvc, isub, (LPWSTR)tbuf);
//	//ListView_SetColumnWidth(hList, isub, LVSCW_AUTOSIZE);
//	isub++;
//
//	//downloaded
//	FormatNumberView(tbuf, 1024, file->done);
//	double pct = file->size > 0 ? ((double)file->done / file->size) : 0;
//	pct *= 100;
//	FormatViewPercentage(tbuf + 256, 256, pct);
//	wsprintf(tbuf + 512, L"%s (%s)", tbuf, tbuf + 256);
//	ListView_SetItemText(hList, lvc, isub, (LPWSTR)tbuf + 512);
//	//ListView_SetColumnWidth(hList, isub, LVSCW_AUTOSIZE);
//	isub++;
//}

int FrameListView::RemoveTorrent(ViewNode* vnode)
{
	switch (listviewconst.content) {
	case LISTCONTENT::TORRENTLIST:
		std::vector<ViewNode*>::iterator itvn = std::find(ownernodes.begin(), ownernodes.end(), vnode);
		if (itvn != ownernodes.end()) {
			ownernodes.erase(itvn);
			ListView_SetItemCountEx(hList, ownernodes.size(), LVSICF_NOSCROLL);
		}
		break;
	}
	return 0;
}

int FrameListView::GetSelectedNodes(std::set<TorrentNodeVT*>& nlist)
{
	switch (listviewconst.content)
	{
	case LISTCONTENT::TORRENTLIST:
	{
		bool keepseek = true;
		int sit = -1;
		LVITEM lvi = { 0 };
		while (keepseek) {
			sit = ListView_GetNextItem(hList, sit, LVNI_SELECTED);
			if (sit >= 0) {
				if (sit <= (int)ownernodes.size()) {
					nlist.insert(ownernodes.at(sit)->torrent.node);
				}
			}
			else {
				keepseek = false;
			}
		}
	}
	break;
	case LISTCONTENT::TORRENTFILE:
		if (listviewconst.currentListNode->torrent.node) {
			nlist.insert(listviewconst.currentListNode->torrent.node);
		}
		break;
	}

	return 0;
}

int FrameListView::GetSelectedFiles(std::set<TorrentNodeFile*>& flist)
{
	if (listviewconst.content == LISTCONTENT::TORRENTFILE) {
		bool keepseek = true;
		int sit = -1;
		LVITEM lvi = { 0 };
		TorrentNodeFile* fnode;
		while (keepseek) {
			sit = ListView_GetNextItem(hList, sit, LVNI_SELECTED);
			if (sit >= 0) {
				fnode = files.at(sit);;
				flist.insert(fnode);
			}
			else {
				keepseek = false;
			}
		}

	}

	return 0;
}

int FrameListView::GetSelectedViewNodes(std::set<ViewNode*>& vlist)
{
	return 0;
}

struct LVTNSS {
	bool sortasc;
	LISTSORTCOL col;
};

int CALLBACK LVComp_TorrentNode(LPARAM lp1, LPARAM lp2, LPARAM lpsort)
{
	TorrentNodeVT* ln = (TorrentNodeVT*)lp1;
	TorrentNodeVT* rn = (TorrentNodeVT*)lp2;
	LVTNSS* lvtns = (LVTNSS*)lpsort;

	int ctn = 0;
	switch (lvtns->col) {
	case LISTSORTCOL::ID:
		ctn = ln->id < rn->id ? -1 : (ln->id > rn->id ? 1 : 0);
		break;
	case LISTSORTCOL::NAME:
		ctn = ln->name.compare(rn->name);
		break;
	case LISTSORTCOL::SIZE:
		ctn = ln->size < rn->size ? -1 : (ln->size > rn->size ? 1 : 0);
		break;
	case LISTSORTCOL::LOCATION:
		ctn = ln->path.fullpath.compare(rn->path.fullpath);
		break;
	case LISTSORTCOL::STATUS:
		ctn = ln->status < rn->status ? -1 : (ln->status > rn->status ? 1 : 0);
		break;
	case LISTSORTCOL::TRACKER:
		if (ln->trackers.count > 0 && rn->trackers.count > 0) {
			ctn = ln->trackers.items[0].name.compare(rn->trackers.items[0].name);
		}
		else {
			ctn = ln->trackers.count > 0 ? 1 : (rn->trackers.count > 0 ? -1 : 0);
		}
		break;
	case LISTSORTCOL::DOWNSPEED:
		ctn = ln->downspeed < rn->downspeed ? -1 : (ln->downspeed > rn->downspeed ? 1 : 0);
		break;
	case LISTSORTCOL::UPSPEED:
		ctn = ln->upspeed < rn->upspeed ? -1 : (ln->upspeed > rn->upspeed ? 1 : 0);
		break;
	case LISTSORTCOL::RATIO:
		ctn = ln->ratio < rn->ratio ? -1 : (ln->ratio > rn->ratio ? 1 : 0);
		break;
	}
	ctn = lvtns->sortasc ? ctn : (ctn > 0 ? -1 : (ctn < 0 ? 1 : 0));
	return ctn;
}

int CALLBACK LVComp_FileNode(LPARAM lp1, LPARAM lp2, LPARAM lpsort)
{
	TorrentNodeFile* ln = (TorrentNodeFile*)lp1;
	TorrentNodeFile* rn = (TorrentNodeFile*)lp2;
	LVTNSS* lvtns = (LVTNSS*)lpsort;

	int ctn = 0;
	switch (lvtns->col) {
	case LISTSORTCOL::ID:
		ctn = ln->id < rn->id ? -1 : (ln->id > rn->id ? 1 : 0);
		break;
	case LISTSORTCOL::NAME:
		ctn = ln->name.compare(rn->name);
		break;
	case LISTSORTCOL::SIZE:
		ctn = ln->size < rn->size ? -1 : (ln->size > rn->size ? 1 : 0);
		break;
	case LISTSORTCOL::LOCATION:
		ctn = ln->path.compare(rn->path);
		break;
	case LISTSORTCOL::DOWNLOAD:
		double lpr = ln->size > 0 ? ((double)ln->done) / ln->size : 0;
		double rpr = rn->size > 0 ? ((double)rn->done) / rn->size : 0;
		ctn = lpr < rpr ? -1 : (lpr > rpr ? 1 : 0);
		break;
	}
	ctn = lvtns->sortasc ? ctn : (ctn > 0 ? -1 : (ctn < 0 ? 1 : 0));
	return ctn;
}

int FrameListView::SortTorrentList(LISTSORTCOL col)
{
	LVTNSS lvtns;
	lvtns.col = col;
	lvtns.sortasc = listviewconst.sortasc;
	ListView_SortItems(hList, LVComp_TorrentNode, &lvtns);
	return 0;
}

int FrameListView::SortTorrentFile(LISTSORTCOL col)
{
	LVTNSS lvtns;
	lvtns.col = col;
	lvtns.sortasc = listviewconst.sortasc;
	ListView_SortItems(hList, LVComp_FileNode, &lvtns);
	return 0;
}

int FrameListView::IncreaseSequenceNo()
{
	listviewconst.listchangesequence++;
	return listviewconst.listchangesequence;
}

LRESULT FrameListView::ProcNotifyCustomDrawNodeList(LPNMLVCUSTOMDRAW& plcd)
{
	LRESULT lrt = CDRF_DODEFAULT;
	switch (plcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		lrt = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
	{
		int sli = ListView_GetNextItem(hList, ((int)plcd->nmcd.dwItemSpec) - 1, LVNI_SELECTED);
		if (sli != plcd->nmcd.dwItemSpec) {
			if (plcd->nmcd.dwItemSpec < ownernodes.size()) {
				ViewNode* vnode = ownernodes.at(plcd->nmcd.dwItemSpec);
				lrt = vnode->torrent.node->error.length() > 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
				lrt = vnode->torrent.node->status == 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
				lrt = vnode->torrent.node->status == 4 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
			}
		}
	}
	break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
	{
		if (plcd->nmcd.dwItemSpec < ownernodes.size()) {
			ViewNode* vnode = ownernodes.at(plcd->nmcd.dwItemSpec);
			if (vnode->torrent.node->status == 0) {
				plcd->clrTextBk = RGB(0xCF, 0xCF, 0xCF);
			}
			if (vnode->torrent.node->status == 4) {
				int nameisub = 1;
				if (plcd->iSubItem == nameisub) {
					RECT src;
					ListView_GetSubItemRect(hList, plcd->nmcd.dwItemSpec, nameisub, LVIR_BOUNDS, &src);
					FillRect(plcd->nmcd.hdc, &src, hbGreen);
					src.left += 2 * GetSystemMetrics(SM_CXEDGE);
					if (vnode->torrent.node->size > 0) {
						RECT urc(src);
						urc.right = (long)((src.right - src.left) * vnode->torrent.node->leftsize / vnode->torrent.node->size) + src.left;
						FillRect(plcd->nmcd.hdc, &urc, hbRed);
						DrawText(plcd->nmcd.hdc, vnode->torrent.node->name.c_str(), (int)vnode->torrent.node->name.length(), &src, DT_LEFT);
						lrt = CDRF_SKIPDEFAULT;
					}
				}

				if (lrt != CDRF_SKIPDEFAULT) {
					plcd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
					plcd->clrText = RGB(0x00, 0x3F, 0x00);
				}
			}
			if (vnode->torrent.node->error.length() > 0) {
				plcd->clrText = RGB(0xFF, 0x00, 0x00);
			}
		}
	}
	break;
	}
	return lrt;
}

LRESULT FrameListView::ProcNotifyCustomDrawNodeDetailPiece(LPNMLVCUSTOMDRAW& plcd)
{
	LRESULT lrt = CDRF_DODEFAULT;

	RECT src;
	HGDIOBJ hpo;
	ListView_GetSubItemRect(hList, plcd->nmcd.dwItemSpec, 1, LVIR_BOUNDS, &src);
	src.left += 2 * GetSystemMetrics(SM_CXEDGE);
	InflateRect(&src, -1, -1);
	hpo = SelectObject(plcd->nmcd.hdc, gradualpenvt[0]);
	TorrentNodeVT* lpd = listviewconst.currentListNode->torrent.node;
	int ide = 0;
	double mgs;
	int ipi = 0;
	unsigned int spi = 0;
	int idx = 0;
	int cwd = src.right - src.left;
	HPEN hpen;
	if (lpd->piecesdata.count > 0) {
		if (cwd > 0) {
			while (spi < lpd->piececount) {
				ide += lpd->piecesdata.items[spi];
				idx++;
				spi++;
				mgs = spi * cwd;
				mgs /= lpd->piececount;

				if (ipi < (int)mgs) {
					ide = ide * 255 / idx;
					hpen = gradualpenvt[ide];
					SelectObject(plcd->nmcd.hdc, hpen);
					while (ipi < (int)mgs) {
						MoveToEx(plcd->nmcd.hdc, ipi + src.left, src.top, NULL);
						LineTo(plcd->nmcd.hdc, ipi + src.left, src.bottom);
						ipi++;
					}
					ide = 0;
					idx = 0;
				}
			}
			SelectObject(plcd->nmcd.hdc, hpo);
			lrt = CDRF_SKIPDEFAULT;
		}
	}

	return lrt;
}

LRESULT FrameListView::ProcNotifyCustomDrawNodeDetail(LPNMLVCUSTOMDRAW& plcd)
{
	LRESULT lrt = CDRF_DODEFAULT;
	switch (plcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		lrt = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
	{
		if (listviewconst.currentListNode->type == VG_TYPE::TORRENT) {
			if ((listviewconst.currentListNode->torrent.node->status == 0)
				|| (listviewconst.currentListNode->torrent.node->status == 4)
				|| (listviewconst.currentListNode->torrent.node->error.length() > 0)) {
				lrt = CDRF_NOTIFYSUBITEMDRAW;
			}
			if (plcd->nmcd.dwItemSpec == 27) {
				lrt = CDRF_NOTIFYSUBITEMDRAW;
			}
		}
	}
	break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
	{
		if (plcd->nmcd.dwItemSpec == 27) {
			if (plcd->iSubItem == 1) {
				lrt = ProcNotifyCustomDrawNodeDetailPiece(plcd);
			}
		}
		else {
			if (listviewconst.currentListNode->torrent.node->status == 0) {
				plcd->clrTextBk = RGB(0xCF, 0xCF, 0xCF);
			}
			if (listviewconst.currentListNode->torrent.node->status == 4) {
				plcd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
				plcd->clrText = RGB(0x00, 0x3F, 0x00);
			}
			if (listviewconst.currentListNode->torrent.node->error.length() > 0) {
				plcd->clrText = RGB(0xFF, 0x00, 0x00);
			}
		}
	}
	break;
	}
	return lrt;
}

LRESULT FrameListView::ProcNotifyCustomDraw(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	LPNMLVCUSTOMDRAW plcd = (LPNMLVCUSTOMDRAW)lParam;

	switch (listviewconst.content) {
	case LISTCONTENT::TORRENTLIST:
		lrt = ProcNotifyCustomDrawNodeList(plcd);
		break;
	case LISTCONTENT::TORRENTDETAIL:
		lrt = ProcNotifyCustomDrawNodeDetail(plcd);
		break;
	}
	return lrt;
}

LRESULT FrameListView::ProcNotifyGetDispInfo(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	switch (listviewconst.content)
	{
	case LISTCONTENT::GROUPLIST:
		lrt = ProcNotifyGetDispInfoGroupList(hWnd, message, wParam, lParam);
		break;
	case LISTCONTENT::TORRENTLIST:
		lrt = ProcNotifyGetDispInfoTorrentList(hWnd, message, wParam, lParam);
		break;
	case LISTCONTENT::TORRENTDETAIL:
		lrt = ProcNotifyGetDispInfoTorrentContent(hWnd, message, wParam, lParam);
		break;
	case LISTCONTENT::TORRENTFILE:
		lrt = ProcNotifyGetDispInfoTorrentFile(hWnd, message, wParam, lParam);
		break;
	default:
		break;
	}
	return lrt;
}

LRESULT FrameListView::ProcNotifyGetDispInfoTorrentContent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	LV_DISPINFO* pdi = (LV_DISPINFO*)lParam;

	if (pdi->item.iSubItem > 0) {
		if (pdi->item.iItem < (int)torrentcontenttitles.size()) {
			if (pdi->item.mask & LVIF_TEXT) {
				ViewNode* cvnode = listviewconst.currentListNode;
				if (cvnode->type == VG_TYPE::TORRENT) {
					switch (pdi->item.iItem) {
					case 0:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%d", cvnode->torrent.node->id);
						break;
					case 1:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", cvnode->torrent.node->name.c_str());
						break;
					case 2:
						FormatNumberView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->size);
						break;
					case 3:
						FormatNumberByteView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->size);
						break;
					case 4:
						FormatNumberByteView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->downspeed);
						break;
					case 5:
						FormatNumberByteView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->upspeed);
						break;
					case 6:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%d", cvnode->torrent.node->status);
						break;
					case 7:
						FormatViewStatus(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->status);
						break;
					case 8:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", cvnode->torrent.node->path.fullpath.c_str());
						break;
					case 9:
						FormatTimeSeconds(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->downloadTime);
						break;
					case 10:
						FormatTimeSeconds(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->seedTime);
						break;
					case 11:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", cvnode->torrent.node->error.c_str());
						break;
					case 12:
						FormatDualByteView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->downloaded);
						break;
					case 13:
						FormatDualByteView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->uploaded);
						break;
					case 14:
						FormatNumberByteView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->corrupt);
						break;
					case 15:
						FormatViewDouble(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->ratio);
						break;
					case 16:
						FormatNumberView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->leftsize);
						break;
					case 17:
						FormatNumberView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->desired);
						break;
					case 18:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", cvnode->torrent.node->privacy ? L"Yes" : L"No");
						break;
					case 19:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", cvnode->torrent.node->done ? L"Yes" : L"No");
						break;
					case 20:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", cvnode->torrent.node->stalled ? L"Yes" : L"No");
						break;
					case 21:
						FormatIntegerDate(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->donedate);
						break;
					case 22:
						FormatIntegerDate(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->activedate);
						break;
					case 23:
						FormatIntegerDate(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->startdate);
						break;
					case 24:
						FormatIntegerDate(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->adddate);
						break;
					case 25:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%d", cvnode->torrent.node->piececount);
						break;
					case 26:
						FormatDualByteView(pdi->item.pszText, pdi->item.cchTextMax, cvnode->torrent.node->downloaded);
						break;
					case 27:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", cvnode->torrent.node->pieces.c_str());
						break;
					case 28:
						if (cvnode->torrent.node->trackers.count > 0) {
							if (cvnode->torrent.node->trackers.count == 1) {
								StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", cvnode->torrent.node->trackers.items[0].url.c_str());
							} else {
								StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%d", cvnode->torrent.node->trackers.count);
							}
						}
						break;
					}
				}
			}
		}
	}
	else {
		if (pdi->item.iItem < (int)torrentcontenttitles.size()) {
			if (pdi->item.mask & LVIF_TEXT) {
				ViewNode* cvnode = listviewconst.currentListNode;
				if (cvnode) {
					if (cvnode->type == VG_TYPE::TORRENT) {
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", torrentcontenttitles.at(pdi->item.iItem).c_str());
					}
				}
			}
		}
	}

	return lrt;
}
LRESULT FrameListView::ProcNotifyGetDispInfoTorrentList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;

	LV_DISPINFO* pdi = (LV_DISPINFO*)lParam;

	if (pdi->item.iSubItem > 0) {
		if (pdi->item.iItem < (int)ownernodes.size()) {
			if (pdi->item.mask & LVIF_TEXT) {
				ViewNode* cvnode = ownernodes.at(pdi->item.iItem);
				if (cvnode->type == VG_TYPE::TORRENT) {
					switch (pdi->item.iSubItem) {
					case 1:
						pdi->item.pszText = cvnode->torrent.viewnodename;
						break;
					case 2:
						pdi->item.pszText = cvnode->torrent.viewnodesize;
						break;
					case 3:
						pdi->item.pszText = cvnode->torrent.viewnodevsize;
						break;
					case 4:
						pdi->item.pszText = cvnode->torrent.viewnodetracker;
						break;
					case 5:
						pdi->item.pszText = cvnode->torrent.viewnodedownload;
						break;
					case 6:
						pdi->item.pszText = cvnode->torrent.viewnodeupload;
						break;
					case 7:
						pdi->item.pszText = cvnode->torrent.viewnodestatus;
						break;
					case 8:
						pdi->item.pszText = cvnode->torrent.viewnoderation;
						break;
					case 9:
						pdi->item.pszText = cvnode->torrent.viewnodelocation;
						break;
					}
				}
			}
		}
	}
	else {
		if (pdi->item.iItem < (int)ownernodes.size()) {
			if (pdi->item.mask & LVIF_TEXT) {
				ViewNode* cvnode = ownernodes.at(pdi->item.iItem);
				if (cvnode->type == VG_TYPE::TORRENT) {
					cvnode->FormatTorrentTextContent();
					pdi->item.pszText = cvnode->torrent.viewnodeid;
				}
			}
		}
	}

	return lrt;
}

int isub = 1;
wchar_t tbuf[1024];

LRESULT FrameListView::ProcNotifyGetDispInfoTorrentFile(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;

	LV_DISPINFO* pdi = (LV_DISPINFO*)lParam;

	if (pdi->item.iSubItem > 0) {
		if (pdi->item.iItem < (int)files.size()) {
			if (pdi->item.mask & LVIF_TEXT) {
				TorrentNodeFile* fnode = files.at(pdi->item.iItem);
				if (fnode->type == TNF_TYPE::file) {
					switch (pdi->item.iSubItem) {
					case 1:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s", fnode->check?L"Yes":L"No");
						break;
					case 2:
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%d", fnode->priority);
						break;
					case 3:
						pdi->item.pszText = (LPWSTR)fnode->path.c_str();
						break;
					case 4:
						pdi->item.pszText = (LPWSTR)fnode->name.c_str();
						break;
					case 5:
						FormatNumberByteView(pdi->item.pszText, pdi->item.cchTextMax, fnode->size);
						break;
					case 6:
					{
						FormatNumberByteView(pdi->item.pszText + 64, 64, fnode->done);
						double pct = fnode->size == 0 ? 0 : ((double)fnode->done)/fnode->size;
						FormatViewPercentage(pdi->item.pszText + 128, 128, pct*100);
						StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%s (%s%)", pdi->item.pszText + 64, pdi->item.pszText + 128);
					}
					break;
					}
				}
			}
		}
	}
	else {
		if (pdi->item.iItem < (int)files.size()) {
			if (pdi->item.mask & LVIF_TEXT) {
				TorrentNodeFile* fnode = files.at(pdi->item.iItem);
				if (fnode->type == TNF_TYPE::file) {
					StringCbPrintf(pdi->item.pszText, pdi->item.cchTextMax, L"%d", fnode->id);
				}
			}
		}
	}

	return lrt;
}

LRESULT FrameListView::ProcNotifyGetDispInfoGroupList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	LV_DISPINFO* pdi = (LV_DISPINFO*)lParam;

	if (pdi->item.iSubItem > 0) {
		if (pdi->item.iItem < (int)ownernodes.size()) {
			if (pdi->item.mask & LVIF_TEXT) {
				ViewNode* cvnode = ownernodes.at(pdi->item.iItem);
				if (cvnode->type == VG_TYPE::GROUP) {
					switch (pdi->item.iSubItem) {
					case 1:
						pdi->item.pszText = cvnode->group.viewgroupsize;
						break;
					case 2:
						pdi->item.pszText = cvnode->group.viewgroupvsize;
						break;
					case 3:
						pdi->item.pszText = cvnode->group.viewgroupcount;
						break;
					case 4:
						pdi->item.pszText = cvnode->group.viewgroupdownload;
						break;
					case 5:
						pdi->item.pszText = cvnode->group.viewgroupup;
						break;
					}
				}
			}
		}
	}
	else {
		if (pdi->item.iItem < (int)ownernodes.size()) {
			if (pdi->item.mask & LVIF_TEXT) {
				ViewNode* cvnode = ownernodes.at(pdi->item.iItem);
				if (cvnode->type == VG_TYPE::GROUP) {
					cvnode->FormatGroupTextContent();
					pdi->item.pszText = cvnode->group.viewgroupname;
				}
			}
		}
	}
	return lrt;
}

#define WBUFSZ 4096

class torrentnodecomp {
public:
	LISTSORTCOL sortcol = LISTSORTCOL::NONE;
	bool asc = true;
	bool operator() (ViewNode* left, ViewNode* right) {
		ViewNode* ll = asc?left:right;
		ViewNode* rr = asc?right:left;
		bool rtn = false;
		switch (sortcol) {
		case LISTSORTCOL::ID:
			rtn = ll->torrent.node->id < rr->torrent.node->id;
			break;
		case LISTSORTCOL::NAME:
			rtn = ll->torrent.node->name.compare(rr->torrent.node->name) == -1;
			break;
		case LISTSORTCOL::SIZE:
			rtn = ll->torrent.node->size < rr->torrent.node->size;
			break;
		case LISTSORTCOL::TRACKER:
			if (ll->torrent.node->trackers.count > 0) {
				if (rr->torrent.node->trackers.count > 0) {
					rtn = ll->torrent.node->trackers.items[0].name.compare(rr->torrent.node->trackers.items[0].name) == -1;
				}
				else {
					rtn = false;
				}
			}
			else {
				rtn = (rr->torrent.node->trackers.count > 0) ? true : false;
			}
			break;
		case LISTSORTCOL::RATIO:
			rtn = ll->torrent.node->ratio < rr->torrent.node->ratio;
			break;
		case LISTSORTCOL::DOWNSPEED:
			rtn = ll->torrent.node->downspeed < rr->torrent.node->downspeed;
			break;
		case LISTSORTCOL::UPSPEED:
			rtn = ll->torrent.node->upspeed < rr->torrent.node->upspeed;
			break;
		case LISTSORTCOL::STATUS:
			rtn = ll->torrent.node->status < rr->torrent.node->status;
			break;
		}

		return rtn;
	}
} torrentcomp;

class groupnodecomp {
public:
	LISTSORTCOL sortcol = LISTSORTCOL::NONE;
	bool asc = true;
	bool operator() (ViewNode* left, ViewNode* right) {
		ViewNode* ll = asc ? left : right;
		ViewNode* rr = asc ? right : left;
		bool rtn = false;
		switch (sortcol) {
		case LISTSORTCOL::NAME:
			rtn = ll->group.groupnode->name.compare(rr->group.groupnode->name) == -1;
			break;
		case LISTSORTCOL::SIZE:
			rtn = ll->group.groupnode->size < rr->group.groupnode->size;
			break;
		case LISTSORTCOL::COUNT:
			rtn = ll->group.groupnode->nodecount < rr->group.groupnode->nodecount;
			break;
		case LISTSORTCOL::DOWNSPEED:
			rtn = ll->group.groupnode->downspeed < rr->group.groupnode->downspeed;
			break;
		case LISTSORTCOL::UPSPEED:
			rtn = ll->group.groupnode->upspeed < rr->group.groupnode->upspeed;
			break;
		}

		return rtn;
	}
} groupcomp;

class filenodecomp {
public:
	LISTSORTCOL sortcol = LISTSORTCOL::NONE;
	bool asc = true;
	bool operator() (TorrentNodeFile* left, TorrentNodeFile* right) {
		bool rtn = false;
		bool asc = true;
		TorrentNodeFile* ll = left;
		TorrentNodeFile* rr = right;

		switch (sortcol) {
		case LISTSORTCOL::ID:
			rtn = ll->id < rr->id;
			break;
		case LISTSORTCOL::NAME:
			rtn = ll->name.compare(rr->name) == -1;
			break;
		case LISTSORTCOL::SIZE:
			rtn = ll->size < rr->size;
			break;
		case LISTSORTCOL::LOCATION:
			rtn = ll->path.compare(rr->path) == -1;
			break;
		case LISTSORTCOL::DOWNLOAD:
			double lpr = ll->size > 0 ? ((double)ll->done) / ll->size : 0;
			double rpr = rr->size > 0 ? ((double)rr->done) / rr->size : 0;
			rtn = lpr < rpr;
			break;
		}
		return rtn;
	}
} filecomp;

LRESULT FrameListView::ProcNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	NMLISTVIEW* plist = (NMLISTVIEW*)lParam;

	switch (plist->hdr.code)
	{
	case LVN_DELETEITEM:
	{
		lrt = 0;
	}
	break;
	case NM_DBLCLK:
		switch (listviewconst.content) {
		case LISTCONTENT::TORRENTLIST:
		case LISTCONTENT::GROUPLIST:
		{
			LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
			if (lpnmitem->iItem < (int)ownernodes.size()) {
				ViewNode* cvnode = ownernodes.at(lpnmitem->iItem);
				PostMessage(hWnd, WM_U_LIST_ACTIVATENODE, (WPARAM)cvnode, 0); //TODO: use callback def
			}
		}
		break;
		}
		break;
	case LVN_COLUMNCLICK:
	{
		switch (listviewconst.content) {
		case LISTCONTENT::TORRENTLIST:
		{
			listviewconst.sortasc = listviewconst.sortasc == false;
			torrentcomp.sortcol = listviewconst.sortcolmap[plist->iSubItem];
			std::sort(ownernodes.begin(), ownernodes.end(), torrentcomp);
			torrentcomp.asc = listviewconst.sortasc;
			ListView_SetItemCountEx(hList, ownernodes.size(), LVSICF_NOSCROLL);
		}
		break;
		case LISTCONTENT::GROUPLIST:
		{
			listviewconst.sortasc = listviewconst.sortasc == false;
			groupcomp.sortcol = listviewconst.sortcolmap[plist->iSubItem];
			std::sort(ownernodes.begin(), ownernodes.end(), groupcomp);
			groupcomp.asc = listviewconst.sortasc;
			ListView_SetItemCountEx(hList, ownernodes.size(), LVSICF_NOSCROLL);
		}
		break;
		case LISTCONTENT::TORRENTFILE:
		{
			listviewconst.sortasc = listviewconst.sortasc == false;
			filecomp.sortcol = listviewconst.sortcolmap[plist->iSubItem];
			std::sort(files.begin(), files.end(), filecomp);
			filecomp.asc = listviewconst.sortasc;
			ListView_SetItemCountEx(hList, files.size(), LVSICF_NOSCROLL);
		}
		break;
		}
	}
	break;
	case LVN_BEGINLABELEDIT:
	{
		lrt = 0;
	}
	break;
	case LVN_GETDISPINFO:
		ProcNotifyGetDispInfo(hWnd, message, wParam, lParam);
		break;
	case NM_CUSTOMDRAW:
		lrt = ProcNotifyCustomDraw(hWnd, message, wParam, lParam);
		break;
	}
	return lrt;
}

int FrameListView::CreateColumns(HWND hlist, struct ColDefVT* cols)
{
	LVCOLUMN ilc = { 0 };
	int ii = 0;
	int rtn;

	while (cols[ii].name[0]) {
		ilc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		ilc.pszText = cols[ii].name;
		ilc.cchTextMax = (int)wcslen(cols[ii].name);
		ilc.cx = cols[ii].width;

		rtn = ListView_InsertColumn(hlist, ii, &ilc);
		listviewconst.sortcolmap[rtn] = cols[ii].sortcol;

		ii++;
	}

	return 0;
}

int FrameListView::CreateGroupColumns()
{
	struct ColDefVT cols[] = {
	{L"Name", 200, LISTSORTCOL::NAME}
	, { L"Size", 100, LISTSORTCOL::SIZE }
	, { L"ViewSize", 100, LISTSORTCOL::SIZE }
	, { L"Count", 100, LISTSORTCOL::COUNT }
	, { L"Download/s", 100, LISTSORTCOL::DOWNSPEED}
	, { L"Upload/s", 100, LISTSORTCOL::UPSPEED}
	, {0}
	};

	CreateColumns(hList, cols);

	return 0;
}

int FrameListView::CreateTorrentListColumns()
{
	struct ColDefVT cols[] = {
	{ L"ID", 30, LISTSORTCOL::ID }
	, { L"Name", 200, LISTSORTCOL::NAME }
	, { L"Size", 100, LISTSORTCOL::SIZE }
	, { L"ViewSize", 100, LISTSORTCOL::SIZE }
	, { L"Tracker", 100, LISTSORTCOL::TRACKER }
	, { L"Download", 50, LISTSORTCOL::DOWNSPEED }
	, { L"Upload", 50, LISTSORTCOL::UPSPEED }
	, { L"Status", 50, LISTSORTCOL::STATUS }
	, { L"Ratio", 50, LISTSORTCOL::RATIO }
	, { L"Location", 100, LISTSORTCOL::LOCATION }
	, {0}
	};

	CreateColumns(hList, cols);

	return 0;
}

int FrameListView::CreateTorrentFileColumns()
{
	struct ColDefVT cols[] = {
	{ L"ID", 30, LISTSORTCOL::ID }
	, { L"Include", 200, LISTSORTCOL::STATUS }
	, { L"Priority", 100, LISTSORTCOL::NONE }
	, { L"Path", 100, LISTSORTCOL::LOCATION }
	, { L"Filename", 100, LISTSORTCOL::NAME }
	, { L"Size", 50, LISTSORTCOL::SIZE }
	, { L"Downloaded", 50, LISTSORTCOL::DOWNLOAD }
	, {0}
	};

	CreateColumns(hList, cols);

	return 0;
}

int FrameListView::CreateTorrentColumns()
{
	struct ColDefVT cols[] = {
		{ L"Parameter", 150 }
		, { L"Value", 200 }
		, {0}
	};

	CreateColumns(hList, cols);
	return 0;
}

int splitColList(WCHAR* colstr, const std::wstring& delim, std::vector<std::wstring>& lstcol)
{
	std::wstring wcs = colstr;
	size_t ppos = 0;
	size_t cpos = wcs.find(delim.at(0), ppos);
	while (cpos != std::wstring::npos) {
		lstcol.push_back(wcs.substr(ppos, cpos - ppos));
		ppos = cpos + 1;
		cpos = wcs.find(delim.at(0), ppos);
	}
	if (ppos < wcs.length()) {
		lstcol.push_back(wcs.substr(ppos));
	}
	return 0;
}

int FrameListView::UpdateTorrentContentWidth(HWND hlist)
{
	RECT rct;
	BOOL bst = GetWindowRect(hlist, &rct);
	if (bst) {
		ListView_SetColumnWidth(hlist, 0, LVSCW_AUTOSIZE);
		int colw = ListView_GetColumnWidth(hlist, 0);
		int cclw = rct.right - rct.left - colw;
		ListView_SetColumnWidth(hlist, 1, cclw);
	}

	return 0;
}

int FrameListView::UpdateTorrentContent(TorrentNodeVT* node)
{
	//UpdateTorrentContentTitle(node);
	//UpdateTorrentContentData(node);
	//UpdateTorrentContentWidth(hList);
	return 0;
}

int FrameListView::UpdateTorrentContentData(TorrentNodeVT * node)
{
	ListView_RedrawItems(hList, 4, 7);
	ListView_RedrawItems(hList, 8, 17);
	ListView_RedrawItems(hList, 19, 21);
	ListView_RedrawItems(hList, 26, 27);
	return 0;
}

//int FrameListView::UpdateTorrentContentData(TorrentNodeVT* node)
//{
//	wchar_t wbuf[128];
//	int iti = 0;
//
//	wsprintf(wbuf, L"%d", node->id);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	ListView_SetItemText(hList, iti, 1, (LPWSTR)node->name.c_str());
//	iti++;
//	FormatNumberView(wbuf, 128, node->size);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatNumberByteView(wbuf, 128, node->size);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatNumberByteView(wbuf + 64, 64, node->downspeed);
//	wsprintf(wbuf, L"%s/s", wbuf + 64);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatNumberByteView(wbuf + 64, 64, node->upspeed);
//	wsprintf(wbuf, L"%s/s", wbuf + 64);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	wsprintf(wbuf, L"%d", node->status);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatViewStatus(wbuf, 128, node->status);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	ListView_SetItemText(hList, iti, 1, (LPWSTR)node->path.fullpath.c_str());
//	iti++;
//	FormatTimeSeconds(wbuf + 64, 64, node->downloadTime);
//	wsprintf(wbuf, L"%s (%d s)", wbuf + 64, node->downloadTime);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatTimeSeconds(wbuf + 64, 64, node->seedTime);
//	wsprintf(wbuf, L"%s (%d s)", wbuf + 64, node->seedTime);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	ListView_SetItemText(hList, iti, 1, (LPWSTR)node->error.c_str());
//	iti++;
//	FormatDualByteView(wbuf, 128, node->downloaded);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatDualByteView(wbuf, 128, node->uploaded);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatNumberByteView(wbuf, 128, node->corrupt);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatViewDouble(wbuf, 128, node->ratio);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	FormatNumberView(wbuf + 64, 64, node->leftsize);
//	double dlp = node->size > 0 ? ((double)node->leftsize) / node->size * 100 : 0;
//	FormatViewDouble(wbuf + 96, 32, dlp);
//	wsprintf(wbuf, L"%s (%s%%)", wbuf + 64, wbuf + 96);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	double dpc = node->leftsize > 0 ? ((double)node->desired) / node->leftsize : 0;
//	FormatNumberView(wbuf + 32, 32, node->desired);
//	FormatViewDouble(wbuf + 64, 64, dpc * 100);
//	wsprintf(wbuf, L"%s (%s%%)", wbuf + 32, wbuf + 64);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	wsprintf(wbuf, L"%s", node->privacy ? L"Yes" : L"No");
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	wsprintf(wbuf, L"%s", node->done ? L"Yes" : L"No");
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//	wsprintf(wbuf, L"%s", node->stalled ? L"Yes" : L"No");
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//
//	FormatIntegerDate(wbuf, 128, node->donedate);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//
//	FormatIntegerDate(wbuf, 128, node->activedate);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//
//	FormatIntegerDate(wbuf, 128, node->startdate);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//
//	FormatIntegerDate(wbuf, 128, node->adddate);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//
//	wsprintf(wbuf, L"%d", node->piececount);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//
//	FormatDualByteView(wbuf, 128, node->piecesize);
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//
//	ListView_SetItemText(hList, iti, 1, (LPWSTR)node->pieces.c_str());
//	iti++;
//
//	if (node->trackers.count == 1) {
//		wsprintf(wbuf, L"%s", node->trackers.items[0].url.c_str());
//	}
//	else {
//		FormatNormalNumber(wbuf, 128, node->trackers.count);
//	}
//	ListView_SetItemText(hList, iti, 1, wbuf);
//	iti++;
//
//	return 0;
//}

WCHAR szTorrentDetailColumns[] = L"ID;Name;Size;Size (View);Download Rate;Upload Rate;Status;Status(View);\
Path;Download Time;Seed Time;Error;Downloaded Size;Uploaded Size;Corrupt Size;Ratio;\
Left Size;Available;Privacy;Finished;Stalled;Done Date;Activity Date;Start Date;\
Added Date;Piece Count;Piece Size;Piece Status;Tracker(s)";

int FrameListView::UpdateTorrentContentTitle()
{
	if (torrentcontenttitles.size() <= 0) {
		splitColList(szTorrentDetailColumns, L";", torrentcontenttitles);
		//LVITEM lvi;
		//int lvc = 0;
		//lvi.mask = LVIF_TEXT | LVIF_PARAM;
		//std::for_each(collist.begin(), collist.end(), [&lvi, &lvc, this](std::wstring& col) {
		//	lvi.pszText = (LPWSTR)col.c_str();
		//	lvi.cchTextMax = (int)col.length();
		//	lvi.iItem = lvc;
		//	lvi.iSubItem = 0;
		//	lvc = ListView_InsertItem(hList, &lvi);
		//	lvc++;
		//});
	}
	return torrentcontenttitles.size();
}

int FrameListView::UpdateGroupView(TorrentGroupVT* group)
{
	return 0;
}

int FrameListView::AutoHeaderWidth()
{
	HWND hhd = ListView_GetHeader(hList);
	if (hhd) {
		int hdc = Header_GetItemCount(hhd);
		if (hdc > 0) {
			for (int ii = 0; ii < hdc; ii++) {
				ListView_SetColumnWidth(hList, ii, LVSCW_AUTOSIZE);
			}
		}
	}

	return 0;
}

int FrameListView::ClearList()
{
	ListView_DeleteAllItems(hList);

	HWND hhd = ListView_GetHeader(hList);
	if (hhd) {
		int hdc = Header_GetItemCount(hhd);
		if (hdc > 0) {
			for (int ii = 0; ii < hdc; ii++) {
				ListView_DeleteColumn(hList, 0);
			}
		}
	}

	ownernodes.clear();
	listviewconst.sortcolmap.clear();


	return 0;
}

HWND FrameListView::GetWnd()
{
	return hList;
}

ViewNode * FrameListView::GetCurrentViewNode()
{
	return listviewconst.currentListNode;
}

int FrameListView::SetCurrentViewNode(ViewNode * vnode)
{
	listviewconst.currentListNode = vnode;
	switch (vnode->type) {
	case VG_TYPE::GROUPGROUP:
		ChangeViewNodeGroupGroup(vnode);
		break;
	case VG_TYPE::GROUP:
		ChangeViewNodeGroup(vnode);
		break;
	case VG_TYPE::TORRENT:
		ChangeViewNodeTorrent(vnode);
		break;
	case VG_TYPE::SESSION:
		ChangeViewNodeSession(vnode);
		break;
	case VG_TYPE::FILE:
		ChangeViewNodeFile(vnode);
		break;
	}
	return 0;
}

void FrameListView::ChangeViewNodeFile(ViewNode * vnode)
{
	ClearList();
	ChangeContent(LISTCONTENT::TORRENTFILE);
	CreateTorrentFileColumns();
	files.clear();
	vnode->GetFileNodes(files);
	ListView_SetItemCountEx(hList, files.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
}

void FrameListView::ChangeViewNodeSession(ViewNode * vnode)
{
	ClearList();
	ChangeContent(LISTCONTENT::SESSION);
}

void FrameListView::ChangeViewNodeTorrent(ViewNode * vnode)
{
	ClearList();
	ChangeContent(LISTCONTENT::TORRENTDETAIL);
	CreateTorrentColumns();
	int cts = UpdateTorrentContentTitle();
	ListView_SetItemCountEx(hList, cts, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
}


void FrameListView::ChangeViewNodeGroupGroup(ViewNode * vnode)
{
	ClearList();
	ChangeContent(LISTCONTENT::GROUPLIST);
	CreateGroupColumns();
	std::set<ViewNode*> grps;
	vnode->GetGroupNodes(grps);
	ownernodes.insert(ownernodes.begin(), grps.begin(), grps.end());
	ListView_SetItemCountEx(hList, ownernodes.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
}

void FrameListView::ChangeViewNodeGroup(ViewNode * vnode)
{
	ClearList();
	ChangeContent(LISTCONTENT::TORRENTLIST);
	CreateTorrentListColumns();
	std::set<ViewNode*> nodes;
	vnode->GetTorrentViewNodes(nodes);
	ownernodes.insert(ownernodes.begin(), nodes.begin(), nodes.end());
	ListView_SetItemCountEx(hList, ownernodes.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
}

LISTCONTENT FrameListView::ChangeContent(LISTCONTENT ctt)
{
	listviewconst.content = ctt;
	return ctt;
}

