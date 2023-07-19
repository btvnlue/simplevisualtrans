#pragma once
#include <string>
#include <map>
#include <curl/curl.h>

class TransmissionProfile;

class CurlSessionCY
{
	static CurlSessionCY *self;
	CURL* core;
	CurlSessionCY();
	std::map<std::wstring, std::wstring> header;
	std::wstring response;
public:
	virtual ~CurlSessionCY();
	static CurlSessionCY* GetInstance();
	static int CleanUp();
	static size_t CB_CurlHeaderParse(void* data, size_t size, size_t nmemb, void* userp);
	static size_t CB_CurlResponseParse(void* data, size_t size, size_t nmemb, void* userp);

	int SetSessionCommonParms(CURL* sss);
	int GetSessionToken(const std::wstring &url, const std::wstring &username, const std::wstring &passwd, _Out_ std::wstring& token);
	int SendCurlRequest(TransmissionProfile* prof, const std::wstring &request, std::wstring & tts);
	int CurlParseReturnCode(int res);
};

