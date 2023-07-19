#pragma once

#include <string>
#include <map>

typedef std::map<std::wstring, std::wstring> CONTENTS;
typedef std::map<std::wstring, CONTENTS*> SECTIONS;

class INIFileTool
{
public:
	SECTIONS sections;
	virtual ~INIFileTool();
	int LoadConfig(std::wstring filename);
	int SaveConfig(std::wstring filename);

};

