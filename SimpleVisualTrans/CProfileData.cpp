#include "CProfileData.h"

#include <Windows.h>
#include <sstream>

CProfileData::CProfileData()
{
}

CProfileData::~CProfileData()
{
	for (std::map<std::wstring, std::map<std::wstring, std::wstring>*>::iterator itsc = sections.begin()
		; itsc != sections.end()
		; itsc++) {
		delete itsc->second;
	}
	sections.clear();
}

#define DEFAULTPROFSECTION L"Default"

int CProfileData::SetDefaultValue(const std::wstring& key, const std::wstring& value)
{
	std::map<std::wstring, std::wstring>* gmp;

	if (sections.find(DEFAULTPROFSECTION) == sections.end()) {
		sections[DEFAULTPROFSECTION] = new std::map<std::wstring, std::wstring>();
	}
	gmp = sections[DEFAULTPROFSECTION];
	(*gmp)[key] = value;
	return 0;
}

int CProfileData::GetDefaultValue(const std::wstring& key, std::wstring& value)
{
	int rtn = 0;

	std::map<std::wstring, std::wstring>* gmp;

	if (sections.find(DEFAULTPROFSECTION) != sections.end()) {
		gmp = sections[DEFAULTPROFSECTION];
		std::map<std::wstring, std::wstring>::iterator itgp = gmp->find(key);
		if (itgp != gmp->end()) {
			value = itgp->second;
			rtn = 1;
		}
	}

	return rtn;
}

int CProfileData::LoadProfile(std::wstring pfname)
{
	DWORD rsz = 1024;
	LPWSTR prz = (LPWSTR)malloc(rsz);
	DWORD rrz = GetPrivateProfileSectionNames(prz, rsz, pfname.c_str());
	if (rrz > rsz) {
		rsz = rrz + 2;
		free(prz);
		prz = (WCHAR*)malloc(rsz);
		rrz = GetPrivateProfileSectionNames(prz, rsz, pfname.c_str());
	}
	DWORD rps = 0;
	std::wstring sck;
	while (prz[rps] > 0) {
		sck = prz + rps;
		if (sections.find(sck) == sections.end()) {
			sections[sck] = new std::map<std::wstring, std::wstring>();
		}
		rps += sck.length() + 1;
	}
	free(prz);


	//BOOL btn = WritePrivateProfileSection(L"application", L"Sections", pfname.c_str());
	//DWORD dle = GetLastError();
	//FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dle, 0, prr, 1024, NULL);
	rsz = 1024;
	prz = (WCHAR*)malloc(rsz);
	size_t fpos;
	std::map<std::wstring, std::wstring>* sms;
	for (std::map<std::wstring, std::map<std::wstring, std::wstring>*>::iterator itsc = sections.begin()
		; itsc != sections.end()
		; itsc++) {
		rrz = GetPrivateProfileSection(itsc->first.c_str(), prz, rsz, pfname.c_str());
		if (rrz > rsz) {
			free(prz);
			rsz = rrz + 2;
			prz = (WCHAR*)malloc(rsz);
			rrz = GetPrivateProfileSection(itsc->first.c_str(), prz, rsz, pfname.c_str());
		}
		rps = 0;
		sms = itsc->second;
		while (prz[rps] > 0) {
			sck = prz + rps;
			fpos = sck.find(L'=');
			if (fpos != std::wstring::npos) {
				(*sms)[sck.substr(0, fpos)] = sck.substr(fpos + 1);
			}
			rps += sck.length() + 1;
		}
	}
	free(prz);

	return 0;
}

#define WRITEPROFBUFSIZE 65535

int CProfileData::StoreProfile(std::wstring pfname)
{
	WCHAR* psf = (WCHAR*)malloc(65535);
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
			WritePrivateProfileSection(itsc->first.c_str(), psf, pfname.c_str());
		}
	}
	free(psf);
	return 0;
}