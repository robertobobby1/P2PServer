#pragma once

namespace Tests {

    void run();

    std::pair<std::string, bool> Peer2MutualExclusion();
    std::pair<std::string, bool> QueuesMutualExclusion();
    std::pair<std::string, bool> QueuesMutualExclusionWithCondition();

}  // namespace Tests