#pragma once

#include "NetworkDevice.h"

using Addrinfo = struct addrinfo;

/**
 * Device that is trying to connect to the provided host(server)
 */
class Client : public NetworkDevice
{
public:
	Client(String Ip, String Port);

private:
    bool ConnectToServer(Addrinfo* start);
};

Client::Client(String Ip, String Port)
{
    WSADATA wsaData;
    Addrinfo hints;

    // Initialize Winsock
    BytesReceived = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (BytesReceived != 0) {
        CMD::PrintDebug() << "WSAStartup failed with error: " << BytesReceived << std::endl;
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
        CMD::PrintDebug() << "getaddrinfo failed with error: " << ErrorCode << std::endl;
        return;
    }

    // Attempt to connect to an address until one succeeds
    while (not ConnectToServer(result))
    {
        CMD::Print() << "Reattempting in 1 second!" << std::endl;
        Sleep(1000);
    }

    if (ClientSocket == INVALID_SOCKET) {
        CMD::Print() << "Unable to connect to server!" << std::endl;
        return;
    }
}

bool Client::ConnectToServer(Addrinfo* start)
{
    for (Addrinfo* ptr = start; ptr != NULL; ptr = ptr->ai_next) {
        // Create a SOCKET for connecting to server
        ClientSocket.GetSocket() = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ClientSocket == INVALID_SOCKET) {
            CMD::PrintDebug() << "socket failed with error: " << WSAGetLastError() << std::endl;
            return false;
        }

        // Connect to server
        
        ErrorCode = connect(ClientSocket.GetSocket(), ptr->ai_addr, (int)ptr->ai_addrlen);
        if (ErrorCode == SOCKET_ERROR) {
            CMD::PrintDebug() << "socket failed with error: " << WSAGetLastError() << std::endl;
            closesocket(ClientSocket);
            ClientSocket = INVALID_SOCKET;
            continue;
        }

        CMD::Print() << "Succefully connected to the server!" << std::endl;
        return true;
    }

    CMD::PrintDebug() << "Failed to connect to the server!\n";
    return false;
}