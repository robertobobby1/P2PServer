#pragma once

#include <vector>
#include <iostream>
#include <cstdint>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <set>

#include "R.h"
#include "NetworkStructs.h"

namespace Rp2p = R::Net::P2P;

namespace P2PServer {
    inline const int SELECT_TIMEOUT_SECONDS = 2;
    inline const int UUID_LENGTH = 6;
    inline const int PORT = 3000;
    inline const int BACKLOG = 10;
    inline const int CLEANUP_TIMER_SECONDS = 10;
    inline const int KEEP_ALIVE_TIMER_SECONDS = 3;

    inline bool keepRunning = true;
    inline std::shared_ptr<R::Net::Server> server;
    inline std::shared_ptr<R::Net::P2P::KeepAliveManager> keepAliveManager;

    void run();
    void worker();

    inline std::thread workerThread;

    inline std::queue<R::Net::Socket> socketsToCloseQueue;
    inline std::mutex socketsToCloseQueueMutex;

    void addFDToSet(R::Net::Socket socket);
    void removeFDFromSet(R::Net::Socket socket);

    inline std::vector<R::Net::Socket> activeSockets;
    inline fd_set readSocketsFDSet;

    inline std::queue<R::Net::Socket> socketQueue;
    inline std::mutex socketQueueMutex;
    inline std::condition_variable socketQueueCondition;

    void handleRequest(R::Buffer buffer, R::Net::Socket clientSocket);
    R::Net::Socket waitForClientSocket();

    void sendUuidToClient(R::Net::Socket clientSocket, std::string &uuid);
    void connectPeersIfNecessary(std::string &uuid);

    void joinPrivateMatch(R::Net::Socket clientSocket, std::string &uuid, uint16_t clientPort);
    std::string findRandomMatch(R::Net::Socket clientSocket, uint16_t clientPort);
    std::string startNewLobby(R::Net::Socket clientSocket, Rp2p::LobbyPrivacyType ClientServerHeaderFlags, uint16_t clientPort);

    std::string generateNewUUID();
    std::string findUUIDbyClientSocket(R::Net::Socket clientSocket);

    void cleanUpLobbyByUUID(std::string &uuid);
    void cleanUpLobbyBySocket(R::Net::Socket clientSocket);

    inline std::queue<R::Net::Socket> socketsToCleanUp;
    inline std::mutex socketsToCleanUpMutex;

    inline std::unordered_map<std::string, Lobby> lobbiesMap;
    inline std::unordered_map<R::Net::Socket, in_addr> socketToIpAddressMap;
    inline std::queue<std::string> matchMakingQueue;

}  // namespace P2PServer