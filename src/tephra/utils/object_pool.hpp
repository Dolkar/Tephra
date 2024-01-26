#pragma once

#include <tephra/common.hpp>
#include <vector>
#include <deque>

namespace tp {

template <typename T, typename = int>
struct HasClearMethod : std::false_type {};

template <typename T>
struct HasClearMethod<T, decltype(&T::clear, 0)> : std::true_type {};

template <typename T>
class ObjectPool {
public:
    ObjectPool() = default;

    T* acquireExisting() {
        if (!freeList.empty()) {
            T* objPtr = freeList.back();
            freeList.pop_back();
            return objPtr;
        } else {
            return nullptr;
        }
    }

    template <typename... Args>
    T* acquireNew(Args&&... args) {
        pool.emplace_back(std::forward<Args>(args)...);
        return &(pool.back());
    }

    void release(T* objPtr) {
        if constexpr (HasClearMethod<T>::value) { // If objPtr->clear() exists, call it
            objPtr->clear();
        }

        freeList.push_back(objPtr);
    }

    size_t objectsAllocated() const {
        return pool.size();
    }

    std::deque<T>& getAllocatedObjects() {
        return pool;
    }

    const std::deque<T>& getAllocatedObjects() const {
        return pool;
    }

    size_t objectsInUse() const {
        return pool.size() - freeList.size();
    }

    void clear() {
        pool.clear();
        freeList.clear();
    }

    TEPHRA_MAKE_NONCOPYABLE(ObjectPool);
    TEPHRA_MAKE_NONMOVABLE(ObjectPool);

private:
    std::deque<T> pool;
    std::vector<T*> freeList;
};

}
