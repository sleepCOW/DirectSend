#pragma once

#include "NetworkDevice.h"

/**
 * Synchronous TCP Server that listens only 1 client
 */
class Server : public NetworkDevice
{
public:
	Server(std::string Port);
};

Server::Server(std::string Port)
{
	WSADATA Wsadata;
	ErrorCode = WSAStartup(MAKEWORD(2, 2), &Wsadata);
	if (ErrorCode != 0)
	{
		std::cerr << "WSAStartup failed: " << ErrorCode << std::endl;
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
	
	ErrorCode = bind(ListenSocket, (SOCKADDR*)&SocketDesc, sizeof(sockaddr_in));
	if (ErrorCode == SOCKET_ERROR) {
		std::cerr << "bind failed with error: " << WSAGetLastError << std::endl;
		return;
	}

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed with error: " << WSAGetLastError << std::endl;
		return;
	}

	// Accept a client socket
	ClientSocket.GetSocket() = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		std::cerr << "accept failed: " << WSAGetLastError << std::endl;
		return;
	}
	std::cout << "Client is connected!\n";
}