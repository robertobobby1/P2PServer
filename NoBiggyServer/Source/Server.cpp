#include "Server.h"

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)

bool Server::startServer() {
    SERVER_SOCKET = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (checkForErrors(bind(SERVER_SOCKET, (sockaddr *)&serverAddress, sizeof(serverAddress)), -1, "[S]Error on socket binding")) 
        return false;

    if (checkForErrors(listen(SERVER_SOCKET, BACKLOG), -1, "[S]Error while starting to listen on port"))
        return false;
    
    return true;
}

void Server::onError(NoBiggySocket socket, bool closeSocket, const char* errorMessage) {
    printf(errorMessage);
}

#elif defined(PLATFORM_WINDOWS)

bool Server::startServer() {
    WSADATA wsaData;
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &service.sin_addr);

    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        onError(SERVER_SOCKET, false, "");
        return false;
    }

    SERVER_SOCKET = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (checkForErrors(SERVER_SOCKET, INVALID_SOCKET, "[S]Error on socket creation", false))
        return false;

    // If error on socket binding it may mean that the port is in use, we can search a new one! 
    if (checkForErrors(bind(SERVER_SOCKET, (SOCKADDR*)&service, sizeof(service)), SOCKET_ERROR, "[S]Error on socket binding", true))
        return false;

    if (checkForErrors(listen(SERVER_SOCKET, 1), SOCKET_ERROR, "[S]Error while starting to listen on port", true))
        return false;

    unsigned long blocking_mode = 0;
    if (checkForErrors(ioctlsocket(SERVER_SOCKET, FIONBIO, &blocking_mode), -1, "[S]Error while setting the blocking mode", true))
        return false;

    return true;
}

void Server::onError(NoBiggySocket socket, bool closeSocket, const char* errorMessage) {
    if (closeSocket) {
        closesocket(socket);
    }
    printf("%s --- winsock2 error code is: %i\n", errorMessage, WSAGetLastError());
    WSACleanup();
}

#endif

void Server::run()
{
    srand((unsigned)time(NULL));
    SOCKET_QUEUE = std::queue<NoBiggySocket>();

    for (unsigned int i = 0; i < MAX_WORKERS; i++) {
        WORKERS.push_back(std::thread(runWorker));
    }

    startServer();
    printf("Listenting for new connections in port %i", PORT);

    while (keepRunning) {
        NoBiggySocket AcceptSocket = accept(SERVER_SOCKET, NULL, NULL);
        if (AcceptSocket == NoBiggyAcceptSocketError) {
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

void Server::runWorker() {
    int maxBufferLength = 512;
    char buffer[512];
    int result = 0;

    bool openConexion = true;
    NoBiggySocket clientSocket;

    while (true) {
        // Blocking until the thread gets a task (New conexion)
        openConexion = true;
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

void Server::setTSQueue(NoBiggySocket socket) {
    std::lock_guard<std::mutex> lock(queueMutex);
    SOCKET_QUEUE.push(socket);
}

NoBiggySocket Server::getTSQueue() {
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCondition.wait(lock);

    if (SOCKET_QUEUE.empty()) 
        return -1;

    NoBiggySocket res = SOCKET_QUEUE.front();
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

bool Server::checkForErrors(NoBiggySocket socket, int errorMacro, const char* errorMessage, bool closeSocket)
{
    if (socket == errorMacro) {
        onError(socket, closeSocket, errorMessage);
        return true;
    }
    return false;
 
}

std::string Server::generateNewUUID() {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string UUID;
    UUID.reserve(UUID_LENGTH);

    for (int i = 0; i < UUID_LENGTH; ++i) {
        UUID += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    if (LOBBIES.find(UUID) != LOBBIES.end()) {
        return generateNewUUID();
    }

    return UUID;
}