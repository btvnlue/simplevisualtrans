#pragma once

#include <string>
#include <curl/curl.h>
#include <map>

class CurlProfile
{
public:
	std::wstring siteUrl;
	std::wstring siteUser;
	std::wstring sitePasswd;
	std::wstring response;
	std::wstring sessionId;
	std::map<std::wstring, std::wstring> header;
};

class CurlSessionVT
{

	CURL* core;
public:
	CurlSessionVT();
	~CurlSessionVT();

	bool ValidateSession(CurlProfile * prof, int level = 0);
	static size_t CB_CurlResponseParse(void* data, size_t size, size_t nmemb, void* userp);
	int SetSessionCommonParms(CURL* sss);
	int PerformQuery(CurlProfile * prof, std::wstring & query, int level = 0);
	static size_t CB_CurlHeaderParse(void* data, size_t size, size_t nmemb, void* userp);
	int performLogin(CurlProfile * prof);
	const std::wstring & GetResponse(CurlProfile * prof);
};

