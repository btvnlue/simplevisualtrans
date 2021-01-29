#include "CurlSession.h"
#include "curl/curl.h"

#include <sstream>
#include <fstream>

#pragma comment(lib, "libcurl.lib")
//#pragma comment(lib, "ws2_32.lib")
//#pragma comment(lib, "Wldap32.lib")
//#pragma comment(lib, "Normaliz.lib")
//#pragma comment(lib, "libucrtd.lib")
//#pragma comment(lib, "libcmtd.lib")

#define xsession L"X-Transmission-Session-Id"

//int Log(const wchar_t* msg);
#define Log(x)

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

size_t CurlSession::CB_CurlHeaderParse(void* data, size_t size, size_t nmemb, void* userp)
{
	char hkey[1024];
	char hvalue[1024];
	CurlSession* self = (CurlSession*)userp;
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
				self->headers[wss.str()] = wsv.str();
			}
		}
	}

	return rsz;
}

size_t CurlSession::CB_CurlResponseParse(void* data, size_t size, size_t nmemb, void* userp)
{
	size_t rsz = size * nmemb;
//	size_t ors;
	CurlSession* self = (CurlSession*)userp;
	unsigned char* resbuf = (unsigned char*)malloc(rsz + 1);

	if (resbuf) {
		memcpy_s(resbuf, rsz, data, rsz);
		resbuf[rsz] = 0;

		std::wstringstream sss;
		sss << self->response.c_str();
		sss << (char*)resbuf;
		self->response = sss.str();
		free(resbuf);
	}

	return rsz;
}

/**
Name: performQuery
Description: internal use, send query/request to curl session, retry login and refresh connection token for 3 times
Input: query: wstring, query content to be posted to service
Input: level: int, call level, initial level 0, call self when retry
Return: 0: success, 1: internal, 2: unknown error, 3: connection fail, 5: out of memory
*/
int CurlSession::performQuery(const std::wstring& query, int level)
{
	int rtn = 0;

	if (sessionId.length() > 0) {
		int rbt;
		char cus[1024];
		char* cqry;
		char xscs[1024];
		char cbup[1024];
		char cuu[256];
		char cup[256];
		char errbuf[CURL_ERROR_SIZE];
		BOOL udc = FALSE;
		std::wstringstream xss;

		int usz = (query.length() + 1) * sizeof(wchar_t);
		cqry = (char*)malloc(usz);
		if (cqry) {
			rbt = ::WideCharToMultiByte(CP_ACP, 0, siteUrl.c_str(), siteUrl.length(), cus, sizeof(cus), NULL, &udc);
			cus[rbt] = 0;
			rbt = ::WideCharToMultiByte(CP_ACP, 0, query.c_str(), query.length(), cqry, usz, NULL, &udc);
			cqry[rbt] = 0;
			rbt = ::WideCharToMultiByte(CP_ACP, 0, siteUser.c_str(), siteUser.length(), cuu, sizeof(cuu), NULL, &udc);
			cuu[rbt] = 0;
			rbt = ::WideCharToMultiByte(CP_ACP, 0, sitePasswd.c_str(), sitePasswd.length(), cup, sizeof(cup), NULL, &udc);
			cup[rbt] = 0;
			sprintf_s(cbup, "%s:%s", cuu, cup);

			response.clear();

			setSessionCommonParms();

			xss << xsession << L": " << sessionId;
			std::wstring xstr = xss.str();
			rbt = ::WideCharToMultiByte(CP_ACP, 0, xstr.c_str(), xstr.length(), xscs, sizeof(xscs), NULL, &udc);
			xscs[rbt] = 0;
			struct curl_slist* custom_headers = curl_slist_append(NULL, xscs);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, custom_headers);

			//		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
			//		curl_easy_setopt(curl, CURLOPT_USERPWD, cbup);

			curl_easy_setopt(curl, CURLOPT_URL, cus);
			curl_easy_setopt(curl, CURLOPT_POST, 1);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CB_CurlResponseParse);
			//		curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
			//		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, CB_CurlHeaderParse);

			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cqry);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(cqry));

			curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
			Log(query.c_str());
			CURLcode res = curl_easy_perform(curl);
			if (res == CURLE_OK) {
				long rsp;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rsp);
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
			free(cqry);
		}
		else {
			rtn = 5;
		}
	}
	else {
		rtn = 1;
	}

	if (rtn == 1) {
		if (level < 3) {
			rtn = performLogin();
			if (rtn == 0) {
				rtn = performQuery(query, level + 1);
			}
		}
	}
	else {
		Log(response.c_str());
	}
	return rtn;
}

int CurlSession::cleanUp()
{
	curl_easy_cleanup(curl);
	
	return 0;
}

int CurlSession::performLogin()
{
	int rtn = 0;
	char cus[1024];
	char cuu[1024];
	char cup[1024];
	char cpu[1024];
	BOOL udc = FALSE;
	int rbt;

	rbt = ::WideCharToMultiByte(CP_ACP, 0, siteUrl.c_str(), siteUrl.length(), cus, sizeof(cus), NULL, &udc);
	cus[rbt] = 0;
	rbt = ::WideCharToMultiByte(CP_ACP, 0, siteUser.c_str(), siteUser.length(), cuu, sizeof(cuu), NULL, &udc);
	cuu[rbt] = 0;
	rbt = ::WideCharToMultiByte(CP_ACP, 0, sitePasswd.c_str(), sitePasswd.length(), cup, sizeof(cup), NULL, &udc);
	cup[rbt] = 0;
	sprintf_s(cpu, "%s:%s", cuu, cup);
	setSessionCommonParms();
	curl_easy_setopt(curl, CURLOPT_POST, 0);
	curl_easy_setopt(curl, CURLOPT_URL, cus);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, CB_CurlHeaderParse);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CB_CurlResponseParse);
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
	curl_easy_setopt(curl, CURLOPT_USERPWD, cpu);

	CURLcode res = curl_easy_perform(curl);
	long rsp;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rsp);
	if (rsp == 409) {
		if (headers.find(xsession) != headers.end()) {
			sessionId = headers[xsession];
		}
	}
	else {
		rtn = -1;
	}

	return rtn;
}

int CurlSession::setSiteInfo(const std::wstring& url, const std::wstring& user, const std::wstring& passwd)
{
	siteUrl = url;
	siteUser = user;
	sitePasswd = passwd;

	return 0;
}

int CurlSession::setSessionCommonParms()
{
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "SimpleClient/0.19.11");
//	curl_easy_setopt(crl, CURLOPT_URL, "http://192.168.5.158:9091/transmission/rpc");
//	curl_easy_setopt(crl, CURLOPT_POST, 1);
//	curl_easy_setopt(crl, CURLOPT_HEADERFUNCTION, CB_CurlHeaderParse);
//	curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, CB_CurlResponseParse);
//	curl_easy_setopt(crl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
//	curl_easy_setopt(crl, CURLOPT_USERPWD, "transmission:transmission");
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "");
	return 0;
}

CurlSession::CurlSession()
	: initialized(false)
{
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
}

CurlSession::~CurlSession()
{
	cleanUp();
	curl_global_cleanup();
}

CurlSessionTest::CurlSessionTest()
{
	CurlSession::CurlSession();
}

CurlSessionTest::~CurlSessionTest()
{
}

void CurlSessionTest::FileLoadResponse(const LPWSTR fname)
{
	HANDLE hfinp = CreateFile(fname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hfinp != INVALID_HANDLE_VALUE) {
		//std::wfstream ifr(L"tag2.txt", std::ios_base::in | std::ios_base::binary);
		DWORD rbs = GetFileSize(hfinp, NULL);
		if (rbs > 0) {
			unsigned char* cbt = (unsigned char*)malloc(rbs + sizeof(wchar_t));
			BOOL btn = ReadFile(hfinp, cbt, rbs, &rbs, NULL);
			if (btn) {
				*(wchar_t*)(cbt + rbs) = 0;
			}
			CurlSession::CB_CurlResponseParse(cbt, rbs, 1, this);
			free(cbt);
		}
		CloseHandle(hfinp);
	}
}

int CurlSessionTest::performQuery(const std::wstring& query, int level)
{
	response = std::wstring();
	if (query.find(L"\"tag\":2", 0) != std::wstring::npos) {
		FileLoadResponse((LPWSTR)L"tag2.txt");
	}
	else if (query.find(L"\"tag\":4", 0) != std::wstring::npos) {
		FileLoadResponse((LPWSTR)L"tag4.txt");
	}
	else if (query.find(L"\"tag\":5", 0) != std::wstring::npos) {
		FileLoadResponse((LPWSTR)L"tag5.txt");
	}
	else {
		CurlSession::performQuery(query, level);
	}

	return 0;
}
