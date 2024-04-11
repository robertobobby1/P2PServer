#pragma once
#include <vector>
#include "Platform.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <netinet/tcp.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
#    include <WinSock2.h>
#    include <ws2tcpip.h>
#    pragma comment(lib, "winmm.lib")
#    pragma comment(lib, "WS2_32.lib")
#    include <Windows.h>
#endif

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
typedef int NoBiggySocket;
#    define NoBiggyAcceptSocketError -1
#elif defined(PLATFORM_WINDOWS)
typedef SOCKET NoBiggySocket;
#    define NoBiggyAcceptSocketError INVALID_SOCKET
#endif

struct Peer {
    NoBiggySocket socket;
    char *ipAddress;
    uint16_t port;
    uint8_t family;
    uint32_t averageRTT;

    void print() {
        printf("Peer information:\n");
        printf("Peer Address Family: %d\n", this->family);
        printf("Peer Port: %d\n", this->port);
        printf("Peer IP Address: %s\n", this->ipAddress);
        printf("Average RTT: %i\n", this->averageRTT);
    }
};

struct Lobby {
    std::string ID_Lobby;
    Peer peer1;
    Peer peer2;

    void print() {
        printf("\nStart lobby info ---- %s\n\n", this->ID_Lobby.c_str());
        printf("Peer 1:\n");
        this->peer1.print();
        printf("\nPeer 2:\n");
        this->peer2.print();
        printf("\nEnd lobby info   ---- %s\n\n", this->ID_Lobby.c_str());
    }
};

namespace Server {

    inline const int UUID_LENGTH = 6;
    inline const int MAX_WORKERS = 10;
    inline const int PORT = 3000;
    inline const int BACKLOG = 10;

    void run();
    void runWorker();
    inline std::vector<std::thread> WORKERS;

    bool startServer();
    void onError(NoBiggySocket socket, bool closeSocket, const char *errorMessage);
    bool checkForErrors(NoBiggySocket socket, int errorMacro, const char *errorMessage, bool closeSocket = false);

    void setTSToSocketQueue(NoBiggySocket socket);
    NoBiggySocket getTSFromSocketQueue();
    inline std::queue<NoBiggySocket> socketQueue;
    inline std::mutex socketQueueMutex;
    inline std::condition_variable socketQueueCondition;

    void setTSToMatchMakingQueue(std::string socket);
    std::string getTSFromMatchMakingQueue();
    inline std::mutex matchMakingQueueMutex;
    inline std::queue<std::string> matchMakingQueue;

    void handleNewRequest(const char *buffer, int bytesReceived, NoBiggySocket clientSocket);
    Peer getPeerInfo(NoBiggySocket clientSocket);
    uint32_t getRTTOfClient(NoBiggySocket clientSocket);

    std::string findRandomMatch(NoBiggySocket clientSocket);
    std::string startNewLobby(NoBiggySocket clientSocket);
    std::string generateNewUUID();
    inline std::unordered_map<std::string, Lobby> lobbiesMap;

    inline NoBiggySocket SERVER_SOCKET;
    inline bool keepRunning = true;
}  // namespace Server