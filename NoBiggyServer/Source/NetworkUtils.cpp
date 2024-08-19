#include "NetworkUtils.h"

#if defined(PLATFORM_MACOS)

uint32_t NetworkUtils::getRTTOfClient(NoBiggySocket clientSocket) {
    struct tcp_connection_info info;
    socklen_t info_len = sizeof(info);

    int result = getsockopt(clientSocket, IPPROTO_TCP, TCP_CONNECTION_INFO, &info, &info_len);

    return info.tcpi_srtt;
}

#elif defined(PLATFORM_LINUX)

uint32_t NetworkUtils::getRTTOfClient(NoBiggySocket clientSocket) {
    struct tcp_info info;
    socklen_t info_len = sizeof(info);

    int result = getsockopt(clientSocket, IPPROTO_TCP, TCP_INFO, &info, &info_len);

    return info.tcpi_srtt;
}

#elif defined(PLATFORM_WINDOWS)

uint32_t NetworkUtils::getRTTOfClient(NoBiggySocket clientSocket) {
    // TODO how to get RTT in windows
}

#endif

Peer NetworkUtils::getPeerInfo(NoBiggySocket clientSocket) {
    struct sockaddr_in peeraddr;
    socklen_t peeraddrlen;
    auto retval = getpeername(clientSocket, (struct sockaddr*)&peeraddr, &peeraddrlen);

    Peer clientInfo;
    clientInfo.family = peeraddr.sin_family;
    clientInfo.ipAddress = peeraddr.sin_addr;
    clientInfo.port = ntohs(peeraddr.sin_port);
    clientInfo.socket = clientSocket;
    clientInfo.averageRTT = getRTTOfClient(clientSocket);

    return clientInfo;
}
