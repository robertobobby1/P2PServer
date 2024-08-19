#pragma once

#include <vector>
#include <iostream>
#include <cstdint>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "Platform.h"
#include "Common.h"
#include "NetworkStructs.h"
#include "NetworkUtils.h"

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