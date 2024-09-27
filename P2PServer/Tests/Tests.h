#pragma once

#include <cstdio>
#include <iostream>
#include <functional>

namespace Tests {

    void run();

    std::pair<std::string, bool> QueuesMutualExclusion();
    std::pair<std::string, bool> QueuesMutualExclusionWithCondition();

}  // namespace Tests