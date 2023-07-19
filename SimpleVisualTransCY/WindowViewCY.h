#pragma once

#include <Windows.h>
#include <oleidl.h>
#include <map>
#include <string>

struct IUIFramework;
class CSplitWnd;
class CPanelWindow;
class TransmissionManager;
class TransmissionProfile;
class ViewManager;

class WindowViewCY
{
	HINSTANCE hInst;
	DWORD thId;
	BOOL active;
	HWND hWnd;
	HWND hContent;
	HWND hLog;
	HWND hTree;
	HWND hList;
	HBRUSH hbGreen;
	HBRUSH hbDarkGreen;
	HBRUSH hbRed;
	HBRUSH hbDarkRed;
	HPEN hpWhite;
	CSplitWnd* splitLog;
	CSplitWnd* splitMain;
	CSplitWnd* splitContent;
	CSplitWnd* splitView;
	CPanelWindow* panel;
	ViewManager* view;
	bool clipboardEnabled;
	IDropTarget* piidrop;

	static LRESULT CALLBACK WndProcS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static DWORD WINAPI ThWindowViewCY(_In_ LPVOID lpParameter);

	ATOM RegisterWindowViewClass(HINSTANCE hInst);
	int InitializeInstance(HINSTANCE hInst);
	LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	int U_InitWnd();
	int U_InitWnd_Ribbon(HINSTANCE hinst, HWND hwnd);
	int U_ProcRibbonCommand(int cmd);
	int U_InitWnd_ContentWindows(HWND hParent);
	int U_ResizeContentWindow();
	int U_InitService();
	int U_ProfileLoaded(TransmissionProfile* prof);
	int U_Close();
	int U_RefreshProfileTorrents(TransmissionProfile* prof);
	int U_InitializeDragDrop(HWND hwnd);

	LRESULT TreeProcessNotify(LPARAM lParam);
	int TreeCleanUp();

	LRESULT ListProcessNotify(LPARAM lParam);
	int LogDrawItems(LPDRAWITEMSTRUCT dis);

	IUIFramework* ribbonFramework;
public:

	WindowViewCY();
	virtual ~WindowViewCY();
	HWND CreateWindowView(HINSTANCE hInst);
	BOOL IsActive();
	int Close();
};

