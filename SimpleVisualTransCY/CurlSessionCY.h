#pragma once
#include <string>
#include <map>
#include <vector>
#include <curl/curl.h>

#include <Windows.h>

class TransmissionProfile;

class CurlSessionCY
{
	static CurlSessionCY *self;
	CURL* core;
	CurlSessionCY();
	std::map<std::wstring, std::wstring> header;
	std::wstring fn_response;
	std::vector<char*> v_response;
	unsigned long sz_response;
	HANDLE h_response;
public:
	virtual ~CurlSessionCY();
	static CurlSessionCY* GetInstance();
	static int CleanUp();
	static size_t CB_CurlHeaderParse(void* data, size_t size, size_t nmemb, void* userp);
	static size_t CB_CurlResponseParse(void* data, size_t size, size_t nmemb, void* userp);
	static size_t CB_CurlResponseParseFile(void* data, size_t size, size_t nmemb, void* userp);
	static size_t CB_CurlResponseParseFileHandle(void* data, size_t size, size_t nmemb, void* userp);

	int SetSessionCommonParms(CURL* sss);
	int GetSessionToken(const std::wstring &url, const std::wstring &username, const std::wstring &passwd, _Out_ std::wstring& token);
	int SendCurlRequest(TransmissionProfile* prof, const std::wstring &request, std::wstring & tts);
	int SendCurlRequestFile(TransmissionProfile* prof, const std::wstring &request, std::wstring & fname);
	int SendCurlRequestFileHandle(TransmissionProfile* prof, const std::wstring &request, HANDLE hresp);

	int CurlParseReturnCode(int res);
};

