#pragma once

#include "NetworkDevice.h"

// Synchronous TCP Server that listens only 1 client
template <size_t BufferSize>
class Server : public NetworkDevice<BufferSize>
{
public:
	Server(std::string Port);
};

template <size_t BufferSize>
Server<BufferSize>::Server(std::string Port)
{
	WSADATA Wsadata;
	this->ErrorCode = WSAStartup(MAKEWORD(2, 2), &Wsadata);
	if (this->ErrorCode != 0)
	{
		std::cerr << "WSAStartup failed: " << this->ErrorCode << std::endl;
		return;
	}
	
	sockaddr_in SocketDesc;
	ZeroMemory(&SocketDesc, sizeof(sockaddr_in));
	SocketDesc.sin_family = AF_INET;
	SocketDesc.sin_port = htons(atoi(Port.data()));
	SocketDesc.sin_addr.s_addr = INADDR_ANY;

	SOCKET ListenSocket = INVALID_SOCKET;

	// Create a SOCKET for the server to listen for client connections
	ListenSocket = socket(SocketDesc.sin_family, SOCK_STREAM, IPPROTO_TCP);

	if (ListenSocket == INVALID_SOCKET) {
		std::cerr << "Error at socket(): " << WSAGetLastError << std::endl;
		return;
	}
	
	this->ErrorCode = bind(ListenSocket, (SOCKADDR*)&SocketDesc, sizeof(sockaddr_in));
	if (this->ErrorCode == SOCKET_ERROR) {
		std::cerr << "bind failed with error: " << WSAGetLastError << std::endl;
		return;
	}

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed with error: " << WSAGetLastError << std::endl;
		return;
	}

	// Accept a client socket
	this->ClientSocket.GetSocket() = accept(ListenSocket, NULL, NULL);
	if (this->ClientSocket == INVALID_SOCKET) {
		std::cerr << "accept failed: " << WSAGetLastError << std::endl;
		return;
	}
}