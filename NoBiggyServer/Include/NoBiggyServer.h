

// begin --- NetworkUtils.cpp --- 



// begin --- NetworkUtils.h --- 

#pragma once

// begin --- NetworkStructs.h --- 

#pragma once

// begin --- Platform.h --- 

#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#    pragma message("WIN32 || _WIN32 || __WIN32__ || __NT__")
#    ifndef PLATFORM_WINDOWS
#        define PLATFORM_WINDOWS
#    endif
#    ifdef _WIN64
#    endif

#elif __APPLE__
#    include <TargetConditionals.h>
#    if TARGET_IPHONE_SIMULATOR
#    elif TARGET_OS_MACCATALYST
#    elif TARGET_OS_IPHONE
#    elif TARGET_OS_MAC
#        ifndef PLATFORM_MACOS
#            define PLATFORM_MACOS
#        endif
#    else
#        error "Unknown Apple platform"
#    endif

#elif __ANDROID__
#    ifndef PLATFORM_LINUX
#        define PLATFORM_LINUX
#    endif
#elif __linux__
#    ifndef PLATFORM_LINUX
#        define PLATFORM_LINUX
#    endif
#elif __unix__
#    ifndef PLATFORM_LINUX
#        define PLATFORM_LINUX
#    endif
#elif defined(_POSIX_VERSION)
#    ifndef PLATFORM_LINUX
#        define PLATFORM_LINUX
#    endif
#else
#    error("Unknown compiler")
#endif

#ifdef PLATFORM_LINUX
#    pragma message("This is linux")
#elif defined(PLATFORM_MACOS)
#    pragma message("This is MacOS")
#elif defined(PLATFORM_WINDOWS)
#    pragma message("This is Windows")
#else
#    pragma message("This is an unknown OS")
#endif

// end --- Platform.h --- 


#include <cstdint>
#include <iostream>
#include <shared_mutex>

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
typedef int NoBiggySocket;
#    define NoBiggyAcceptSocketError -1
#elif defined(PLATFORM_WINDOWS)
typedef SOCKET NoBiggySocket;
#    define NoBiggyAcceptSocketError INVALID_SOCKET
#endif

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <netinet/tcp.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#    include <fcntl.h>
#elif defined(PLATFORM_WINDOWS)
#    include <WinSock2.h>
#    include <ws2tcpip.h>
#    pragma comment(lib, "winmm.lib")
#    pragma comment(lib, "WS2_32.lib")
#    include <Windows.h>
#endif

// Client-Server data flags
enum ClientServerHeaderFlags {
    // Type of the lobby public/private
    ClientServerHeaderFlags_Public = 1 << 5,  // 00100000
    // Type of the action we are trying create/connect/disconnect/peersConnectSuccess
    ClientServerHeaderFlags_Bit1 = 1 << 7,  // 10000000
    ClientServerHeaderFlags_Bit2 = 1 << 6,  // 01000000
};

// Server-Client data flags
enum ServerClientHeaderFlags {
    // Action of the request
    ServerClientHeaderFlags_Action = 1 << 7,  // 10000000
};

enum LobbyPrivacyType {
    Private,
    Public
};

enum ActionType {
    Create,
    Connect,
    Disconnect,
    PeerConnectSuccess
};

struct Peer {
    NoBiggySocket socket;
    // always 4 bytes B1.B2.B3.B4 it is already in network order!
    in_addr ipAddress;
    uint16_t port;
    uint8_t family;
    uint32_t averageRTT;

    void print() {
        printf("Peer information:\n");
        printf("Peer Address Family: %d\n", this->family);
        printf("Peer Port: %d\n", this->port);
        printf("Peer IP Address: %s\n", inet_ntoa(this->ipAddress));
        printf("Average RTT: %i\n", this->averageRTT);
    }
};

struct Lobby {
    // private to protect with mutex
   private:
    Peer peer2;
    bool isLobbyComplete = false;

   public:
    std::string ID_Lobby;
    Peer peer1;
    LobbyPrivacyType lobbyPrivacyType;
    bool peerConnectionSendFailure = false;

    bool IsLobbyComplete(std::mutex* mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        return isLobbyComplete;
    };
    bool SetPeer2IfPossible(Peer peer, std::mutex* mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        if (!isLobbyComplete) {
            peer2 = peer;
            isLobbyComplete = true;
            return true;
        }

        return false;
    }
    Peer GetPeer2(std::mutex* mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        return peer2;
    };

    void Print() {
        printf("\nStart lobby info ---- %s\n\n", this->ID_Lobby.c_str());
        printf("Peer 1:\n");
        this->peer1.print();
        printf("\nPeer 2:\n");
        this->peer2.print();
        printf("\nEnd lobby info   ---- %s\n\n", this->ID_Lobby.c_str());
    }
};

// end --- NetworkStructs.h --- 



namespace NetworkUtils {
    Peer getPeerInfo(NoBiggySocket clientSocket);
    uint32_t getRTTOfClient(NoBiggySocket clientSocket);

}  // namespace NetworkUtils

// end --- NetworkUtils.h --- 



#if defined(PLATFORM_MACOS)

uint32_t NetworkUtils::getRTTOfClient(NoBiggySocket clientSocket) {
    struct tcp_connection_info info;
    socklen_t info_len = sizeof(info);

    int result = getsockopt(clientSocket, IPPROTO_TCP, TCP_CONNECTION_INFO, &info, &info_len);

    return info.tcpi_srtt;
}

#elif defined(PLATFORM_LINUX)

uint32_t NetworkUtils::getRTTOfClient(NoBiggySocket clientSocket) {
    struct tcp_info info;
    socklen_t info_len = sizeof(info);

    int result = getsockopt(clientSocket, IPPROTO_TCP, TCP_INFO, &info, &info_len);

    return info.tcpi_srtt;
}

#elif defined(PLATFORM_WINDOWS)

uint32_t NetworkUtils::getRTTOfClient(NoBiggySocket clientSocket) {
    // TODO how to get RTT in windows
}

#endif

Peer NetworkUtils::getPeerInfo(NoBiggySocket clientSocket) {
    struct sockaddr_in peeraddr;
    socklen_t peeraddrlen;
    auto retval = getpeername(clientSocket, (struct sockaddr*)&peeraddr, &peeraddrlen);

    Peer clientInfo;
    clientInfo.family = peeraddr.sin_family;
    clientInfo.ipAddress = peeraddr.sin_addr;
    clientInfo.port = ntohs(peeraddr.sin_port);
    clientInfo.socket = clientSocket;
    clientInfo.averageRTT = getRTTOfClient(clientSocket);

    return clientInfo;
}


// end --- NetworkUtils.cpp --- 



// begin --- Server.cpp --- 



// begin --- Server.h --- 

#pragma once

#include <vector>
#include <iostream>
#include <cstdint>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

// begin --- Common.h --- 

#pragma once

#include <cstdint>
#include <queue>

namespace Common {
    inline bool isInRange(int value, int lowRange, int highRange) {
        return value <= highRange && value >= lowRange;
    }

    inline bool isFlagSet(uint8_t flagObject, uint8_t flag) {
        return 0 != (flagObject & flag);
    }

    inline void setFlag(uint8_t &flagObject, uint8_t flag) {
        flagObject |= flag;
    }

    inline void unsetFlag(uint8_t &flagObject, uint8_t flag) {
        flagObject &= ~flag;
    }

    template <typename T>
    inline T getFromQueue(std::queue<T> &queue) {
        if (queue.empty()) {
            if constexpr (std::is_same_v<T, std::string>) {
                return "";
            } else if constexpr (std::is_same_v<T, int>) {
                return -1;
            } else {
                return nullptr;
            }
        }

        auto value = queue.front();
        queue.pop();
        return value;
    }

    template <typename T>
    inline T getThreadSafeFromQueue(std::queue<T> &queue, std::mutex &queueMutex) {
        std::lock_guard<std::mutex> lock(queueMutex);
        return getFromQueue(queue);
    }

    template <typename T>
    inline T getThreadSafeFromQueue(std::queue<T> &queue, std::mutex &queueMutex, std::condition_variable &condition) {
        std::unique_lock<std::mutex> lock(queueMutex);
        condition.wait(lock);
        return getFromQueue(queue);
    }

    template <typename T>
    inline void setThreadSafeToQueue(std::queue<T> &queue, std::mutex &queueMutex, T value) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push(value);
    }

    inline std::string generateUUID(int length) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        std::string uuid;
        uuid.reserve(length);

        for (int i = 0; i < length; ++i) {
            uuid += alphanum[rand() % (sizeof(alphanum) - 1)];
        }
        return uuid;
    };

    inline int randomNumber(int min, int max) {
        return rand() % (max - min + 1) + min;
    }

    inline unsigned int randomUintNumber(int min, int max) {
        return (unsigned int)(rand() % (max - min + 1) + min);
    }
}  // namespace Common

// end --- Common.h --- 



namespace Server {
    inline const int UUID_LENGTH = 6;
    inline const int MAX_workers = 10;
    inline const int PORT = 3000;
    inline const int BACKLOG = 10;
    inline const long TIMEOUT = 1;  // in seconds
    inline const int SECURITY_HEADER_LENGTH = 19;
    inline const char *SECURITY_HEADER = "";

    void run();
    void runWorker();
    inline std::vector<std::thread> workers;

    bool startServer();
    bool setServerNonBlockingMode();
    void onError(NoBiggySocket socket, bool closeSocket, const char *errorMessage);
    bool checkForErrors(NoBiggySocket socket, int errorMacro, const char *errorMessage, bool closeSocket = false);
    void addFDToSet(NoBiggySocket socket);
    void removeFDFromSet(NoBiggySocket socket);
    inline std::queue<NoBiggySocket> socketsToCloseQueue;
    inline std::mutex socketsToCloseQueueMutex;
    inline std::vector<NoBiggySocket> activeSockets;
    inline fd_set readSocketsFDSet;
    inline NoBiggySocket serverSocket;
    inline bool keepRunning = true;

    inline std::queue<NoBiggySocket> socketQueue;
    inline std::mutex socketQueueMutex;
    inline std::condition_variable socketQueueCondition;

    inline std::mutex matchMakingQueueMutex;
    inline std::queue<std::string> matchMakingQueue;

    void handleRequest(const char *buffer, int bytesReceived, NoBiggySocket clientSocket);
    bool isValidSecurityHeader(const char *buffer);
    ActionType getActionTypeFromHeaderByte(uint8_t headerByte);
    LobbyPrivacyType getLobbyPrivacyTypeFromHeaderByte(uint8_t headerByte);
    void sendUuidToClient(NoBiggySocket clientSocket, std::string &uuid);
    void connectPeersIfNecessary(std::string &uuid);

    std::string findRandomMatch(NoBiggySocket clientSocket);
    std::string startNewLobby(NoBiggySocket clientSocket, LobbyPrivacyType ClientServerHeaderFlags);
    std::string generateNewUUID();
    inline std::unordered_map<std::string, Lobby> lobbiesMap;
    inline std::unordered_map<std::string, std::unique_ptr<std::mutex>> lobbiesMutexMap;
}  // namespace Server

// end --- Server.h --- 



#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)

bool Server::startServer() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (checkForErrors(bind(serverSocket, (sockaddr *)&serverAddress, sizeof(serverAddress)), -1, "[S]Error on socket binding", true)) {
        return false;
    }

    if (checkForErrors(listen(serverSocket, BACKLOG), -1, "[S]Error while starting to listen on port", true)) {
        return false;
    }

    setServerNonBlockingMode();
    return true;
}

void Server::onError(NoBiggySocket socket, bool closeSocket, const char *errorMessage) {
    printf("%s - errno %i\n", errorMessage, errno);
    if (closeSocket) {
        close(socket);
    }
}

bool Server::setServerNonBlockingMode() {
    int flags = fcntl(serverSocket, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    return (fcntl(serverSocket, F_SETFL, flags) == 0);
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
        onError(serverSocket, false, "");
        return false;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (checkForErrors(serverSocket, INVALID_SOCKET, "[S]Error on socket creation", false))
        return false;

    // If error on socket binding it may mean that the port is in use, we can search a new one!
    if (checkForErrors(bind(serverSocket, (SOCKADDR *)&service, sizeof(service)), SOCKET_ERROR, "[S]Error on socket binding", true))
        return false;

    if (checkForErrors(listen(serverSocket, 1), SOCKET_ERROR, "[S]Error while starting to listen on port", true))
        return false;

    unsigned long blocking_mode = 0;
    if (checkForErrors(ioctlsocket(serverSocket, FIONBIO, &blocking_mode), -1, "[S]Error while setting the blocking mode", true))
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

void Server::setServerNonBlockingMode() {
    unsigned long mode = 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0);
}

#endif

void Server::run() {
    srand((unsigned)time(NULL));
    struct timeval timeout = {TIMEOUT, 0};
    FD_ZERO(&readSocketsFDSet);
    socketQueue = std::queue<NoBiggySocket>();
    matchMakingQueue = std::queue<std::string>();
    socketsToCloseQueue = std::queue<NoBiggySocket>();

    if (!startServer())
        return;

    for (unsigned int i = 0; i < MAX_workers; i++) {
        workers.push_back(std::thread(runWorker));
    }

    printf("Listenting for new connections in port %i\n", PORT);
    addFDToSet(serverSocket);

    while (keepRunning) {
        NoBiggySocket socketToClose = Common::getThreadSafeFromQueue(socketsToCloseQueue, socketsToCloseQueueMutex);
        while (socketToClose != -1) {
            removeFDFromSet(socketToClose);
            socketToClose = Common::getThreadSafeFromQueue(socketsToCloseQueue, socketsToCloseQueueMutex);
        }

        fd_set temporarySet = readSocketsFDSet;
        if (select(FD_SETSIZE, &temporarySet, NULL, NULL, NULL) < 0) {
            onError(serverSocket, false, "[S] Error during select");
            keepRunning = false;
            break;
        }

        for (auto &activeSocket : activeSockets) {
            if (!FD_ISSET(activeSocket, &readSocketsFDSet)) {
                continue;
            }

            NoBiggySocket socketToAttend;
            if (activeSocket == serverSocket) {
                NoBiggySocket acceptSocket = accept(serverSocket, NULL, NULL);
                if (acceptSocket == NoBiggyAcceptSocketError) {
                    // socket is non blocking, just continue, this is already handled
                    if (errno == 35) {
                        break;
                    }
                    onError(serverSocket, true, "[S] Error during accept");
                    keepRunning = false;
                    break;
                }

                printf("New connection!\n");
                addFDToSet(acceptSocket);
                socketToAttend = acceptSocket;
            } else {
                socketToAttend = activeSocket;
            }

            Common::setThreadSafeToQueue(socketQueue, socketQueueMutex, socketToAttend);
            socketQueueCondition.notify_one();
        }
    }

    std::terminate();
}

void Server::removeFDFromSet(NoBiggySocket socket) {
    FD_CLR(socket, &readSocketsFDSet);
    activeSockets.erase(std::remove(activeSockets.begin(), activeSockets.end(), socket), activeSockets.end());
    close(socket);
}

void Server::addFDToSet(NoBiggySocket socket) {
    FD_SET(socket, &readSocketsFDSet);
    activeSockets.push_back(socket);
}

void Server::runWorker() {
    int maxBufferLength = 512;
    char buffer[512];
    int bytesReceived = 0;

    bool openConexion = true;
    NoBiggySocket clientSocket;

    while (true) {
        clientSocket = Common::getThreadSafeFromQueue(socketQueue, socketQueueMutex, socketQueueCondition);

        bytesReceived = recv(clientSocket, buffer, maxBufferLength, 0);
        if (bytesReceived > 0) {
            handleRequest(buffer, bytesReceived, clientSocket);
        } else {
            printf("recv failed\n");
            Common::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
        }
    }
}

/*
 * 24-29 BYTES should be received including:
 *  - 23 Bytes of security header 0-22
 *  - 1 Byte for header protocol data, includes ClientServerHeaderFlags and if its new or to connect lobby 23
 *  - 5 Optional bytes that are the game hash 24-29
 */
void Server::handleRequest(const char *buffer, int bytesReceived, NoBiggySocket clientSocket) {
    if (!Common::isInRange(bytesReceived, 24, 29) || !isValidSecurityHeader(buffer)) {
        printf("Bad protocol\n");
        Common::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
        return;
    }

    std::string uuid = "";
    uint8_t dataHeaderFlags = buffer[23];
    LobbyPrivacyType lobbyPrivacyType = getLobbyPrivacyTypeFromHeaderByte(dataHeaderFlags);
    ActionType action = getActionTypeFromHeaderByte(dataHeaderFlags);

    if (action == ActionType::Disconnect || action == ActionType::PeerConnectSuccess) {
        Common::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
        return;
    }

    // public lobby - connect/create
    if (lobbyPrivacyType == LobbyPrivacyType::Public) {
        uuid = findRandomMatch(clientSocket);
    } else if (action == ActionType::Create) {
        // private lobby - create action
        uuid = startNewLobby(clientSocket, LobbyPrivacyType::Private);
    } else if (action == ActionType::Connect) {
        // private lobby - connect action
        uuid = std::string(buffer[24], (size_t)5);
        auto lobby = lobbiesMap.find(uuid);
        if (lobby == lobbiesMap.end() || lobby->second.IsLobbyComplete(lobbiesMutexMap[uuid].get())) {
            Common::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
            return;
        }
    }

    connectPeersIfNecessary(uuid);
}

/*
 * 24-29 BYTES should be received including:
 *  - 23 Bytes of security header 0-22
 *  - 1 Byte for header protocol data, includes ServerClientHeaderFlags and if its new or to connect lobby 23
 *  - 4 bytes for ip address of other peer
 *  - 2 bytes for port of other peer
 *  - 4 bytes for delay to sync with other peer (in ms)
 */
void Server::connectPeersIfNecessary(std::string &uuid) {
    Lobby lobby = lobbiesMap[uuid];
    if (!lobby.IsLobbyComplete(lobbiesMutexMap[uuid].get())) {
        sendUuidToClient(lobbiesMap[uuid].peer1.socket, uuid);
        return;
    }

    int bufferLength = SECURITY_HEADER_LENGTH + 1 + 4 + 2 + 4;
    std::unique_ptr<char[]> bufferForPeer1(new char[bufferLength]);
    std::unique_ptr<char[]> bufferForPeer2(new char[bufferLength]);

    uint8_t headerFlags = 0;
    Common::setFlag(headerFlags, ServerClientHeaderFlags::ServerClientHeaderFlags_Action);

    memcpy(bufferForPeer1.get(), SECURITY_HEADER, SECURITY_HEADER_LENGTH);
    memcpy(bufferForPeer1.get() + SECURITY_HEADER_LENGTH, &headerFlags, 1);
    // both same header
    memcpy(bufferForPeer2.get(), bufferForPeer1.get(), SECURITY_HEADER_LENGTH + 1);

    Peer peer2 = lobby.GetPeer2(lobbiesMutexMap[uuid].get());
    uint16_t delayPeer2, delayPeer1 = 0;
    if (peer2.averageRTT > lobby.peer1.averageRTT) {
        delayPeer2 = peer2.averageRTT - lobby.peer1.averageRTT;
    } else {
        delayPeer1 = lobby.peer1.averageRTT - peer2.averageRTT;
    }

    // peer1
    // TODO htonl?
    unsigned int ipAddressPeer1 = lobby.peer1.ipAddress.s_addr;
    uint16_t portPeer1 = lobby.peer1.port;

    memcpy(bufferForPeer1.get() + SECURITY_HEADER_LENGTH + 1, &ipAddressPeer1, 4);
    memcpy(bufferForPeer1.get() + SECURITY_HEADER_LENGTH + 1 + 4, &portPeer1, 2);
    memcpy(bufferForPeer1.get() + SECURITY_HEADER_LENGTH + 1 + 4 + 2, &delayPeer1, 4);

    // peer2
    unsigned int ipAddressPeer2 = peer2.ipAddress.s_addr;
    uint16_t portPeer2 = peer2.port;
    memcpy(bufferForPeer2.get() + SECURITY_HEADER_LENGTH + 1, &ipAddressPeer2, 4);
    memcpy(bufferForPeer2.get() + SECURITY_HEADER_LENGTH + 1 + 4, &portPeer2, 2);
    memcpy(bufferForPeer2.get() + SECURITY_HEADER_LENGTH + 1 + 4 + 2, &delayPeer2, 4);

    auto peer1Response = send(lobby.peer1.socket, bufferForPeer1.get(), bufferLength, 0);
    auto peer2Response = send(peer2.socket, bufferForPeer2.get(), bufferLength, 0);

    if (peer1Response == -1 || peer2Response == -1) {
        lobby.peerConnectionSendFailure = true;
    }
}

void Server::sendUuidToClient(NoBiggySocket clientSocket, std::string &uuid) {
    int bufferLength = SECURITY_HEADER_LENGTH + 1 + uuid.size();
    std::unique_ptr<char[]> buffer(new char[bufferLength]);
    // action 0 is send uuid
    uint8_t headerFlags = 0;

    // TODO htonl for uuid?
    memcpy(buffer.get(), SECURITY_HEADER, SECURITY_HEADER_LENGTH);
    memcpy(buffer.get() + SECURITY_HEADER_LENGTH, &headerFlags, 1);
    memcpy(buffer.get() + SECURITY_HEADER_LENGTH + 1, uuid.c_str(), uuid.size());

    auto sendResponse = send(clientSocket, buffer.get(), bufferLength, 0);
    if (sendResponse == -1) {
        Common::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
    }
}

LobbyPrivacyType Server::getLobbyPrivacyTypeFromHeaderByte(uint8_t headerByte) {
    if (Common::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Public)) {
        return LobbyPrivacyType::Public;
    }
    return LobbyPrivacyType::Private;
}

ActionType Server::getActionTypeFromHeaderByte(uint8_t headerByte) {
    // 11 = connect, 10 = createLobby, 01 = disconnect, 00 = peersConnectSuccess
    bool isBit1Set = Common::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Bit1);
    bool isBit2Set = Common::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Bit2);

    if (isBit1Set && isBit2Set) {
        return ActionType::Connect;
    } else if (isBit1Set && !isBit2Set) {
        return ActionType::Create;
    } else if (!isBit1Set && isBit2Set) {
        return ActionType::Disconnect;
    } else if (!isBit1Set && !isBit2Set) {
        return ActionType::PeerConnectSuccess;
    }
};

bool Server::isValidSecurityHeader(const char *buffer) {
    return strncmp(buffer, SECURITY_HEADER, SECURITY_HEADER_LENGTH) == 0;
}

std::string Server::startNewLobby(NoBiggySocket clientSocket, LobbyPrivacyType lobbyPrivacyType) {
    Lobby lobby;
    lobby.ID_Lobby = generateNewUUID();
    lobby.peer1 = NetworkUtils::getPeerInfo(clientSocket);
    lobby.lobbyPrivacyType = lobbyPrivacyType;

    if (lobbyPrivacyType == LobbyPrivacyType::Public) {
        Common::setThreadSafeToQueue(matchMakingQueue, matchMakingQueueMutex, lobby.ID_Lobby);
    }

    lobbiesMap[lobby.ID_Lobby] = lobby;
    lobbiesMutexMap[lobby.ID_Lobby] = std::make_unique<std::mutex>();
    return lobby.ID_Lobby;
};

std::string Server::findRandomMatch(NoBiggySocket clientSocket) {
    std::string uuid = Common::getThreadSafeFromQueue(matchMakingQueue, matchMakingQueueMutex);
    if (uuid.empty()) {
        return startNewLobby(clientSocket, LobbyPrivacyType::Public);
    }

    lobbiesMap[uuid].SetPeer2IfPossible(NetworkUtils::getPeerInfo(clientSocket), lobbiesMutexMap[uuid].get());
    lobbiesMap[uuid].Print();
    return uuid;
}

bool Server::checkForErrors(NoBiggySocket socket, int errorMacro, const char *errorMessage, bool closeSocket) {
    if (socket == errorMacro) {
        onError(socket, closeSocket, errorMessage);
        return true;
    }
    return false;
}

std::string Server::generateNewUUID() {
    std::string uuid = Common::generateUUID(UUID_LENGTH);

    if (lobbiesMap.find(uuid) != lobbiesMap.end()) {
        return generateNewUUID();
    }

    return uuid;
}

// end --- Server.cpp --- 

