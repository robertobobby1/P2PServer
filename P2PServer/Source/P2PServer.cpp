#include "P2PServer.h"

void P2PServer::run() {
    timeval selectTimeout{SELECT_TIMEOUT_SECONDS, 0};
    // turn SIGPIPE into EPIPE so that the program doesn't terminate

    srand((unsigned)time(NULL));

    FD_ZERO(&readSocketsFDSet);

    socketQueue = std::queue<R::Net::Socket>();
    matchMakingQueue = std::queue<std::string>();
    socketsToCloseQueue = std::queue<R::Net::Socket>();

    server = R::Net::Server::makeAndRun(PORT, BACKLOG);
    if (!server->isRunning)
        return;

    workerThread = std::thread(worker);
    keepAliveManager = Rp2p::KeepAliveManager::makeAndRun(KEEP_ALIVE_TIMER_SECONDS);
    keepAliveManager->addOnConnectionClosedCallback([](R::Net::Socket socket) {
        // add to queue so that worker can handle it to avoid mutual exclusion problems
        R::Utils::setThreadSafeToQueue(socketsToCleanUp, socketsToCleanUpMutex, socket);
    });
    keepAliveManager->addOnKeepAliveMaxPackagesSent(5);

    addFDToSet(server->_socket);

    while (keepRunning) {
        auto socketToClose = R::Utils::getThreadSafeFromQueue(socketsToCloseQueue, socketsToCloseQueueMutex);
        while (socketToClose != -1) {
            removeFDFromSet(socketToClose);
            socketToClose = R::Utils::getThreadSafeFromQueue(socketsToCloseQueue, socketsToCloseQueueMutex);
        }

        fd_set temporarySet = readSocketsFDSet;
        auto selectResponse = select(FD_SETSIZE, &temporarySet, NULL, NULL, &selectTimeout);
        if (selectResponse < 0) {
            R::Net::onError(server->_socket, true, "[Logic Server] Error during select");
            keepRunning = false;
            break;
        }

        // timeout
        if (selectResponse == 0) {
            continue;
        }

        R::Net::Socket socketToAttend;
        for (auto &activeSocket : activeSockets) {
            if (!R::Net::isValidSocket(activeSocket) || !FD_ISSET(activeSocket, &readSocketsFDSet)) {
                continue;
            }

            if (activeSocket == server->_socket) {
                auto acceptResponse = server->acceptNewConnection(false);
                auto acceptSocket = acceptResponse.socket;
                if (acceptSocket == 0) {
                    continue;
                }

                if (acceptSocket == SocketError) {
                    // socket is non blocking, just continue, this is already handled
                    // socket is a broken pipe, just ignore it
                    if (errno == EAGAIN || errno == EPIPE) {
                        break;
                    }
                    R::Net::onError(server->_socket, true, "[Logic Server] Error during accept");
                    keepRunning = false;
                    break;
                }

                RLog("[Logic Server] New connection!\n");
                socketToIpAddressMap[acceptSocket] = acceptResponse.ipAddress;
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
    if (!R::Net::isValidSocket(socket)) {
        return;
    }
    R::Utils::removeFromVector(activeSockets, socket);
    socketToIpAddressMap.erase(socket);

    FD_CLR(socket, &readSocketsFDSet);
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

    while (keepRunning) {
        // clean up all disconnected sockets
        auto socketToCleanUp = R::Utils::getThreadSafeFromQueue(socketsToCleanUp, socketsToCleanUpMutex);
        while (socketToCleanUp != -1) {
            cleanUpLobbyBySocket(socketToCleanUp);
            socketToCleanUp = R::Utils::getThreadSafeFromQueue(socketsToCleanUp, socketsToCleanUpMutex);
        }

        clientSocket = waitForClientSocket();
        if (clientSocket == -1) {
            continue;
        }

        buffer = server->readMessage(clientSocket);
        if (buffer.size > 0) {
            handleRequest(buffer, clientSocket);
        } else {
            cleanUpLobbyBySocket(clientSocket);
        }
    }
}

R::Net::Socket P2PServer::waitForClientSocket() {
    R::Net::Socket clientSocket;
    while (keepRunning) {
        clientSocket = R::Utils::getThreadSafeFromQueue(socketQueue, socketQueueMutex);
        if (clientSocket != -1) {
            return clientSocket;
        }

        // for long wait times, it will mostly be waiting here, not completely waiting!
        // it must check for broken connections to close
        return R::Utils::getThreadSafeFromQueue(socketQueue, socketQueueMutex, socketQueueCondition);
    }

    return -1;
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
    if (!R::Utils::keyExistsInMap(lobbiesMap, uuid)) {
        return;
    }

    R::Utils::removeFromQueue(matchMakingQueue, uuid);

    auto &lobby = lobbiesMap[uuid];
    if (lobby.isLobbyComplete) {
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, lobby.peer2.socket);
    }

    R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, lobby.peer1.socket);
    RLog("[Logic Server] Cleaned up lobby with uuid: %s\n", lobby.ID_Lobby.c_str());
    lobbiesMap.erase(lobby.ID_Lobby);
}

std::string P2PServer::findUUIDbyClientSocket(R::Net::Socket clientSocket) {
    for (auto i = lobbiesMap.begin(); i != lobbiesMap.end(); i++) {
        if (i->second.peer2.socket == clientSocket || i->second.peer1.socket == clientSocket) {
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
        return;
    }

    auto dataHeaderFlags = Rp2p::getProtocolHeader(buffer);
    auto lobbyPrivacyType = Rp2p::getLobbyPrivacyTypeFromHeaderByte(dataHeaderFlags);
    auto action = Rp2p::getClientActionTypeFromHeaderByte(dataHeaderFlags);

    if (action == Rp2p::ClientActionType::PeerConnectSuccess || action == Rp2p::ClientActionType::Disconnect) {
        // TODO keep as succesful lobby?
        auto uuid = findUUIDbyClientSocket(clientSocket);
        if (R::Utils::keyExistsInMap(lobbiesMap, uuid)) {
            lobbiesMap[uuid].Print();
        }

        cleanUpLobbyBySocket(clientSocket);
        return;
    }

    std::string uuid = "";
    auto payload = Rp2p::getPayload(buffer);
    auto clientPort = ntohs(payload.read<uint16_t>(0));
    if (lobbyPrivacyType == Rp2p::LobbyPrivacyType::Public) {  // public lobby - connect/create
        uuid = findRandomMatch(clientSocket, clientPort);
    } else if (action == Rp2p::ClientActionType::Create) {  // private lobby - create action
        uuid = startNewLobby(clientSocket, Rp2p::LobbyPrivacyType::Private, clientPort);
    } else if (action == Rp2p::ClientActionType::Connect) {  // private lobby - connect action
        uuid = std::string(payload[2], (size_t)5);
        joinPrivateMatch(clientSocket, uuid, clientPort);
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
        return;
    }

    Lobby lobby = lobbiesMap[uuid];
    if (!lobby.isLobbyComplete) {
        sendUuidToClient(lobby.peer1.socket, uuid);
        return;
    }

    uint16_t delayPeer2 = 0, delayPeer1 = 0;
    if (lobby.peer2.averageRTT > lobby.peer1.averageRTT) {
        delayPeer2 = lobby.peer2.averageRTT - lobby.peer1.averageRTT;
    } else {
        delayPeer1 = lobby.peer1.averageRTT - lobby.peer2.averageRTT;
    }

    auto bufferForPeer1 = Rp2p::createServerConnectBuffer(lobby.peer1.ipAddress.s_addr, lobby.peer2.port, delayPeer1);
    auto bufferForPeer2 = Rp2p::createServerConnectBuffer(lobby.peer2.ipAddress.s_addr, lobby.peer1.port, delayPeer2);

    auto peer1Response = server->sendMessage(lobby.peer1.socket, bufferForPeer1);
    auto peer2Response = server->sendMessage(lobby.peer2.socket, bufferForPeer2);

    if (peer1Response == -1 || peer2Response == -1) {
        cleanUpLobbyByUUID(uuid);
    }
}

void P2PServer::sendUuidToClient(R::Net::Socket clientSocket, std::string &uuid) {
    auto buffer = Rp2p::createServerSendUUIDBuffer(uuid);

    auto sendResponse = server->sendMessage(clientSocket, buffer);
    if (sendResponse == -1) {
        R::Utils::setThreadSafeToQueue(socketsToCloseQueue, socketsToCloseQueueMutex, clientSocket);
    }
}

std::string P2PServer::startNewLobby(R::Net::Socket clientSocket, Rp2p::LobbyPrivacyType lobbyPrivacyType, uint16_t clientPort) {
    Lobby lobby;
    lobby.ID_Lobby = generateNewUUID();
    lobby.peer1 = {clientSocket, clientPort, socketToIpAddressMap[clientSocket], R::Net::getRTTOfClient(clientSocket)};
    lobby.lobbyPrivacyType = lobbyPrivacyType;

    if (lobbyPrivacyType == Rp2p::LobbyPrivacyType::Public) {
        matchMakingQueue.push(lobby.ID_Lobby);
    }

    lobbiesMap[lobby.ID_Lobby] = lobby;
    return lobby.ID_Lobby;
};

std::string P2PServer::findRandomMatch(R::Net::Socket clientSocket, uint16_t clientPort) {
    std::string uuid = R::Utils::getFromQueue(matchMakingQueue);
    if (uuid.empty() || lobbiesMap[uuid].isLobbyComplete) {
        return startNewLobby(clientSocket, Rp2p::LobbyPrivacyType::Public, clientPort);
    }

    lobbiesMap[uuid].peer2 = {clientSocket, clientPort, socketToIpAddressMap[clientSocket], R::Net::getRTTOfClient(clientSocket)};
    lobbiesMap[uuid].isLobbyComplete = true;
    // lobbiesMap[uuid].Print();
    return uuid;
}

void P2PServer::joinPrivateMatch(R::Net::Socket clientSocket, std::string &uuid, uint16_t clientPort) {
    if (!R::Utils::keyExistsInMap(lobbiesMap, uuid)) {
        cleanUpLobbyBySocket(clientSocket);
        return;
    }

    if (lobbiesMap[uuid].isLobbyComplete) {
        cleanUpLobbyBySocket(clientSocket);
        return;
    }

    lobbiesMap[uuid].peer2 = {clientSocket, clientPort, socketToIpAddressMap[clientSocket], R::Net::getRTTOfClient(clientSocket)};
    lobbiesMap[uuid].isLobbyComplete = true;
}

std::string P2PServer::generateNewUUID() {
    std::string uuid = R::Utils::generateUUID(UUID_LENGTH);

    if (R::Utils::keyExistsInMap(lobbiesMap, uuid)) {
        return generateNewUUID();
    }

    return uuid;
}