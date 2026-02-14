#ifndef COMMON_H
#define COMMON_H

#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <iomanip>

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512
#define MAX_CLIENTS 10

enum MessageType {
    MSG_LOGIN,
    MSG_LOGOUT,
    MSG_PUBLIC,
    MSG_PRIVATE,
    MSG_LIST_USERS,
    MSG_ERROR,
    MSG_SUCCESS,
    MSG_HISTORY
};

struct PacketHeader {
    MessageType type;
    int dataLength;
};

struct UserInfo {
    SOCKET socket;
    std::string username;
    bool isActive;
};

// Функция для получения текущего времени
inline std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

// Безопасная отправка данных
inline bool sendData(SOCKET socket, const char* buffer, int length) {
    int total = 0;
    int bytesLeft = length;
    int n;
    
    while (total < length) {
        n = send(socket, buffer + total, bytesLeft, 0);
        if (n == SOCKET_ERROR) {
            return false;
        }
        total += n;
        bytesLeft -= n;
    }
    return true;
}

// Отправка сообщения с заголовком
inline bool sendMessage(SOCKET socket, MessageType type, const std::string& message) {
    PacketHeader header;
    header.type = type;
    header.dataLength = message.length();
    
    if (!sendData(socket, reinterpret_cast<char*>(&header), sizeof(PacketHeader))) {
        return false;
    }
    
    if (header.dataLength > 0) {
        if (!sendData(socket, message.c_str(), message.length())) {
            return false;
        }
    }
    return true;
}

// Получение данных
inline int receiveData(SOCKET socket, char* buffer, int length) {
    int total = 0;
    int bytesLeft = length;
    int n;
    
    while (total < length) {
        n = recv(socket, buffer + total, bytesLeft, 0);
        if (n <= 0) {
            return n;
        }
        total += n;
        bytesLeft -= n;
    }
    return total;
}

#endif