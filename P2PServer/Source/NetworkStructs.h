#pragma once

#include "R.h"

// Client-Server data flags
enum ClientServerHeaderFlags {
    // Type of the lobby public/private
    ClientServerHeaderFlags_Public = 1 << 5,  // 00100000
    // Type of the action we are trying create/connect/disconnect/peersConnectSuccess
    ClientServerHeaderFlags_Bit1 = 1 << 7,  // 10000000
    ClientServerHeaderFlags_Bit2 = 1 << 6,  // 01000000
};

// Server-Client data flags
enum ServerClientHeaderFlags {
    // Action of the request
    ServerClientHeaderFlags_Action = 1 << 7,  // 10000000
};

enum LobbyPrivacyType {
    Private,
    Public
};

enum ActionType {
    Create,
    Connect,
    Disconnect,
    PeerConnectSuccess
};

struct Peer {
    R::Net::Socket socket;
    // always 4 bytes B1.B2.B3.B4 it is already in network order!
    in_addr ipAddress;
    uint16_t port;
    uint8_t family;
    uint32_t averageRTT;

    void print() {
        printf("Peer information:\n");
        printf("Peer Address Family: %d\n", this->family);
        printf("Peer Port: %d\n", this->port);
        printf("Peer IP Address: %s\n", inet_ntoa(this->ipAddress));
        printf("Average RTT: %i\n", this->averageRTT);
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
    LobbyPrivacyType lobbyPrivacyType;
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
        printf("\nStart lobby info ---- %s\n\n", this->ID_Lobby.c_str());
        printf("Peer 1:\n");
        this->peer1.print();
        printf("\nPeer 2:\n");
        this->peer2.print();
        printf("\nEnd lobby info   ---- %s\n\n", this->ID_Lobby.c_str());
    }
};