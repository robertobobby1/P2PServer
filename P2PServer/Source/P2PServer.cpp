#include "P2PServer.h"

void P2PServer::run() {
    R::Utils::stackTracing();
    srand((unsigned)time(NULL));
    FD_ZERO(&readSocketsFDSet);

    socketQueue = std::queue<R::Net::Socket>();
    matchMakingQueue = std::queue<std::string>();
    socketsToCloseQueue = std::queue<R::Net::Socket>();

    server = R::Net::Server::makeAndRun(PORT, BACKLOG);
    if (!server->isRunning)
        return;

    for (unsigned int i = 0; i < MAX_workers; i++) {
        workers.push_back(std::thread(worker));
    }

    addFDToSet(server->_socket);

    while (keepRunning) {
        auto socketToClose = R::Utils::getThreadSafeFromQueue(socketsToCloseQueue, socketsToCloseQueueMutex);
        while (socketToClose != -1) {
            removeFDFromSet(socketToClose);
            socketToClose = R::Utils::getThreadSafeFromQueue(socketsToCloseQueue, socketsToCloseQueueMutex);
        }

        fd_set temporarySet = readSocketsFDSet;
        auto selectResponse = select(FD_SETSIZE, &temporarySet, NULL, NULL, NULL);
        if (selectResponse < 0) {
            R::Net::onError(server->_socket, false, "[Logic Server] Error during select");
            keepRunning = false;
            break;
        }

        for (auto &activeSocket : activeSockets) {
            if (!FD_ISSET(activeSocket, &readSocketsFDSet)) {
                continue;
            }

            R::Net::Socket socketToAttend;
            if (activeSocket == server->_socket) {
                auto acceptSocket = server->acceptNewConnection(false);
                if (acceptSocket == SocketError) {
                    // socket is non blocking, just continue, this is already handled
                    // check if applies also in windows
                    if (errno == 35) {
                        break;
                    }
                    R::Net::onError(server->_socket, true, "[Logic Server] Error during accept");
                    keepRunning = false;
                    break;
                }

                printf("[Logic Server] New connection!\n");
                addFDToSet(acceptSocket);
                socketToAttend = acceptSocket;
            } else {
                socketToAttend = activeSocket;
            }

            R::Utils::setThreadSafeToQueue(socketQueue, socketQueueMutex, socketToAttend);
            socketQueueCondition.notify_one();
        }
    }

    printf("[Logic Server] Program terminated normally!");
    std::terminate();
}

void P2PServer::removeFDFromSet(R::Net::Socket socket) {
    FD_CLR(socket, &readSocketsFDSet);
    activeSockets.erase(std::remove(activeSockets.begin(), activeSockets.end(), socket), activeSockets.end());
    close(socket);
}

void P2PServer::addFDToSet(R::Net::Socket socket) {
    FD_SET(socket, &readSocketsFDSet);
    activeSockets.push_back(socket);
}

void P2PServer::worker() {
    R::Buffer buffer(255);
    R::Net::Socket clientSocket;

    while (true) {
        clientSocket = R::Utils::getThreadSafeFromQueue(socketQueue, socketQueueMutex, socketQueueCondition);
        buffer = server->readMessage(clientSocket);

        if (buffer.size > 0) {
            handleRequest(buffer, clientSocket);
        } else {
            // clean up lobby if necessary
            auto uuid = findUUIDbyClientSocket(clientSocket);
            if (uuid != "") {
                cleanUpLobbyByUUID(uuid);
            } else {
                R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
            }
        }
    }
}

void P2PServer::cleanUpLobbyByUUID(std::string &uuid) {
    if (uuid == "") {
        return;
    }

    auto lobbyMutex = lobbiesMutexMap[uuid].get();
    if (lobbiesMap[uuid].IsLobbyComplete(lobbyMutex)) {
        auto peer2 = lobbiesMap[uuid].GetPeer2(lobbyMutex);
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, peer2.socket);
    }

    R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, lobbiesMap[uuid].peer1.socket);
    lobbiesMap.erase(uuid);
    lobbiesMutexMap.erase(uuid);
}

std::string P2PServer::findUUIDbyClientSocket(R::Net::Socket clientSocket) {
    for (auto i = lobbiesMap.begin(); i != lobbiesMap.end(); i++) {
        auto mutex = lobbiesMutexMap[i->first].get();
        if (i->second.GetPeer2(mutex).socket == clientSocket || i->second.peer1.socket == clientSocket) {
            return i->second.ID_Lobby;
        }
    }

    return "";
}

/*
 * 24-29 BYTES should be received including:
 *  - 23 Bytes of security header 0-22
 *  - 1 Byte for header protocol data, includes ClientServerHeaderFlags and if its new or to connect lobby 23
 *  - 5 Optional bytes that are the game hash 24-29
 */
void P2PServer::handleRequest(R::Buffer buffer, R::Net::Socket clientSocket) {
    if (!R::Utils::isInRange(buffer.size, 24, 29) || !isValidSecurityHeader(buffer.ini)) {
        printf("[Logic Server] Bad protocol!\n");
        printf("[Logic Server] received header: %s\n", buffer.ini);
        printf("[Logic Server] security header: %s\n", SECURITY_HEADER);
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
        return;
    }

    std::string uuid = "";
    uint8_t dataHeaderFlags = buffer[23];
    LobbyPrivacyType lobbyPrivacyType = getLobbyPrivacyTypeFromHeaderByte(dataHeaderFlags);
    ActionType action = getActionTypeFromHeaderByte(dataHeaderFlags);

    if (action == ActionType::Disconnect || action == ActionType::PeerConnectSuccess) {
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
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
            R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
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
void P2PServer::connectPeersIfNecessary(std::string &uuid) {
    Lobby lobby = lobbiesMap[uuid];
    if (!lobby.IsLobbyComplete(lobbiesMutexMap[uuid].get())) {
        sendUuidToClient(lobbiesMap[uuid].peer1.socket, uuid);
        return;
    }

    int bufferLength = SECURITY_HEADER_LENGTH + 1 + 4 + 2 + 4;
    std::unique_ptr<char[]> bufferForPeer1(new char[bufferLength]);
    std::unique_ptr<char[]> bufferForPeer2(new char[bufferLength]);

    uint8_t headerFlags = 0;
    R::Utils::setFlag(headerFlags, ServerClientHeaderFlags::ServerClientHeaderFlags_Action);

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

void P2PServer::sendUuidToClient(R::Net::Socket clientSocket, std::string &uuid) {
    auto buffer = R::Buffer(SECURITY_HEADER_LENGTH + 1 + uuid.size());
    // action 0 is send uuid
    uint8_t headerFlags = 0;

    buffer.write(SECURITY_HEADER, SECURITY_HEADER_LENGTH);
    buffer.write(headerFlags);
    buffer.write(uuid.c_str(), uuid.size());

    auto sendResponse = server->sendMessage(clientSocket, buffer);

    if (sendResponse == -1) {
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
    }
}

LobbyPrivacyType P2PServer::getLobbyPrivacyTypeFromHeaderByte(uint8_t headerByte) {
    if (R::Utils::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Public)) {
        return LobbyPrivacyType::Public;
    }
    return LobbyPrivacyType::Private;
}

ActionType P2PServer::getActionTypeFromHeaderByte(uint8_t headerByte) {
    bool isBit1Set = R::Utils::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Bit1);
    bool isBit2Set = R::Utils::isFlagSet(headerByte, ClientServerHeaderFlags::ClientServerHeaderFlags_Bit2);

    if (isBit1Set) {
        if (isBit2Set) {
            // 11 = connect
            return ActionType::Connect;
        } else {
            // 10 = createLobby
            return ActionType::Create;
        }
    } else {
        if (isBit2Set) {
            // 01 = disconnect
            return ActionType::Disconnect;
        } else {
            // 00 = peersConnectSuccess
            return ActionType::PeerConnectSuccess;
        }
    }
}

bool P2PServer::isValidSecurityHeader(const char *buffer) {
    return strncmp(buffer, SECURITY_HEADER, SECURITY_HEADER_LENGTH) == 0;
}

std::string P2PServer::startNewLobby(R::Net::Socket clientSocket, LobbyPrivacyType lobbyPrivacyType) {
    Lobby lobby;
    lobby.ID_Lobby = generateNewUUID();
    lobby.peer1 = NetworkUtils::getPeerInfo(clientSocket);
    lobby.lobbyPrivacyType = lobbyPrivacyType;

    if (lobbyPrivacyType == LobbyPrivacyType::Public) {
        R::Utils::setThreadSafeToQueue(matchMakingQueue, matchMakingQueueMutex, lobby.ID_Lobby);
    }

    lobbiesMap[lobby.ID_Lobby] = lobby;
    lobbiesMutexMap[lobby.ID_Lobby] = std::make_shared<std::mutex>();
    return lobby.ID_Lobby;
};

std::string P2PServer::findRandomMatch(R::Net::Socket clientSocket) {
    std::string uuid = R::Utils::getThreadSafeFromQueue(matchMakingQueue, matchMakingQueueMutex);
    if (uuid.empty()) {
        return startNewLobby(clientSocket, LobbyPrivacyType::Public);
    }

    lobbiesMap[uuid].SetPeer2IfPossible(NetworkUtils::getPeerInfo(clientSocket), lobbiesMutexMap[uuid].get());
    lobbiesMap[uuid].Print();
    return uuid;
}

std::string P2PServer::generateNewUUID() {
    std::string uuid = R::Utils::generateUUID(UUID_LENGTH);

    if (lobbiesMap.find(uuid) != lobbiesMap.end()) {
        return generateNewUUID();
    }

    return uuid;
}