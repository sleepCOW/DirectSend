#pragma once

#include <windows.h>
#include <chrono>

using std::string;

std::chrono::milliseconds GetTimePast(std::chrono::steady_clock::time_point& start)
{
	auto stop = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
}

bool IsHeaderEnd(char* Array, size_t Size)
{
	for (size_t I = 0; I < Size; ++I)
	{
		if (Array[I] == ';')
		{
			return true;
		}
	}
	return false;
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

enum class OperationCode : uint8_t
{
	Invalid = 0,
	ReceiveFileName,
	ReceiveFileData,
	FinishedTransfer
};

struct NetHeader
{
	OperationCode OpCode;
	uint16_t PacketSize;
};

constexpr WORD HeaderSize = sizeof(NetHeader);

class Socket
{
public:
	Socket() : m_Socket(INVALID_SOCKET) {}
	Socket(SOCKET Sock) : m_Socket(Sock) {}

	~Socket()
	{
		closesocket(m_Socket);
	}

	operator SOCKET() const { return m_Socket; }
	SOCKET& GetSocket() { return m_Socket; }

protected:
	SOCKET m_Socket;
};

string GetFileName(char* FullName)
{
	string result;
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