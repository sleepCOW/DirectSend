#pragma once

#include "NetworkDevice.h"

using Addrinfo = struct addrinfo;

class Client : public NetworkDevice
{
public:
	Client(std::string Ip, std::string Port);

private:
    bool ConnectToServer(Addrinfo* start);
};

Client::Client(std::string Ip, std::string Port)
{
    WSADATA wsaData;
    Addrinfo hints;

    // Initialize Winsock
    BytesReceived = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (BytesReceived != 0) {
        std::cerr << "WSAStartup failed with error: " << BytesReceived << std::endl;
        return;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    addrinfo* result = nullptr;
    BytesReceived = getaddrinfo(Ip.data(), Port.data(), &hints, &result);
    if (ErrorCode != 0) {
        std::cerr << "getaddrinfo failed with error: " << ErrorCode << std::endl;
        return;
    }

    // Attempt to connect to an address until one succeeds
    while (not ConnectToServer(result))
    {
        std::cerr << "Reattempting in 1 second!" << std::endl;
        Sleep(1000);
    }

    if (ClientSocket == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server!" << std::endl;
        return;
    }
}

bool Client::ConnectToServer(Addrinfo* start)
{
    for (Addrinfo* ptr = start; ptr != NULL; ptr = ptr->ai_next) {
        // Create a SOCKET for connecting to server
        ClientSocket.GetSocket() = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ClientSocket == INVALID_SOCKET) {
            std::cerr << "socket failed with error: " << WSAGetLastError() << std::endl;
            return false;
        }

        // Connect to server
        
        ErrorCode = connect(ClientSocket.GetSocket(), ptr->ai_addr, (int)ptr->ai_addrlen);
        if (ErrorCode == SOCKET_ERROR) {
            std::cerr << "socket failed with error: " << WSAGetLastError() << std::endl;
            closesocket(ClientSocket);
            ClientSocket = INVALID_SOCKET;
            continue;
        }

        std::cout << "Succefully connected to the server!" << std::endl;
        return true;
    }

    std::cerr << "Failed to connect to the server!\n";
    return false;
}