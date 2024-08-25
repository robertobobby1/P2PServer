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

namespace P2PServer {
    inline const int UUID_LENGTH = 6;
    inline const int MAX_workers = 10;
    inline const int PORT = 3000;
    inline const int BACKLOG = 10;
    inline const int SECURITY_HEADER_LENGTH = 23;
    inline const char *SECURITY_HEADER = "0sdFGeVi3ItN1qwsHp3mcDF";

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
    bool isValidSecurityHeader(const char *buffer);
    ActionType getActionTypeFromHeaderByte(uint8_t headerByte);
    LobbyPrivacyType getLobbyPrivacyTypeFromHeaderByte(uint8_t headerByte);
    void sendUuidToClient(R::Net::Socket clientSocket, std::string &uuid);
    void connectPeersIfNecessary(std::string &uuid);

    std::string findRandomMatch(R::Net::Socket clientSocket);
    std::string startNewLobby(R::Net::Socket clientSocket, LobbyPrivacyType ClientServerHeaderFlags);
    std::string generateNewUUID();
    std::string findUUIDbyClientSocket(R::Net::Socket clientSocket);
    void cleanUpLobbyByUUID(std::string &uuid);
    inline std::unordered_map<std::string, Lobby> lobbiesMap;
    inline std::unordered_map<std::string, std::shared_ptr<std::mutex>> lobbiesMutexMap;
}  // namespace P2PServer