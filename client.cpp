#include "common.h"
#include <conio.h>

std::atomic<bool> clientRunning(true);
std::string username;

void receiveMessages(SOCKET serverSocket) {
    char recvbuf[DEFAULT_BUFLEN];
    PacketHeader header;
    
    while (clientRunning) {
        int result = receiveData(serverSocket, reinterpret_cast<char*>(&header), sizeof(PacketHeader));
        if (result <= 0) {
            std::cout << "\nConnection to server lost!" << std::endl;
            clientRunning = false;
            break;
        }
        
        if (header.dataLength > 0) {
            result = receiveData(serverSocket, recvbuf, header.dataLength);
            if (result <= 0) {
                std::cout << "\nConnection to server lost!" << std::endl;
                clientRunning = false;
                break;
            }
            recvbuf[header.dataLength] = '\0';
        }
        
        switch (header.type) {
            case MSG_PUBLIC:
            case MSG_PRIVATE:
                std::cout << "\n" << recvbuf << std::endl;
                std::cout << "[" << getCurrentTime() << "] You: ";
                fflush(stdout);
                break;
                
            case MSG_LIST_USERS:
                std::cout << "\n" << recvbuf << std::endl;
                std::cout << "[" << getCurrentTime() << "] You: ";
                fflush(stdout);
                break;
                
            case MSG_ERROR:
                std::cout << "\n[ERROR] " << recvbuf << std::endl;
                std::cout << "[" << getCurrentTime() << "] You: ";
                fflush(stdout);
                break;
                
            case MSG_SUCCESS:
                std::cout << "\n[SUCCESS] " << recvbuf << std::endl;
                std::cout << "[" << getCurrentTime() << "] You: ";
                fflush(stdout);
                break;
                
            default:
                break;
        }
    }
}

void printHelp() {
    std::cout << "\n=== Chat Commands ===" << std::endl;
    std::cout << "/help - Show this help" << std::endl;
    std::cout << "/users - Show active users" << std::endl;
    std::cout << "/msg <username> <text> - Send private message" << std::endl;
    std::cout << "/quit - Exit chat" << std::endl;
    std::cout << "Any other text - Send public message" << std::endl;
    std::cout << "===================\n" << std::endl;
}

int main() {
    WSADATA wsaData;
    SOCKET serverSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL, *ptr = NULL, hints;
    
    // Инициализация Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }
    
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    std::string serverAddress;
    std::cout << "Enter server address (default localhost): ";
    std::getline(std::cin, serverAddress);
    if (serverAddress.empty()) {
        serverAddress = "localhost";
    }
    
    iResult = getaddrinfo(serverAddress.c_str(), DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        serverSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (serverSocket == INVALID_SOCKET) {
            continue;
        }
        
        iResult = connect(serverSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(serverSocket);
            serverSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
    
    freeaddrinfo(result);
    
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "Unable to connect to server!" << std::endl;
        WSACleanup();
        return 1;
    }
    
    // Вход в чат
    std::cout << "Connected to server!" << std::endl;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);
    
    if (!sendMessage(serverSocket, MSG_LOGIN, username)) {
        std::cout << "Failed to send login info" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    
    // Ожидание подтверждения
    PacketHeader header;
    char recvbuf[DEFAULT_BUFLEN];
    
    iResult = receiveData(serverSocket, reinterpret_cast<char*>(&header), sizeof(PacketHeader));
    if (iResult > 0 && header.dataLength > 0) {
        iResult = receiveData(serverSocket, recvbuf, header.dataLength);
        if (iResult > 0) {
            recvbuf[header.dataLength] = '\0';
            
            if (header.type == MSG_ERROR) {
                std::cout << "Login failed: " << recvbuf << std::endl;
                closesocket(serverSocket);
                WSACleanup();
                return 1;
            } else if (header.type == MSG_SUCCESS) {
                std::cout << recvbuf << std::endl;
            }
        }
    }
    
    std::cout << "\nWelcome to the chat, " << username << "!" << std::endl;
    printHelp();
    
    // Запуск потока для получения сообщений
    std::thread receiver(receiveMessages, serverSocket);
    
    // Отправка сообщений
    std::string input;
    std::cout << "[" << getCurrentTime() << "] You: ";
    
    while (clientRunning) {
        std::getline(std::cin, input);
        
        if (!clientRunning) break;
        
        if (input.empty()) {
            std::cout << "[" << getCurrentTime() << "] You: ";
            continue;
        }
        
        if (input == "/quit") {
            sendMessage(serverSocket, MSG_LOGOUT, "");
            clientRunning = false;
            break;
        }
        else if (input == "/help") {
            printHelp();
            std::cout << "[" << getCurrentTime() << "] You: ";
        }
        else if (input == "/users") {
            sendMessage(serverSocket, MSG_LIST_USERS, "");
        }
        else if (input.substr(0, 5) == "/msg ") {
            size_t space1 = input.find(' ', 5);
            if (space1 != std::string::npos) {
                std::string target = input.substr(5, space1 - 5);
                std::string msg = input.substr(space1 + 1);
                sendMessage(serverSocket, MSG_PRIVATE, target + " " + msg);
            } else {
                std::cout << "Usage: /msg <username> <message>" << std::endl;
            }
            std::cout << "[" << getCurrentTime() << "] You: ";
        }
        else {
            sendMessage(serverSocket, MSG_PUBLIC, input);
            std::cout << "[" << getCurrentTime() << "] You: ";
        }
    }
    
    receiver.detach();
    closesocket(serverSocket);
    WSACleanup();
    
    std::cout << "Disconnected from server." << std::endl;
    return 0;
}