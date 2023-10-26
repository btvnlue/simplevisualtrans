#include "WindowViewCY.h"
#include "Resource.h"
#include "CSplitWnd.h"
#include "TransmissionManager.h"
#include "TransmissionProfile.h"
#include "ViewManager.h"
#include "CPanelWindow.h"
#include "RibbonHandlerMain.h"

#include <UIRibbon.h>
#include <CommCtrl.h>
#include <Windows.h>
#include <windowsx.h>

#pragma comment(lib, "Comctl32.lib")

#define MAX_LOADSTRING 100
#define WM_U_INITIALIZEWINDOW WM_USER + 0x111
#define WM_U_RIBBONCOMMAND	WM_USER + 0x112
#define WM_U_INITIALIZESERVICE WM_USER + 0x113
#define WM_U_PROFILELOADED WM_USER + 0x114
#define WM_U_REFRESHPROFILETORRENTS WM_USER + 0x115
#define WM_U_INPUTCOMMAND WM_USER + 0x121
#define WM_U_ADDLINKBYDROP WM_USER + 0x123

ATOM windowViewCYClass = 0;
WCHAR szWindowClass[MAX_LOADSTRING] = { 0 };            // the main window class name
WindowViewCY* window = NULL;

class WVCYRibbonHandler : public IUICommandHandler
{
	HWND hParent;
	unsigned long refcnt;
	unsigned int callbackMessage;
public:
	WVCYRibbonHandler(HWND hwnd, UINT cbMsg)
		: hParent(hwnd)
		, refcnt(0)
		, callbackMessage(cbMsg)
	{}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
	{
		HRESULT hr = S_OK;

		if (((riid == __uuidof(IUnknown))
			|| (riid == __uuidof(IUICommandHandler)))) {
			AddRef();
			*ppvObject = this;
		}
		else {
			*ppvObject = NULL;
			hr = E_NOINTERFACE;
		}
		return hr;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return InterlockedIncrement(&refcnt);
	}
	virtual ULONG STDMETHODCALLTYPE Release()
	{
		ULONG rct = InterlockedDecrement(&refcnt);
		if (rct == 0) {
			delete this;
		}
		return rct;
	}
	virtual /* [annotation] */
		_Check_return_
		HRESULT STDMETHODCALLTYPE Execute(
			UINT32 commandId,
			UI_EXECUTIONVERB verb,
			/* [annotation][in] */
			_In_opt_  const PROPERTYKEY *key,
			/* [annotation][in] */
			_In_opt_  const PROPVARIANT *currentValue,
			/* [annotation][in] */
			_In_opt_  IUISimplePropertySet *commandExecutionProperties)
	{
		switch (verb) {
		case UI_EXECUTIONVERB_EXECUTE:
			PostMessage(hParent, callbackMessage, commandId, 0);
			break;
		}
		return S_OK;
	}

	virtual /* [annotation] */
		_Check_return_
		HRESULT STDMETHODCALLTYPE UpdateProperty(
			UINT32 commandId,
			/* [in] */ REFPROPERTYKEY key,
			/* [annotation][in] */
			_In_opt_  const PROPVARIANT *currentValue,
			/* [out] */ PROPVARIANT *newValue)
	{
		return E_NOTIMPL;
	}

};

class WVCYRibbonApplication : public IUIApplication
{
	unsigned long refcnt;
	HWND hParent;
	WVCYRibbonHandler* handler;
	unsigned int callbackMessage;
public:
	WVCYRibbonApplication(HWND hwnd, UINT cbMsg)
		:hParent(hwnd)
		, handler(NULL)
		, refcnt(0)
		, callbackMessage(cbMsg)
	{}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
	{
		HRESULT hr = S_OK;

		if (((riid == __uuidof(IUnknown))
			|| (riid == __uuidof(IUIApplication)))) {
			AddRef();
			*ppvObject = this;
		}
		else {
			*ppvObject = NULL;
			hr = E_NOINTERFACE;
		}
		return hr;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return InterlockedIncrement(&refcnt);
	}

	virtual ULONG STDMETHODCALLTYPE Release()
	{
		ULONG rct = InterlockedDecrement(&refcnt);
		if (rct == 0) {
			delete this;
		}
		return rct;
	}
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged(
		UINT32 viewId,
		UI_VIEWTYPE typeID,
		/* [in] */ IUnknown *view,
		UI_VIEWVERB verb,
		INT32 uReasonCode)
	{
		//switch (verb) {
		//case UI_VIEWVERB_SIZE:
		//	PostMessage(hParent, WM_U_RESIZECONTENT, 0, 0);
		//	break;
		//}
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand(
		UINT32 commandId,
		UI_COMMANDTYPE typeID,
		/* [out] */ IUICommandHandler **commandHandler)
	{
		HRESULT hr = S_OK;
		if (handler == NULL) {
			handler = new WVCYRibbonHandler(hParent, callbackMessage);
		}

		if (handler) {
			hr = handler->QueryInterface(__uuidof(IUICommandHandler), (void**)commandHandler);
		}
		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand(
		UINT32 commandId,
		UI_COMMANDTYPE typeID,
		/* [annotation][in] */
		_In_opt_  IUICommandHandler *commandHandler)
	{
		if (commandHandler) {
			commandHandler->Release();
		}
		return S_OK;
	}
};

class DropTargetWindowsView : public IDropTarget
{
	static ULONG refcount;
	HWND hWnd;
public:
	DropTargetWindowsView(HWND hwnd) {
		hWnd = hwnd;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		refcount++;
		return refcount;
	}
	virtual ULONG STDMETHODCALLTYPE Release()
	{
		refcount--;
		if (refcount == 0) {
			delete this;
			return 0;
		}
		return refcount;
	}
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (riid == IID_IUnknown) {
			AddRef();
			*ppvObject = this;
		}
		else if (riid == IID_IDropTarget) {
			AddRef();
			*ppvObject = this;
		}
		else {
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE DragEnter(
		/* [unique][in] */ __RPC__in_opt IDataObject* pDataObj,
		/* [in] */ DWORD grfKeyState,
		/* [in] */ POINTL pt,
		/* [out][in] */ __RPC__inout DWORD* pdwEffect)
	{
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE DragOver(
		/* [in] */ DWORD grfKeyState,
		/* [in] */ POINTL pt,
		/* [out][in] */ __RPC__inout DWORD* pdwEffect)
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE DragLeave(void)
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Drop(
		/* [unique][in] */ __RPC__in_opt IDataObject* pDataObj,
		/* [in] */ DWORD grfKeyState,
		/* [in] */ POINTL pt,
		/* [out][in] */ __RPC__inout DWORD* pdwEffect)
	{
		FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		STGMEDIUM medium;

		HRESULT hr = pDataObj->GetData(&fmte, &medium);
		if (SUCCEEDED(hr))
		{
			UINT const count = DragQueryFile((HDROP)medium.hGlobal, -1, NULL, 0);
			WCHAR* szPath;
			int cpc;
			for (unsigned int ii = 0; ii < count; ii++) {
				szPath = new WCHAR[MAX_PATH];
				cpc = DragQueryFile((HDROP)medium.hGlobal, ii, szPath, MAX_PATH);
				if (cpc > 0) {
					PostMessage(hWnd, WM_U_ADDLINKBYDROP, (WPARAM)szPath, 0);
				}
				else {
					delete szPath;
				}
			}
			ReleaseStgMedium(&medium);
			return S_OK;
		}

		fmte = { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		hr = pDataObj->GetData(&fmte, &medium);
		if (SUCCEEDED(hr))
		{
			WCHAR* szPath;
			void* vpd = GlobalLock(medium.hGlobal);
			if (vpd) {
				size_t tln = wcslen((WCHAR*)vpd);
				szPath = new WCHAR[tln + 1];
				wcscpy_s(szPath, tln + 1, (WCHAR*)vpd);
				PostMessage(hWnd, WM_U_ADDLINKBYDROP, (WPARAM)szPath, 0);
			}
			GlobalUnlock(medium.hGlobal);
			ReleaseStgMedium(&medium);
			return S_OK;
		}

		return S_OK;
	}

};

ULONG DropTargetWindowsView::refcount = 0;

WindowViewCY::WindowViewCY()
	:active(TRUE)
	, hContent(NULL)
	, clipboardEnabled(false)
{
}

WindowViewCY::~WindowViewCY()
{
	U_Close();
}

HWND WindowViewCY::CreateWindowView(HINSTANCE hinst)
{
	active = TRUE;
	hInst = hinst;
	int rtn;
	if (windowViewCYClass == 0) {
		if (szWindowClass[0] == 0) {
			rtn = LoadStringW(hInst, IDC_SIMPLEVISUALTRANSCYCLASS, szWindowClass, MAX_LOADSTRING);
		}
		windowViewCYClass = RegisterWindowViewClass(hInst);
	}
	window = this;

	HANDLE hwv = CreateThread(NULL, 0, ThWindowViewCY, this, 0, &thId);
	while (hWnd == NULL) {
		Sleep(100);
	}

	return hWnd;

}

int WindowViewCY::U_InitializeDragDrop(HWND hwnd)
{
	IDropTarget* idt = new DropTargetWindowsView(hwnd);
	HRESULT hr = RegisterDragDrop(hwnd, idt);
	if (hr == S_OK) {
		piidrop = idt;
	}
	else {
		delete idt;
	}
	return hr == S_OK ? 0 : -1;
}

BOOL WindowViewCY::IsActive()
{
	return active;
}

int WindowViewCY::Close()
{
	return 0;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//	
ATOM WindowViewCY::RegisterWindowViewClass(HINSTANCE hInst)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProcS;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInst;
	wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SIMPLEVISUALTRANSCY));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SIMPLEVISUALTRANSCY);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	ATOM wvc = RegisterClassExW(&wcex);
	return wvc;
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
int WindowViewCY::InitializeInstance(HINSTANCE hInst)
{
	WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
	LoadStringW(hInst, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

	hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInst, nullptr);

	if (!hWnd)
	{
		return 1;
	}

	ShowWindow(hWnd, TRUE);
	UpdateWindow(hWnd);

	return 0;
}

DWORD WindowViewCY::ThWindowViewCY(LPVOID lpParameter)
{
	WindowViewCY* self = reinterpret_cast<WindowViewCY*>(lpParameter);

	// Perform application initialization:
	if (self->InitializeInstance(self->hInst) == 0)
	{

		HACCEL hAccelTable = LoadAccelerators(self->hInst, MAKEINTRESOURCE(IDC_SIMPLEVISUALTRANSCY));

		MSG msg;

		// Main message loop:
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		self->active = FALSE;
	}
	else {
		return 1;
	}
	return 0;
}

LRESULT WindowViewCY::WndProcS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (window) {
		return window->WndProc(hWnd, message, wParam, lParam);
	}
	else {
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}


// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

ItemArray<HPEN*> piececolorref;

int InitializePieceColorRef(ItemArray<HPEN*>& pcr)
{
	pcr.count = 256;
	pcr.items = new HPEN[pcr.count];
	int rfc;
	for (int ii = 0; ii < 128; ii++) {
		rfc = ii * ii / 0x7F; // refactor for contrast of color
		pcr.items[ii] = CreatePen(PS_SOLID, 1, RGB(0xFF, rfc * 0x7F / 0x7F + 0x7F, 0x7F));
		pcr.items[ii + 128] = CreatePen(PS_SOLID, 1, RGB(0xFF - rfc * 0x7F / 0x7F, 0xFF, 0));
	}
	return 0;
}

LRESULT WindowViewCY::ListProcessNotify(LPARAM lParam)
{
	LRESULT rtn = 0;
	LPNMHDR nmh = (LPNMHDR)lParam;

	switch (nmh->code) {
	case NM_CUSTOMDRAW:
	{
		if (view->currentnode) {
			if (view->currentnode->GetType() == VNT_GROUP) {
				NMLVCUSTOMDRAW* plvcd = (LPNMLVCUSTOMDRAW)lParam;
				if (plvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
					rtn = CDRF_NOTIFYITEMDRAW;
				}
				else if (plvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
					ViewNode* tnd = view->currentnodelist.at(plvcd->nmcd.dwItemSpec);
					TorrentNode *ctt = tnd->GetTorrent();
					if (ctt->errorflag) {
						plvcd->clrText = RGB(0xCF, 0x00, 0x00);
					}

					if (ctt->status == TTS_PAUSED) {
						plvcd->clrTextBk = RGB(0xEF, 0xEF, 0xEF);
						rtn = CDRF_DODEFAULT;
					}
					else if (ctt->status == TTS_VERIFY) {
						plvcd->clrTextBk = RGB(0xCF, 0xCF, 0xFF);
						rtn = CDRF_DODEFAULT;
					}
					else if (ctt->status == TTS_PENDING) {
						plvcd->clrTextBk = RGB(0xFF, 0xFF, 0xCF);
						rtn = CDRF_DODEFAULT;
					}
					else if (ctt->leftsize > 0) {
						rtn = CDRF_NOTIFYSUBITEMDRAW;
					}
					else {
						rtn = CDRF_DODEFAULT;
					}


				}
				else if (plvcd->nmcd.dwDrawStage == (CDDS_SUBITEM | CDDS_ITEMPREPAINT)) {
					if (plvcd->iSubItem == 1) {
						ViewNode* tnd = view->currentnodelist.at(plvcd->nmcd.dwItemSpec);
						if (tnd->GetTorrent()->size > 0) {
							RECT trc;
							ListView_GetSubItemRect(hList, plvcd->nmcd.dwItemSpec, plvcd->iSubItem, LVIR_BOUNDS, &trc);
							trc.left += 1;
							trc.bottom--;

							RECT frc = trc;
							//InflateRect(&trc, -1, 0);
							frc.left = (LONG)(tnd->GetTorrent()->leftsize * (trc.right - trc.left) / tnd->GetTorrent()->size + frc.left);
							int isc = ListView_GetNextItem(hList, ((int)plvcd->nmcd.dwItemSpec) - 1, LVNI_SELECTED);
							HBRUSH hbd = (plvcd->nmcd.dwItemSpec == isc) ? hbDarkGreen : hbGreen;
							FillRect(plvcd->nmcd.hdc, &frc, hbd);
							frc.right = frc.left;
							frc.left = trc.left;
							hbd = (plvcd->nmcd.dwItemSpec == isc) ? hbDarkRed : hbRed;
							FillRect(plvcd->nmcd.hdc, &frc, hbd);

							trc.left += 4;
							if (plvcd->nmcd.dwItemSpec == isc) {
								COLORREF cro = SetTextColor(plvcd->nmcd.hdc, RGB(0xFF, 0xFF, 0xFF));
								DrawText(plvcd->nmcd.hdc, tnd->GetTorrent()->name.c_str(), (int)tnd->GetTorrent()->name.length(), &trc, DT_LEFT);
								SetTextColor(plvcd->nmcd.hdc, cro);
							}
							else {
								DrawText(plvcd->nmcd.hdc, tnd->GetTorrent()->name.c_str(), (int)tnd->GetTorrent()->name.length(), &trc, DT_LEFT);
							}

							rtn = CDRF_SKIPDEFAULT;
						}
						else {
							rtn = CDRF_DODEFAULT;
						}
					}
					else {
						plvcd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
						rtn = CDRF_DODEFAULT;
					}
				}
			} // if currentnode type group
			else if (view->currentnode->GetType() == VNT_TORRENT) {
				NMLVCUSTOMDRAW* plvcd = (LPNMLVCUSTOMDRAW)lParam;
				if (plvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
					rtn = CDRF_NOTIFYITEMDRAW;
				}
				else if (plvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
					if (plvcd->nmcd.dwItemSpec == view->pieceItemIndex) {
						rtn = CDRF_NOTIFYSUBITEMDRAW;
					}
					else {
						rtn = CDRF_DODEFAULT;
					}
				}
				else if (plvcd->nmcd.dwDrawStage == (CDDS_SUBITEM | CDDS_ITEMPREPAINT)) {
					if (plvcd->iSubItem == 1) {
						RECT trc;
						ListView_GetSubItemRect(hList, plvcd->nmcd.dwItemSpec, plvcd->iSubItem, LVIR_BOUNDS, &trc);
						long trcww = trc.right - trc.left;
						long trchh = trc.bottom - trc.top;
						long baridx;
						long nbaridx;
						long vvvidx;
						long vvvcnt;
						if (trcww > 0) {
							if (view->currentnode->GetTorrent()->piecesdata.count > 0) {
								HPEN cpn = NULL;
								cpn = (HPEN)GetCurrentObject(plvcd->nmcd.hdc, OBJ_PEN);
								for (int ii = 0; ii < trcww; ii++) {
									vvvidx = 0;
									baridx = ii * view->currentnode->GetTorrent()->piecesdata.count / trcww;
									nbaridx = (ii + 1) * view->currentnode->GetTorrent()->piecesdata.count / trcww;
									nbaridx += baridx == nbaridx ? 1 : 0;
									vvvcnt = nbaridx - baridx;
									while (baridx < nbaridx) {
										vvvidx += view->currentnode->GetTorrent()->piecesdata.items[baridx];
										baridx++;
									}
									vvvidx /= vvvcnt;
									if (piececolorref.count == 0) {
										InitializePieceColorRef(piececolorref);
									}
									SelectObject(plvcd->nmcd.hdc, piececolorref.items[vvvidx]);
									MoveToEx(plvcd->nmcd.hdc, trc.left + ii, trc.top, NULL);
									LineTo(plvcd->nmcd.hdc, trc.left + ii, trc.bottom);
								}
								SelectObject(plvcd->nmcd.hdc, cpn);
								rtn = CDRF_SKIPDEFAULT;
							}
							else {
								rtn = CDRF_DODEFAULT;
							}
						}
						else {
							rtn = CDRF_DODEFAULT;
						}
					}
					else {
						rtn = CDRF_DODEFAULT;
					}
				}

			} // if currentnode type torrent
			else {
				rtn = CDRF_DODEFAULT;
			}
		}
		else {
			rtn = CDRF_DODEFAULT;
		}
	}
	break;
	case LVN_GETDISPINFO:
	{
		NMLVDISPINFO* plvdi = (NMLVDISPINFO*)lParam;
		if (view->currentnode) {
			int vnt = view->currentnode->GetType();
			switch (vnt)
			{
			case VNT_GROUP:
				if (plvdi->item.iItem < (int)view->currentnodelist.size()) {
					ViewNode* tnd = view->currentnodelist.at(plvdi->item.iItem);
					TorrentNode *ctt = tnd->GetTorrent();

					switch (plvdi->item.iSubItem) {
					case LISTSORT_NAME:
						plvdi->item.pszText = (LPWSTR)ctt->name.c_str();
						break;
					case LISTSORT_ID:
						plvdi->item.pszText = ctt->dispid;
						break;
					case LISTSORT_SIZE:
						plvdi->item.pszText = ctt->dispsize;
						break;
					case LISTSORT_DOWNSPEED:
						plvdi->item.pszText = ctt->dispdownspeed;
						break;
					case LISTSORT_UPSPEED:
						plvdi->item.pszText = ctt->dispupspeed;
						break;
					case LISTSORT_STATUS:
						plvdi->item.pszText = ctt->dispstatus;
						break;
					case LISTSORT_LOCATION:
						plvdi->item.pszText = (LPWSTR)ctt->_path;
						break;
					case LISTSORT_RATIO:
						plvdi->item.pszText = ctt->dispratio;
						break;
					case LISTSORT_TRACKER:
						plvdi->item.pszText = ctt->disptracker;
						break;
					case LISTSORT_ERROR:
						plvdi->item.pszText = ctt->_error;
						break;
					}
				}
				break;
			case VNT_FILEPATH:
				if (plvdi->item.iItem < (int)view->currentnodelist.size()) {
					ViewNode* tnd = view->currentnodelist.at(plvdi->item.iItem);
					TorrentFileNode* tfn = tnd->file;
					if (tfn) {
						switch (plvdi->item.iSubItem) {
						case LISTFILESORT_ID:
							plvdi->item.pszText = (LPWSTR)tfn->dispid;
							break;
						case LISTFILESORT_HAS:
							plvdi->item.pszText = (LPWSTR)tfn->disphas;
							break;
						case LISTFILESORT_PR:
							plvdi->item.pszText = (LPWSTR)tfn->disppr;
							break;
						case LISTFILESORT_NAME:
							plvdi->item.pszText = (LPWSTR)tfn->dispname;
							break;
						case LISTFILESORT_PATH:
							plvdi->item.pszText = (LPWSTR)tfn->disppath;
							break;
						case LISTFILESORT_SIZE:
							plvdi->item.pszText = (LPWSTR)tfn->dispsize;
							break;
						}
					}
				}
				break;
			case VNT_TORRENT:
			case VNT_PROFILE:
			{
				switch (plvdi->item.iSubItem) {
				case 0:
				{
					//std::map<int, std::wstring>::iterator ittp = view->tnpnames.find(plvdi->item.iItem);
					//if (ittp != view->tnpnames.end()) {
					//	plvdi->item.pszText = (LPWSTR)ittp->second.c_str();

					//}
					if (plvdi->item.iItem < (int)view->tnpdisp.size()) {
						plvdi->item.pszText = (LPWSTR)view->tnpdisp.at(plvdi->item.iItem).first.c_str();
					}
				}
				break;
				case 1:
				{
					//std::map<int, std::wstring>::iterator ittp = view->tnpvalues.find(plvdi->item.iItem);
					//if (ittp != view->tnpvalues.end()) {
					//	plvdi->item.pszText = (LPWSTR)ittp->second.c_str();
					//}
					if (plvdi->item.iItem < (int)view->tnpdisp.size()) {
						plvdi->item.pszText = (LPWSTR)view->tnpdisp.at(plvdi->item.iItem).second.c_str();
					}
				}
				break;
				}
			}
			break;
			case VNT_CLIPBOARD:
			case VNT_CLIPBOARD_ITEM:
			{
				if (plvdi->item.iItem < (int)view->currentnodelist.size()) {
					if (plvdi->item.iSubItem == 1) {
						plvdi->item.pszText = (LPWSTR)(view->currentnodelist.at(plvdi->item.iItem))->name.c_str();
					}
				}
			}
			break;
			}
		}
	}
	break;
	case LVN_COLUMNCLICK:
		if (view->currentnode) {
			LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
			ViewNodeType vnt = view->currentnode->GetType();
			switch (vnt) {
			case VNT_GROUP:
				if (view->listsortindex == pnmv->iSubItem) {
					view->listsortdesc = !view->listsortdesc;
				}
				view->listsortindex = pnmv->iSubItem;
				view->ListSortTorrentGroup(view->listsortindex);
				break;
			case VNT_FILEPATH:
				if (view->listsortindex == pnmv->iSubItem) {
					view->listsortdesc = !view->listsortdesc;
				}
				view->listsortindex = pnmv->iSubItem;
				view->ListSortFiles(view->currentnodelist, view->listsortindex, view->listsortdesc);
				PostMessage(hList, LVM_REDRAWITEMS, 0, view->currentnodelist.size() - 1);
				break;
			}
		}
		break;
	case NM_DBLCLK:
		if (view->currentnode) {
			if (view->currentnode->GetType() == VNT_GROUP) {
				LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
				if (lpnmitem->iItem >= 0) {
					if (lpnmitem->iItem < (int)view->currentnodelist.size()) {
						ViewNode* ctt = view->currentnodelist.at(lpnmitem->iItem);
						std::map<ViewNode*, HTREEITEM>::iterator ittn = view->tree_node_handle_map.find(ctt);
						if (ittn != view->tree_node_handle_map.end()) {
							TreeView_Select(hTree, ittn->second, TVGN_CARET);
							TreeView_Select(hTree, ittn->second, TVGN_FIRSTVISIBLE);
						}
					}
				}
			}
		}
		break;
	}

	return rtn;
}

LRESULT WindowViewCY::TreeProcessNotify(LPARAM lParam)
{
	LRESULT rtn = CDRF_DODEFAULT;
	LPNMHDR nmh = (LPNMHDR)lParam;

	switch (nmh->code) {
	case NM_CUSTOMDRAW:
	{
		LPNMTVCUSTOMDRAW ncd = (LPNMTVCUSTOMDRAW)lParam;
		if (ncd->nmcd.dwDrawStage == CDDS_PREPAINT) {
			rtn = CDRF_NOTIFYITEMDRAW;
		}
		else if (ncd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
			ViewNode* vnd = (ViewNode*)ncd->nmcd.lItemlParam;
			TorrentNode* ctt = vnd->GetTorrent();
			if (vnd) {
				if (ctt) {
					if (ctt->errorflag) {
						ncd->clrText = RGB(0xCF, 0x00, 0x00);
					}

					if (ctt->status == TTS_DOWNLOADING) {
						ncd->clrTextBk = RGB(0xCF, 0xFF, 0xCF);
						rtn = CDRF_DODEFAULT;
					}
					else if (ctt->status == TTS_VERIFY) {
						ncd->clrTextBk = RGB(0xCF, 0xCF, 0xFF);
						rtn = CDRF_DODEFAULT;
					}
					else {
						rtn = CDRF_DODEFAULT;
					}

				}
				else {
					rtn = CDRF_DODEFAULT;
				}
			}
			else {
				rtn = CDRF_DODEFAULT;
			}
		}
		else {
			rtn = CDRF_DODEFAULT;
		}
	}
	break;
	case TVN_GETDISPINFO:
	{
		LPNMTVDISPINFO lptvdi = (LPNMTVDISPINFO)lParam;
		ViewNode* vnd = (ViewNode*)lptvdi->item.lParam;
		if (lptvdi->item.mask & TVIF_TEXT) {
			lptvdi->item.pszText = (LPWSTR)vnd->name.c_str();
		}
		if (lptvdi->item.mask & TVIF_CHILDREN) {
			if (vnd->GetType() == VNT_FILEPATH) {
				lptvdi->item.cChildren = vnd->file->pathcount;
			}
			else {
				lptvdi->item.cChildren = vnd->nodes.size();
			}
		}
	}
	break;
	case TVN_SELCHANGED:
	{
		LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
		ViewNode* vnd = (ViewNode*)pnmtv->itemNew.lParam;
		view->ChangeSelectedViewNode(vnd);
		PostMessage(hTree, TVM_EXPAND, TVE_EXPAND, (LPARAM)(pnmtv->itemNew.hItem));
	}
	break;
	case TVN_ITEMEXPANDING:
	{
		LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;

		view->TreeExpandItem(pnmtv->itemNew.hItem);
	}
	}

	return rtn;
}

int WindowViewCY::DrawListItemLog(LPDRAWITEMSTRUCT dis)
{
	DrawText(dis->hDC, view->log.items[dis->itemID].data, wcslen(view->log.items[dis->itemID].data), &dis->rcItem, DT_LEFT);
	return 0;
}

int WindowViewCY::TreeCleanUp()
{
	TreeView_DeleteAllItems(hTree);
	return 0;
}
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT WindowViewCY::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT rtn = 0;
	switch (message)
	{
	case WM_CREATE:
		PostMessage(hWnd, WM_U_INITIALIZEWINDOW, 0, 0);
		PostMessage(hWnd, WM_U_INITIALIZESERVICE, 0, 0);
		break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			U_Close();
			DestroyWindow(hWnd);
			break;
		case ID_CONTEXT_DOWNLIMIT:
			view->ViewSetLimit(CSLA_DOWN);
			break;
		case ID_CONTEXT_UPLIMIT:
			view->ViewSetLimit(CSLA_UP);
			break;
		case ID_CONTEXT_ENABLEDOWN:
			view->ViewSetLimit(CSLA_ENABLE_DOWN);
			break;
		case ID_CONTEXT_ENABLEUP:
			view->ViewSetLimit(CSLA_ENABLE_UP);
			break;
		case ID_CONTEXT_DISABLEDOWN:
			view->ViewSetLimit(CSLA_DISABLE_DOWN);
			break;
		case ID_CONTEXT_DISABLEUP:
			view->ViewSetLimit(CSLA_DISABLE_UP);
			break;
		case ID_CONTEXT_INCLUDE:
			view->ViewEnableFile(TRUE);
			break;
		case ID_CONTEXT_EXCLUDE:
			view->ViewEnableFile(FALSE);
			break;
		case ID_CONTEXT_HIGHPRIORITY:
			view->ViewPriorityFile(1);
			break;
		case ID_CONTEXT_NORMALPRIORITY:
			view->ViewPriorityFile(0);
			break;
		case ID_CONTEXT_LOWPRIORITY:
			view->ViewPriorityFile(-1);
			break;
		case ID_CONTEXT_PAUSE:
			view->ViewPauseTorrent(TRUE);
			break;
		case ID_CONTEXT_RESUME:
			view->ViewPauseTorrent(FALSE);
			break;
		case ID_CONTEXT_VERIFY:
			view->ViewVerifyTorrent();
			break;
		case ID_CONTEXT_DELETE:
			view->ViewDeleteContent(FALSE);
			break;
		case ID_CONTEXT_DELETEDATA:
			view->ViewDeleteContent(TRUE);
			break;
		case ID_CONTEXT_SETLOCATION:
			view->ViewSetLocation();
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	//case WM_PAINT:
	//{
	//	//PAINTSTRUCT ps;
	//	//HDC hdc = BeginPaint(hWnd, &ps);
	//	// TODO: Add any drawing code that uses hdc here...
	//	//EndPaint(hWnd, &ps);
	//	if (splitMain) {
	//		PostMessage(*splitMain, WM_PAINT, wParam, lParam);
	//	}
	//}
	//break;
	case WM_DESTROY:
		U_Close();
		PostQuitMessage(0);
		break;
	case WM_U_INITIALIZEWINDOW:
		U_InitWnd();
		::PostMessage(hWnd, WM_SIZE, 0, 0);
		break;
	case WM_U_RIBBONCOMMAND:
		U_ProcRibbonCommand(wParam);
		break;
	case WM_U_INITIALIZESERVICE:
		U_InitService();
		break;
	case WM_U_PROFILELOADED:
		U_ProfileLoaded((TransmissionProfile*)wParam);
		PostMessage(hWnd, WM_U_REFRESHPROFILETORRENTS, wParam, NULL);
		break;
	case WM_U_REFRESHPROFILETORRENTS:
		U_RefreshProfileTorrents((TransmissionProfile*)wParam);
		break;
	case WM_U_ADDLINKBYDROP:
	{
		WCHAR* wpt = (WCHAR*)wParam;
		view->AddLinkByDrop(wpt);
		delete[] wpt;
	}
	break;
	case WM_SIZE:
		U_ResizeContentWindow();
		break;
	case WM_CONTEXTMENU:
		view->ShowContextMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_NOTIFY:
		if (lParam) {
			NMHDR * nmh = (NMHDR*)lParam;
			if (nmh->hwndFrom == hTree) {
				rtn = TreeProcessNotify(lParam);
			}
			else if (nmh->hwndFrom == hList) {
				rtn = ListProcessNotify(lParam);
			}
		}
		break;
	case WM_CLIPBOARDUPDATE:
		view->ProcessClipboardEntry();
		break;
	case WM_U_INPUTCOMMAND:
		view->ProcessInputCommand();
		break;
	case WM_DRAWITEM:
	{
		LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
		if (dis->hwndItem == hLog) {
			DrawListItemLog(dis);
		}
	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return rtn;
}

int WindowViewCY::U_InitWnd()
{
	hbGreen = ::CreateSolidBrush(RGB(0xCF, 0xFF, 0xCF));
	hbDarkGreen = ::CreateSolidBrush(RGB(0x3F, 0x7F, 0x3F));
	hbRed = ::CreateSolidBrush(RGB(0xFF, 0xCF, 0xCF));
	hbDarkRed = ::CreateSolidBrush(RGB(0x7F, 0x3F, 0x3F));
	hpWhite = ::CreatePen(PS_SOLID, 1, RGB(0xFF, 0xFF, 0xFF));
	U_InitWnd_Ribbon(hInst, hWnd);
	U_InitWnd_ContentWindows(hWnd);

	AddClipboardFormatListener(hWnd);
	clipboardEnabled = true;

	U_InitializeDragDrop(hWnd);

	return 0;
}

int WindowViewCY::U_InitWnd_Ribbon(HINSTANCE hinst, HWND hwnd)
{
	HRESULT hr = ::CoCreateInstance(
		CLSID_UIRibbonFramework,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&ribbonFramework));
	if (SUCCEEDED(hr))
	{
		WVCYRibbonApplication* papp = new WVCYRibbonApplication(hwnd, WM_U_RIBBONCOMMAND);
		hr = ribbonFramework->Initialize(hwnd, papp);
	}
	if (SUCCEEDED(hr))
	{
		hr = ribbonFramework->LoadUI(hinst, L"SIMPLEVISUALTRANS_RIBBON");
		ribbonFramework->FlushPendingInvalidations();
	}
	return 0;
}

int WindowViewCY::U_ProcRibbonCommand(int cmd)
{
	switch (cmd) {
	case cmdUpperNode:
		view->TreeMoveParenetViewNode();
		break;
	case cmdAddTorrent:
		view->ViewCommitTorrents(0);
		break;
	case cmdDeleteContentTorrent:
		view->ViewDeleteContent(TRUE);
		break;
	case cmdDeleteTorrent:
		view->ViewDeleteContent(FALSE);
		break;
	case cmdPauseTorrent:
		view->ViewPauseTorrent(TRUE);
		break;
	case cmdResumeTorrent:
		view->ViewPauseTorrent(FALSE);
		break;
	case cmdVerifyTorrent:
		view->ViewVerifyTorrent();
		break;
	case cmdSetLocation:
		view->ViewSetLocation();
		break;
	default:
	{
		WCHAR wbuf[10];
		_itow_s(cmd, wbuf, 10);
		MessageBox(hWnd, wbuf, L"ribbon", MB_OK);
	}
	}
	return 0;
}

WNDPROC edProfileOriProc = NULL;
HWND edHwnd = NULL;

LRESULT APIENTRY EditBoxKeypressSubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT lst = 0;

	if (uMsg == WM_KEYUP) {
		if (wParam == VK_RETURN) {
			if (edHwnd) {
				PostMessage(edHwnd, WM_U_INPUTCOMMAND, 0, 0);
			}
		}
	}
	else {
		lst = CallWindowProc(edProfileOriProc, hwnd, uMsg, wParam, lParam);
	}
	return lst;
}

int WindowViewCY::U_InitWnd_ContentWindows(HWND hParent)
{
	HRESULT hr = OleInitialize(NULL);
	::InitCommonControls();
	HWND hStatus;
	HINSTANCE hinst = ::GetModuleHandle(NULL);

	// Setup main window
	splitMain = CSplitWnd::CreateSplitWindow(hParent);
	hStatus = ::CreateWindow(STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 200, 10, *splitMain, NULL, hinst, NULL);
	splitView = CSplitWnd::CreateSplitWindow(*splitMain);
	splitMain->style = CSplitWnd::SPW_STYLE::DOCKDOWN;
	splitMain->SetWindow(*splitView);
	splitMain->SetWindow(hStatus);

	hContent = *splitMain;

	// Setup log window
	//panel = CPanelWindow::CreatePanelWindow(*splitView);
	//hLog = CreateWindow(WC_LISTBOX, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_VISIBLE | LBS_NOINTEGRALHEIGHT, 0, 0, 200, 10, *panel, NULL, hinst, NULL);
	splitInput = CSplitWnd::CreateSplitWindow(*splitView);

	splitContent = CSplitWnd::CreateSplitWindow(*splitView);
	splitView->style = CSplitWnd::SPW_STYLE::TOPDOWN;
	splitView->SetRatio(0.7);
	splitView->SetWindow(*splitContent);
	splitView->SetWindow(*splitInput);

	// Setup input text and log
	hLog = CreateWindow(WC_LISTBOX, L"",
		WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_VISIBLE | LBS_NOINTEGRALHEIGHT | LBS_NODATA | LBS_OWNERDRAWFIXED,
		0, 0, 200, 10, *splitInput, NULL, hinst, NULL
	);
	hInput = CreateWindow(WC_EDIT, L"",
		WS_CHILD | WS_VISIBLE | WS_EX_STATICEDGE | WS_BORDER | WS_TABSTOP | ES_WANTRETURN,
		0, 0, 200, 20, *splitInput, NULL, hinst, NULL
	);
	splitInput->style = CSplitWnd::SPW_STYLE::DOCKDOWN;
	splitInput->SetWindow(hLog);
	splitInput->SetWindow(hInput);

	// Setup Tree-VIew and List-View
	hTree = hTree = CreateWindow(WC_TREEVIEW, L"", WS_CHILD | WS_VSCROLL | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT, 0, 0, 100, 100, *splitContent, NULL, hinst, NULL);
	hList = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | LVS_REPORT | WS_VISIBLE | LVS_OWNERDATA, 0, 0, 200, 10, *splitContent, NULL, hinst, NULL);
	splitContent->style = CSplitWnd::SPW_STYLE::LEFTRIGHT;
	splitContent->SetRatio(0.2);
	splitContent->SetWindow(hTree);
	splitContent->SetWindow(hList);

	ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	HWND hhd = ListView_GetHeader(hList);
	if (hhd) {
		LONG_PTR lps = GetWindowLongPtr(hhd, GWL_STYLE);
		lps |= HDS_FLAT;
		//lps &= ~HDS_BUTTONS;
		SetWindowLongPtr(hhd, GWL_STYLE, lps);
	}

	edProfileOriProc = (WNDPROC)GetWindowLongPtr(hInput, GWLP_WNDPROC);
	edProfileOriProc = (WNDPROC)SetWindowLongPtr(hInput, GWLP_WNDPROC, (LONG_PTR)&EditBoxKeypressSubProc);
	edHwnd = hParent;

	HFONT hsysfont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	::SendMessage(hLog, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hList, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hTree, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));
	::SendMessage(hInput, WM_SETFONT, (WPARAM)hsysfont, MAKELPARAM(TRUE, 0));

	return 0;
}

int WindowViewCY::U_ResizeContentWindow()
{
	if (hContent)
	{
		RECT rct;
		::GetClientRect(hWnd, &rct);
		if (ribbonFramework) {
			IUIRibbon* pview;
			ribbonFramework->GetView(0, __uuidof(IUIRibbon), (void**)&pview);
			if (pview) {
				UINT32 uht;
				HRESULT hr = pview->GetHeight(&uht);
				if (SUCCEEDED(hr)) {
					rct.top += uht;
				}
			}
		}
		::MoveWindow(hContent, rct.left, rct.top, rct.right - rct.left, rct.bottom - rct.top, FALSE);
	}
	return 0;
}

//class CBLoadProf : public ManagerCommand
//{
//public:
//	HWND wself;
//	CmdLoadProfile* cmd;
//	int Process(TransmissionManager* mgr) {
//		PostMessage(wself, WM_U_PROFILELOADED, (WPARAM)cmd->profile, NULL);
//		return 0;
//	}
//};

int WindowViewCY::U_InitService()
{
	view = new ViewManager();
	view->hTree = hTree;
	view->hLog = hLog;
	view->hList = hList;
	view->hWnd = hWnd;
	view->hInput = hInput;
	view->Start();

	VCInit* vci = new VCInit();
	view->PutCommand(vci);
	VCRotation *vcr = new VCRotation();
	view->PutCommand(vcr);

	return 0;
}

int WindowViewCY::U_ProfileLoaded(TransmissionProfile * prof)
{
	return 0;
}

int WindowViewCY::U_Close()
{
	if (piidrop) {
		RevokeDragDrop(hWnd);
		piidrop = NULL;
	}
	if (clipboardEnabled) {
		::RemoveClipboardFormatListener(hWnd);
		clipboardEnabled = false;
	}
	if (view) {
		view->Stop();
	}

	TreeCleanUp();

	if (view) {
		delete view;
		view = nullptr;
	}

	if (hbGreen) {
		::DeleteObject(hbGreen);
		hbGreen = NULL;
	}
	if (hbDarkGreen) {
		::DeleteObject(hbDarkGreen);
		hbDarkGreen = NULL;
	}
	if (hbRed) {
		::DeleteObject(hbRed);
		hbRed = NULL;
	}
	if (hbDarkRed) {
		::DeleteObject(hbDarkRed);
		hbDarkRed = NULL;
	}
	if (hpWhite) {
		::DeleteObject(hpWhite);
		hpWhite = NULL;
	}
	if (piececolorref.count > 0) {
		delete[] piececolorref.items;
		piececolorref.count = 0;
	}

	return 0;
}

int WindowViewCY::U_RefreshProfileTorrents(TransmissionProfile * prof)
{
	return 0;
}