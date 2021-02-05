#include "Common.h"

std::chrono::milliseconds GetTimePast(std::chrono::steady_clock::time_point& start)
{
	auto stop = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
}

LPWSTR CharToWChar(const char* Str)
{
	LPWSTR WideStr;
	// required size
	int nChars = MultiByteToWideChar(CP_ACP, 0, Str, -1, NULL, 0);
	// allocate it
	WideStr = new WCHAR[nChars];
	MultiByteToWideChar(CP_ACP, 0, Str, -1, (LPWSTR)WideStr, nChars);
	return WideStr;
}

std::string GetFileName(char* FullName)
{
	std::string result;
	int NameStart = strlen(FullName) - 1;
	for (NameStart; NameStart > 0; --NameStart)
	{
		if (FullName[NameStart] == '\\' or FullName[NameStart] == '/')
		{
			break;
		}
		result.push_back(FullName[NameStart]);
	}
	reverse(begin(result), end(result));
	return result;
}

void CutToFileName(std::string& FullName)
{
	int NameStart = FullName.size() - 1;
	for (NameStart; NameStart > 0; --NameStart)
	{
		if (FullName[NameStart] == '\\' or FullName[NameStart] == '/')
		{
			++NameStart;
			break;
		}
	}
	FullName = FullName.substr(NameStart);
}