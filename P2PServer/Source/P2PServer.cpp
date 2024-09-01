#include "P2PServer.h"

void P2PServer::run() {
    R::Utils::stackTracing();
    // turn SIGPIPE into EPIPE so that the program doesn't terminate
    signal(SIGPIPE, SIG_IGN);

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

    keepAliveManager = Rp2p::KeepAliveManager::makeAndRun(KEEP_ALIVE_TIMER_SECONDS);
    keepAliveManager->addOnConnectionClosedCallback(cleanUpLobbyBySocket);

    auto lobbyCleanUpThread = cleanUpMarkedLobbiesThread();
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
                    // socket is a broken pipe, just ignore it
                    // TODO check if applies also in windows
                    if (errno == 35 || errno == 32) {
                        break;
                    }
                    R::Net::onError(server->_socket, true, "[Logic Server] Error during accept");
                    keepRunning = false;
                    break;
                }

                RLog("[Logic Server] New connection!\n");
                addFDToSet(acceptSocket);
                socketToAttend = acceptSocket;
            } else {
                socketToAttend = activeSocket;
            }

            R::Utils::setThreadSafeToQueue(socketQueue, socketQueueMutex, socketToAttend);
            socketQueueCondition.notify_one();
        }
    }

    RLog("[Logic Server] Program terminated normally!\n");
    std::terminate();
}

void P2PServer::removeFDFromSet(R::Net::Socket socket) {
    FD_CLR(socket, &readSocketsFDSet);
    R::Utils::removeFromVector(activeSockets, socket);
    close(socket);
}

void P2PServer::addFDToSet(R::Net::Socket socket) {
    FD_SET(socket, &readSocketsFDSet);
    activeSockets.push_back(socket);
    if (socket != server->_socket) {
        keepAliveManager->addNewSocketToKeepAlive(socket);
    }
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
            cleanUpLobbyBySocket(clientSocket);
        }
    }
}

void P2PServer::cleanUpLobbyBySocket(R::Net::Socket clientSocket) {
    R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
    // clean up lobby if necessary
    auto uuid = findUUIDbyClientSocket(clientSocket);
    if (uuid != "") {
        cleanUpLobbyByUUID(uuid);
    }
}

void P2PServer::cleanUpLobbyByUUID(std::string &uuid) {
    if (!R::Utils::keyExistsInMap(lobbiesMap, uuid)) {
        return;
    }

    removeFromMatchmakingQueueByUUID(uuid);
    std::unique_lock<std::mutex> lock(*lobbiesMap[uuid].mutex);
    if (lobbiesMap[uuid].isLobbyComplete) {
        auto peer2 = lobbiesMap[uuid].peer2;
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, peer2.socket);
        // TODO notify to peer, check if it is active by pinging? --- send to matchmaking again when non-private lobby?
    }

    R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, lobbiesMap[uuid].peer1.socket);
    // TODO notify to peer check if it is active by pinging? --- send to matchmaking again when non-private lobby?
    lobbiesMap[uuid].isMarkedForCleanup = true;
}

std::thread P2PServer::cleanUpMarkedLobbiesThread() {
    return std::thread([]() -> void {
        while (keepRunning) {
            std::this_thread::sleep_for(std::chrono::seconds(CLEANUP_TIMER_SECONDS));
            for (auto i = lobbiesMap.begin(); i != lobbiesMap.end(); i++) {
                std::unique_lock<std::mutex> lock(*i->second.mutex);
                if (!i->second.isMarkedForCleanup) {
                    continue;
                }

                RLog("[Logic Server] Cleaned up lobby with uuid: %s\n", i->second.ID_Lobby.c_str());
                lobbiesMap.erase(i->second.ID_Lobby);
            }
        }
    });
}

std::string P2PServer::findUUIDbyClientSocket(R::Net::Socket clientSocket) {
    for (auto i = lobbiesMap.begin(); i != lobbiesMap.end(); i++) {
        if (i->second.peer2.socket == clientSocket || i->second.peer1.socket == clientSocket) {
            return i->second.ID_Lobby;
        }
    }

    return "";
}

void P2PServer::removeFromMatchmakingQueueByUUID(std::string &uuid) {
    std::unique_lock lock(matchMakingQueueMutex);
    auto queueC = R::Utils::getQueueCObject(matchMakingQueue);

    for (auto it = queueC.begin(); it != queueC.end(); ++it) {
        if (*it == uuid) {
            queueC.erase(it);
        }
    }
}

/*
 * 24-29 BYTES should be received including:
 *  - 23 Bytes of security header 0-22
 *  - 1 Byte for header protocol data, includes ClientServerHeaderFlags and if its new or to connect lobby 23
 *  - 5 Optional bytes that are the game hash 24-29 isValidAuthedRequest
 */
void P2PServer::handleRequest(R::Buffer buffer, R::Net::Socket clientSocket) {
    if (!Rp2p::isValidAuthedRequest(buffer)) {
        RLog("[Logic Server] Bad protocol!\n");
        cleanUpLobbyBySocket(clientSocket);
        return;
    }

    std::string uuid = "";
    uint8_t dataHeaderFlags = Rp2p::getProtocolHeader(buffer);
    Rp2p::LobbyPrivacyType lobbyPrivacyType = Rp2p::getLobbyPrivacyTypeFromHeaderByte(dataHeaderFlags);
    Rp2p::ClientActionType action = Rp2p::getClientActionTypeFromHeaderByte(dataHeaderFlags);

    if (action == Rp2p::ClientActionType::PeerConnectSuccess) {
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
        return;
    } else if (action == Rp2p::ClientActionType::Disconnect) {
        cleanUpLobbyBySocket(clientSocket);
        return;
    }

    // public lobby - connect/create
    if (lobbyPrivacyType == Rp2p::LobbyPrivacyType::Public) {
        uuid = findRandomMatch(clientSocket);
    } else if (action == Rp2p::ClientActionType::Create) {
        // private lobby - create action
        uuid = startNewLobby(clientSocket, Rp2p::LobbyPrivacyType::Private);
    } else if (action == Rp2p::ClientActionType::Connect) {
        // private lobby - connect action
        uuid = std::string(buffer[24], (size_t)5);
        auto lobby = lobbiesMap.find(uuid);
        if (lobby == lobbiesMap.end() || lobby->second.isLobbyComplete) {
            cleanUpLobbyBySocket(clientSocket);
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
    if (!R::Utils::keyExistsInMap(lobbiesMap, uuid)) {
        RLog("[Logic Server] The lobby has been removed already!\n");
        return;
    }

    Lobby lobby = lobbiesMap[uuid];
    std::unique_lock<std::mutex> lock(*lobby.mutex);
    // if the lobby has been marked for clean up just skip
    if (lobby.isMarkedForCleanup) {
        return;
    }
    // check for existance of lobbies map
    if (!lobby.isLobbyComplete) {
        sendUuidToClient(lobby.peer1.socket, uuid);
        return;
    }

    uint16_t delayPeer2, delayPeer1 = 0;
    if (lobby.peer2.averageRTT > lobby.peer1.averageRTT) {
        delayPeer2 = lobby.peer2.averageRTT - lobby.peer1.averageRTT;
    } else {
        delayPeer1 = lobby.peer1.averageRTT - lobby.peer2.averageRTT;
    }

    auto bufferForPeer1 = Rp2p::createServerConnectBuffer(lobby.peer1.ipAddress.s_addr, lobby.peer1.port, delayPeer1);
    auto bufferForPeer2 = Rp2p::createServerConnectBuffer(lobby.peer2.ipAddress.s_addr, lobby.peer2.port, delayPeer2);

    auto peer1Response = server->sendMessage(lobby.peer1.socket, bufferForPeer1);
    auto peer2Response = server->sendMessage(lobby.peer2.socket, bufferForPeer2);

    if (peer1Response == -1 || peer2Response == -1) {
        lobby.peerConnectionSendFailure = true;
    }
}

void P2PServer::sendUuidToClient(R::Net::Socket clientSocket, std::string &uuid) {
    auto buffer = Rp2p::createServerSendUUIDBuffer(uuid);

    auto sendResponse = server->sendMessage(clientSocket, buffer);
    if (sendResponse == -1) {
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
    }
}

std::string P2PServer::startNewLobby(R::Net::Socket clientSocket, Rp2p::LobbyPrivacyType lobbyPrivacyType) {
    Lobby lobby;
    lobby.ID_Lobby = generateNewUUID();
    lobby.peer1 = NetworkUtils::getPeerInfo(clientSocket);
    lobby.lobbyPrivacyType = lobbyPrivacyType;

    if (lobbyPrivacyType == Rp2p::LobbyPrivacyType::Public) {
        R::Utils::setThreadSafeToQueue(matchMakingQueue, matchMakingQueueMutex, lobby.ID_Lobby);
    }

    lobbiesMap[lobby.ID_Lobby] = lobby;
    return lobby.ID_Lobby;
};

std::string P2PServer::findRandomMatch(R::Net::Socket clientSocket) {
    std::string uuid = R::Utils::getThreadSafeFromQueue(matchMakingQueue, matchMakingQueueMutex);
    if (uuid.empty()) {
        return startNewLobby(clientSocket, Rp2p::LobbyPrivacyType::Public);
    }

    std::unique_lock<std::mutex> lock(*lobbiesMap[uuid].mutex);
    if (lobbiesMap[uuid].isMarkedForCleanup || lobbiesMap[uuid].isLobbyComplete) {
        return startNewLobby(clientSocket, Rp2p::LobbyPrivacyType::Public);
    }

    lobbiesMap[uuid].peer2 = NetworkUtils::getPeerInfo(clientSocket);
    lobbiesMap[uuid].isLobbyComplete = true;
    lobbiesMap[uuid].Print();
    return uuid;
}

std::string P2PServer::generateNewUUID() {
    std::string uuid = R::Utils::generateUUID(UUID_LENGTH);

    if (R::Utils::keyExistsInMap(lobbiesMap, uuid)) {
        return generateNewUUID();
    }

    return uuid;
}