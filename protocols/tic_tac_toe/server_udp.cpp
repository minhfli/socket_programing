// server_udp.cpp
// Compile: g++ server_udp.cpp -o server_udp.exe -lws2_32
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

const int PORT = 8765;
const int BUFSIZE = 1024;
const char* HOST = "0.0.0.0";

struct ClientInfo {
    sockaddr_in addr;
    int id;
};

std::map<std::string, ClientInfo> clients; // key = "ip:port"
std::mutex mtx;

std::string keyOf(sockaddr_in addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(addr.sin_port);
    return std::string(ip) + ":" + std::to_string(port);
}

void sendTo(SOCKET sock, const std::string& msg, sockaddr_in& addr) {
    sendto(sock, msg.c_str(), (int)msg.size(), 0, (sockaddr*)&addr, sizeof(addr));
}

void gameLoop(SOCKET sock, ClientInfo a, ClientInfo b) {
    std::vector<char> board(9, ' ');
    int turn = 0; // 0: a, 1: b
    bool running = true;

    auto printBoard = [&]() {
        std::string s;
        for (int i = 0; i < 9; i++) {
            s += (board[i] == ' ' ? std::to_string(i + 1) : std::string(1, board[i]));
            if (i % 3 == 2) s += "\n";
            else s += "|";
        }
        return s;
    };

    auto checkWin = [&]() {
        const int win[8][3] = {
            {0,1,2},{3,4,5},{6,7,8},
            {0,3,6},{1,4,7},{2,5,8},
            {0,4,8},{2,4,6}
        };
        for (auto& w : win) {
            if (board[w[0]] != ' ' && board[w[0]] == board[w[1]] && board[w[1]] == board[w[2]])
                return board[w[0]];
        }
        return ' ';
    };

    while (running) {
        ClientInfo current = (turn == 0) ? a : b;
        ClientInfo other   = (turn == 0) ? b : a;

        std::string msg = "BOARD:\n" + printBoard() + "\nYour turn (" + std::string(turn == 0 ? "X" : "O") + "): ";
        sendTo(sock, msg, current.addr);
        sendTo(sock, "Waiting for opponent...\n", other.addr);

        // wait for move
        char buf[BUFSIZE];
        sockaddr_in from;
        int fromLen = sizeof(from);
        int bytes = recvfrom(sock, buf, BUFSIZE, 0, (sockaddr*)&from, &fromLen);
        if (bytes <= 0) break;
        buf[bytes] = 0;

        int move = atoi(buf);
        if (move < 1 || move > 9 || board[move - 1] != ' ') {
            sendTo(sock, "Invalid move.\n", current.addr);
            continue;
        }

        board[move - 1] = (turn == 0) ? 'X' : 'O';
        char winner = checkWin();

        if (winner != ' ') {
            std::string endMsg = "Player " + std::string(1, winner) + " wins!\n" + printBoard();
            sendTo(sock, endMsg, a.addr);
            sendTo(sock, endMsg, b.addr);
            break;
        }

        bool draw = true;
        for (char c : board) if (c == ' ') draw = false;
        if (draw) {
            std::string endMsg = "Draw!\n" + printBoard();
            sendTo(sock, endMsg, a.addr);
            sendTo(sock, endMsg, b.addr);
            break;
        }

        turn = 1 - turn;
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
    bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));

    std::cout << "UDP Tic Tac Toe server started on port " << PORT << "\n";

    int nextID = 1;
    char buffer[BUFSIZE];
    sockaddr_in clientAddr;
    int clientLen = sizeof(clientAddr);

    while (true) {
        int bytes = recvfrom(sock, buffer, BUFSIZE, 0, (sockaddr*)&clientAddr, &clientLen);
        if (bytes <= 0) continue;
        buffer[bytes] = 0;

        std::string key = keyOf(clientAddr);
        std::string msg(buffer);

        std::lock_guard<std::mutex> lock(mtx);
        if (!clients.count(key)) {
            ClientInfo info{clientAddr, nextID++};
            clients[key] = info;
            sendTo(sock, "Your ID: " + std::to_string(info.id) + "\n", clientAddr);
            continue;
        }

        ClientInfo sender = clients[key];
        if (msg.substr(0, 4) == "play") { // smessage tart with play
            int targetID = std::stoi(msg.substr(4));
            ClientInfo target{};
            bool found = false;
            for (auto& [_, c] : clients)
                if (c.id == targetID) { target = c; found = true; break; }

            if (!found) {
                sendTo(sock, "Target not found.\n", sender.addr);
                continue;
            }

            sendTo(sock, "Player " + std::to_string(sender.id) + " challenges you! Reply 'y' or 'n'\n", target.addr);
            sendTo(sock, "Waiting for opponent to accept...\n", sender.addr);

            // wait for reply
            sockaddr_in replyAddr;
            int replyLen = sizeof(replyAddr);
            int replyBytes = recvfrom(sock, buffer, BUFSIZE, 0, (sockaddr*)&replyAddr, &replyLen);
            if (replyBytes <= 0) continue;
            buffer[replyBytes] = 0;
            std::string reply(buffer);

            if (reply == "y" || reply == "Y") {
                sendTo(sock, "Match starting!\n", sender.addr);
                sendTo(sock, "Match starting!\n", target.addr);
                std::thread(gameLoop, sock, sender, target).detach();
            } else {
                sendTo(sock, "Opponent declined.\n", sender.addr);
                sendTo(sock, "You declined.\n", target.addr);
            }
        }
    }

    closesocket(sock);
    WSACleanup();
}
