// server.cpp
#define _WIN32_WINNT 0x0601  
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

const int PORT = 8080;
std::mutex cout_mutex;

int main() {
    // ніціалізуємо Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    // створюємо слухаючий сокет
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // прив’язка до  порту
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);
    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    // починаємо слухати
    listen(listenSock, SOMAXCONN);
    std::cout << "Server listening on port " << PORT << "\n";

    // цикл прийому клієнтів
    while (true) {
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock != INVALID_SOCKET) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "New client connected\n";
            closesocket(clientSock);  // просто закриваємо
        }
    }

    // сleanup, що не досягається
    closesocket(listenSock);
    WSACleanup();
    return 0;
}
