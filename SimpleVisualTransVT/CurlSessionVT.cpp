#include "CurlSessionVT.h"
#include <sstream>

#ifdef CURL_STATICLIB
#pragma comment(lib, "libcurl_a.lib")
//#pragma comment(lib, "openssl.lib")
//#pragma comment(lib, "libssh2.lib")
//#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Normaliz.lib")
#else
#pragma comment(lib, "libcurl.lib")
#endif
#define XSESSIONHTTPHEAD L"X-Transmission-Session-Id"


int Log(std::wstring& str) {
	return 0;
}

bool reformatValue(char* vvv)
{
	bool donevalue = false;

	size_t vsl = strnlen_s(vvv, 1024);
	if (vsl > 0) {
		switch (vvv[vsl - 1]) {
		case '\r':
		case '\n':
		case ' ':
			donevalue = true;
			vvv[vsl - 1] = 0;
			break;
		}
	}
	return donevalue;
}

CurlSessionVT::CurlSessionVT()
{
	//siteUrl = L"http://192.168.5.158:9091/transmission/rpc";
	//siteUser = L"transmission";
	//sitePasswd = L"transmission";

	curl_global_init(CURL_GLOBAL_ALL);
	core = curl_easy_init();
}

CurlSessionVT::~CurlSessionVT()
{
	curl_easy_cleanup(core);
	curl_global_cleanup();
}

bool CurlSessionVT::ValidateSession(CurlProfile* prof, int level)
{
	bool btn = false;
	if (prof->sessionId.length() > 0) {
		return true;
	}
	else {
		if (level < 1) {
			if (prof->siteUrl.length() > 0) {
				performLogin(prof);
			}
			btn = ValidateSession(prof, level + 1);
		}
	}

	return btn;
}

size_t CurlSessionVT::CB_CurlResponseParse(void* data, size_t size, size_t nmemb, void* userp)
{
	size_t rsz = size * nmemb;
	//	size_t ors;
	CurlProfile* prof = (CurlProfile*)userp;
	unsigned char* resbuf = (unsigned char*)malloc(rsz + 1);

	if (resbuf) {
		memcpy_s(resbuf, rsz, data, rsz);
		resbuf[rsz] = 0;

		std::wstringstream sss;
		sss << prof->response.c_str();
		sss << (char*)resbuf;
		prof->response = sss.str();
		free(resbuf);
	}

	return rsz;
}

int CurlSessionVT::SetSessionCommonParms(CURL* sss)
{
	curl_easy_setopt(sss, CURLOPT_USERAGENT, "SimpleTransClient/0.19.11");
	curl_easy_setopt(sss, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(sss, CURLOPT_ENCODING, "");

	return 0;
}

int CurlSessionVT::PerformQuery(CurlProfile* prof, std::wstring& query, int level)
{
	int rtn = 0;
	bool btn = ValidateSession(prof, level);
	if (btn) {

		std::wstringstream xss;
		BOOL udc;
		int rbt;
		char* errbuf = (char*)malloc(CURL_ERROR_SIZE);

		prof->response.clear();
		size_t xallz;

		xallz = prof->siteUrl.length() * sizeof(wchar_t) + 1;
		char* ccurl = (char*)malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, prof->siteUrl.c_str(), prof->siteUrl.length(), ccurl, xallz, NULL, &udc);
		ccurl[rbt] = 0;
		curl_easy_setopt(core, CURLOPT_URL, ccurl);

		xss << XSESSIONHTTPHEAD << L": " << prof->sessionId;
		std::wstring xstr = xss.str();
		size_t xssz = xstr.size();
		xallz = xssz * sizeof(wchar_t) + 1;
		char* ccsss = (char*)malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, xss.str().c_str(), xss.str().length(), ccsss, xallz, NULL, &udc);
		ccsss[rbt] = 0;
		struct curl_slist* custom_headers = curl_slist_append(NULL, ccsss);
		curl_easy_setopt(core, CURLOPT_HTTPHEADER, custom_headers);

		xallz = query.length() * sizeof(wchar_t) + 1;
		char* ccquery = (char*)malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, query.c_str(), query.length(), ccquery, xallz, NULL, &udc);
		ccquery[rbt] = 0;
		curl_easy_setopt(core, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(core, CURLOPT_ENCODING, "");
		curl_easy_setopt(core, CURLOPT_POST, 1);
		curl_easy_setopt(core, CURLOPT_WRITEDATA, prof);
		curl_easy_setopt(core, CURLOPT_WRITEFUNCTION, CB_CurlResponseParse);
		//		curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
		//		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, CB_CurlHeaderParse);

		curl_easy_setopt(core, CURLOPT_POSTFIELDS, ccquery);
		curl_easy_setopt(core, CURLOPT_POSTFIELDSIZE, strlen(ccquery));

		curl_easy_setopt(core, CURLOPT_ERRORBUFFER, errbuf);
		Log(query);
		CURLcode res = curl_easy_perform(core);
		if (res == CURLE_OK) {
			long rsp;
			curl_easy_getinfo(core, CURLINFO_RESPONSE_CODE, &rsp);
			if (rsp == 401) {
				rtn = 1;
			}
			else if (rsp == 409) {
				rtn = 1;
			}
		}
		else if (res == CURLE_COULDNT_CONNECT) {
			rtn = 3;
		}
		else {
			rtn = 2;
		}
		free(ccquery);
		free(ccsss);
		free(errbuf);
	}
	else {
		rtn = 5;
	}

	if (rtn == 1) {
		if (level < 3) {
			rtn = performLogin(prof);
			if (rtn == 0) {
				rtn = PerformQuery(prof, query, level + 1);
			}
		}
	}
	return rtn;
}

size_t CurlSessionVT::CB_CurlHeaderParse(void* data, size_t size, size_t nmemb, void* userp)
{
	char hkey[1024];
	char hvalue[1024];
	CurlProfile* prof = (CurlProfile*)userp;
	size_t rsz = size * nmemb;
	char* header = (char*)data;
	size_t cpos = 0;
	bool keepseek = cpos < rsz;
	while (keepseek) {
		if (header[cpos] == ':') {
			keepseek = false;
		}
		cpos++;
		keepseek = cpos < rsz ? keepseek : false;
	}
	if (cpos < rsz) {
		if (cpos > 2)
		{
			memcpy_s(hkey, 1024, header, cpos - 1);
			hkey[cpos - 1] = 0;
			if (cpos + 1 < rsz) {
				memcpy_s(hvalue, 1024, header + cpos + 1, rsz - cpos - 1);
				hvalue[rsz - cpos - 1] = 0;
				keepseek = true;
				while (keepseek) {
					keepseek = reformatValue(hvalue);
				}

				std::wstringstream wss;
				wss << hkey;
				std::wstringstream wsv;
				wsv << hvalue;
				prof->header[wss.str()] = wsv.str();
			}
		}
	}

	return rsz;
}

int CurlSessionVT::performLogin(CurlProfile* prof)
{
	int rtn = 0;

	if (prof->siteUrl.length() > 0) {
		BOOL udc = FALSE;
		int rbt;
		size_t xallz;

		xallz = prof->siteUrl.length() * sizeof(wchar_t) + 1;
		char* ccurl = (char*)malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, prof->siteUrl.c_str(), prof->siteUrl.length(), ccurl, xallz, NULL, &udc);
		ccurl[rbt] = 0;

		xallz = prof->siteUser.length() * sizeof(wchar_t) + 1;
		char* ccuser = (char*)malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, prof->siteUser.c_str(), prof->siteUser.length(), ccuser, xallz, NULL, &udc);
		ccuser[rbt] = 0;

		xallz = prof->sitePasswd.length() * sizeof(wchar_t) + 1;
		char* ccpasswd = (char*)malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, prof->sitePasswd.c_str(), prof->sitePasswd.length(), ccpasswd, xallz, NULL, &udc);
		ccpasswd[rbt] = 0;

		xallz = strlen(ccuser) + strlen(ccpasswd);
		xallz++; // add len for ':'
		xallz++; // add len for terminator
		char* ccuserpwd = (char*)malloc(xallz);
		sprintf_s(ccuserpwd, xallz, "%s:%s", ccuser, ccpasswd);

		SetSessionCommonParms(core);
		curl_easy_setopt(core, CURLOPT_POST, 0);
		curl_easy_setopt(core, CURLOPT_URL, ccurl);
		//curl_easy_setopt(core, CURLOPT_TIMEOUT, 15);
		curl_easy_setopt(core, CURLOPT_HEADERDATA, prof);
		curl_easy_setopt(core, CURLOPT_HEADERFUNCTION, CB_CurlHeaderParse);
		curl_easy_setopt(core, CURLOPT_WRITEDATA, prof);
		curl_easy_setopt(core, CURLOPT_WRITEFUNCTION, CB_CurlResponseParse);
		curl_easy_setopt(core, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		curl_easy_setopt(core, CURLOPT_USERPWD, ccuserpwd);

		CURLcode res = curl_easy_perform(core);
		long rsp;
		curl_easy_getinfo(core, CURLINFO_RESPONSE_CODE, &rsp);
		if (rsp == 409) {
			if (prof->header.find(XSESSIONHTTPHEAD) != prof->header.end()) {
				prof->sessionId = prof->header[XSESSIONHTTPHEAD];
			}
		}
		else {
			rtn = -2;
		}
	}
	else {
		rtn = -1;
	}

	return rtn;
}

const std::wstring& CurlSessionVT::GetResponse(CurlProfile* prof)
{
	return prof->response;
}
