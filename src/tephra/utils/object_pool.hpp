#pragma once

#include <tephra/common.hpp>
#include <vector>
#include <deque>

namespace tp {

template <typename T>
struct CallClearIfPresent { // Based on https://stackoverflow.com/a/12069785/2044117
    template <typename A>
    static std::true_type test(decltype(&A::clear)) {
        return std::true_type();
    }

    template <typename A>
    static std::false_type test(...) {
        return std::false_type();
    }

    static void eval(T& obj, std::true_type) {
        obj.clear();
    }

    static void eval(...) {}

    static void eval(T& obj) {
        using type = decltype(test<T>(nullptr));
        eval(obj, type());
    }
};

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
        CallClearIfPresent<T>::eval(*objPtr); // If objPtr->clear() exists, call it

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
