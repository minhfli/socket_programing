// server.cpp
// compile: g++ server.cpp -o server.exe -lws2_32
// note: only on windows

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <map>
#include <vector>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

const int HEADER = 64;
const int PORT = 8765;

std::mutex mtx;
int clientCounter = 1;

struct Game {
    int p1, p2;
    char board[9];
    int turn;
    bool active;
};

std::map<int, SOCKET> clients;
std::map<int, Game> games; // key = smaller id
std::map<int, int> currentOpponent; // id -> opponent id

// --- Helper functions ---
void sendMsg(SOCKET sock, const std::string& msg) {
    std::string header = std::to_string((int)msg.size());
    header.append(HEADER - header.size(), ' ');
    send(sock, header.c_str(), HEADER, 0);
    send(sock, msg.c_str(), (int)msg.size(), 0);
}

std::string recvMsg(SOCKET sock) {
    char headerBuf[HEADER + 1] = {0};
    int bytes = recv(sock, headerBuf, HEADER, 0);
    if (bytes <= 0) return "";
    int msgLen = std::stoi(headerBuf);
    std::string msg(msgLen, '\0');
    int total = 0;
    while (total < msgLen) {
        int r = recv(sock, &msg[total], msgLen - total, 0);
        if (r <= 0) break;
        total += r;
    }
    return msg;
}

std::string renderBoard(const char b[9]) {
    std::string s;
    for (int i = 0; i < 9; i++) {
        s += (b[i] == ' ' ? std::to_string(i + 1) : std::string(1, b[i]));
        if ((i + 1) % 3 == 0) s += "\n";
        else s += " | ";
    }
    return s;
}

int checkWinner(const char b[9]) {
    int wins[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},
        {0,3,6}, {1,4,7}, {2,5,8},
        {0,4,8}, {2,4,6}
    };
    for (auto &w : wins) {
        if (b[w[0]] != ' ' && b[w[0]] == b[w[1]] && b[w[1]] == b[w[2]]) {
            return b[w[0]] == 'X' ? 1 : 2;
        }
    }
    for (int i = 0; i < 9; i++)
        if (b[i] == ' ') return 0;
    return 3; // draw
}

// --- Client thread ---
void handleClient(int id) {
    SOCKET sock = clients[id];
    sendMsg(sock, "Your ID is " + std::to_string(id));

    while (true) {
        std::string msg = recvMsg(sock);
        if (msg.empty()) break;

        std::lock_guard<std::mutex> lock(mtx);

        // handle "PLAY <id>"
        if (msg.rfind("PLAY ", 0) == 0) {
            int target = std::stoi(msg.substr(5));
            if (clients.find(target) == clients.end()) {
                sendMsg(sock, "No client with ID " + std::to_string(target));
                continue;
            }
            sendMsg(clients[target], "Client " + std::to_string(id) + " wants to play. (y/n)?");
            currentOpponent[target] = id;
            continue;
        }

        // handle "Y" or "N"
        if (msg == "y" || msg == "Y") {
            int challenger = currentOpponent[id];
            if (!challenger) continue;
            Game g;
            g.p1 = challenger; g.p2 = id;
            g.turn = challenger;
            g.active = true;
            std::fill(std::begin(g.board), std::end(g.board), ' ');
            games[std::min(id, challenger)] = g;
            sendMsg(clients[challenger], "Game start! You are X\n" + renderBoard(g.board));
            sendMsg(clients[id], "Game start! You are O\n" + renderBoard(g.board));
            sendMsg(clients[g.turn], "Your move (1-9):");
            continue;
        }

        if (msg == "n" || msg == "N") {
            int challenger = currentOpponent[id];
            if (challenger) sendMsg(clients[challenger], "Client " + std::to_string(id) + " declined.");
            continue;
        }

        // handle moves (number 1-9)
        if (msg.size() == 1 && msg[0] >= '1' && msg[0] <= '9') {
            int pos = msg[0] - '1';
            int key = -1;
            Game* gptr = nullptr;
            for (auto &kv : games) {
                if ((kv.second.p1 == id || kv.second.p2 == id) && kv.second.active) {
                    gptr = &kv.second;
                    key = kv.first;
                    break;
                }
            }
            if (!gptr) continue;
            Game &g = *gptr;
            if (g.turn != id) {
                sendMsg(sock, "Not your turn.");
                continue;
            }
            if (g.board[pos] != ' ') {
                sendMsg(sock, "Invalid move.");
                continue;
            }
            char mark = (id == g.p1) ? 'X' : 'O';
            g.board[pos] = mark;

            int win = checkWinner(g.board);
            if (win) {
                std::string result = renderBoard(g.board);
                if (win == 1) result += "Player X wins!";
                else if (win == 2) result += "Player O wins!";
                else result += "It's a draw!";
                sendMsg(clients[g.p1], result);
                sendMsg(clients[g.p2], result);
                g.active = false;
                continue;
            }

            g.turn = (id == g.p1) ? g.p2 : g.p1;
            std::string boardView = renderBoard(g.board);
            sendMsg(clients[g.p1], boardView + "\n" + (g.turn == g.p1 ? "Your move:" : "Opponent's move..."));
            sendMsg(clients[g.p2], boardView + "\n" + (g.turn == g.p2 ? "Your move:" : "Opponent's move..."));
            continue;
        }
    }

    std::cout << "Client " << id << " disconnected.\n";
    closesocket(sock);
    clients.erase(id);
    currentOpponent.erase(id);
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    // sock stream use TCP
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listenSock, (sockaddr*)&addr, sizeof(addr));
    listen(listenSock, 5);
    std::cout << "Server running on port " << PORT << "\n";

    while (true) {
        sockaddr_in caddr{};
        int len = sizeof(caddr);
        SOCKET clientSock = accept(listenSock, (sockaddr*)&caddr, &len);
        if (clientSock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            std::cerr << "accept() failed, WSA error: " << err << std::endl;
            Sleep(1000);  // prevent CPU burn
            continue;
        }
        int id = clientCounter++;
        {
            std::lock_guard<std::mutex> lock(mtx);
            clients[id] = clientSock;
        }
        std::cout << "Client " << id << " connected.\n";
        std::thread(handleClient, id).detach();
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}
