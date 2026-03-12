#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#error "This code is for Windows only (MinGW)"
#endif

const int PORT = 8888;
const int BUFFER_SIZE = 4096;

std::mutex clients_mutex;
std::vector<SOCKET> clients;
std::vector<std::string> client_names;
bool server_running = true;

void broadcastMessage(const std::string& message, SOCKET excludeSock) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    uint32_t len = htonl(message.size());
    for (size_t i = 0; i < clients.size(); ++i) {
        if (clients[i] != excludeSock) {
            send(clients[i], (char*)&len, sizeof(len), 0);
            send(clients[i], message.c_str(), message.size(), 0);
        }
    }
}

void removeClient(SOCKET sock) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = std::find(clients.begin(), clients.end(), sock);
    if (it != clients.end()) {
        int index = it - clients.begin();
        clients.erase(it);
        client_names.erase(client_names.begin() + index);
    }
}

void handleClient(SOCKET clientSock) {
    char nameBuffer[256];
    uint32_t net_len;
    int ret = recv(clientSock, (char*)&net_len, sizeof(net_len), 0);
    if (ret <= 0) { closesocket(clientSock); return; }
    uint32_t name_len = ntohl(net_len);
    if (name_len >= sizeof(nameBuffer)) { closesocket(clientSock); return; }
    ret = recv(clientSock, nameBuffer, name_len, 0);
    if (ret <= 0) { closesocket(clientSock); return; }
    nameBuffer[name_len] = '\0';
    std::string clientName(nameBuffer);

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back(clientSock);
        client_names.push_back(clientName);
    }

    std::string joinMsg = clientName + " joined the chat.";
    std::cout << joinMsg << std::endl;
    broadcastMessage(joinMsg, clientSock);

    char buffer[BUFFER_SIZE];
    while (true) {
        ret = recv(clientSock, (char*)&net_len, sizeof(net_len), 0);
        if (ret <= 0) break;
        uint32_t msg_len = ntohl(net_len);
        if (msg_len >= BUFFER_SIZE) break;
        ret = recv(clientSock, buffer, msg_len, 0);
        if (ret <= 0) break;
        buffer[msg_len] = '\0';
        std::string msg(buffer, msg_len);

        if (msg == "/quit") break;

        std::string fullMsg = clientName + ": " + msg;
        std::cout << fullMsg << std::endl;
        broadcastMessage(fullMsg, clientSock);
    }

    removeClient(clientSock);
    std::string leaveMsg = clientName + " left the chat.";
    std::cout << leaveMsg << std::endl;
    broadcastMessage(leaveMsg, clientSock);
    closesocket(clientSock);
}

void consoleCommandListener() {
    std::string command;
    while (server_running) {
        std::getline(std::cin, command);
        if (command == "/exit") {
            server_running = false;
            break;
        }
    }
}

int main() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    setlocale(LC_ALL, ".UTF-8");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind failed" << std::endl;
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed" << std::endl;
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;
    std::cout << "Type /exit to shutdown server." << std::endl;

    std::thread consoleThread(consoleCommandListener);

    while (server_running) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSock, &readSet);
        struct timeval tv = {1, 0};

        int selectResult = select(0, &readSet, nullptr, nullptr, &tv);
        if (selectResult == SOCKET_ERROR) {
            std::cerr << "select error" << std::endl;
            break;
        }

        if (!server_running) break;

        if (FD_ISSET(listenSock, &readSet)) {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &addrLen);
            if (clientSock == INVALID_SOCKET) {
                std::cerr << "accept failed" << std::endl;
                continue;
            }
            std::thread clientThread(handleClient, clientSock);
            clientThread.detach();
        }
    }

    std::cout << "Shutting down server..." << std::endl;
    closesocket(listenSock);
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (SOCKET sock : clients) closesocket(sock);
        clients.clear();
        client_names.clear();
    }

    WSACleanup();
    if (consoleThread.joinable()) consoleThread.join();
    std::cout << "Server stopped." << std::endl;
    return 0;
}
