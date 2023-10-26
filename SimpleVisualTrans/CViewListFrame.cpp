#include "CViewListFrame.h"
#include "TransAnalyzer.h"
#include "resource.h"
#include "CProfileData.h"
#include "TransAnalyzer.h"

#include <sstream>

#define WM_U_TREESELECTNODE WM_USER + 0x123a
#define WM_U_TREESELECTGROUP WM_USER + 0x123b

int FormatNumberByteView(wchar_t* buf, size_t bufsz, unsigned __int64 size);
int FormatDualByteView(wchar_t* wbuf, int bsz, unsigned __int64 size);
int FormatNormalNumber(wchar_t* buf, size_t bufsz, long num);
int FormatNumberView(wchar_t* buf, size_t bufsz, unsigned __int64 size);
int FormatViewDouble(wchar_t* wbuf, size_t bsz, double dval);
int FormatViewStatus(wchar_t* wbuf, size_t bsz, long status);
int FormatViewDouble(wchar_t* wbuf, size_t bsz, double dval);
int FormatTimeSeconds(wchar_t* wbuf, size_t bsz, long secs);
int FormatIntegerDate(wchar_t* wbuf, size_t bsz, long mtt);

bool sortasc = true;

CViewListFrame::CViewListFrame(HWND hlist)
	:hList(hlist)
	, listContent(LISTCONTENT::NONE)
{
	hbRed = ::CreateSolidBrush(RGB(0xFF, 0xCF, 0xCF));
	hbGreen = ::CreateSolidBrush(RGB(0xCF, 0xFF, 0xCF));
	hbWhite = ::CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));

	InitializeGradualPen();
	InitializeTorrentDetailTitle();
}

CViewListFrame::~CViewListFrame()
{
	for (std::list<ListParmData*>::iterator itlp = listparms.begin()
		; itlp != listparms.end()
		; itlp++) {
		delete* itlp;
	}
	listparms.clear();

	for (int ii = 0; ii < 256; ii++) {
		DeleteObject(gradualpen[ii]);
	}

	DeleteObject(hbRed);
	DeleteObject(hbGreen);
	DeleteObject(hbWhite);

	FreeTorrentDetailTitle();
}

void CViewListFrame::InitializeGradualPen()
{
	for (unsigned int ii = 0; ii < 256; ii++) {
		if (ii < 128) {
			gradualpen[ii] = CreatePen(PS_SOLID, 1, RGB(0xFF, 0x7F + ii, 0x7F));
		}
		else {
			gradualpen[ii] = CreatePen(PS_SOLID, 1, RGB(0xFF - (ii - 0x80), 0xFF, 0x7F));
		}
	}
}

void CViewListFrame::InitStyle()
{
	ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	HWND hhd = ListView_GetHeader(hList);
	if (hhd) {
		LONG_PTR lps = GetWindowLongPtr(hhd, GWL_STYLE);
		lps |= HDS_FLAT;
		SetWindowLongPtr(hhd, GWL_STYLE, lps);
	}

	HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	::SendMessage(hList, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
}

CViewListFrame::operator HWND() const
{
	return hList;
}

int CViewListFrame::TreeViewSelectItem(TorrentNode* node)
{
	PostMessage(hMain, WM_U_TREESELECTNODE, (WPARAM)node, 0);
	return 0;
}

int CViewListFrame::TreeViewSelectItem(TorrentGroup* group)
{
	PostMessage(hMain, WM_U_TREESELECTGROUP, (WPARAM)group, 0);
	return 0;
}


int CViewListFrame::GetListSelectedTorrents(std::set<TorrentNode*>& tts)
{
	int sit = -1;
	bool keepseek = true;
	LVITEM lvi = { 0 };
	BOOL btn;
	ListParmData* lpd;

	if (listContent == LISTCONTENT::TORRENTLIST) {
		while (keepseek) {
			sit = ListView_GetNextItem(hList, sit, LVNI_SELECTED);
			if (sit >= 0) {
				lvi.iItem = sit;
				lvi.mask = LVIF_PARAM;
				btn = ListView_GetItem(hList, &lvi);
				if (btn) {
					lpd = (ListParmData*)lvi.lParam;
					if (lpd->type == ListParmData::Node) {
						tts.insert(lpd->node);
					}
				}
			}
			else {
				keepseek = false;
			}
		}

	}
	return 0;
}

int CViewListFrame::GetListSelectedFiles(std::set<TorrentNodeFileNode*>& tfs)
{
	int sit = -1;
	bool keepseek = true;
	LVITEM lvi = { 0 };
	BOOL btn;
	ListParmData* lpd;
	if (listContent == LISTCONTENT::TORRENTFILE) {
		while (keepseek) {
			sit = ListView_GetNextItem(hList, sit, LVNI_SELECTED);
			if (sit >= 0) {
				lvi.iItem = sit;
				lvi.mask = LVIF_PARAM;
				btn = ListView_GetItem(hList, &lvi);
				if (btn) {
					lpd = (ListParmData*)lvi.lParam;
					if (lpd->type == ListParmData::File) {
						tfs.insert(lpd->file);
					}
				}
			}
			else {
				keepseek = false;
			}
		}

	}
	return 0;
}

int CViewListFrame::UpdateListViewSession(SessionInfo* ssn)
{
	if (listContent == LISTCONTENT::SESSIONINFO) {
		wchar_t wbuf[128];
		int iti = 0;

		wsprintf(wbuf, L"%d", ssn->torrentcount);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatNumberByteView(wbuf, 128, ssn->downloaded);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatNumberByteView(wbuf, 128, ssn->uploaded);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatNumberByteView(wbuf + 64, 64, ssn->downloadspeed);
		wsprintf(wbuf, L"%s/s", wbuf + 64);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;

		FormatNumberByteView(wbuf + 64, 64, ssn->uploadspeed);
		wsprintf(wbuf, L"%s/s", wbuf + 64);
		ListView_SetItemText(hList, iti, 1, wbuf);
		iti++;
	}

	return 0;
}

int CViewListFrame::ClearColumns()
{
	listContent = LISTCONTENT::NONE;
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
	return 0;
}

int CViewListFrame::DeleteNodeItem(TorrentNode* node)
{
	if (listContent == LISTCONTENT::TORRENTLIST) {
		int lic = ListView_GetItemCount(hList);
		ListParmData* lpd;
		LVITEM lvi;
		BOOL btn;
		for (int ii=0; ii<lic; ii++) {
			lvi.mask = LVIF_PARAM;
			lvi.iItem = ii;
			btn = ListView_GetItem(hList, &lvi);
			if (btn) {
				lpd = (ListParmData*)lvi.lParam;
				if (lpd->type == ListParmData::LIPDTYPE::Node) {
					if (lpd->node == node) {
						ListView_DeleteItem(hList, ii);
						break;
					}
				}
			}
		}
	}

	return 0;
}

int CViewListFrame::ProcContextMenuList(int xx, int yy)
{
	POINT pnt = { xx, yy };
	BOOL bst;
	if ((listContent == LISTCONTENT::TORRENTLIST)
		|| (listContent == LISTCONTENT::TORRENTFILE)) {
		HINSTANCE hinst = ::GetModuleHandle(NULL);
		HMENU hcm = LoadMenu(hinst, MAKEINTRESOURCE(IDR_CONTEXTPOPS));
		if (hcm) {
			HMENU hsm = GetSubMenu(hcm, 1);
			if (hsm) {
				ClientToScreen(hList, &pnt);
				bst = TrackPopupMenu(hsm, TPM_LEFTALIGN | TPM_LEFTBUTTON, pnt.x, pnt.y, 0, hMain, NULL);
				DestroyMenu(hsm);
			}
		}
	}
	return 0;
}

int CViewListFrame::MakeListParmIdle(ListParmData* lpd)
{
	lpd->idle = true;
	listparms.remove(lpd);
	listparms.push_front(lpd);
	return 0;
}

#define INTCOMP(I1,I2) (I1)<(I2)?-1:((I1)>(I2)?1:0)

int CALLBACK LVComp_Nodes(LPARAM lp1, LPARAM lp2, LPARAM lpsort)
{
	ListParmData* lpd1 = (ListParmData*)lp1;
	ListParmData* lpd2 = (ListParmData*)lp2;

	TorrentNode* nd1 = lpd1->node;
	TorrentNode* nd2 = lpd2->node;
	long iitem = (long)lpsort;

	int icr = 0;

	if (iitem == 0) {
		icr = INTCOMP(nd1->id, nd2->id);
	}
	else if (iitem == 1) {
		icr = nd1->name.compare(nd2->name);
	}
	else if ((iitem == 2) || (iitem == 3)) {
		icr = INTCOMP(nd1->size, nd2->size);
	}
	else if (iitem == 4) {
		if (nd1->trackerscount > 0) {
			if (nd2->trackerscount > 0) {
				icr = nd1->trackers[0].compare(nd2->trackers[0]);
			}
			else {
				icr = 1;
			}
		}
		else {
			if (nd2->trackerscount > 0) {
				icr = -1;
			}
			else {
				icr = 0;
			}
		}
	}
	else if (iitem == 5) {
		icr = INTCOMP(nd1->downspeed, nd2->downspeed);
	}
	else if (iitem == 6) {
		icr = INTCOMP(nd1->upspeed, nd2->upspeed);
	}
	else if (iitem == 7) {
		icr = INTCOMP(nd1->status, nd2->status);
	}
	else if (iitem == 8) {
		icr = INTCOMP(nd1->ratio, nd2->ratio);
	}

	icr *= sortasc ? 1 : -1;
	return icr;
}

int CALLBACK LVComp_Group(LPARAM lp1, LPARAM lp2, LPARAM lpsort)
{
	ListParmData* lpd1 = (ListParmData*)lp1;
	ListParmData* lpd2 = (ListParmData*)lp2;

	TorrentGroup* nd1 = lpd1->group;
	TorrentGroup* nd2 = lpd2->group;
	long iitem = (long)lpsort;
	int icr = 0;

	switch (iitem) {
	case 0:
		icr = nd1->name.compare(nd2->name);
		break;
	case 1:
	case 2:
		icr = nd1->size > nd2->size ? 1 : (nd1->size < nd2->size ? -1 : 0);
		break;
	case 3:
	{
		size_t nd1s = nd1->GetTorrentsCount();
		size_t nd2s = nd2->GetTorrentsCount();

		icr = nd1s > nd2s ? 1 : (nd1s < nd2s ? -1 : 0);
	}
	break;
	case 4:
		icr = nd1->downspeed > nd2->downspeed ? 1 : (nd1->downspeed < nd2->downspeed ? -1 : 0);
		break;
	case 5:
		icr = nd1->upspeed > nd2->upspeed ? 1 : (nd1->upspeed < nd2->upspeed ? -1 : 0);
		break;
	default:
		break;
	}

	icr *= sortasc ? 1 : -1;

	return icr;
}

int CALLBACK LVComp_Files(LPARAM lp1, LPARAM lp2, LPARAM lpsort)
{
	int icr = 0;
	ListParmData* lpd1 = (ListParmData*)lp1;
	ListParmData* lpd2 = (ListParmData*)lp2;

	TorrentNodeFileNode* nd1 = lpd1->file;
	TorrentNodeFileNode* nd2 = lpd2->file;
	long iitem = (long)lpsort;

	if (iitem == 0) {
		icr = INTCOMP(nd1->id, nd2->id);
	}
	else if (iitem == 3) {
		icr = nd1->path.compare(nd2->path);
	}
	else if (iitem == 4) {
		icr = nd1->name.compare(nd2->name);
	}
	else if (iitem == 5) {
		icr = INTCOMP(nd1->file->size, nd2->file->size);
	}
	else if (iitem == 6) {
		icr = INTCOMP(nd1->file->size > 0 ? ((double)nd1->file->donesize) / nd1->file->size : 0, nd2->file->size > 0 ? ((double)nd2->file->donesize) / nd2->file->size : 0);
	}

	icr *= sortasc ? 1 : -1;
	return icr;
}

LRESULT CViewListFrame::ProcNotifyList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	NMLISTVIEW* plist = (NMLISTVIEW*)lParam;

	switch (plist->hdr.code)
	{
	case LVN_DELETEITEM:
	{
		ListParmData* lpd = (ListParmData*)plist->lParam;
		MakeListParmIdle(lpd);
	}
	break;
	case LVN_COLUMNCLICK:
	{
		if (listContent == LISTCONTENT::TORRENTLIST) {
			sortasc = sortasc == false;
			ListView_SortItems(hList, LVComp_Nodes, plist->iSubItem);
			std::wstringstream wss;
			wss << plist->iSubItem;
			profile->SetDefaultValue(L"LocalListSort", wss.str());
			wss.str(std::wstring());
			wss << sortasc;
			profile->SetDefaultValue(L"LocalSortDirection", wss.str());
		}
		else if (listContent == LISTCONTENT::TORRENTGROUP) {
			sortasc = sortasc == false;
			ListView_SortItems(hList, LVComp_Group, plist->iSubItem);
			std::wstringstream wss;
			wss << plist->iSubItem;
			profile->SetDefaultValue(L"LocalGroupSort", wss.str());
			wss.str(std::wstring());
			wss << sortasc;
			profile->SetDefaultValue(L"LocalSortDirection", wss.str());
		}
		else if (listContent == LISTCONTENT::TORRENTFILE) {
			sortasc = sortasc == false;
			ListView_SortItems(hList, LVComp_Files, plist->iSubItem);

			std::wstringstream wss;
			wss << plist->iSubItem;
			profile->SetDefaultValue(L"LocalListFileSort", wss.str());
			wss.str(std::wstring());
			wss << sortasc;
			profile->SetDefaultValue(L"LocalSortDirection", wss.str());

		}
	}
	break;
	case NM_DBLCLK:
	{
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
		LVITEM lit = { 0 };
		lit.mask = LVIF_PARAM;
		lit.iItem = lpnmitem->iItem;
		lit.iSubItem = lpnmitem->iSubItem;
		ListParmData* lpd;

		ListView_GetItem(hList, &lit);
		lpd = (ListParmData*)lit.lParam;
		switch (listContent) {
		case LISTCONTENT::TORRENTGROUP:
			if (lpd->type == ListParmData::Group) {
				TreeViewSelectItem(lpd->group);
			}
			break;
		case LISTCONTENT::TORRENTLIST:
			if (lpd->type == ListParmData::Node) {
				TreeViewSelectItem(lpd->node);
			}
			break;
		case LISTCONTENT::TORRENTDETAIL:
			if (lpd->type == ListParmData::Tracker) {
				TorrentNode* node = lpd->node;
				if (node->trackerscount == 1) {
					TorrentGroup* group = analyzer->GetTrackerGroup(node->trackers[0]);
					if (group) {
						TreeViewSelectItem(group);
					}
				}
			}
		}
	}
	break;
	case NM_CUSTOMDRAW:
	{
		LPNMLVCUSTOMDRAW plcd = (LPNMLVCUSTOMDRAW)lParam;
		switch (listContent) {
		case LISTCONTENT::TORRENTDETAIL:
			lrt = ProcCustDrawListDetail(plcd);
			break;
		case LISTCONTENT::TORRENTLIST:
			lrt = ProcCustDrawListNodes(plcd);
			break;
		case LISTCONTENT::TORRENTFILE:
			lrt = ProcCustDrawListFile(plcd);
			break;
		}
	} // customdraw
	} // notify messages
	return lrt;
}

LRESULT CViewListFrame::ProcCustDrawListNodes(LPNMLVCUSTOMDRAW& plcd)
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
			ListParmData* ctn = (ListParmData*)plcd->nmcd.lItemlParam;
			if (ctn->type == ListParmData::Node) {
				lrt = ctn->node->error.length() > 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
				lrt = ctn->node->status == 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
				lrt = ctn->node->status == 4 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
			}
		}
	}
	break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
	{
		ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
		if (lpd->type == ListParmData::Node) {
			TorrentNode* ctn = lpd->node;
			if (ctn->status == 0) {
				plcd->clrTextBk = RGB(0xCF, 0xCF, 0xCF);
			}
			if (ctn->status == 4) {
				int nameisub = 1;
				if (plcd->iSubItem == nameisub) {
					RECT src;
					ListView_GetSubItemRect(hList, plcd->nmcd.dwItemSpec, nameisub, LVIR_BOUNDS, &src);
					FillRect(plcd->nmcd.hdc, &src, hbGreen);
					src.left += 2 * GetSystemMetrics(SM_CXEDGE);
					//InflateRect(&src, -1, -1);
					if (lpd->node->size > 0) {
						RECT urc(src);
						urc.right = (long)((src.right - src.left) * lpd->node->leftsize / lpd->node->size) + src.left;
						FillRect(plcd->nmcd.hdc, &urc, hbRed);
						//LVITEM lvi = { 0 };
						//lvi.iSubItem = 1;
						//lvi.pszText = wbuf;
						//lvi.cchTextMax = 1024;
						//int tls = ::SendMessage(hFileList, LVM_GETITEMTEXT, plcd->nmcd.dwItemSpec, (LPARAM)&lvi);
						DrawText(plcd->nmcd.hdc, lpd->node->name.c_str(), lpd->node->name.length(), &src, DT_LEFT);
						lrt = CDRF_SKIPDEFAULT;
					}
				}

				if (lrt != CDRF_SKIPDEFAULT) {
					plcd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
					plcd->clrText = RGB(0x00, 0x3F, 0x00);
				}
			}
			if (ctn->error.length() > 0) {
				plcd->clrText = RGB(0xFF, 0x00, 0x00);
			}
		}
	}
	break;
	}
	return lrt;
}

LRESULT CViewListFrame::ProcCustDrawListDetail(LPNMLVCUSTOMDRAW& plcd)
{
	LRESULT lrt = CDRF_DODEFAULT;
	switch (plcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		lrt = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
		switch (plcd->nmcd.dwItemSpec) {
		case 1:
		case 6:
		case 7:
		case 11:
		{
			ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;

			TorrentNode* ctn = lpd->node;
			lrt = ctn->status == 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
			lrt = ctn->error.length() > 0 ? CDRF_NOTIFYSUBITEMDRAW : lrt;
		}
		break;
		case 27:
			lrt = CDRF_NOTIFYSUBITEMDRAW;
			break;
		default:
			lrt = CDRF_DODEFAULT;
			break;
		}
		break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
		lrt = CDRF_DODEFAULT;
		switch (plcd->nmcd.dwItemSpec) {
		case 27:
			if (plcd->iSubItem == 1)
			{
				lrt = ProcCustDrawListPieces(plcd);
			}
			break;
		case 1:
		case 6:
		case 7:
		{
			ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;

			TorrentNode* ctn = lpd->node;
			if (ctn->status == 0) {
				plcd->clrTextBk = RGB(0xCC, 0xCC, 0xCC);
			}
		}
		case 11:
		{
			ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
			TorrentNode* ctn = lpd->node;
			if (ctn->error.length() > 0) {
				plcd->clrText = RGB(0xFF, 0x00, 0x00);
			}
		}
		break;
		}
	}

	return lrt;
}

LRESULT CViewListFrame::ProcCustDrawListName(const LPNMLVCUSTOMDRAW& plcd, const COLORREF& ocr)
{
	LRESULT lrt = CDRF_DODEFAULT;
	RECT src;
	ListView_GetSubItemRect(hList, plcd->nmcd.dwItemSpec, 1, LVIR_BOUNDS, &src);
	int imm = 3 * GetSystemMetrics(SM_CXEDGE);
	src.left += imm;
	if (src.right > src.left) {
		ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
		TorrentNode* ctn = lpd->node;
		DrawText(plcd->nmcd.hdc, ctn->name.c_str(), (DWORD)ctn->name.length(), &src, DT_LEFT);
		SetTextColor(plcd->nmcd.hdc, ocr);
		lrt = CDRF_SKIPDEFAULT;
	}

	return lrt;
}

LRESULT CViewListFrame::ProcCustDrawListPieces(const LPNMLVCUSTOMDRAW& plcd)
{
	RECT src;
	HGDIOBJ hpo;
	ListView_GetSubItemRect(hList, plcd->nmcd.dwItemSpec, 1, LVIR_BOUNDS, &src);
	src.left += 3 * GetSystemMetrics(SM_CXEDGE);
	InflateRect(&src, -1, -1);
	hpo = SelectObject(plcd->nmcd.hdc, gradualpen[0]);
	ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
	TorrentNode* ctn = lpd->node;
	int ide = 0;
	double mgs;
	int ipi = 0;
	unsigned int spi = 0;
	int idx = 0;
	int cwd = src.right - src.left;
	HPEN hpen;
	LRESULT lrt = CDRF_DODEFAULT;
	if (ctn->pieces.length() > 0) {
		if (cwd > 0) {
			while (spi < ctn->piececount) {
				ide += ctn->piecesdata[spi];
				idx++;
				spi++;
				mgs = spi * cwd;
				mgs /= ctn->piececount;

				if (ipi < (int)mgs) {
					ide = ide * 255 / idx;
					hpen = gradualpen[ide];
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

LRESULT CViewListFrame::ProcCustDrawListFile(LPNMLVCUSTOMDRAW& plcd)
{
	LRESULT lrt = CDRF_DODEFAULT;
	switch (plcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		lrt = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
	{
		ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
		TorrentNodeFileNode* cfn = lpd->file;
		plcd->clrTextBk = cfn->check ? plcd->clrTextBk : RGB(0xCF, 0xCF, 0xCF);
		plcd->clrText = cfn->priority > 0 ? RGB(0x00, 0x7F, 0x00) : (cfn->priority < 0 ? RGB(0x3F, 0x3F, 0xCF) : plcd->clrText);
	}
	break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
	{
		ListParmData* lpd = (ListParmData*)plcd->nmcd.lItemlParam;
		TorrentNode* ctn = lpd->node;
		if (ctn->status == 0) {
			plcd->clrTextBk = RGB(0xCF, 0xCF, 0xCF);
		}
		if (ctn->status == 4) {
			plcd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
			//plcd->clrFace = RGB(0xCF, 0xFF, 0xCF);
			plcd->clrText = RGB(0x00, 0x3F, 0x00);
		}
		if (ctn->error.length() > 0) {
			plcd->clrText = RGB(0xFF, 0x00, 0x00);
		}
	}
	break;
	}
	return lrt;
}

ListParmData* CViewListFrame::GetListParmData(unsigned long idx)
{
	ListParmData* lpd = NULL;

	if (listparms.size() > 0) {
		lpd = *listparms.begin();
		if (lpd->idle) {
			lpd->idle = false;
			listparms.pop_front();
			listparms.push_back(lpd);
		}
		else {
			lpd = NULL;
		}
	}

	if (lpd == NULL) {
		lpd = new ListParmData();
		lpd->idle = false;
		listparms.push_back(lpd);
	}

	return lpd;
}

int CViewListFrame::CreateListColumnsSession()
{
	struct ColDef cols[] = {
	{ L"Session", 150 }
	, { L"Value", 200 }
	, {0}
	};

	CreateListColumns(cols);
	listContent = LISTCONTENT::SESSIONINFO;
	return 0;
}

int CViewListFrame::CreateListColumnsFiles()
{
	struct ColDef cols[] = {
	{ L"Id", 50}
	, { L"In", 50 }
	, { L"Priority", 50}
	, { L"Path", 150}
	, { L"File", 150 }
	, { L"Size", 200 }
	, { L"Downloaded Size", 200 }
	, {0}
	};

	CreateListColumns(cols);
	listContent = LISTCONTENT::TORRENTFILE;
	return 0;
}

int CViewListFrame::CreateListColumnsTorrent()
{
	struct ColDef cols[] = {
	{ L"Parameter", 150 }
	, { L"Value", 200 }
	, {0}
	};

	CreateListColumns(cols);
	listContent = LISTCONTENT::TORRENTDETAIL;
	return 0;
}

int CViewListFrame::CreateListColumns(struct ColDef* cols)
{
	LVCOLUMN ilc = { 0 };
	int ii = 0;
	int rtn;

	while (cols[ii].name[0]) {
		ilc.mask = LVCF_TEXT | LVCF_WIDTH;
		ilc.pszText = cols[ii].name;
		ilc.cchTextMax = (int)wcslen(cols[ii].name);
		ilc.cx = cols[ii].width;

		rtn = ListView_InsertColumn(hList, ii, &ilc);

		ii++;
	}

	return 0;
}

int CViewListFrame::UpdateListViewTorrentFileDetail(HWND hlist, int fii, TorrentNodeFileNode* cfl)
{
	WCHAR wbuf[128];
	LVITEM lvi;

	lvi.mask = LVIF_TEXT;
	wsprintf(wbuf, L"%d", cfl->id);
	lvi.pszText = wbuf;
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	lvi.iItem = fii;
	lvi.iSubItem = 0;
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 1;
	lvi.pszText = (LPWSTR)(cfl->check ? L"Yes" : L"No");
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 2;
	wsprintf(wbuf, L"%d", cfl->priority);
	lvi.pszText = (LPWSTR)wbuf;
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 3;
	lvi.pszText = (LPWSTR)cfl->path.c_str();
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 4;
	lvi.pszText = (LPWSTR)cfl->name.c_str();
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 5;
	FormatNumberView(wbuf, 128, cfl->file->size);
	lvi.pszText = wbuf;
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = fii;
	lvi.iSubItem = 6;
	FormatNumberView(wbuf + 32, 32, cfl->done);
	double dsp = cfl->file->size > 0 ? ((double)cfl->done) / cfl->file->size : 0;
	FormatViewDouble(wbuf + 64, 64, dsp * 100);
	wsprintf(wbuf, L"%s (%s%%)", wbuf + 32, wbuf + 64);
	lvi.pszText = wbuf;
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	ListView_SetItem(hlist, &lvi);

	return 0;
}

int CViewListFrame::UpdateListViewTorrentFiles(TorrentNodeFileNode* files)
{
	if (listContent == LISTCONTENT::TORRENTFILE) {
		TorrentNodeFileNode* cfl;
		std::set<TorrentNodeFileNode*> fst;
		LVFINDINFO lfi = { 0 };
		LVITEM lvi = { 0 };
		std::wstring wst;
		ULARGE_INTEGER llt;
		ListParmData* lpd;

		TorrentNodeHelper::GetNodeFileSet(files, fst);
		int lvc = ListView_GetItemCount(hList);
		for (int ii = 0; ii < lvc; ii++) {
			lvi.mask = LVIF_PARAM;
			lvi.iItem = ii;
			ListView_GetItem(hList, &lvi);

			lpd = (ListParmData*)lvi.lParam;
			if (lpd->type == ListParmData::File) {
				cfl = lpd->file;
				fst.erase(cfl);
				if (cfl->updatetick > cfl->readtick) {
					llt.QuadPart = GetTickCount64();
					cfl->readtick = llt.LowPart;
					UpdateListViewTorrentFileDetail(hList, ii, cfl);
				}
			}
		}
		for (std::set<TorrentNodeFileNode*>::iterator itfn = fst.begin()
			; itfn != fst.end()
			; itfn++) {
			cfl = *itfn;
			if (cfl->file) {
				lvi.mask = LVIF_TEXT;
				lvi.pszText = (LPWSTR)L"";
				lvi.cchTextMax = 0;
				lvi.iItem = lvc + 1;
				lvi.lParam = (LPARAM)cfl;
				lvi.iSubItem = 0;
				lvc = ListView_InsertItem(hList, &lvi);

				lvi.iItem = lvc;
				lvi.mask = LVIF_PARAM;
				lpd = GetListParmData(lvc);
				lpd->type = ListParmData::File;
				lpd->file = cfl;
				lvi.lParam = (LPARAM)lpd;
				ListView_SetItem(hList, &lvi);
				UpdateListViewTorrentFileDetail(hList, lvc, cfl);
			}
		}

		std::wstring vvv;
		profile->GetDefaultValue(L"LocalListFileSort", vvv);
		if (vvv.length() > 0) {
			std::wstringstream wss(vvv);
			int sit;
			wss >> sit;
			if (sit >= 0) {
				ListView_SortItems(hList, LVComp_Files, sit);
			}
		}
	}
	return 0;
}

int CViewListFrame::UpdateListViewTorrentDetailData(TorrentNode* node)
{
	wchar_t wbuf[128];
	int iti = 0;

	if (listContent == LISTCONTENT::TORRENTDETAIL) {
		if (node->updatetick > node->readtick) {
			ULARGE_INTEGER lit;
			lit.QuadPart = GetTickCount64();
			node->readtick = lit.LowPart;

			wsprintf(wbuf, L"%d", node->id);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			ListView_SetItemText(hList, iti, 1, (LPWSTR)node->name.c_str());
			iti++;
			FormatNumberView(wbuf, 128, node->size);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatNumberByteView(wbuf, 128, node->size);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatNumberByteView(wbuf + 64, 64, node->downspeed);
			wsprintf(wbuf, L"%s/s", wbuf + 64);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatNumberByteView(wbuf + 64, 64, node->upspeed);
			wsprintf(wbuf, L"%s/s", wbuf + 64);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			wsprintf(wbuf, L"%d", node->status);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatViewStatus(wbuf, 128, node->status);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			ListView_SetItemText(hList, iti, 1, (LPWSTR)node->path.c_str());
			iti++;
			FormatTimeSeconds(wbuf + 64, 64, node->downloadTime);
			wsprintf(wbuf, L"%s (%d s)", wbuf + 64, node->downloadTime);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatTimeSeconds(wbuf + 64, 64, node->seedTime);
			wsprintf(wbuf, L"%s (%d s)", wbuf + 64, node->seedTime);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			ListView_SetItemText(hList, iti, 1, (LPWSTR)node->error.c_str());
			iti++;
			FormatDualByteView(wbuf, 128, node->downloaded);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatDualByteView(wbuf, 128, node->uploaded);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatNumberByteView(wbuf, 128, node->corrupt);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatViewDouble(wbuf, 128, node->ratio);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			FormatNumberView(wbuf + 64, 64, node->leftsize);
			double dlp = node->size > 0 ? ((double)node->leftsize) / node->size * 100 : 0;
			FormatViewDouble(wbuf + 96, 32, dlp);
			wsprintf(wbuf, L"%s (%s%%)", wbuf + 64, wbuf + 96);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			double dpc = node->leftsize > 0 ? ((double)node->desired) / node->leftsize : 0;
			FormatNumberView(wbuf + 32, 32, node->desired);
			FormatViewDouble(wbuf + 64, 64, dpc * 100);
			wsprintf(wbuf, L"%s (%s%%)", wbuf + 32, wbuf + 64);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			wsprintf(wbuf, L"%s", node->privacy ? L"Yes" : L"No");
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			wsprintf(wbuf, L"%s", node->done ? L"Yes" : L"No");
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
			wsprintf(wbuf, L"%s", node->stalled ? L"Yes" : L"No");
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;

			FormatIntegerDate(wbuf, 128, node->donedate);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;

			FormatIntegerDate(wbuf, 128, node->activedate);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;

			FormatIntegerDate(wbuf, 128, node->startdate);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;

			FormatIntegerDate(wbuf, 128, node->adddate);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;

			wsprintf(wbuf, L"%d", node->piececount);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;

			FormatDualByteView(wbuf, 128, node->piecesize);
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;

			ListView_SetItemText(hList, iti, 1, (LPWSTR)node->pieces.c_str());
			iti++;

			if (node->trackerscount == 1) {
				wsprintf(wbuf, L"%s", node->trackers[0].c_str());
			}
			else {
				FormatNormalNumber(wbuf, 128, node->trackerscount);
			}
			ListView_SetItemText(hList, iti, 1, wbuf);
			iti++;
		}
	}
	return 0;
}

int CViewListFrame::ListUpdateFiles(TorrentNodeFileNode* files)
{
	ClearColumns();
	CreateListColumnsFiles();

	UpdateListViewTorrentFiles(files);

	return 0;
}

int CViewListFrame::ListUpdateSession(SessionInfo* ssn)
{
	ClearColumns();
	CreateListColumnsSession();

	ListUpdateSessionTitle(ssn);
	UpdateListViewSession(ssn);

	return 0;
}

int CViewListFrame::UpdateListViewTorrentDetail(TorrentNode* node)
{
	ClearColumns();
	CreateListColumnsTorrent();

	ListUpdateTorrentTitle(node);
	node->readtick = 0;
	UpdateListViewTorrentDetailData(node);

	::SendMessage(hList, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
	::SendMessage(hList, LVM_SETCOLUMNWIDTH, 1, LVSCW_AUTOSIZE_USEHEADER);

	return 0;
}

void CViewListFrame::ListUpdateTorrentTitle(TorrentNode* node)
{
	LVITEM lvi = { 0 };
	int iti = 0;
	ListParmData* lpd;

	int tts = _msize(torrentDetailTitles) / sizeof(wchar_t*);

	for (int ii = 0; ii < tts; ii++) {
		lvi.mask = LVIF_TEXT;
		lvi.pszText = (LPWSTR)torrentDetailTitles[ii];
		lvi.cchTextMax = (int)wcslen(lvi.pszText);
		lvi.iItem = iti;
		iti = ListView_InsertItem(hList, &lvi);

		lvi.iItem = iti;
		lvi.mask = LVIF_PARAM;
		lpd = GetListParmData(iti);
		lpd->type = ListParmData::Parameter;
		lpd->node = node;
		lvi.lParam = (LPARAM)lpd;
		ListView_SetItem(hList, &lvi);

		iti++;
	}

	lvi.mask = LVIF_TEXT;
	lvi.pszText = (LPWSTR)L"Tracker";
	lvi.cchTextMax = (int)wcslen(lvi.pszText);
	lvi.iItem = iti;
	iti = ListView_InsertItem(hList, &lvi);

	lvi.iItem = iti;
	lvi.mask = LVIF_PARAM;
	lpd = GetListParmData(iti);
	lpd->type = ListParmData::Tracker;
	lpd->node = node;
	lvi.lParam = (LPARAM)lpd;
	ListView_SetItem(hList, &lvi);
	iti++;
}

void CViewListFrame::ListUpdateSessionTitle(SessionInfo* ssn)
{
	LVITEM lvi = { 0 };
	int iti = 0;
	ListParmData* lpd;


	int tts = _msize(torrentSessionTitles) / sizeof(wchar_t*);

	for (int ii = 0; ii < tts; ii++) {
		lvi.mask = LVIF_TEXT;
		lvi.pszText = (LPWSTR)torrentSessionTitles[ii];
		lvi.cchTextMax = (int)wcslen(lvi.pszText);
		lvi.iItem = iti;
		iti = ListView_InsertItem(hList, &lvi);

		lvi.iItem = iti;
		lvi.mask = LVIF_PARAM;
		lpd = GetListParmData(iti);
		lpd->type = ListParmData::Parameter;
		lpd->session = ssn;
		lvi.lParam = (LPARAM)lpd;
		ListView_SetItem(hList, &lvi);

		iti++;
	}
}

void CViewListFrame::InitializeTorrentDetailTitle()
{
	wchar_t whs[][32] = {
		L"ID"
		,L"Name"
		,L"Size"
		,L"Size (View)"
		,L"Download Rate"
		,L"Upload Rate"
		,L"Status"
		,L"Status(View)"
		,L"Path"
		,L"Download Time"
		,L"Seed Time"
		,L"Error"
		,L"Downloaded Size"
		,L"Uploaded Size"
		,L"Corrupt Size"
		,L"Ratio"
		,L"Left Size"
		,L"Available"
		,L"Privacy"
		,L"Finished"
		,L"Stalled"
		,L"Done Date"
		,L"Activity Date"
		,L"Start Date"
		,L"Added Date"
		,L"Piece Count"
		,L"Piece Size"
		,L"Piece Status"
	};

	wchar_t whn[][32] = {
		L"Available Torrents"
		, L"Total Download"
		, L"Total Upload"
		, L"Download Speed"
		, L"Upload Speed"
	};

	int ays = sizeof(whs);
	ays /= sizeof(wchar_t);
	ays /= 32;
	int sln;
	torrentDetailTitles = (wchar_t**)malloc(ays * sizeof(wchar_t*));
	if (torrentDetailTitles) {
		for (int ii = 0; ii < ays; ii++) {
			sln = wcsnlen_s(whs[ii], 32);
			torrentDetailTitles[ii] = (wchar_t*)malloc((sln + 1) * sizeof(wchar_t*));
			wcscpy_s(torrentDetailTitles[ii], sln + 1, whs[ii]);
		}
	}

	ays = sizeof(whn);
	ays /= sizeof(wchar_t);
	ays /= 32;
	torrentSessionTitles = (wchar_t**)malloc(ays * sizeof(wchar_t*));
	if (torrentSessionTitles) {
		for (int ii = 0; ii < ays; ii++) {
			sln = wcsnlen_s(whn[ii], 32);
			torrentSessionTitles[ii] = (wchar_t*)malloc((sln + 1) * sizeof(wchar_t*));
			wcscpy_s(torrentSessionTitles[ii], sln + 1, whn[ii]);
		}
	}
}

int CViewListFrame::FreeTorrentDetailTitle()
{
	int tsz = _msize(torrentDetailTitles);
	tsz /= sizeof(wchar_t*);

	if (tsz > 0) {
		for (int ii = 0; ii < tsz; ii++) {
			free(torrentDetailTitles[ii]);
		}
		free(torrentDetailTitles);
		torrentDetailTitles = NULL;
	}

	tsz = _msize(torrentSessionTitles);
	tsz /= sizeof(wchar_t*);

	if (tsz > 0) {
		for (int ii = 0; ii < tsz; ii++) {
			free(torrentSessionTitles[ii]);
		}
		free(torrentSessionTitles);
		torrentSessionTitles = NULL;
	}
	return 0;
}

int CViewListFrame::CBUpdateGroupNode(bool, void*, TorrentGroup*, TorrentNode*)
{
	return 0;
}

int CViewListFrame::UpdateListViewTorrentNodes(TorrentGroup* grp)
{
	std::set<TorrentNode*> nodes;
	ListParmData* lpd;
	wchar_t wbuf[1024];

	grp->GetNodes(nodes);

	int itc = ListView_GetItemCount(hList);
	LVITEM lvi = { 0 };
	for (int ii = 0; ii < itc; ii++) {
		lvi.iItem = ii;
		lvi.mask = LVIF_PARAM;
		ListView_GetItem(hList, &lvi);
		lpd = (ListParmData*)lvi.lParam;
		if (lpd->type == ListParmData::Node) {
			if (nodes.find(lpd->node) != nodes.end()) {
				nodes.erase(lpd->node);
				if (lpd->node->updatetick > lpd->node->readtick) {
					UpdateListViewTorrentNodesDetail(hList, ii, lpd->node);
				}
			}
		}
	}
	itc++;
	for (std::set<TorrentNode*>::iterator itnd = nodes.begin()
		; itnd != nodes.end()
		; itnd++) {
		if ((*itnd)->valid) {
			lvi.mask = LVIF_TEXT;
			wsprintf(wbuf, L"%d", (*itnd)->id);
			lvi.pszText = wbuf;
			lvi.cchTextMax = (int)wcslen(lvi.pszText);
			lvi.iItem = itc;
			itc = ListView_InsertItem(hList, &lvi);

			lvi.iItem = itc;
			lvi.mask = LVIF_PARAM;
			lpd = GetListParmData(itc);
			lpd->type = ListParmData::Node;
			lpd->node = *itnd;
			lvi.lParam = (LPARAM)lpd;
			ListView_SetItem(hList, &lvi);

			UpdateListViewTorrentNodesDetail(hList, itc, *itnd);
			itc++;
		}
	}

	std::wstring vvv;
	profile->GetDefaultValue(L"LocalListSort", vvv);
	if (vvv.length() > 0) {
		std::wstringstream wss(vvv);
		int sit;
		wss >> sit;
		if (sit >= 0) {
			ListView_SortItems(hList, LVComp_Nodes, sit);
		}
	}

	return 0;
}

int CViewListFrame::UpdateListViewTorrentNodesDetail(HWND hlist, int iti, TorrentNode* nod)
{
	wchar_t wbuf[128];
	int isub = 0;

	FormatNumberView(wbuf, 128, nod->id);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	ListView_SetItemText(hlist, iti, isub, (LPWSTR)nod->name.c_str());
	isub++;

	FormatNumberView(wbuf, 128, nod->size);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	FormatNumberByteView(wbuf, 128, nod->size);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	if (nod->trackerscount > 0) {
		ListView_SetItemText(hlist, iti, isub, (LPWSTR)nod->trackers[0].c_str());
	}
	isub++;

	FormatNumberByteView(wbuf, 128, nod->downspeed);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	FormatNumberByteView(wbuf, 128, nod->upspeed);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	FormatViewStatus(wbuf, 128, nod->status);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	FormatViewDouble(wbuf, 128, nod->ratio);
	ListView_SetItemText(hlist, iti, isub, wbuf);
	isub++;

	ULARGE_INTEGER lit;
	lit.QuadPart = GetTickCount64();
	nod->readtick = lit.LowPart;

	return 0;
}

int CViewListFrame::UpdateListViewGroups()
{
	if (listContent == LISTCONTENT::TORRENTGROUP) {
		int lsc = ListView_GetItemCount(hList);
		LVITEM lvi;
		BOOL btn;
		ListParmData* lpd;

		for (int ii = 0; ii < lsc; ii++) {
			lvi.mask = LVIF_PARAM;
			lvi.iItem = ii;

			btn = ListView_GetItem(hList, &lvi);
			if (btn) {
				lpd = (ListParmData*)lvi.lParam;
				if (lpd->type == ListParmData::Group) {
					UpdateListViewTorrentGroupData(hList, ii, lpd->group);
				}
			}
		}

		std::wstring vvv;
		profile->GetDefaultValue(L"LocalGroupSort", vvv);
		if (vvv.length() > 0) {
			std::wstringstream wss(vvv);
			int sit;
			wss >> sit;
			if (sit >= 0) {
				ListView_SortItems(hList, LVComp_Group, sit);
			}
		}

	}
	return 0;
}

int CViewListFrame::UpdateListViewTorrentGroupData(HWND hlist, int iti, TorrentGroup* nod)
{
	wchar_t wbuf[128];
	int itt = 0;
	itt++;

	FormatNumberView(wbuf, 128, nod->size);
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	FormatNumberByteView(wbuf, 128, nod->size);
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	wsprintf(wbuf, L"%d", nod->GetTorrentsCount());
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	FormatNumberByteView(wbuf + 64, 64, nod->downspeed);
	wsprintf(wbuf, L"%s/s", wbuf + 64);
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	FormatNumberByteView(wbuf + 64, 64, nod->upspeed);
	wsprintf(wbuf, L"%s/s", wbuf + 64);
	ListView_SetItemText(hlist, iti, itt, wbuf);
	itt++;

	return 0;
}

int CViewListFrame::CreateListColumnsGroupList()
{
	struct ColDef cols[] = {
		{L"Name", 200}
		, { L"Size", 100 }
		, { L"ViewSize", 100 }
		, { L"Count", 100}
		, { L"Download/s", 100}
		, { L"Upload/s", 100}
		, {0}
	};

	CreateListColumns(cols);
	listContent = LISTCONTENT::TORRENTGROUP;
	return 0;
}

int CViewListFrame::CreateListColumnsNodeList()
{
	struct ColDef cols[] = {
		{ L"ID", 30 }
		, { L"Name", 200 }
		, { L"Size", 100 }
		, { L"ViewSize", 100 }
		, { L"Tracker", 100 }
		, { L"Download", 50 }
		, { L"Upload", 50 }
		, { L"Status", 50 }
		, { L"Ratio", 50 }
		, {0}
	};

	CreateListColumns(cols);
	listContent = LISTCONTENT::TORRENTLIST;
	return 0;
}

int CViewListFrame::UpdateListViewTorrentGroup(TorrentGroup* grp)
{

	LVITEM lvi = { 0 };
	int iti = 0;
	ListParmData* lpd;

	ClearColumns();

	if (grp->subs.size() > 0) {
		CreateListColumnsGroupList();
		for (std::map<std::wstring, TorrentGroup*>::iterator itgp = grp->subs.begin()
			; itgp != grp->subs.end()
			; itgp++) {
			lvi.mask = LVIF_TEXT;
			lvi.pszText = (LPWSTR)itgp->second->name.c_str();
			lvi.cchTextMax = (int)wcslen(lvi.pszText);
			lvi.iItem = iti;
			iti = ListView_InsertItem(hList, &lvi);

			lvi.iItem = iti;
			lvi.mask = LVIF_PARAM;
			lpd = GetListParmData(iti);
			lpd->type = ListParmData::Group;
			lpd->group = itgp->second;
			lvi.lParam = (LPARAM)lpd;
			ListView_SetItem(hList, &lvi);

			UpdateListViewTorrentGroupData(hList, iti, itgp->second);
			iti++;
		}
	}
	else {
		CreateListColumnsNodeList();
		UpdateListViewTorrentNodes(grp);

		std::wstring vvv;
		profile->GetDefaultValue(L"LocalListSort", vvv);
		if (vvv.length() > 0) {
			std::wstringstream wss(vvv);
			int sit;
			wss >> sit;
			if (sit >= 0) {
				ListView_SortItems(hList, LVComp_Nodes, sit);
			}
		}
	}
	return 0;
}
