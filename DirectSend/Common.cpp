#include "Common.h"

const char* ToStr(OperationCode OpCode)
{
	switch (OpCode)
	{
	case OperationCode::Invalid:
		return "Invalid";
	case OperationCode::ReceiveFileSeek:
		return "ReceiveFileSeek";
	case OperationCode::ReceiveFileName:
		return "ReceiveFileName";
	case OperationCode::ReceiveFileSize:
		return "ReceiveFileSize";
	case OperationCode::ReceiveFileData:
		return "ReceiveFileData";
	case OperationCode::FinishedTransfer:
		return "FinishedTransfer";
	default:
		return "OperationCode doesn't match with any values in enum!";
	}
}

std::chrono::seconds GetTimePast(std::chrono::steady_clock::time_point& start)
{
	auto stop = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::seconds>(stop - start);
}

LPWSTR CharToWChar(const char* Str)
{
	LPWSTR WideStr;
	// required size
	int nChars = MultiByteToWideChar(CP_ACP, 0, Str, -1, NULL, 0);
	// allocate it
	// #TODO remove memory leak
	WideStr = new WCHAR[nChars];
	MultiByteToWideChar(CP_ACP, 0, Str, -1, (LPWSTR)WideStr, nChars);
	return WideStr;
}

String GetFileName(char* FullName)
{
	String result;
	size_t NameStart = strlen(FullName) - 1;
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

String CutToFileName(const String& FullName)
{
	size_t NameStart = FullName.size() - 1;
	for (NameStart; NameStart > 0; --NameStart)
	{
		if (FullName[NameStart] == '\\' or FullName[NameStart] == '/')
		{
			++NameStart;
			break;
		}
	}
	return FullName.substr(NameStart);
}