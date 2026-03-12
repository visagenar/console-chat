#include <iostream>
#include <string>
#include <thread>
#include <atomic>
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

std::atomic<bool> client_running(true);
SOCKET sock;

void receiveMessages() {
    char buffer[BUFFER_SIZE];
    while (client_running) {
        uint32_t net_len;
        int ret = recv(sock, (char*)&net_len, sizeof(net_len), 0);
        if (ret <= 0) {
            if (client_running) std::cout << "Disconnected from server." << std::endl;
            client_running = false;
            break;
        }
        uint32_t msg_len = ntohl(net_len);
        if (msg_len >= BUFFER_SIZE) continue;
        ret = recv(sock, buffer, msg_len, 0);
        if (ret <= 0) {
            if (client_running) std::cout << "Disconnected from server." << std::endl;
            client_running = false;
            break;
        }
        buffer[msg_len] = '\0';
        std::string msg(buffer, msg_len);
        std::cout << msg << std::endl;
    }
}

void sendMessages(const std::string& name) {
    uint32_t name_len = htonl(name.size());
    send(sock, (char*)&name_len, sizeof(name_len), 0);
    send(sock, name.c_str(), name.size(), 0);

    std::string input;
    while (client_running) {
        std::getline(std::cin, input);
        if (!client_running) break;

        if (input == "/quit") {
            uint32_t msg_len = htonl(input.size());
            send(sock, (char*)&msg_len, sizeof(msg_len), 0);
            send(sock, input.c_str(), input.size(), 0);
            client_running = false;
            break;
        }

        uint32_t msg_len = htonl(input.size());
        send(sock, (char*)&msg_len, sizeof(msg_len), 0);
        send(sock, input.c_str(), input.size(), 0);
    }
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    setlocale(LC_ALL, ".UTF-8");

    if (argc < 2) {
        std::cout << "Usage: client.exe <server_ip> [port]" << std::endl;
        return 1;
    }

    std::string serverIP = argv[1];
    int port = (argc >= 3) ? std::atoi(argv[2]) : PORT;

    std::cout << "Enter your name: ";
    std::string name;
    std::getline(std::cin, name);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "connection failed" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server. Type /quit to exit." << std::endl;

    std::thread recvThread(receiveMessages);
    std::thread sendThread(sendMessages, name);

    recvThread.join();
    sendThread.join();

    closesocket(sock);
    WSACleanup();
    std::cout << "Client stopped." << std::endl;
    return 0;
}
