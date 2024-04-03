#include "Server.h"

void Server::run()
{
    SOCKET_QUEUE = std::queue<SOCKET>();

    for (unsigned int i = 0; i < MAX_WORKERS; i++) {
        WORKERS.push_back(std::thread(runWorker));
    }

    setup();
    printf("Listenting for new connections in port %i", PORT);

    while (keepRunning) {
        SOCKET AcceptSocket = accept(SERVER_SOCKET, NULL, NULL);
        if (AcceptSocket == INVALID_SOCKET) {
            continue;
        }

        printf("New connection!");

        setTSQueue(AcceptSocket);
        queueCondition.notify_one();
    }

    for (auto& thread: WORKERS) {
        thread.join();
    }
}

void Server::setup() {
    WSADATA wsaData;
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &service.sin_addr);

    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        onError(SERVER_SOCKET, false);
        return;
    }

    SERVER_SOCKET = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (checkForErrors(SERVER_SOCKET, INVALID_SOCKET, false))
        return;

    if (checkForErrors(bind(SERVER_SOCKET, (SOCKADDR*)&service, sizeof(service)), SOCKET_ERROR, true))
        return;

    if (checkForErrors(listen(SERVER_SOCKET, 1), SOCKET_ERROR, true))
        return;

    unsigned long blocking_mode = 0;
    if (checkForErrors(ioctlsocket(SERVER_SOCKET, FIONBIO, &blocking_mode), -1, true))
        return;

}

void Server::runWorker() {
    int maxBufferLength = 512;
    char buffer[512];
    int result = 0;

    bool openConexion = true;
    SOCKET clientSocket;

    while (true) {
        // Blocking until the thread gets a task (New conexion)
        clientSocket = getTSQueue();
        incrementActiveConexions();
        while (openConexion) {
            result = recv(clientSocket, buffer, maxBufferLength, 0);
            if (result > 0) {
                std::cout << buffer << std::endl;
            } else {
                openConexion = false;
                reduceActiveConexions();
            }
        }
    }
}


void Server::onError(SOCKET socket, bool closeSocket) {
    if (closeSocket) {
        closesocket(socket);
    }
    std::cout << WSAGetLastError() << std::endl;
    WSACleanup();
}

bool Server::checkForErrors(SOCKET socket, int errorMacro, bool closeSocket)
{
    if (socket == errorMacro) {
        onError(socket, closeSocket);
        return true;
    }
    return false;
}

void Server::setTSQueue(SOCKET socket) {
    std::lock_guard<std::mutex> lock(queueMutex);
    SOCKET_QUEUE.push(socket);
}

SOCKET Server::getTSQueue() {
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCondition.wait(lock);

    if (SOCKET_QUEUE.empty()) 
        return -1;

    SOCKET res = SOCKET_QUEUE.front();
    SOCKET_QUEUE.pop();

    return res;
}

void Server::reduceActiveConexions() {
    std::lock_guard<std::mutex> lock(activeConnectionsMutex);
    activeConnexions--;
}

void Server::incrementActiveConexions() {
    std::lock_guard<std::mutex> lock(activeConnectionsMutex);
    activeConnexions++;
}