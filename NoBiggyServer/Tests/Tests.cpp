#include <cstdio>
#include <functional>

#include "Tests.h"
#include "Server.h"

const std::function<std::pair<std::string, bool>()> Tests_functionArray[] = {
    Tests::Peer2MutualExclusion,
    Tests::QueuesMutualExclusion,
    Tests::QueuesMutualExclusionWithCondition
};

void Tests::run() {
    std::string testResult = "";
    for (auto& function : Tests_functionArray) {
        auto response = function();
        std::cout << "Finished running test " << response.first.c_str() << "\n\n";

        testResult = testResult
                         .append("[")
                         .append(response.first.c_str())
                         .append("] ")
                         .append((response.second) ? "success" : "failure")
                         .append("\n");
    }

    std::cout << testResult;
}
