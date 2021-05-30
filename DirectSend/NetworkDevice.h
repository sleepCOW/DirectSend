#pragma once

#include "Common.h"
#include <iostream>

#define BUFLEN 1024 * 16

/**
 * General network device
 * Can Listen/Send to the provided socket
 */
class NetworkDevice
{
public:
	virtual ~NetworkDevice();

	/**
	 * Listen to the socket in a sync manner
	 * @Function - function to perform on received package, must take OperationCode
	 * @Condition - function to perform check whether we should continue to listening, must take OperationCode
	 */
	template <typename Func, typename Cond>
	bool Listen(Func& Function, Cond& Condition);

	/**
	 * Send current buffer
	 * @OpCode - operation identifier for the package
	 * NOTE: Send is synchronous operation
	 * @return true on success, false otherwise
	 */
	bool Send(OperationCode OpCode);
	// Send num of bytes from current buffer
	bool Send(OperationCode OpCode, uint16_t NumberOfBytesToSend);
	bool Send(OperationCode OpCode, const char* Data);
	bool Send(OperationCode OpCode, const char* Data, uint16_t Size);
	bool Send(OperationCode OpCode, std::string& Data);

	char* GetData();
	constexpr DWORD GetBufferSize() const { return BUFLEN; }
	int GetBytesReceived() const;
	int GetErrorCode() const;
	Socket GetSocket();
	
	// #TODO: move to private/protected
	/**
	 * @return true if no errors
	 */
	virtual bool CheckError(const char* ErrorMsg, int Error) const;

private:
	bool Send_Internal(OperationCode OpCode, const char* Data, uint16_t Size);
	NetHeader MakeHeader(OperationCode OpCode, uint16_t Size);

protected:
	Socket ClientSocket = INVALID_SOCKET;

	NetHeader Header;
	char Buffer[BUFLEN];

	int BytesReceived = 0;
	int ErrorCode = 0;
};


template <typename Func, typename Cond>
bool NetworkDevice::Listen(Func& Function, Cond& Condition)
{
	do
	{
		// Receive header
		BytesReceived = recv(ClientSocket, (char*)(&Header), HeaderSize, 0);

		while (BytesReceived < HeaderSize)
		{
			BytesReceived += recv(ClientSocket, (char*)(&Header) + BytesReceived, HeaderSize - BytesReceived, 0);
		}

		if (Header.PacketSize > GetBufferSize())
		{
			return false; // Error
		}

		// Receive actual data
		BytesReceived = 0;
		do
		{
			BytesReceived += recv(ClientSocket, Buffer + BytesReceived, Header.PacketSize - BytesReceived, 0);

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