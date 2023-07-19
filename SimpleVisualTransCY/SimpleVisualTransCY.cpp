// SimpleVisualTransCY.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "SimpleVisualTransCY.h"
#include "WindowViewCY.h"
#include "INIFileTool.h"
#include "TransmissionProfile.h"

#include <shellapi.h>
#include <string>
#include <set>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

std::map<std::wstring, TransmissionProfile *> profiles;

int LoadIniProfiles(INIFileTool* ift) {
	SECTIONS::iterator itsc = ift->sections.begin();
	std::wstring::size_type pos;
	TransmissionProfile* profile;

	while (itsc != ift->sections.end()) {
		const std::wstring &pname = itsc->first;
		pos = pname.find(L"Profile");
		if (pos == 0) {
			std::map<std::wstring, TransmissionProfile*>::iterator itpf;
			itpf = profiles.find(pname);
			if (itpf == profiles.end()) {
				profile = new TransmissionProfile();
				profiles[pname] = profile;
			}
			else {
				profile = itpf->second;
			}

			CONTENTS* pctt = itsc->second;
			if (pctt->find(L"url") != pctt->end()) {
				profile->url = (*pctt)[L"url"];
			}
			if (pctt->find(L"username") != pctt->end()) {
				profile->username = (*pctt)[L"username"];
			}
			if (pctt->find(L"passwd") != pctt->end()) {
				profile->passwd = (*pctt)[L"passwd"];
			}
		}
		itsc++;
	}

	return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR  lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	
	int argc;

	LPWSTR* cmdline = CommandLineToArgvW(L"", &argc);
	std::wstring wcs(*cmdline);
	std::wstring::size_type itex = wcs.find(L".exe");
	if (itex == std::string::npos) {
		itex = wcs.find(L".EXE");
	}

	if (itex != std::string::npos) {
		wcs.replace(itex, 4, L".ini");
	}
	INIFileTool ift;
	int rtn = ift.LoadConfig(wcs);
	if (rtn == 0) {
		LoadIniProfiles(&ift);
	}
	// TODO: Place code here.
	WindowViewCY* viewcy = new WindowViewCY();
	HWND hwnd = viewcy->CreateWindowView(hInstance);

	BOOL keepwait = TRUE;
	while (keepwait) {
		Sleep(100);
		keepwait = viewcy->IsActive();
	}

	viewcy->Close();
	delete viewcy;

    return 0;
}