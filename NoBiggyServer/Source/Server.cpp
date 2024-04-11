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

void Server::onError(NoBiggySocket socket, bool closeSocket, const char *errorMessage) {
    printf("%s\n", errorMessage);
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
    if (checkForErrors(bind(SERVER_SOCKET, (SOCKADDR *)&service, sizeof(service)), SOCKET_ERROR, "[S]Error on socket binding", true))
        return false;

    if (checkForErrors(listen(SERVER_SOCKET, 1), SOCKET_ERROR, "[S]Error while starting to listen on port", true))
        return false;

    unsigned long blocking_mode = 0;
    if (checkForErrors(ioctlsocket(SERVER_SOCKET, FIONBIO, &blocking_mode), -1, "[S]Error while setting the blocking mode", true))
        return false;

    return true;
}

void Server::onError(NoBiggySocket socket, bool closeSocket, const char *errorMessage) {
    if (closeSocket) {
        closesocket(socket);
    }
    printf("%s --- winsock2 error code is: %i\n", errorMessage, WSAGetLastError());
    WSACleanup();
}

#endif

// get RTT from client socket
#if defined(PLATFORM_MACOS)

uint32_t Server::getRTTOfClient(NoBiggySocket clientSocket) {
    struct tcp_connection_info info;
    socklen_t info_len = sizeof(info);

    int result = getsockopt(clientSocket, IPPROTO_TCP, TCP_CONNECTION_INFO, &info, &info_len);

    return info.tcpi_srtt;
}

#elif defined(PLATFORM_LINUX)

uint32_t Server::getRTTOfClient(NoBiggySocket clientSocket) {
    struct tcp_info info;
    socklen_t info_len = sizeof(info);

    int result = getsockopt(clientSocket, IPPROTO_TCP, TCP_INFO, &info, &info_len);

    return info.tcpi_srtt;
}

#elif defined(PLATFORM_WINDOWS)

uint32_t Server::getRTTOfClient(NoBiggySocket clientSocket) {
    // TODO how to get RTT in windows
}

#endif

void Server::run() {
    srand((unsigned)time(NULL));
    socketQueue = std::queue<NoBiggySocket>();
    matchMakingQueue = std::queue<std::string>();

    for (unsigned int i = 0; i < MAX_WORKERS; i++) {
        WORKERS.push_back(std::thread(runWorker));
    }

    if (!startServer())
        return;

    printf("Listenting for new connections in port %i\n", PORT);

    while (keepRunning) {
        NoBiggySocket AcceptSocket = accept(SERVER_SOCKET, NULL, NULL);
        if (AcceptSocket == NoBiggyAcceptSocketError) {
            continue;
        }

        printf("New connection!\n");
        setTSToSocketQueue(AcceptSocket);
        socketQueueCondition.notify_one();
    }

    for (auto &thread : WORKERS) {
        thread.join();
    }
}

void Server::runWorker() {
    int maxBufferLength = 512;
    char buffer[512];
    int bytesReceived = 0;

    bool openConexion = true;
    NoBiggySocket clientSocket;

    while (true) {
        // Blocking until the thread gets a task (New conexion)
        clientSocket = getTSFromSocketQueue();

        bytesReceived = recv(clientSocket, buffer, maxBufferLength, 0);
        if (bytesReceived > 0) {
            handleNewRequest(buffer, bytesReceived, clientSocket);
        } else {
            printf("Connexion error!");
        }
        close(clientSocket);
    }
}

Peer Server::getPeerInfo(NoBiggySocket clientSocket) {
    struct sockaddr_in peeraddr;
    socklen_t peeraddrlen;
    auto retval = getpeername(clientSocket, (struct sockaddr *)&peeraddr, &peeraddrlen);

    Peer clientInfo;
    clientInfo.family = peeraddr.sin_family;
    clientInfo.ipAddress = inet_ntoa(peeraddr.sin_addr);
    clientInfo.port = ntohs(peeraddr.sin_port);
    clientInfo.socket = clientSocket;
    clientInfo.averageRTT = getRTTOfClient(clientSocket);

    return clientInfo;
}

void Server::handleNewRequest(const char *buffer, int bytesReceived, NoBiggySocket clientSocket) {
    // TODO security, make sure it is my protocol
    // TODO read buffer and bytes searching for specific game
    auto uuid = findRandomMatch(clientSocket);
}

std::string Server::startNewLobby(NoBiggySocket clientSocket) {
    // start a new lobby and wait for another peer to connect
    Lobby lobby;
    lobby.ID_Lobby = generateNewUUID();
    lobby.peer1 = getPeerInfo(clientSocket);

    setTSToMatchMakingQueue(lobby.ID_Lobby);
    lobbiesMap[lobby.ID_Lobby] = lobby;
    return lobby.ID_Lobby;
};

std::string Server::findRandomMatch(NoBiggySocket clientSocket) {
    std::string uuid = getTSFromMatchMakingQueue();
    if (uuid.empty()) {
        return startNewLobby(clientSocket);
    }

    lobbiesMap[uuid].peer2 = getPeerInfo(clientSocket);
    lobbiesMap[uuid].print();
    return uuid;
}

void Server::setTSToMatchMakingQueue(std::string uuid) {
    std::lock_guard<std::mutex> lock(matchMakingQueueMutex);
    matchMakingQueue.push(uuid);
}

std::string Server::getTSFromMatchMakingQueue() {
    std::unique_lock<std::mutex> lock(matchMakingQueueMutex);

    if (matchMakingQueue.empty())
        return "";

    auto uuid = matchMakingQueue.front();
    matchMakingQueue.pop();

    return uuid;
}

void Server::setTSToSocketQueue(NoBiggySocket socket) {
    std::lock_guard<std::mutex> lock(socketQueueMutex);
    socketQueue.push(socket);
}

NoBiggySocket Server::getTSFromSocketQueue() {
    std::unique_lock<std::mutex> lock(socketQueueMutex);
    socketQueueCondition.wait(lock);

    if (socketQueue.empty())
        return -1;

    auto socket = socketQueue.front();
    socketQueue.pop();

    return socket;
}

bool Server::checkForErrors(NoBiggySocket socket, int errorMacro, const char *errorMessage, bool closeSocket) {
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

    if (lobbiesMap.find(UUID) != lobbiesMap.end()) {
        return generateNewUUID();
    }

    return UUID;
}