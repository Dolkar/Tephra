#pragma once

#include "error_reporting.hpp"
#include "debugging.hpp"
#include "utils/math.hpp"
#include "utils/scratch_allocator.hpp"
#include <cstring>

#ifdef _MSC_VER
    #include <shared_mutex>
#else
    #include <mutex>
#endif

namespace tp {

// Shared mutex is smaller and more efficient on msvc at this point:
// https://developercommunity.visualstudio.com/t/unique-lock-of-stdmutex-performs-much-worse-than-u/1168461
#ifdef _MSC_VER
using Mutex = std::shared_mutex;
#else
using Mutex = std::mutex;
#endif

inline bool containsString(ArrayParameter<const char* const> list, const char* string) {
    for (const char* entry : list) {
        if (strcmp(string, entry) == 0)
            return true;
    }
    return false;
}

}
