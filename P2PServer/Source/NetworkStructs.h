#pragma once

#include "R.h"

struct Peer {
    R::Net::Socket socket;
    // always 4 bytes B1.B2.B3.B4 it is already in network order!
    in_addr ipAddress;
    uint16_t port;
    uint8_t family;
    uint32_t averageRTT;

    void print() {
        RLog("Peer information:\n");
        RLog("Peer socket: %i\n", socket);
        RLog("Peer Address Family: %d\n", this->family);
        RLog("Peer Port: %d\n", this->port);
        RLog("Peer IP Address: %s\n", inet_ntoa(this->ipAddress));
        RLog("Average RTT: %i\n", this->averageRTT);
    }
};

struct Lobby {
    // private to protect with mutex
   private:
    Peer peer2;
    bool isLobbyComplete = false;

   public:
    std::string ID_Lobby;
    Peer peer1;
    R::Net::P2P::LobbyPrivacyType lobbyPrivacyType;
    bool peerConnectionSendFailure = false;

    bool IsLobbyComplete(std::mutex* mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        return isLobbyComplete;
    };

    bool SetPeer2IfPossible(Peer peer, std::mutex* mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        if (!isLobbyComplete) {
            peer2 = peer;
            isLobbyComplete = true;
            return true;
        }

        return false;
    }

    Peer GetPeer2(std::mutex* mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        return peer2;
    };

    void Print() {
        RLog("\nStart lobby info ---- %s\n\n", this->ID_Lobby.c_str());
        RLog("Peer 1:\n");
        this->peer1.print();
        RLog("\nPeer 2:\n");
        this->peer2.print();
        RLog("\nEnd lobby info   ---- %s\n\n", this->ID_Lobby.c_str());
    }
};