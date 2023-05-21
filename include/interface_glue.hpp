#pragma once

// Use this file to define various types and conversion functions for a more comfortable
// usage of Tephra across different standards.

#include <tephra/tools/array.hpp>
#include <memory>

namespace tp {

/// A customizable base class that is used for objects returned by the API in an OwningPtr.
class Ownable {};

/// Owning pointer is returned by the API when the ownership of the object is being transferred
/// over to the user. It needs to be constructible by passing T* as a parameter. Upon destruction
/// std::default_delete or equivalent should be called.
template <typename T>
using OwningPtr = std::unique_ptr<T>;
// using OwningPtr = std::shared_ptr<T>;

/// Returns the owned pointer without releasing ownership
template <typename T>
const T* getOwnedPtr(const OwningPtr<T>& owningPtr) {
    return owningPtr.get();
}

/// Returns the owned pointer without releasing ownership
template <typename T>
T* getOwnedPtr(OwningPtr<T>& owningPtr) {
    return owningPtr.get();
}

// Include here additional overloads for tp::view() and tp::viewRange() for any contiguous
// array classes that will be used with Tephra

}
