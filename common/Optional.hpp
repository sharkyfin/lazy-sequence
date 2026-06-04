#pragma once

#include "common/Exceptions.hpp"

struct NullOptType {
    constexpr NullOptType() {}
};

constexpr NullOptType NullOpt{};

template <class T>
class Optional {
private:
    bool present;
    T stored;

public:
    Optional()
        : present(false), stored() {}

    Optional(NullOptType)
        : present(false), stored() {}

    Optional(const T& value)
        : present(true), stored(value) {}

    Optional<T>& operator=(NullOptType) {
        present = false;
        return *this;
    }

    Optional<T>& operator=(const T& value) {
        stored = value;
        present = true;
        return *this;
    }

    bool has_value() const {
        return present;
    }

    explicit operator bool() const {
        return present;
    }

    T& Value() {
        if (!present) {
            throw InvalidOperation("Optional has no value.");
        }
        return stored;
    }

    const T& Value() const {
        if (!present) {
            throw InvalidOperation("Optional has no value.");
        }
        return stored;
    }

    T& operator*() {
        return Value();
    }

    const T& operator*() const {
        return Value();
    }

    T* operator->() {
        return &Value();
    }

    const T* operator->() const {
        return &Value();
    }

    void Reset() {
        present = false;
    }
};
