#include "CViewTreeFrame.h"
#include "resource.h"

#include <sstream>

int FormatViewSize(wchar_t* buf, size_t bufsz, unsigned __int64 size);
int FormatByteSize(wchar_t* buf, size_t bufsz, unsigned __int64 size);

std::set<TreeParmData*> TreeItemParmDataHelper::parms;

TreeParmData* TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::TIPDTYPE type)
{
	TreeParmData* npd = new TreeParmData();
	npd->ItemType = type;
	parms.insert(npd);
	return npd;
}

int TreeItemParmDataHelper::ClearTreeItemParmData()
{
	for (std::set<TreeParmData*>::iterator itpd = parms.begin()
		; itpd != parms.end()
		; itpd++) {
		delete* itpd;
	}
	parms.clear();
	return 0;
}

CViewTreeFrame::CViewTreeFrame(HWND htree)
{
	hTree = htree;
}

CViewTreeFrame::~CViewTreeFrame()
{
	if (torrentsViewOrgs) {
		delete torrentsViewOrgs;
		torrentsViewOrgs = NULL;
	}
}

CViewTreeFrame::operator HWND() const
{
	return hTree;
}

TreeParmData* CViewTreeFrame::GetItemParm(HTREEITEM hti)
{
	TreeParmData* tpd = NULL;
	TVITEM tvi = { 0 };
	tvi.hItem = hti;
	tvi.mask = TVIF_PARAM;
	BOOL btn = TreeView_GetItem(hTree, &tvi);
	if (btn) {
		if (tvi.lParam) {
			tpd = (TreeParmData*)tvi.lParam;
		}
	}
	return tpd;
}

HTREEITEM CViewTreeFrame::GetSelectedItem()
{
	HTREEITEM hrt = NULL;
	hrt = TreeView_GetSelection(hTree);
	return hrt;
}
int CViewTreeFrame::UpdateTreeViewTorrentDetailNode(TorrentNode* node)
{
	TorrentParmItems* tpi = torrentsViewOrgs->GetNodeParmItems(node);
	UpdateTreeViewTorrentDetail(tpi);

	return 0;
}

int CViewTreeFrame::UpdateTreeViewTorrentDetail(TorrentParmItems* items)
{
	wchar_t tbuf[1024];
	TreeParmData* ntgip;
	TorrentNode* trt = NULL;

	TVITEM tvi = { 0 };
	tvi.hItem = items->torrent;
	tvi.mask = TVIF_PARAM;
	BOOL btn = TreeView_GetItem(hTree, &tvi);
	if (btn) {
		if (tvi.lParam) {
			TreeParmData* tip = (TreeParmData*)tvi.lParam;
			if (tip->ItemType == TreeParmData::Torrent) {
				trt = tip->node;
			}
		}
	}

	if (trt) {
		if (items->id == NULL) {
			TVINSERTSTRUCT tvi;

			tvi = { 0 };
			tbuf[0] = 0;
			tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
			tvi.item.pszText = tbuf;
			tvi.item.cchTextMax = 0;
			ntgip = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Attribute);
			tvi.item.lParam = (LPARAM)ntgip;
			ntgip->node = trt;
			tvi.hParent = items->torrent;
			items->size = TreeView_InsertItem(hTree, &tvi);
			items->viewsize = TreeView_InsertItem(hTree, &tvi);
			items->id = TreeView_InsertItem(hTree, &tvi);
			items->tracker = TreeView_InsertItem(hTree, &tvi);;
		}

		if (trt->updatetick > items->readtick) {
			ULARGE_INTEGER ltt;
			ltt.QuadPart = GetTickCount64();
			items->readtick = ltt.LowPart;

			TVITEM tiu = { 0 };
			tiu.mask = TVIF_TEXT;
			tiu.hItem = items->id;
			wsprintf(tbuf, L"ID: %d", trt->id);
			tiu.pszText = tbuf;
			tiu.cchTextMax = (int)wcslen(tbuf);
			TreeView_SetItem(hTree, &tiu);

			FormatViewSize(tbuf + 512, 512, trt->size);
			wsprintf(tbuf, L"Size: %s", tbuf + 512);
			tiu.cchTextMax = (int)wcslen(tbuf);
			tiu.hItem = items->size;
			TreeView_SetItem(hTree, &tiu);

			FormatByteSize(tbuf + 512, 512, trt->size);
			wsprintf(tbuf, L"Size (View): %s", tbuf + 512);
			tiu.cchTextMax = (int)wcslen(tbuf);
			tiu.hItem = items->viewsize;
			TreeView_SetItem(hTree, &tiu);

			wsprintf(tbuf, L"%s (%s)", trt->name.c_str(), tbuf + 512);
			tiu.cchTextMax = (int)wcslen(tbuf);
			tiu.hItem = items->torrent;
			TreeView_SetItem(hTree, &tiu);

			wsprintf(tbuf, L"Trackers (%d)", trt->trackerscount);
			tiu.cchTextMax = (int)wcslen(tbuf);
			tiu.hItem = items->tracker;
			TreeView_SetItem(hTree, &tiu);


			if (trt->trackerscount > 0) {
				UpdateTreeViewTorrentDetailTrackers(items, trt);
			}

			if (items->file == NULL) {
				if (trt->files.node) {
					items->file = UpdateTreeViewTorrentDetailFiles(items->torrent, trt->files.node);
				}
			}
		}
	}
	return 0;
}

HTREEITEM CViewTreeFrame::UpdateTreeViewTorrentDetailFiles(HTREEITEM hpnt, TorrentNodeFileNode* fnode)
{
	HTREEITEM hnt = NULL;
	if (fnode->sub.size() > 0) {
		WCHAR wbuf[1024];
		TVINSERTSTRUCT tvi = { 0 };
		tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
		if (fnode->type == TorrentNodeFileNode::DIR) {
			std::set<TorrentNodeFileNode*> fls;
			TorrentNodeHelper::GetNodeFileSet(fnode, fls);
			wsprintf(wbuf, L"%s (%d)", fnode->name.c_str(), fls.size());
		}
		else {
			wsprintf(wbuf, L"%s", fnode->name.c_str());
		}
		tvi.item.pszText = (LPWSTR)wbuf;
		tvi.item.cchTextMax = (int)wcslen(tvi.item.pszText);
		tvi.hParent = hpnt;
		TreeParmData* ntgip = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::File);
		ntgip->file = fnode;
		tvi.item.lParam = (LPARAM)ntgip;
		hnt = TreeView_InsertItem(hTree, &tvi);
		for (std::set<TorrentNodeFileNode*>::iterator itfn = fnode->sub.begin()
			; itfn != fnode->sub.end()
			; itfn++) {
			UpdateTreeViewTorrentDetailFiles(hnt, *itfn);
		}
	}
	return hnt;
}

HTREEITEM CViewTreeFrame::UpdateTreeViewTorrentDetailTrackers(TorrentParmItems* items, TorrentNode* node)
{
	HTREEITEM hnt = NULL;
	TVINSERTSTRUCT tvi = { 0 };
	TVITEM tim = { 0 };
	TreeParmData* tpd;

	if (node->trackerscount > 0) {
		if (items->tracker) {
			if (node->trackerscount > 1) {
				hnt = (HTREEITEM)::SendMessage(hTree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)items->tracker);
				for (unsigned int ii = 0; ii < node->trackerscount; ii++) {
					if (hnt == NULL) {
						tvi.item.mask = TVIF_PARAM;
						tvi.hParent = items->tracker;
						tpd = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Tracker);
						tpd->node = node;
						tvi.item.lParam = (LPARAM)tpd;
						hnt = TreeView_InsertItem(hTree, &tvi);
					}
					if (hnt) {
						tim.mask = TVIF_TEXT;
						tim.hItem = hnt;
						tim.pszText = (LPWSTR)node->trackers[ii].c_str();
						tim.cchTextMax = (int)wcslen(tim.pszText);
						TreeView_SetItem(hTree, &tim);
					}
					hnt = (HTREEITEM)::SendMessage(hTree, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hnt);
				}
			}
			else {
				tim.hItem = items->tracker;
				tim.mask = TVIF_PARAM;
				TreeView_GetItem(hTree, &tim);

				TreeParmData* tpd = (TreeParmData*)tim.lParam;
				tpd->ItemType = TreeParmData::Tracker;
				tpd->node = node;

				tim.mask = TVIF_TEXT;
				tim.hItem = items->tracker;
				tim.pszText = (LPWSTR)node->trackers[0].c_str();
				tim.cchTextMax = (int)wcslen(tim.pszText);
				TreeView_SetItem(hTree, &tim);
			}
		}
	}

	return hnt;
}

int CViewTreeFrame::GetTreeSelectedFiles(std::set<TorrentNodeFileNode*>& trfs)
{
	HTREEITEM hti = TreeView_GetSelection(hTree);
	std::wstringstream wss;

	if (hti) {
		TVITEM tvi = { 0 };
		tvi.hItem = hti;
		tvi.mask = TVIF_PARAM;
		BOOL btn = TreeView_GetItem(hTree, &tvi);
		if (btn) {
			if (tvi.lParam) {
				TreeParmData* tip = (TreeParmData*)tvi.lParam;
				if (tip->ItemType == TreeParmData::File) {
					TorrentNodeHelper::GetNodeFileSet(tip->file, trfs);
				}
			}
		}
	}
	return 0;
}

int CViewTreeFrame::GetTreeSelectedTorrents(std::set<TorrentNode*>& trts)
{
	HTREEITEM hti = TreeView_GetSelection(hTree);
	std::wstringstream wss;

	if (hti) {
		TVITEM tvi = { 0 };
		tvi.hItem = hti;
		tvi.mask = TVIF_PARAM;
		BOOL btn = TreeView_GetItem(hTree, &tvi);
		if (btn) {
			if (tvi.lParam) {
				TreeParmData* tip = (TreeParmData*)tvi.lParam;
				if (tip->ItemType == TreeParmData::Torrent) {
					trts.insert(tip->node);
				}
				else if (tip->ItemType == TreeParmData::Group) {
					tip->group->GetNodes(trts);
				}
			}
		}
	}
	return 0;
}

void CViewTreeFrame::UpdateViewRemovedTorrents()
{
	int rid;
	std::set<HTREEITEM> rts;
	TorrentNode* trt;

	if (analyzer->removed.size() > 0) {
		HTREEITEM hsi = TreeView_GetSelection(hTree);
		TVITEM tvi = { 0 };

		for (std::list<int>::iterator itrm = analyzer->removed.begin()
			; itrm != analyzer->removed.end()
			; itrm++) {
			rid = *itrm;
			rts.clear();
			trt = analyzer->GetTorrentNodeById(rid);
			if (trt) {
				torrentsViewOrgs->getTorrentItems(trt, rts);

				for (std::set<HTREEITEM>::iterator itti = rts.begin()
					; itti != rts.end()
					; itti++) {
					if (*itti == hsi) {
						PostMessage(hMain, WM_U_CLEARLISTVIEW, 0, 0);
					}

					tvi.hItem = *itti;
					tvi.mask = TVIF_PARAM;
					BOOL btn = TreeView_GetItem(hTree, &tvi);
					if (btn) {
						TreeParmData* tip = (TreeParmData*)tvi.lParam;
						if (tip->ItemType == TreeParmData::Torrent) {
							TorrentNode* node = tip->node;
							PostMessage(hMain, WM_U_DELETELISTNODE, (WPARAM)node, 0);
						}
					}


					TreeView_DeleteItem(hTree, *itti);
				}
				torrentsViewOrgs->removeItem(trt);
			}
			analyzer->RemoveTorrent(rid);
		}
		analyzer->removed.clear();
	}
}

int CViewTreeFrame::TreeViewSelectItem(TorrentNode* node)
{
	HTREEITEM hti = torrentsViewOrgs->findTreeItem(node);

	if (hti) {
		TreeView_Select(hTree, hti, TVGN_CARET);
		TreeView_Select(hTree, hti, TVGN_FIRSTVISIBLE);
	}
	return 0;
}

LRESULT CViewTreeFrame::ProcNotifyTree(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lrt = 0;
	NMTREEVIEW* pntree = (NMTREEVIEW*)lParam;

	switch (pntree->hdr.code)
	{
	case TVN_SELCHANGED:
	{
		if (pntree->itemNew.hItem)
		{
			if (pntree->itemNew.lParam) {
				TreeParmData* tipd = reinterpret_cast<TreeParmData*>(pntree->itemNew.lParam);
				if (tipd->ItemType == TreeParmData::Group) {
					::PostMessage(hMain, WM_U_UPDATETORRENTGROUP, (WPARAM)tipd->group, 0);
					TreeView_Expand(hTree, pntree->itemNew.hItem, TVE_EXPAND);
				}
				else if (tipd->ItemType == TreeParmData::Torrent) {
					::PostMessage(hMain, WM_U_UPDATETORRENTNODEDETAIL, (WPARAM)tipd->group, 0);
					TreeView_Expand(hTree, pntree->itemNew.hItem, TVE_EXPAND);
					PostMessage(hMain, WM_U_REFRESHTORRENTDETAIL, (WPARAM)tipd->node, 0);
				}
				else if (tipd->ItemType == TreeParmData::Session) {
					::PostMessage(hMain, WM_U_UPDATESESSION, (WPARAM)tipd->session, 0);
				}
				else if (tipd->ItemType == TreeParmData::File) {
					::PostMessage(hMain, WM_U_UPDATETORRENTNODEFILE, (WPARAM)tipd->file, 0);
				}
			}
		}
	}
	break;
	case NM_DBLCLK:
	{
		HTREEITEM htv = TreeView_GetSelection(hTree);
		WCHAR wbuf[1024];

		if (htv) {
			TVITEM tvi = { 0 };
			tvi.hItem = htv;
			tvi.mask = TVIF_PARAM | TVIF_TEXT;
			tvi.pszText = wbuf;
			tvi.cchTextMax = 1024;
			BOOL btn = TreeView_GetItem(hTree, &tvi);
			if (btn) {
				TreeParmData* tpd = (TreeParmData*)tvi.lParam;
				if (tpd->ItemType == TreeParmData::Tracker) {
					TorrentGroup* grp = analyzer->GetTrackerGroup(tvi.pszText);
					if (grp) {
						TreeViewSelectItem(grp);
					}
				}
			}
		}
	}
	break;
	case NM_CUSTOMDRAW:
	{
		LPNMTVCUSTOMDRAW pntd = (LPNMTVCUSTOMDRAW)lParam;
		lrt = CDRF_DODEFAULT;
		switch (pntd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			lrt = CDRF_NOTIFYITEMDRAW;
			break;
		case CDDS_ITEMPREPAINT:
		{
			TreeParmData* tpd = (TreeParmData*)pntd->nmcd.lItemlParam;
			if (tpd->ItemType == TreeParmData::Torrent) {
				if (tpd->node->status == 0) {
					pntd->clrTextBk = RGB(0xCC, 0xCC, 0xCC);
				}
				if (tpd->node->status == 4) {
					pntd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
					pntd->clrText = RGB(0x00, 0x3F, 0x00);
				}
				else if (tpd->node->error.length() > 0) {
					pntd->clrText = RGB(0xFF, 0x00, 0x00);
				}
			}
		}
		break;
		}
	}
	break;
	}
	return lrt;
}

int CViewTreeFrame::TreeViewSelectItem(TorrentGroup* grp)
{
	HTREEITEM hti = torrentsViewOrgs->findTreeItem(grp);
	if (hti) {
		TreeView_Select(hTree, hti, TVGN_CARET);
		TreeView_Select(hTree, hti, TVGN_FIRSTVISIBLE);
	}
	return 0;
}

int CViewTreeFrame::ProcContextMenuTree(int xx, int yy)
{
	RECT rct;
	POINT pnt = { xx, yy };
	HTREEITEM hti = TreeView_GetSelection(hTree);
	if (hti) {
		*(HTREEITEM*)&rct = hti;
		BOOL bst = TreeView_GetItemRect(hTree, hti, &rct, TRUE);
		//bst = ((*(HTREEITEM*)&rct = hti), (BOOL)SendMessage(hTree, TVM_GETITEMRECT, TRUE, (LPARAM)& rct));
		if (bst) {
			if (PtInRect(&rct, pnt)) {
				HINSTANCE hinst = ::GetModuleHandle(NULL);
				HMENU hcm = LoadMenu(hinst, MAKEINTRESOURCE(IDR_CONTEXTPOPS));
				if (hcm) {
					HMENU hsm = GetSubMenu(hcm, 0);
					if (hsm) {
						ClientToScreen(hTree, &pnt);
						bst = TrackPopupMenu(hsm, TPM_LEFTALIGN | TPM_LEFTBUTTON, pnt.x, pnt.y, 0, hMain, NULL);
						DestroyMenu(hsm);
					}
				}
			}
		}
	}
	return 0;
}

void CViewTreeFrame::UpdateTreeViewGroup(TreeGroupShadow* gti)
{
	TVINSERTSTRUCT tis;
	wchar_t tbuf[1024];
	TorrentGroup* grp = gti->group;
	TreeParmData* ntipd;
	TorrentParmItems* ims;
	std::map<unsigned long, HTREEITEM>::iterator ittt;
	HTREEITEM hti;
	TorrentNode* node;

	for (std::map<unsigned long, TorrentNode*>::iterator ittn = grp->torrents.begin()
		; ittn != grp->torrents.end()
		; ittn++) {
		node = ittn->second;
		hti = UpdateTreeViewNodeItem(gti, node);
		if (hti) {
			ims = gti->GetNodeParmItems(node);
			UpdateTreeViewTorrentDetail(ims);
		}
	}

	TreeGroupShadow* ntg;
	std::map<TorrentGroup*, TreeGroupShadow*>::iterator itgg;
	TorrentGroup* ttg;
	LARGE_INTEGER itt;

	for (std::map<std::wstring, TorrentGroup*>::iterator ittg = grp->subs.begin()
		; ittg != grp->subs.end()
		; ittg++) {
		ttg = ittg->second;
		ntg = torrentsViewOrgs->GetTreeGroupShadow(ttg);
		if (ntg) {
			if (ntg->hnode == NULL) {
				tis = { 0 };
				tis.item.mask = TVIF_TEXT | TVIF_PARAM;
				FormatByteSize(tbuf + 512, 512, ttg->size);
				wsprintf(tbuf, L"(%s) %s", tbuf + 512, ttg->name.c_str());
				tis.item.cchTextMax = (int)wcslen(tbuf);
				tis.item.pszText = (LPWSTR)tbuf;
				ntipd = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Group);
				ntipd->group = ttg;
				tis.hParent = gti->hnode;
				tis.item.lParam = (LPARAM)ntipd;

				hti = TreeView_InsertItem(hTree, &tis);
				ntg->hnode = hti;
			}
			else {
				if (ttg->updatetick > ttg->readtick) {
					itt.QuadPart = GetTickCount64();
					ttg->readtick = itt.LowPart;
					TVITEM tvi;
					tvi.hItem = ntg->hnode;
					tvi.mask = TVIF_TEXT;
					FormatByteSize(tbuf + 512, 512, ittg->second->size);
					wsprintf(tbuf, L"(%s) %s", tbuf + 512, ittg->second->name.c_str());
					tvi.pszText = tbuf;
					tvi.cchTextMax = wcslen(tbuf);
					TreeView_SetItem(hTree, &tvi);
				}
			}
			UpdateTreeViewGroup(ntg);
		}
	}
}

void CViewTreeFrame::UpdateViewTorrentGroup(TorrentGroup* grp)
{
	wchar_t tbuf[1024];
	TreeGroupShadow* ngi;
	TreeParmData* npd;

	TVINSERTSTRUCT tvi;
	if (torrentsViewOrgs) {
		ngi = torrentsViewOrgs->GetTreeGroupShadow(grp);
		if (ngi) {
			if (ngi->hnode == NULL) {
				tvi = { 0 };
				wsprintf(tbuf, L"%s", grp->name.c_str());
				tvi.item.mask = TVIF_TEXT | TVIF_PARAM;
				tvi.item.cchTextMax = (int)wcslen(tbuf);
				tvi.item.pszText = (LPWSTR)tbuf;
				tvi.hParent = NULL;
				npd = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Session);
				npd->session = &analyzer->session;
				tvi.item.lParam = (LPARAM)npd;
				tvi.hInsertAfter = TVI_ROOT;
				ngi->hnode = TreeView_InsertItem(hTree, &tvi);
			}
			UpdateTreeViewGroup(ngi);

			TreeView_Expand(hTree, ngi->hnode, TVE_EXPAND);
		}
	}
}

HTREEITEM CViewTreeFrame::UpdateTreeViewNodeItem(TreeGroupShadow* tgs, TorrentNode* node)
{
	std::map<TorrentNode*, HTREEITEM>::iterator ittt = tgs->nodeitems.find(node);
	TVINSERTSTRUCT tis;
	wchar_t tbuf[1024];
	TreeParmData* ntipd;
	HTREEITEM hti;

	if (ittt == tgs->nodeitems.end()) {
		tis = { 0 };
		tis.item.mask = TVIF_TEXT | TVIF_PARAM;
		wsprintf(tbuf, L"%s", node->name.c_str());
		tis.item.cchTextMax = (int)wcslen(tbuf);
		tis.item.pszText = (LPWSTR)tbuf;
		tis.hParent = tgs->hnode;
		ntipd = TreeItemParmDataHelper::CreateTreeItemParmData(TreeParmData::Torrent);
		ntipd->node = node;
		tis.item.lParam = (LPARAM)ntipd;

		hti = TreeView_InsertItem(hTree, &tis);
		if (hti) {
			tgs->nodeitems[node] = hti;
		}
	}
	else {
		hti = ittt->second;
	}

	return hti;
}

int CViewTreeFrame::SyncGroup(TorrentGroup* group)
{
	if (torrentsViewOrgs == NULL) {
		torrentsViewOrgs = TreeGroupShadow::CreateTreeGroupShadow(group, NULL);
	}
	torrentsViewOrgs->SyncGroup();
	return 0;
}

int CViewTreeFrame::SelectTreeParentNode()
{
	HTREEITEM hst = TreeView_GetSelection(hTree);
	if (hst) {
		hst = TreeView_GetNextItem(hTree, hst, TVGN_PARENT);
		if (hst) {
			TreeView_Select(hTree, hst, TVGN_CARET);
			TreeView_Select(hTree, hst, TVGN_FIRSTVISIBLE);
		}
	}
	return 0;
}
