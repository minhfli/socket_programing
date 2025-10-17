// client.cpp
// compile to exe with: g++ client.cpp -o client.exe -lws2_32

#define _WIN32_WINNT 0x0600  // Vista or later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

const int HEADER = 64;
const char* FORMAT = "utf-8";
const char* HOST = "127.0.0.1";
const int PORT = 8765;

SOCKET clientSocket;

// helper to send a string with header
void sendMsg(const std::string& msg) {
    std::string message = msg;
    int msgLength = static_cast<int>(message.size());

    // prepare header with padding
    std::string header = std::to_string(msgLength);
    header.append(HEADER - header.size(), ' ');

    // send header
    send(clientSocket, header.c_str(), HEADER, 0);

    // send body
    send(clientSocket, message.c_str(), msgLength, 0);
}

// helper to receive a string with header
std::string recvMsg() {
    char headerBuf[HEADER + 1] = {0};
    int bytesReceived = recv(clientSocket, headerBuf, HEADER, 0);
    if (bytesReceived <= 0) return "";

    int msgLength = std::stoi(headerBuf);

    std::string msg(msgLength, '\0');
    int totalReceived = 0;
    while (totalReceived < msgLength) {
        int received = recv(clientSocket, &msg[totalReceived], msgLength - totalReceived, 0);
        if (received <= 0) break;
        totalReceived += received;
    }

    std::cout << "[SERVER] len >> " << msgLength << " bytes\n";
    std::cout << "[SERVER] msg >> " << msg << "\n";
    return msg;
}

void disconnect() {
    sendMsg("!DISCONNECT");
    closesocket(clientSocket);
}

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    int wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaerr != 0) {
        std::cerr << "WSAStartup failed: " << wsaerr << std::endl;
        return 1;
    }

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    // Setup server address
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &serverAddr.sin_addr);

    // Connect
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "Connected to server " << HOST << ":" << PORT << "\n";
    std::cout << "Type messages to send. Type !DISCONNECT to quit.\n\n";

    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        sendMsg(input);

        if (input == "!DISCONNECT") {
            std::cout << "Disconnecting...\n";
            break;
        }

        std::string reply = recvMsg();
        if (reply.empty()) {
            std::cout << "Server closed the connection.\n";
            break;
        }
    }

    disconnect();

    // Cleanup
    WSACleanup();
    return 0;
}
