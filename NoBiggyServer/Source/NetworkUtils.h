#pragma once

#include "NetworkStructs.h"

namespace NetworkUtils {
    Peer getPeerInfo(NoBiggySocket clientSocket);
    uint32_t getRTTOfClient(NoBiggySocket clientSocket);

}  // namespace NetworkUtils