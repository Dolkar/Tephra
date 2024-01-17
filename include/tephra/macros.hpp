#pragma once

#define TEPHRA_MAKE_NONCOPYABLE(className) \
    className(const className&) noexcept = delete; \
    className& operator=(const className&) noexcept = delete

#define TEPHRA_MAKE_COPYABLE_DEFAULT(className) \
    className(const className&) noexcept = default; \
    className& operator=(const className&) noexcept = default

#define TEPHRA_MAKE_NONMOVABLE(className) \
    className(className&&) noexcept = delete; \
    className& operator=(className&&) noexcept = delete

#define TEPHRA_MAKE_MOVABLE_DEFAULT(className) \
    className(className&&) noexcept = default; \
    className& operator=(className&&) noexcept = default

#define TEPHRA_MAKE_MOVABLE(className) \
    className(className&&) noexcept; \
    className& operator=(className&&) noexcept

#define TEPHRA_MAKE_INTERFACE(className) \
    TEPHRA_MAKE_NONCOPYABLE(className); \
    TEPHRA_MAKE_NONMOVABLE(className); \
    virtual ~className() {}
