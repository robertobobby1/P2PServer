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

    RLog("[Logic Server] Program terminated normally!");
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
            cleanUpLobbyBySocket(clientSocket);
        }
    }
}

void P2PServer::cleanUpLobbyBySocket(R::Net::Socket clientSocket) {
    // clean up lobby if necessary
    auto uuid = findUUIDbyClientSocket(clientSocket);
    if (uuid != "") {
        cleanUpLobbyByUUID(uuid);
    } else {
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
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
        // TODO notify to peer
    }

    R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, lobbiesMap[uuid].peer1.socket);
    // TODO notify to peer
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
 *  - 5 Optional bytes that are the game hash 24-29 isValidAuthedRequest
 */
void P2PServer::handleRequest(R::Buffer buffer, R::Net::Socket clientSocket) {
    if (!Rp2p::isValidAuthedRequest(buffer)) {
        RLog("[Logic Server] Bad protocol!\n");
        cleanUpLobbyBySocket(clientSocket);
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
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

    Peer peer2 = lobby.GetPeer2(lobbiesMutexMap[uuid].get());
    uint16_t delayPeer2, delayPeer1 = 0;
    if (peer2.averageRTT > lobby.peer1.averageRTT) {
        delayPeer2 = peer2.averageRTT - lobby.peer1.averageRTT;
    } else {
        delayPeer1 = lobby.peer1.averageRTT - peer2.averageRTT;
    }

    auto bufferForPeer1 = Rp2p::createServerConnectBuffer(lobby.peer1.ipAddress.s_addr, lobby.peer1.port, delayPeer1);
    auto bufferForPeer2 = Rp2p::createServerConnectBuffer(peer2.ipAddress.s_addr, peer2.port, delayPeer2);

    auto peer1Response = server->sendMessage(lobby.peer1.socket, bufferForPeer1);
    auto peer2Response = server->sendMessage(peer2.socket, bufferForPeer2);

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
    lobbiesMutexMap[lobby.ID_Lobby] = std::make_shared<std::mutex>();
    return lobby.ID_Lobby;
};

std::string P2PServer::findRandomMatch(R::Net::Socket clientSocket) {
    std::string uuid = R::Utils::getThreadSafeFromQueue(matchMakingQueue, matchMakingQueueMutex);
    if (uuid.empty()) {
        return startNewLobby(clientSocket, Rp2p::LobbyPrivacyType::Public);
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