#pragma once

#include "R.h"

struct Peer {
   public:
    R::Net::Socket socket;
    // always 4 bytes B1.B2.B3.B4 it is already in network order!
    in_addr ipAddress;
    uint16_t port;
    uint8_t family;
    uint32_t averageRTT;

    void print() {
        RLog("\nStart peer info ---- %i\n\n", socket);
        RLog("Peer Port: %d\n", this->port);
        RLog("Peer IP Address: %s\n", inet_ntoa(this->ipAddress));
        RLog("Average RTT: %i\n", this->averageRTT);
        RLog("\nEnd peer info   ---- %i\n\n", socket);
    }
};

struct Lobby {
   public:
    std::string ID_Lobby;

    Peer peer2;
    Peer peer1;

    R::Net::P2P::LobbyPrivacyType lobbyPrivacyType;
    bool isLobbyComplete = false;
    bool isMarkedForCleanup = false;
    bool peerConnectionSendFailure = false;

    std::mutex* mutex = new std::mutex();

    void Print() {
        RLog("\nStart lobby info ---- %s\n", this->ID_Lobby.c_str());
        this->peer1.print();
        this->peer2.print();
        RLog("End lobby info   ---- %s\n\n", this->ID_Lobby.c_str());
    }
};