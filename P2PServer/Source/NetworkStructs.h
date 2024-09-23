#pragma once

#include "R.h"

struct Peer {
   public:
    R::Net::Socket socket;
    uint16_t port;
    // always 4 bytes B1.B2.B3.B4 it is already in network order!
    in_addr ipAddress;
    uint32_t averageRTT;

    void print() {
        char ipBuffer[INET_ADDRSTRLEN]{0};

        RLog("\nStart peer info ---- %i\n\n", socket);
        RLog("Peer Port: %i\n", this->port);

        RLog("Peer IP Address: %s\n", inet_ntop(AF_INET, &this->ipAddress, ipBuffer, INET_ADDRSTRLEN));

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

    void Print() {
        RLog("\nStart lobby info ---- %s\n", this->ID_Lobby.c_str());
        this->peer1.print();
        this->peer2.print();
        RLog("End lobby info   ---- %s\n\n", this->ID_Lobby.c_str());
    }
};