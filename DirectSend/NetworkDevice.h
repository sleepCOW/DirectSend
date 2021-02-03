#pragma once

#include "stdint.h"
#include <string>
#include <array>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "Common.h"

template <size_t BufferSize>
class NetworkDevice
{
public:
	virtual ~NetworkDevice();

	template <typename Func, typename Cond>
	bool Listen(Func& Function, Cond& Condition);

	// Send current buffer
	bool Send(OperationCode OpCode);
	// Send num of bytes from current buffer
	bool Send(OperationCode OpCode, uint16_t NumberOfBytesToSend);
	bool Send(OperationCode OpCode, const char* Data);
	bool Send(OperationCode OpCode, std::string& Data);

	std::array<char, BufferSize>& GetBuffer();
	char* GetData();
	int GetBytesReceived() const;
	int GetErrorCode() const;
	Socket GetSocket();
	

	bool IsHeaderEnd();

	// Return true if no errors
	virtual bool CheckError(const char* ErrorMsg, int Error) const;

private:
	bool Send_Internal(OperationCode OpCode, const char* Data, uint16_t Size);
	NetHeader MakeHeader(OperationCode OpCode, uint16_t Size);

protected:
	Socket ClientSocket = INVALID_SOCKET;

	NetHeader Header;
	std::array<char, BufferSize> Buffer;

	int BytesReceived = 0;
	int PacketEnd = 0;
	int ErrorCode = 0;
};

template <size_t BufferSize>
bool NetworkDevice<BufferSize>::Send_Internal(OperationCode OpCode, const char* Data, uint16_t Size)
{
	NetHeader SendHeader = MakeHeader(OpCode, Size);
	ErrorCode = send(ClientSocket.GetSocket(), (char*)(&SendHeader), HeaderSize, 0);

	if (not CheckError("Send failed", SOCKET_ERROR))
	{
		return false;
	}

	ErrorCode = send(ClientSocket.GetSocket(), Data, SendHeader.PacketSize, 0);
	return CheckError("Send failed", SOCKET_ERROR);
}

template <size_t BufferSize>
NetHeader NetworkDevice<BufferSize>::MakeHeader(OperationCode OpCode, uint16_t Size)
{
	return NetHeader{ OpCode, Size };
}

template <size_t BufferSize>
NetworkDevice<BufferSize>::~NetworkDevice()
{
	WSACleanup();
}

template <size_t BufferSize>
bool NetworkDevice<BufferSize>::Send(OperationCode OpCode)
{
	return Send_Internal(OpCode, Buffer.data(), Buffer.size());
}

template <size_t BufferSize>
bool NetworkDevice<BufferSize>::Send(OperationCode OpCode, std::string& Data)
{
	return Send_Internal(OpCode, Data.data(), Data.size());
}

template <size_t BufferSize>
bool NetworkDevice<BufferSize>::Send(OperationCode OpCode, const char* Data)
{
	return Send_Internal(OpCode, Data, strlen(Data));
}

template <size_t BufferSize>
bool NetworkDevice<BufferSize>::Send(OperationCode OpCode, uint16_t NumberOfBytesToSend)
{
	return Send_Internal(OpCode, Buffer.data(), NumberOfBytesToSend);
}

template <size_t BufferSize>
char* NetworkDevice<BufferSize>::GetData()
{
	return Buffer.data();
}

template <size_t BufferSize>
std::array<char, BufferSize>& NetworkDevice<BufferSize>::GetBuffer()
{
	return Buffer;
}

template <size_t BufferSize>
int NetworkDevice<BufferSize>::GetBytesReceived() const
{
	return BytesReceived;
}

template <size_t BufferSize>
int NetworkDevice<BufferSize>::GetErrorCode() const
{
	return ErrorCode;
}

template <size_t BufferSize>
Socket NetworkDevice<BufferSize>::GetSocket()
{
	return ClientSocket;
}

template <size_t BufferSize>
bool NetworkDevice<BufferSize>::CheckError(const char* ErrorMsg, int Error) const
{
	if (ErrorCode == Error)
	{
		std::cerr << ErrorMsg << ": " << WSAGetLastError() << std::endl;
		return false;
	}
	return true;
}

template <size_t BufferSize>
template <typename Func, typename Cond>
bool NetworkDevice<BufferSize>::Listen(Func& Function, Cond& Condition)
{
	do
	{
		// Receive header
		BytesReceived = recv(ClientSocket, (char*)(&Header), HeaderSize, 0);

		while (BytesReceived < HeaderSize)
		{
			BytesReceived += recv(ClientSocket, (char*)(&Header) + BytesReceived, HeaderSize - BytesReceived, 0);
		}

		if (Header.PacketSize > BufferSize)
		{
			return false; // Error
		}

		// Receive actual data
		BytesReceived = 0;
		do
		{
			BytesReceived += recv(ClientSocket, Buffer.data() + BytesReceived, Header.PacketSize - BytesReceived, 0);

			if (BytesReceived > 0)
			{
			}
			else if (BytesReceived == 0)
			{
				std::cerr << "Connection closing..." << std::endl;
			}
			else
			{
				std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
				return false;
			}

		} while (BytesReceived != Header.PacketSize);

		Function(Header.OpCode);

	} while (Condition(Header.OpCode));

	return true;
}