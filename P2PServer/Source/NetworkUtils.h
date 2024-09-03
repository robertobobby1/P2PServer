#pragma once

#include "NetworkStructs.h"
#include "R.h"

namespace NetworkUtils {
    inline Peer getPeerInfo(R::Net::Socket clientSocket, uint16_t port) {
        struct sockaddr_in peeraddr;
        socklen_t peeraddrlen;

        auto retval = getpeername(clientSocket, (struct sockaddr*)&peeraddr, &peeraddrlen);

        Peer clientInfo;
        clientInfo.family = peeraddr.sin_family;
        clientInfo.ipAddress = peeraddr.sin_addr;
        clientInfo.port = port;
        clientInfo.socket = clientSocket;
        clientInfo.averageRTT = R::Net::getRTTOfClient(clientSocket);

        return clientInfo;
    };

}  // namespace NetworkUtils