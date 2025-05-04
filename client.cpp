// client.cpp
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

const int PORT = 8080;
const char* SERVER_IP = "127.0.0.1";

bool writeAll(SOCKET sock, const void* buf, size_t len) {
    const char* ptr = static_cast<const char*>(buf);
    while (len > 0) {
        int n = send(sock, ptr, (int)len, 0);
        if (n <= 0) return false;
        ptr += n; len -= n;
    }
    return true;
}

bool readAll(SOCKET sock, void* buf, size_t len) {
    char* ptr = static_cast<char*>(buf);
    while (len > 0) {
        int n = recv(sock, ptr, (int)len, 0);
        if (n <= 0) return false;
        ptr += n; len -= n;
    }
    return true;
}

void writeUint32(SOCKET s, uint32_t v) {
    uint32_t net = htonl(v);
    writeAll(s, &net, sizeof(net));
}

uint32_t readUint32(SOCKET s) {
    uint32_t net = 0;
    readAll(s, &net, sizeof(net));
    return ntohl(net);
}

bool sendMessage(SOCKET s, const std::string& msg) {
    writeUint32(s, (uint32_t)msg.size());
    return writeAll(s, msg.data(), msg.size());
}

std::string readMessage(SOCKET s) {
    uint32_t len = readUint32(s);
    if (len == 0) return {};
    std::string buf(len, '\0');
    readAll(s, &buf[0], len);
    return buf;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &srv.sin_addr);

    if (connect(sock, (sockaddr*)&srv, sizeof(srv)) == SOCKET_ERROR) {
        std::cerr << "Connect failed\n";
        WSACleanup();
        return 1;
    }

    // CONFIG
    sendMessage(sock, "CONFIG threads=4");
    std::cout << readMessage(sock) << "\n";

    // DATA
    std::vector<int> data = {1,2,3,4,5,6,7,8};
    std::ostringstream oss;
    oss << "DATA";
    for (int x : data) oss << " " << x;
    sendMessage(sock, oss.str());
    std::cout << readMessage(sock) << "\n";

    // START
    sendMessage(sock, "START");
    std::cout << readMessage(sock) << "\n";

    // STATUS
    sendMessage(sock, "STATUS");
    std::cout << "Status: " << readMessage(sock) << "\n";

    // RESULT
    sendMessage(sock, "RESULT");
    std::cout << "Result: " << readMessage(sock) << "\n";

    // QUIT
    sendMessage(sock, "QUIT");

    closesocket(sock);
    WSACleanup();
    return 0;
}
