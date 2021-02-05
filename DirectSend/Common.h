#pragma once

#include "winsock2.h"
#include <windows.h>
#include <string>
#include <chrono>
#include <ws2tcpip.h>

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

std::chrono::milliseconds GetTimePast(std::chrono::steady_clock::time_point& start);

LPWSTR CharToWChar(const char* Str);
std::string GetFileName(char* FullName);
void CutToFileName(std::string& FullName);