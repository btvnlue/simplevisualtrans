#pragma once

#include <string>
#include <map>
#include "curl/curl.h"

class CurlSession
{
	bool initialized;
	CURL* curl;
	int setSessionCommonParms();
	int cleanUp();
	std::map<std::wstring, std::wstring> headers;
	std::wstring sessionId;
	static size_t CB_CurlHeaderParse(void* data, size_t size, size_t nmemb, void* userp);
	std::wstring siteUrl;
	std::wstring siteUser;
	std::wstring sitePasswd;
public:
	static size_t CB_CurlResponseParse(void* data, size_t size, size_t nmemb, void* userp);
	std::wstring response;
	CurlSession();
	virtual ~CurlSession();
	int performLogin();
	int setSiteInfo(const std::wstring& url, const std::wstring& user, const std::wstring& passwd);
	virtual int performQuery(const std::wstring& query, int level = 0);
};

class CurlSessionTest : public CurlSession
{
public:
	CurlSessionTest();
	~CurlSessionTest();
	void FileLoadResponse(const LPWSTR fname);
	int performQuery(const std::wstring& query, int level = 0);
};

