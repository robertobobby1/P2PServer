#pragma once

#include "R.h"
#include "NetworkStructs.h"

namespace LobbyManager {
    class Lobby {
       public:
        std::string ID_Lobby;

        Peer peer2;
        Peer peer1;

        R::Net::P2P::LobbyPrivacyType lobbyPrivacyType;
        bool isLobbyComplete = false;
        bool isMarkedForCleanup = false;

        void Print() {
            RLog("\nStart lobby info ---- %s\n", this->ID_Lobby.c_str());
            this->peer1.print();
            this->peer2.print();
            RLog("End lobby info   ---- %s\n\n", this->ID_Lobby.c_str());
        }
    };
}  // namespace LobbyManager