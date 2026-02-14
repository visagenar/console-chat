#include "common.h"

std::vector<UserInfo> clients;
std::mutex clientsMutex;
std::atomic<bool> serverRunning(true);

void broadcastMessage(const std::string& message, SOCKET senderSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    std::string formattedMsg = "[PUBLIC] " + message;
    
    for (const auto& client : clients) {
        if (client.socket != senderSocket && client.isActive) {
            sendMessage(client.socket, MSG_PUBLIC, formattedMsg);
        }
    }
    
    std::cout << formattedMsg << std::endl;
}

void sendPrivateMessage(const std::string& from, const std::string& to, 
                       const std::string& message, SOCKET senderSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    bool found = false;
    std::string formattedMsg = "[PRIVATE from " + from + "] " + message;
    
    for (const auto& client : clients) {
        if (client.username == to && client.isActive) {
            sendMessage(client.socket, MSG_PRIVATE, formattedMsg);
            sendMessage(senderSocket, MSG_SUCCESS, "Message sent to " + to);
            found = true;
            std::cout << from << " -> " << to << ": " << message << std::endl;
            break;
        }
    }
    
    if (!found) {
        sendMessage(senderSocket, MSG_ERROR, "User " + to + " not found or offline");
    }
}

void sendUserList(SOCKET socket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    std::string userList = "Active users:\n";
    
    for (const auto& client : clients) {
        if (client.isActive) {
            userList += "- " + client.username + "\n";
        }
    }
    
    sendMessage(socket, MSG_LIST_USERS, userList);
}

void removeClient(SOCKET socket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = std::find_if(clients.begin(), clients.end(),
        [socket](const UserInfo& client) { return client.socket == socket; });
    
    if (it != clients.end()) {
        std::string logoutMsg = it->username + " has left the chat";
        it->isActive = false;
        closesocket(it->socket);
        clients.erase(it);
        broadcastMessage(logoutMsg, INVALID_SOCKET);
        std::cout << logoutMsg << std::endl;
    }
}

void handleClient(SOCKET clientSocket) {
    char recvbuf[DEFAULT_BUFLEN];
    PacketHeader header;
    std::string username;
    
    // Аутентификация
    int result = receiveData(clientSocket, reinterpret_cast<char*>(&header), sizeof(PacketHeader));
    if (result <= 0 || header.type != MSG_LOGIN) {
        closesocket(clientSocket);
        return;
    }
    
    result = receiveData(clientSocket, recvbuf, header.dataLength);
    if (result <= 0) {
        closesocket(clientSocket);
        return;
    }
    
    recvbuf[header.dataLength] = '\0';
    username = recvbuf;
    
    // Проверка уникальности имени
    bool nameTaken = false;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto& client : clients) {
            if (client.username == username && client.isActive) {
                nameTaken = true;
                break;
            }
        }
    }
    
    if (nameTaken) {
        sendMessage(clientSocket, MSG_ERROR, "Username already taken");
        closesocket(clientSocket);
        return;
    }
    
    // Добавление клиента
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.push_back({clientSocket, username, true});
    }
    
    sendMessage(clientSocket, MSG_SUCCESS, "Welcome to the chat, " + username + "!");
    std::string joinMsg = username + " has joined the chat";
    broadcastMessage(joinMsg, clientSocket);
    std::cout << joinMsg << std::endl;
    
    // Обработка сообщений
    while (serverRunning) {
        result = receiveData(clientSocket, reinterpret_cast<char*>(&header), sizeof(PacketHeader));
        if (result <= 0) {
            break;
        }
        
        if (header.dataLength > 0) {
            result = receiveData(clientSocket, recvbuf, header.dataLength);
            if (result <= 0) {
                break;
            }
            recvbuf[header.dataLength] = '\0';
        }
        
        switch (header.type) {
            case MSG_PUBLIC:
                broadcastMessage(username + ": " + recvbuf, clientSocket);
                break;
                
            case MSG_PRIVATE: {
                std::string msg(recvbuf);
                size_t spacePos = msg.find(' ');
                if (spacePos != std::string::npos) {
                    std::string to = msg.substr(0, spacePos);
                    std::string privateMsg = msg.substr(spacePos + 1);
                    sendPrivateMessage(username, to, privateMsg, clientSocket);
                }
                break;
            }
            
            case MSG_LIST_USERS:
                sendUserList(clientSocket);
                break;
                
            case MSG_LOGOUT:
                removeClient(clientSocket);
                return;
                
            default:
                break;
        }
    }
    
    removeClient(clientSocket);
}

int main() {
    WSADATA wsaData;
    SOCKET listenSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL, hints;
    
    // Инициализация Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }
    
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        std::cout << "socket failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    
    iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::cout << "bind failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    
    freeaddrinfo(result);
    
    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::cout << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    
    std::cout << "Chat server started on port " << DEFAULT_PORT << std::endl;
    std::cout << "Waiting for connections..." << std::endl;
    
    std::vector<std::thread> clientThreads;
    
    while (serverRunning) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            if (serverRunning) {
                std::cout << "accept failed: " << WSAGetLastError() << std::endl;
            }
            continue;
        }
        
        std::cout << "New client connected" << std::endl;
        
        clientThreads.push_back(std::thread(handleClient, clientSocket));
        clientThreads.back().detach();
        
        // Очистка завершенных потоков
        clientThreads.erase(
            std::remove_if(clientThreads.begin(), clientThreads.end(),
                [](std::thread& t) { 
                    if (t.joinable()) {
                        t.join(); 
                        return true;
                    }
                    return false;
                }),
            clientThreads.end()
        );
    }
    
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}