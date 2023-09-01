#include "CurlSessionCY.h"
#include "TransmissionProfile.h"
#include "Utilities.h"

//#include <Windows.h>
#include <sstream>

#define XSESSIONHTTPHEAD L"X-Transmission-Session-Id"

#ifdef CURL_STATICLIB
#pragma comment(lib, "libcurl_a.lib")
#pragma comment(lib, "crypt32.lib")
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

CurlSessionCY* CurlSessionCY::self = nullptr;

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

CurlSessionCY::CurlSessionCY()
{
	curl_global_init(CURL_GLOBAL_ALL);
	core = curl_easy_init();
}

CurlSessionCY::~CurlSessionCY()
{
	curl_easy_cleanup(core);
	curl_global_cleanup();

	header.clear();
	v_response.clear();
	sz_response = 0;
}

CurlSessionCY * CurlSessionCY::GetInstance()
{
	if (self == nullptr) {
		self = new CurlSessionCY();
	}
	return self;
}

int CurlSessionCY::CleanUp()
{
	if (self) {
		delete self;
		self = nullptr;
	}
	return 0;
}

int CurlSessionCY::SetSessionCommonParms(CURL* sss)
{
	curl_easy_setopt(sss, CURLOPT_USERAGENT, "SimpleTransClient/0.19.11");
	curl_easy_setopt(sss, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(sss, CURLOPT_ENCODING, "");

	return 0;
}

int CurlSessionCY::CurlParseReturnCode(int res)
{
	int rtn = 0;

	switch (res)
	{
	case CURLE_OK:
	{
		long rsp;
		curl_easy_getinfo(core, CURLINFO_RESPONSE_CODE, &rsp);
		switch (rsp)
		{
		case 200:
			rtn = 0;
			break;
		case 401:
			rtn = 1;
			break;
		case 409:
			rtn = 2;
			break;
		default:
			rtn = 6;
			break;
		}
	}
	break;
	case CURLE_COULDNT_CONNECT:
		rtn = 3;
		break;
	case CURLE_OPERATION_TIMEDOUT:
		rtn = 5;
		break;
	default:
		rtn = 4;
	}
	return rtn;
}

int CurlSessionCY::GetSessionToken(const std::wstring & url, const std::wstring & username, const std::wstring & passwd, std::wstring & token)
{
	int rtn = 0;
	if (url.length() > 0) {
		BOOL udc = FALSE;
		int rbt;
		size_t xallz;

		xallz = url.length() * sizeof(wchar_t) + 1;
		//char* ccurl = (char*)malloc(xallz);
		char* ccurl = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, url.c_str(), url.length(), ccurl, xallz, NULL, &udc);
		ccurl[rbt] = 0;

		xallz = username.length() * sizeof(wchar_t) + 1;
		//char* ccuser = (char*)malloc(xallz);
		char* ccuser = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, username.c_str(), username.length(), ccuser, xallz, NULL, &udc);
		ccuser[rbt] = 0;

		xallz = passwd.length() * sizeof(wchar_t) + 1;
		//char* ccpasswd = (char*)malloc(xallz);
		char* ccpasswd = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, passwd.c_str(), passwd.length(), ccpasswd, xallz, NULL, &udc);
		ccpasswd[rbt] = 0;

		char ccuserpwd[1024];
		sprintf_s(ccuserpwd, 1024, "%s:%s", ccuser, ccpasswd);
		DWORD b64sz;
		BOOL btn = CryptBinaryToString((unsigned char*)ccuserpwd, xallz, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &b64sz);
		if (b64sz > 0) {
			//TCHAR* b64str = (TCHAR*)malloc(b64sz * sizeof(TCHAR));
			//char* ccb64str = (char*)malloc(b64sz + 1);
			TCHAR* b64str = (TCHAR*)BufferedJsonAllocator::GetInstance()->Malloc(b64sz * sizeof(TCHAR));
			char* ccb64str = (char*)BufferedJsonAllocator::GetInstance()->Malloc(b64sz + 1);
			if (b64str) {
				btn = CryptBinaryToString((unsigned char*)ccuserpwd, xallz, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64str, &b64sz);
				if (btn) {
					rbt = ::WideCharToMultiByte(CP_ACP, 0, b64str, b64sz * 2, ccb64str, b64sz + 1, NULL, &udc);
					sprintf_s(ccuserpwd, 1024, "Basic %s", ccb64str);
				}
			}
			//free(ccb64str);
			//free(b64str);
		}


		SetSessionCommonParms(core);
		curl_easy_setopt(core, CURLOPT_POST, 1);
		curl_easy_setopt(core, CURLOPT_URL, ccurl);
		curl_easy_setopt(core, CURLOPT_TIMEOUT, 90);
		curl_easy_setopt(core, CURLOPT_HEADERDATA, this);
		curl_easy_setopt(core, CURLOPT_HEADERFUNCTION, CB_CurlHeaderParse);
		curl_easy_setopt(core, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(core, CURLOPT_WRITEFUNCTION, CB_CurlResponseParse);
		//curl_easy_setopt(core, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		curl_easy_setopt(core, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(core, CURLOPT_USERPWD, ccuserpwd);

		this->header.clear();
		this->sz_response = 0;
		this->v_response.clear();

		CURLcode res = curl_easy_perform(core);
		rtn = CurlParseReturnCode(res);
		if (rtn == 2) {
			if (this->header.find(XSESSIONHTTPHEAD) != this->header.end()) {
				token = this->header[XSESSIONHTTPHEAD];
				rtn = 0;
			}
		}

		//free(ccurl);
		//free(ccuser);
		//free(ccpasswd);
	}

	return rtn;
}

int CurlSessionCY::SendCurlRequest(TransmissionProfile* prof, const std::wstring &request, std::wstring & tts)
{
	int rtn = 0;
	std::wstringstream xss;
	BOOL udc;
	int rbt;
	//char* errbuf = (char*)malloc(CURL_ERROR_SIZE);
	char* errbuf = (char*)BufferedJsonAllocator::GetInstance()->Malloc(CURL_ERROR_SIZE);

	tts.clear();
	size_t xallz;

	xallz = prof->url.length() * sizeof(wchar_t) + 1;
	//char* ccurl = (char*)malloc(xallz);
	char* ccurl = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
	rbt = ::WideCharToMultiByte(CP_ACP, 0, prof->url.c_str(), prof->url.length(), ccurl, xallz, NULL, &udc);
	ccurl[rbt] = 0;
	curl_easy_setopt(core, CURLOPT_URL, ccurl);

	xss << XSESSIONHTTPHEAD << L": " << prof->token.c_str();
	std::wstring xstr = xss.str();
	size_t xssz = xstr.size();
	xallz = xssz * sizeof(wchar_t) + 1;
	//char* ccsss = (char*)malloc(xallz);
	char* ccsss = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
	rbt = ::WideCharToMultiByte(CP_ACP, 0, xss.str().c_str(), xss.str().length(), ccsss, xallz, NULL, &udc);
	ccsss[rbt] = 0;
	struct curl_slist* custom_headers = curl_slist_append(NULL, ccsss);
	curl_easy_setopt(core, CURLOPT_HTTPHEADER, custom_headers);

	xallz = request.length() * sizeof(wchar_t) + 1;
	//char* ccquery = (char*)malloc(xallz);
	char* ccquery = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
	rbt = ::WideCharToMultiByte(CP_ACP, 0, request.c_str(), request.length(), ccquery, xallz, NULL, &udc);
	ccquery[rbt] = 0;

	SetSessionCommonParms(core);
	curl_easy_setopt(core, CURLOPT_POST, 1);
	curl_easy_setopt(core, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(core, CURLOPT_WRITEFUNCTION, CB_CurlResponseParse);
	curl_easy_setopt(core, CURLOPT_TIMEOUT, 90);
	curl_easy_setopt(core, CURLOPT_POSTFIELDS, ccquery);
	curl_easy_setopt(core, CURLOPT_POSTFIELDSIZE, strlen(ccquery));

	curl_easy_setopt(core, CURLOPT_ERRORBUFFER, errbuf);

	self->v_response.clear();
	self->sz_response = 0;
	CURLcode res = curl_easy_perform(core);
	rtn = CurlParseReturnCode(res);
	switch (rtn)
	{
	case 2:
		if (this->header.find(XSESSIONHTTPHEAD) != this->header.end()) {
			prof->token = this->header[XSESSIONHTTPHEAD];
		}
		break;
	case 0:
		if (self->sz_response > 0) {
			unsigned long rbsz = self->sz_response + 1;
			char* resbuf = (char*)BufferedJsonAllocator::GetInstance()->Malloc(rbsz);
			resbuf[0] = 0;
			for (std::vector<char*>::iterator itrb = v_response.begin();
				itrb != v_response.end();
				itrb++) {
				strcat_s(resbuf, rbsz, *itrb);
			}
			int ressz = strlen(resbuf);
			int xccsz = (ressz + 1) * sizeof(wchar_t);
			wchar_t* wcbuf = (wchar_t*)BufferedJsonAllocator::GetInstance()->Malloc(xccsz);
			rbt = ::MultiByteToWideChar(CP_ACP, 0, resbuf, ressz, wcbuf, xccsz);
			wcbuf[rbt] = 0;
			tts = wcbuf;
		}
		break;
	}

	//free(ccurl);
	//free(ccquery);
	//free(ccsss);
	//free(errbuf);

	return rtn;
}

wchar_t cwbuf[2048];
wchar_t wwbuf[4500];

int CurlSessionCY::SendCurlRequestFile(TransmissionProfile* prof, const std::wstring &request, std::wstring & fname)
{
	int rtn = 0;
	BOOL udc;
	int rbt;

	char* errbuf = (char*)BufferedJsonAllocator::GetInstance()->Malloc(CURL_ERROR_SIZE);

	fname.clear();
	size_t xallz;

	xallz = prof->url.length() * sizeof(wchar_t) + 1;
	//char* ccurl = (char*)malloc(xallz);
	char* ccurl = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
	rbt = ::WideCharToMultiByte(CP_ACP, 0, prof->url.c_str(), prof->url.length(), ccurl, xallz, NULL, &udc);
	ccurl[rbt] = 0;
	curl_easy_setopt(core, CURLOPT_URL, ccurl);

	wsprintf(cwbuf, L"%s:%s", XSESSIONHTTPHEAD, prof->token.c_str());
	size_t xssz = wcslen(cwbuf);
	xallz = xssz * sizeof(wchar_t) + 1;
	//char* ccsss = (char*)malloc(xallz);
	char* ccsss = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
	rbt = ::WideCharToMultiByte(CP_ACP, 0, cwbuf, xssz, ccsss, xallz, NULL, &udc);
	ccsss[rbt] = 0;
	struct curl_slist* custom_headers = curl_slist_append(NULL, ccsss);
	curl_easy_setopt(core, CURLOPT_HTTPHEADER, custom_headers);

	//xallz = request.length() * sizeof(wchar_t) + 1;
	//char* ccquery = (char*)malloc(xallz);
	xallz = ::WideCharToMultiByte(CP_ACP, 0, request.c_str(), request.length(), NULL, 0, NULL, &udc);
	if (xallz > 0) {
		char* ccquery = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz + 1);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, request.c_str(), request.length(), ccquery, xallz, NULL, &udc);
		ccquery[rbt] = 0;

		UINT urt = GetTempFileName(L".", L"_se", 0, cwbuf);
		self->fn_response = cwbuf;

		SetSessionCommonParms(core);
		curl_easy_setopt(core, CURLOPT_POST, 1);
		curl_easy_setopt(core, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(core, CURLOPT_WRITEFUNCTION, CB_CurlResponseParseFile);
		curl_easy_setopt(core, CURLOPT_TIMEOUT, 90);
		curl_easy_setopt(core, CURLOPT_POSTFIELDS, ccquery);
		curl_easy_setopt(core, CURLOPT_POSTFIELDSIZE, strlen(ccquery));
		curl_easy_setopt(core, CURLOPT_ERRORBUFFER, errbuf);

		CURLcode res = curl_easy_perform(core);
		rtn = CurlParseReturnCode(res);
		switch (rtn)
		{
		case 2:
			if (this->header.find(XSESSIONHTTPHEAD) != this->header.end()) {
				prof->token = this->header[XSESSIONHTTPHEAD];
			}
			break;
		case 0:
			if (self->fn_response.length() > 0) {
				UINT urt = GetTempFileName(L".", L"_sp", 0, cwbuf);
				fname = cwbuf;

				HANDLE hdw = CreateFile(cwbuf, GENERIC_WRITE, 0, NULL, CREATE_NEW | OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hdw != INVALID_HANDLE_VALUE) {
					HANDLE hrr = CreateFile(self->fn_response.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hrr != INVALID_HANDLE_VALUE) {
						unsigned char* dat = (unsigned char*)cwbuf;
						DWORD dwrr;

						BOOL brr = ReadFile(hrr, dat, 4096, &dwrr, NULL);
						DWORD drshift;
						bool keepseek;
						while (dwrr > 0) {
							int epos = ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, (char*)dat, dwrr, NULL, 0);
							keepseek = epos == 0;
							drshift = 0;
							while (keepseek) {
								drshift++;
								epos = ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, (char*)dat, dwrr - drshift, NULL, 0);
								keepseek = epos == 0;
								keepseek = drshift < 10 ? keepseek : false;
							}
							if (epos > 0) {
								dwrr -= drshift;
								epos = ::MultiByteToWideChar(CP_ACP, 0, (char*)dat, dwrr, wwbuf, 4500);
								if (epos > 0) {
									brr = WriteFile(hdw, wwbuf, epos * sizeof(wchar_t), &dwrr, NULL);
								}
							}
							if (drshift > 0) {
								::SetFilePointer(hrr, -(int)drshift, NULL, FILE_CURRENT);
							}
							brr = ReadFile(hrr, dat, 4096, &dwrr, NULL);
						}
						CloseHandle(hrr);
						DeleteFile(self->fn_response.c_str());
					}
					else {
						rtn = 23;
					}
					CloseHandle(hdw);
				}
				else {
					rtn = 22;
				}
			}
			else {
				rtn = 21;
			}
			break;
		}
	}
	else {
		rtn = 12;
	}

	//free(ccurl);
	//free(ccquery);
	//free(ccsss);
	//free(errbuf);

	return rtn;
}

int CurlSessionCY::SendCurlRequestFileHandle(TransmissionProfile* prof, const std::wstring &request, HANDLE hresp)
{
	int rtn = 0;
	BOOL udc;
	int rbt;

	char* errbuf = (char*)BufferedJsonAllocator::GetInstance()->Malloc(CURL_ERROR_SIZE);

	size_t xallz;

	xallz = prof->url.length() * sizeof(wchar_t) + 1;
	//char* ccurl = (char*)malloc(xallz);
	char* ccurl = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
	rbt = ::WideCharToMultiByte(CP_ACP, 0, prof->url.c_str(), prof->url.length(), ccurl, xallz, NULL, &udc);
	ccurl[rbt] = 0;
	curl_easy_setopt(core, CURLOPT_URL, ccurl);

	wsprintf(cwbuf, L"%s:%s", XSESSIONHTTPHEAD, prof->token.c_str());
	size_t xssz = wcslen(cwbuf);
	xallz = xssz * sizeof(wchar_t) + 1;
	//char* ccsss = (char*)malloc(xallz);
	char* ccsss = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz);
	rbt = ::WideCharToMultiByte(CP_ACP, 0, cwbuf, xssz, ccsss, xallz, NULL, &udc);
	ccsss[rbt] = 0;
	struct curl_slist* custom_headers = curl_slist_append(NULL, ccsss);
	curl_easy_setopt(core, CURLOPT_HTTPHEADER, custom_headers);

	//xallz = request.length() * sizeof(wchar_t) + 1;
	//char* ccquery = (char*)malloc(xallz);
	xallz = ::WideCharToMultiByte(CP_ACP, 0, request.c_str(), request.length(), NULL, 0, NULL, &udc);
	if (xallz > 0) {
		char* ccquery = (char*)BufferedJsonAllocator::GetInstance()->Malloc(xallz + 1);
		rbt = ::WideCharToMultiByte(CP_ACP, 0, request.c_str(), request.length(), ccquery, xallz, NULL, &udc);
		ccquery[rbt] = 0;

		UINT urt = GetTempFileName(L".", L"_se", 0, cwbuf);
		h_response = CreateFile(cwbuf, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW | OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);

		SetSessionCommonParms(core);
		curl_easy_setopt(core, CURLOPT_POST, 1);
		curl_easy_setopt(core, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(core, CURLOPT_WRITEFUNCTION, CB_CurlResponseParseFileHandle);
		curl_easy_setopt(core, CURLOPT_TIMEOUT, 90);
		curl_easy_setopt(core, CURLOPT_POSTFIELDS, ccquery);
		curl_easy_setopt(core, CURLOPT_POSTFIELDSIZE, strlen(ccquery));
		curl_easy_setopt(core, CURLOPT_ERRORBUFFER, errbuf);

		CURLcode res = curl_easy_perform(core);
		rtn = CurlParseReturnCode(res);
		switch (rtn)
		{
		case 2:
			if (this->header.find(XSESSIONHTTPHEAD) != this->header.end()) {
				prof->token = this->header[XSESSIONHTTPHEAD];
			}
			break;
		case 0:
			//UINT urt = GetTempFileName(L".", L"_sp", 0, cwbuf);
			//fname = cwbuf;

			//HANDLE hdw = CreateFile(cwbuf, GENERIC_WRITE, 0, NULL, CREATE_NEW | OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hresp != INVALID_HANDLE_VALUE) {
				//HANDLE hrr = CreateFile(self->fn_response.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (h_response != INVALID_HANDLE_VALUE) {
					unsigned char* dat = (unsigned char*)cwbuf;
					DWORD dwrr;

					dwrr = SetFilePointer(h_response, 0, NULL, FILE_BEGIN);
					BOOL brr = ReadFile(h_response, dat, 4096, &dwrr, NULL);
					while (dwrr > 0) {
						int epos = ::MultiByteToWideChar(CP_ACP, 0, (char*)dat, dwrr, wwbuf, 4500);
						if (epos > 0) {
							brr = WriteFile(hresp, wwbuf, epos * sizeof(wchar_t), &dwrr, NULL);
						}
						brr = ReadFile(h_response, dat, 4096, &dwrr, NULL);
					}
					CloseHandle(h_response);
				}
				else {
					rtn = 23;
				}
				//CloseHandle(hdw);
			}
			else {
				rtn = 22;
			}
			break;
		}
	}
	else {
		rtn = 12;
	}

	//free(ccurl);
	//free(ccquery);
	//free(ccsss);
	//free(errbuf);

	return rtn;
}

size_t CurlSessionCY::CB_CurlHeaderParse(void* data, size_t size, size_t nmemb, void* userp)
{
	char hkey[1024];
	char hvalue[1024];
	CurlSessionCY* self = (CurlSessionCY*)userp;
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
				self->header[wss.str()] = wsv.str();
				wss.clear();
				wsv.clear();
			}
		}
	}

	return rsz;
}


size_t CurlSessionCY::CB_CurlResponseParse(void* data, size_t size, size_t nmemb, void* userp)
{
	size_t rsz = size * nmemb;
	CurlSessionCY* self = (CurlSessionCY*)userp;
	char* resbuf = (char*)BufferedJsonAllocator::GetInstance()->Malloc(rsz + 1);

	if (resbuf) {
		memcpy_s(resbuf, rsz, data, rsz);
		resbuf[rsz] = 0;

		self->v_response.insert(self->v_response.end(), resbuf);
		self->sz_response += rsz;
	}

	return rsz;
}

size_t CurlSessionCY::CB_CurlResponseParseFile(void * data, size_t size, size_t nmemb, void * userp)
{
	size_t rsz = 0;
	CurlSessionCY* self = (CurlSessionCY*)userp;

	if (self->fn_response.length() > 0) {
		rsz = size * nmemb;
		HANDLE hwwt = CreateFile(self->fn_response.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW | OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hwwt != INVALID_HANDLE_VALUE) {
			DWORD dwPos = SetFilePointer(hwwt, 0, NULL, FILE_END);
			BOOL blf;
			//BOOL blf = LockFile(hwwt, dwPos, 0, rsz, 0);
			//if (blf) {
			DWORD dbw;
			blf = WriteFile(hwwt, data, rsz, &dbw, NULL);
			//blf = UnlockFile(hwwt, dwPos, 0, rsz, 0);
		//}
			CloseHandle(hwwt);
		}
	}
	return rsz;
}

size_t CurlSessionCY::CB_CurlResponseParseFileHandle(void * data, size_t size, size_t nmemb, void * userp)
{
	size_t rsz = 0;
	CurlSessionCY* self = (CurlSessionCY*)userp;

	//if (self->fn_response.length() > 0) {
		rsz = size * nmemb;
	//	HANDLE hwwt = CreateFile(self->fn_response.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW | OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (self->h_response != INVALID_HANDLE_VALUE) {
			DWORD dwPos = SetFilePointer(self->h_response, 0, NULL, FILE_END);
			BOOL blf;
			//BOOL blf = LockFile(hwwt, dwPos, 0, rsz, 0);
			//if (blf) {
			DWORD dbw;
			blf = WriteFile(self->h_response, data, rsz, &dbw, NULL);
			//blf = UnlockFile(hwwt, dwPos, 0, rsz, 0);
		//}
			//CloseHandle(hwwt);
		}
	//}
	return rsz;
}
