// client_udp.cpp
// Compile: g++ client_udp.cpp -o client_udp.exe -lws2_32
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "ws2_32.lib")

const char* HOST = "127.0.0.1";
const int PORT = 8765;
const int BUFSIZE = 1024;

void recvLoop(SOCKET sock) {
    char buf[BUFSIZE];
    sockaddr_in from;
    int fromLen = sizeof(from);
    while (true) {
        int bytes = recvfrom(sock, buf, BUFSIZE, 0, (sockaddr*)&from, &fromLen);
        if (bytes <= 0) continue;
        buf[bytes] = 0;
        std::cout << "\n[SERVER] " << buf << "\n> ";
        std::cout.flush();
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &serverAddr.sin_addr);

    // Send hello to register
    std::string hello = "hello";
    sendto(sock, hello.c_str(), (int)hello.size(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

    std::thread(recvLoop, sock).detach();

    while (true) {
        std::string msg;
        std::cout << "> ";
        std::getline(std::cin, msg);
        if (msg == "quit") break;
        sendto(sock, msg.c_str(), (int)msg.size(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
