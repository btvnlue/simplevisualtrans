#include "INIFileTool.h"

#include <Windows.h>
#include <map>

INIFileTool::~INIFileTool()
{
	for (std::map<std::wstring, std::map<std::wstring, std::wstring>*>::iterator itsc = sections.begin()
		; itsc != sections.end()
		; itsc++) {
		delete itsc->second;
	}
	sections.clear();
}

int INIFileTool::LoadConfig(std::wstring filename)
{
	DWORD rsz = 1024;
	LPWSTR prz = (LPWSTR)malloc(rsz);
	DWORD rrz = GetPrivateProfileSectionNames(prz, rsz, filename.c_str());
	if (rrz > rsz) {
		rsz = rrz + 2;
		free(prz);
		prz = (WCHAR*)malloc(rsz);
		rrz = GetPrivateProfileSectionNames(prz, rsz, filename.c_str());
	}
	DWORD rps = 0;
	std::wstring sck;
	while (prz[rps] > 0) {
		sck = prz + rps;
		if (sections.find(sck) == sections.end()) {
			sections[sck] = new CONTENTS();
		}
		rps += sck.length() + 1;
	}
	free(prz);

	rsz = 1024;
	prz = (WCHAR*)malloc(rsz);
	size_t fpos;
	std::map<std::wstring, std::wstring>* sms;
	std::wstring kkk, vvv;
	for (SECTIONS::iterator itsc = sections.begin()
		; itsc != sections.end()
		; itsc++) {
		rrz = GetPrivateProfileSection(itsc->first.c_str(), prz, rsz, filename.c_str());
		if (rrz > rsz) {
			free(prz);
			rsz = rrz + 2;
			prz = (WCHAR*)malloc(rsz);
			rrz = GetPrivateProfileSection(itsc->first.c_str(), prz, rsz, filename.c_str());
		}
		rps = 0;
		sms = itsc->second;
		while (prz[rps] > 0) {
			sck = prz + rps;
			fpos = sck.find(L'=');
			if (fpos != std::wstring::npos) {
				kkk = sck.substr(0, fpos);
				vvv = sck.substr(fpos + 1);
				//(*sms)[kkk] = vvv;
				sms->insert(std::pair<std::wstring, std::wstring>(kkk, vvv));
			}
			rps += sck.length() + 1;
		}
	}
	free(prz);

	return 0;
}

#define WRITEPROFBUFSIZE 65432

int INIFileTool::SaveConfig(std::wstring filename)
{
	std::wstring opfname;

	WCHAR* psf = (WCHAR*)malloc(WRITEPROFBUFSIZE);
	DWORD cpp;
	WCHAR pbp[1024];
	DWORD cbz;
	std::map<std::wstring, std::wstring>* pkv;
	for (std::map<std::wstring, std::map<std::wstring, std::wstring>*>::iterator itsc = sections.begin()
		; itsc != sections.end()
		; itsc++) {
		pkv = itsc->second;
		cpp = 0;
		psf[cpp] = 0;
		for (std::map<std::wstring, std::wstring>::iterator itkv = pkv->begin()
			; itkv != pkv->end()
			; itkv++) {
			wsprintf(pbp, L"%s=%s", itkv->first.c_str(), itkv->second.c_str());
			cbz = WRITEPROFBUFSIZE / 2 - cpp;
			wcscpy_s(psf + cpp, cbz, pbp);
			cpp += wcslen(pbp);
			cpp++;
		}
		if (cpp > 0) {
			psf[cpp] = 0;
			WritePrivateProfileSection(itsc->first.c_str(), psf, opfname.c_str());
		}
	}
	free(psf);
	return 0;
}
