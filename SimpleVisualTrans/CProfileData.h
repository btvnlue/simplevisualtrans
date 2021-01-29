#pragma once

#include <string>
#include <map>

class CProfileData
{
	std::map<std::wstring, std::map<std::wstring, std::wstring>*> sections;
public:
	CProfileData();
	virtual ~CProfileData();
	int SetDefaultValue(const std::wstring& key, const std::wstring& value);
	int GetDefaultValue(const std::wstring& key, std::wstring& value);
	int LoadProfile(std::wstring pfname);
	int StoreProfile(std::wstring pfname);
};

