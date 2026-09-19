#pragma once

#include <vector>

// 3 LRU ARC LFU
// policies[0] - L1
// policies[1] - L2
// policies[2] - L3



namespace caches 
{
    enum class Policy {
        LRU,
        ARC,
        TwoQ,
        LFU,
        LIRS
    };

    struct Config {
        std::vector<Policy> policies;
    };
} 
