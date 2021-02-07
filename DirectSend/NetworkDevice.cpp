#include "NetworkDevice.h"

#include "stdint.h"
#include <string>
#include <array>
#include <iostream>

bool NetworkDevice::Send_Internal(OperationCode OpCode, const char* Data, uint16_t Size)
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

NetHeader NetworkDevice::MakeHeader(OperationCode OpCode, uint16_t Size)
{
	return NetHeader{ OpCode, Size };
}

NetworkDevice::~NetworkDevice()
{
	WSACleanup();
}

bool NetworkDevice::Send(OperationCode OpCode)
{
	return Send_Internal(OpCode, Buffer, BUFLEN);
}

bool NetworkDevice::Send(OperationCode OpCode, std::string& Data)
{
	return Send_Internal(OpCode, Data.data(), static_cast<uint16_t>(Data.size()));
}

bool NetworkDevice::Send(OperationCode OpCode, const char* Data)
{
	return Send_Internal(OpCode, Data, static_cast<uint16_t>(strlen(Data)));
}

bool NetworkDevice::Send(OperationCode OpCode, uint16_t NumberOfBytesToSend)
{
	return Send_Internal(OpCode, Buffer, NumberOfBytesToSend);
}

bool NetworkDevice::Send(OperationCode OpCode, const char* Data, uint16_t Size)
{
	return Send_Internal(OpCode, Data, Size);
}

char* NetworkDevice::GetData()
{
	return Buffer;
}

int NetworkDevice::GetBytesReceived() const
{
	return BytesReceived;
}

int NetworkDevice::GetErrorCode() const
{
	return ErrorCode;
}

Socket NetworkDevice::GetSocket()
{
	return ClientSocket;
}

bool NetworkDevice::CheckError(const char* ErrorMsg, int Error) const
{
	if (ErrorCode == Error)
	{
		std::cerr << ErrorMsg << ": " << WSAGetLastError() << std::endl;
		return false;
	}
	return true;
}