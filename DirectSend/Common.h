#pragma once

#include "winsock2.h"
#include <windows.h>
#include <string>
#include <chrono>
#include <ws2tcpip.h>

using TimePoint = std::chrono::steady_clock::time_point;

enum class OperationCode : uint8_t
{
	Invalid = 0,
	ReceiveFileName,
	ReceiveFileSize,
	ReceiveFileData,
	FinishedTransfer
};

struct NetHeader
{
	OperationCode OpCode;
	uint16_t PacketSize;
};

struct Buffer
{
	char* Data;
	uint16_t Size = 0;

	Buffer(uint16_t _Size) : Size(_Size)
	{
		Data = new char[Size];
	}

	~Buffer()
	{
		delete[] Data;
	}
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

struct FileHandle
{
	FileHandle(HANDLE Handle) : m_Handle(Handle) {}
	~FileHandle() { CloseHandle(m_Handle); }

	void operator=(HANDLE Handle)
	{
		if (m_Handle != nullptr)
			CloseHandle(m_Handle);

		m_Handle = Handle;
	}

	operator HANDLE() const { return m_Handle; }

	HANDLE m_Handle;
};

struct LargeInteger
{
	LargeInteger(int64_t Value = 0)
	{
		m_Number.QuadPart = 0;
	}

	LargeInteger(const char* Data)
	{
		m_Number = *(LargeInteger*)Data;
	}

	LONGLONG ToMB() const { return m_Number.QuadPart / 1024 / 1024; }
	double ToMBd() const { return double(m_Number.QuadPart) / 1024. / 1024.; }
	LONGLONG ToGB() const { return m_Number.QuadPart / 1024 / 1024 / 1024; }
	double ToGBd() const { return double(m_Number.QuadPart) / 1024. / 1024. / 1024.; }

	void operator +=(LONGLONG Value) { m_Number.QuadPart += Value; }
	void operator -=(LONGLONG Value) { m_Number.QuadPart -= Value; }
	LargeInteger operator +(LONGLONG Value) { return m_Number.QuadPart -= Value; }
	LargeInteger operator -(LONGLONG Value) { return m_Number.QuadPart -= Value; }

	operator LARGE_INTEGER() const { return m_Number; }
	operator PLARGE_INTEGER() const { return PLARGE_INTEGER(this); }
	operator char*() const { return (char*)(this); }
	operator double() const { return static_cast<double>(m_Number.QuadPart); }

	LARGE_INTEGER m_Number;
};

std::chrono::milliseconds GetTimePast(std::chrono::steady_clock::time_point& start);

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)
#define MAKE_UNIQUE(x) CONCATENATE(x, __COUNTER__)

template <size_t Order>
bool EachNSeconds(float Seconds)
{
	static TimePoint Start = std::chrono::high_resolution_clock::now();
	TimePoint Stop = std::chrono::high_resolution_clock::now();
	bool bResult = std::chrono::duration_cast<std::chrono::seconds>(Stop - Start).count() > Seconds;
	if (bResult)
		Start = std::chrono::high_resolution_clock::now();
	return bResult;
}

#define EACH_N_SECONDS(Seconds) EachNSeconds<__LINE__>(Seconds)

LPWSTR CharToWChar(const char* Str);
std::string GetFileName(char* FullName);
void CutToFileName(std::string& FullName);