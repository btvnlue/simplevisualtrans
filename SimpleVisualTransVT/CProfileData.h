#pragma once

#include <string>
#include <map>

class CProfileData
{
	std::map<std::wstring, std::map<std::wstring, std::wstring>*> sections;
	std::wstring lastprofilename;
public:
	CProfileData();
	virtual ~CProfileData();
	int SetDefaultValue(const std::wstring& key, const std::wstring& value);
	int GetDefaultValue(const std::wstring& key, std::wstring& value);
	int LoadProfile(const std::wstring& pfname = L"");
	int StoreProfile(const std::wstring& pfname = L"");
};

