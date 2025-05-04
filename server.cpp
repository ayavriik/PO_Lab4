// server.cpp
#define _WIN32_WINNT 0x0601  
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <sstream>
#include <cstdint>


#pragma comment(lib, "ws2_32.lib")

const int PORT = 8080;
std::mutex cout_mutex;

// читання size байтів у buffer
bool readAll(SOCKET sock, void* buffer, size_t size) {
    char* ptr = static_cast<char*>(buffer);
    size_t remaining = size;
    while (remaining > 0) {
        int n = recv(sock, ptr, (int)remaining, 0);
        if (n <= 0) return false;
        ptr += n;
        remaining -= n;
    }
    return true;
}

// відправка size байтів з buffer
bool writeAll(SOCKET sock, const void* buffer, size_t size) {
    const char* ptr = static_cast<const char*>(buffer);
    size_t remaining = size;
    while (remaining > 0) {
        int n = send(sock, ptr, (int)remaining, 0);
        if (n <= 0) return false;
        ptr += n;
        remaining -= n;
    }
    return true;
}

// читання 32-бітного числа у мережевому порядку
uint32_t readUint32(SOCKET sock) {
    uint32_t net = 0;
    if (!readAll(sock, &net, sizeof(net))) return 0;
    return ntohl(net);
}

// відправляємо 32-бітне число у мережевому порядку
void writeUint32(SOCKET sock, uint32_t value) {
    uint32_t net = htonl(value);
    writeAll(sock, &net, sizeof(net));
}

// читаємо повідомлення з префіксом довжини
std::string readMessage(SOCKET sock) {
    uint32_t len = readUint32(sock);
    if (len == 0) return {};
    std::string buf(len, '\0');
    if (!readAll(sock, &buf[0], len)) return {};
    return buf;
}

// відправляємо повідомлення з префіксом довжини
bool sendMessage(SOCKET sock, const std::string& msg) {
    writeUint32(sock, (uint32_t)msg.size());
    return writeAll(sock, msg.data(), msg.size());
}

void handleClient(SOCKET clientSock) {
    std::vector<int> data;
    int numThreads = 1;
    bool computing = false;
    long result = 0;

    while (true) {
        std::string msg = readMessage(clientSock);
        if (msg.empty()) break;
        std::istringstream iss(msg);
        std::string cmd; iss >> cmd;

        if (cmd == "CONFIG") {
            std::string param;
            while (iss >> param) {
                if (param.rfind("threads=", 0)==0)
                    numThreads = std::stoi(param.substr(8));
            }
            sendMessage(clientSock, "OK");

        } else if (cmd == "DATA") {
            data.clear();
            int x;
            while (iss >> x) data.push_back(x);
            sendMessage(clientSock, "OK");

        } else if (cmd == "START") {
            computing = true;
            result = 0;
            size_t chunk = data.size() / numThreads;
            std::vector<std::thread> workers;
            std::mutex res_mtx;

            for (int i = 0; i < numThreads; ++i) {
                size_t begin = i * chunk;
                size_t end = (i == numThreads - 1 ? data.size() : begin + chunk);
                workers.emplace_back([&](){
                    long partial = 0;
                    for (size_t j = begin; j < end; ++j) partial += data[j];
                    std::lock_guard<std::mutex> lock(res_mtx);
                    result += partial;
                });
            }
            for (auto& t : workers) t.join();
            computing = false;
            sendMessage(clientSock, "DONE");

        } else if (cmd == "STATUS") {
            sendMessage(clientSock, computing ? "BUSY" : "IDLE");

        } else if (cmd == "RESULT") {
            sendMessage(clientSock, computing ? "BUSY" : std::to_string(result));

        } else if (cmd == "QUIT") {
            break;

        } else {
            sendMessage(clientSock, "ERROR Unknown command");
        }
    }

    closesocket(clientSock);
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Client disconnected\n";
    }
}

int main() {
    // ініціалізуємо Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    // створюємо сокет
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // задаємо reuse addr
    BOOL opt = TRUE;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    //  прив’язуємо до порту
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    // починаємо слухати
    listen(listenSock, SOMAXCONN);
    std::cout << "Server listening on port " << PORT << "\n";

    // приймаємо клієнтів
    while (true) {
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "New client connected\n";
        }
        std::thread(handleClient, clientSock).detach();
    }

    // очистка 
    closesocket(listenSock);
    WSACleanup();
    return 0;
}