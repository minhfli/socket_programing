// client.cpp
// compile: g++ client.cpp -o client.exe -lws2_32
// note: only on windows

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

const int HEADER = 64;
const char* HOST = "0.0.0.0";
const int PORT = 8765;
SOCKET clientSocket;

void sendMsg(const std::string& msg) {
    std::string header = std::to_string((int)msg.size());
    header.append(HEADER - header.size(), ' ');
    send(clientSocket, header.c_str(), HEADER, 0);
    send(clientSocket, msg.c_str(), (int)msg.size(), 0);
}

std::string recvMsg() {
    char headerBuf[HEADER + 1] = {0};
    int bytes = recv(clientSocket, headerBuf, HEADER, 0);
    if (bytes <= 0) return "";
    int msgLen = std::stoi(headerBuf);
    std::string msg(msgLen, '\0');
    int total = 0;
    while (total < msgLen) {
        int r = recv(clientSocket, &msg[total], msgLen - total, 0);
        if (r <= 0) break;
        total += r;
    }
    return msg;
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // sock stream use TCP
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &serverAddr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to TicTacToe server!\n";

    bool running = true;

    // receive thread
    std::thread recvThread([&]() {
        while (running) {
            std::string msg = recvMsg();
            if (msg.empty()) {
                std::cout << "Server closed connection.\n";
                running = false;
                break;
            }
            std::cout << "\n" << msg << "\n> ";
        }
    });

    std::string input;
    while (running) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (input == "!exit") {
            running = false;
            break;
        }
        if (!input.empty()) sendMsg(input);
    }

    recvThread.join();
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
