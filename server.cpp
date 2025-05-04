// server.cpp
#define _WIN32_WINNT 0x0601  
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

const int PORT = 8080;
std::mutex cout_mutex;

bool readAll(SOCKET sock, void* buffer, size_t size) {
    char* ptr = static_cast<char*>(buffer);
    size_t rem = size;
    while (rem > 0) {
        int n = recv(sock, ptr, (int)rem, 0);
        if (n <= 0) return false;
        ptr += n;
        rem -= n;
    }
    return true;
}

bool writeAll(SOCKET sock, const void* buffer, size_t size) {
    const char* ptr = static_cast<const char*>(buffer);
    size_t rem = size;
    while (rem > 0) {
        int n = send(sock, ptr, (int)rem, 0);
        if (n <= 0) return false;
        ptr += n;
        rem -= n;
    }
    return true;
}

uint32_t readUint32(SOCKET sock) {
    uint32_t net = 0;
    if (!readAll(sock, &net, sizeof(net))) return 0;
    return ntohl(net);
}

void writeUint32(SOCKET sock, uint32_t val) {
    uint32_t net = htonl(val);
    writeAll(sock, &net, sizeof(net));
}

std::string readMessage(SOCKET sock) {
    uint32_t len = readUint32(sock);
    if (len == 0) return {};
    std::string buf(len, '\0');
    if (!readAll(sock, &buf[0], len)) return {};
    return buf;
}

bool sendMessage(SOCKET sock, const std::string& msg) {
    writeUint32(sock, (uint32_t)msg.size());
    return writeAll(sock, msg.data(), msg.size());
}

void handleClient(SOCKET clientSock) {
    closesocket(clientSock);
}

int main() {
    // ніціалізуємо Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    // створюємо і прив'язуємо сокет
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);
    bind(listenSock, (sockaddr*)&addr, sizeof(addr));

    // слухаємо
    listen(listenSock, SOMAXCONN);
    std::cout << "Server listening on port " << PORT << "\n";

    // приймаємо клієнтів у окремому потоці
    while (true) {
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "New client connected\n";
        }
        // відправляємо clientSock у новий потік
        std::thread(handleClient, clientSock).detach();
    }

    // сleanup
    closesocket(listenSock);
    WSACleanup();
    return 0;
}
