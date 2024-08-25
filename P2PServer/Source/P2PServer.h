#pragma once

#include <vector>
#include <iostream>
#include <cstdint>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "R.h"
#include "NetworkStructs.h"
#include "NetworkUtils.h"

namespace Rp2p = R::Net::P2P;

namespace P2PServer {
    inline const int UUID_LENGTH = 6;
    inline const int MAX_workers = 10;
    inline const int PORT = 3000;
    inline const int BACKLOG = 10;

    void run();
    void worker();
    inline std::vector<std::thread> workers;

    void addFDToSet(R::Net::Socket socket);
    void removeFDFromSet(R::Net::Socket socket);
    inline std::queue<R::Net::Socket> socketsToCloseQueue;
    inline std::mutex socketsToCloseQueueMutex;
    inline std::vector<R::Net::Socket> activeSockets;
    inline fd_set readSocketsFDSet;
    inline std::shared_ptr<R::Net::Server> server;
    inline bool keepRunning = true;

    inline std::queue<R::Net::Socket> socketQueue;
    inline std::mutex socketQueueMutex;
    inline std::condition_variable socketQueueCondition;

    inline std::mutex matchMakingQueueMutex;
    inline std::queue<std::string> matchMakingQueue;

    void handleRequest(R::Buffer buffer, R::Net::Socket clientSocket);
    void sendUuidToClient(R::Net::Socket clientSocket, std::string &uuid);
    void connectPeersIfNecessary(std::string &uuid);

    std::string findRandomMatch(R::Net::Socket clientSocket);
    std::string startNewLobby(R::Net::Socket clientSocket, Rp2p::LobbyPrivacyType ClientServerHeaderFlags);
    std::string generateNewUUID();
    std::string findUUIDbyClientSocket(R::Net::Socket clientSocket);
    void cleanUpLobbyByUUID(std::string &uuid);
    void cleanUpLobbyBySocket(R::Net::Socket clientSocket);
    inline std::unordered_map<std::string, Lobby> lobbiesMap;
    inline std::unordered_map<std::string, std::shared_ptr<std::mutex>> lobbiesMutexMap;
}  // namespace P2PServer