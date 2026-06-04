#pragma once

#include <cstddef>
#include <functional>

#include "common/Exceptions.hpp"
#include "stream/ReadStream.hpp"

template <class T>
class GeneratedReader : public ReadStream<T> {
private:
    std::size_t count;
    std::function<T(std::size_t)> valueAt;
    bool isOpen{};
    std::size_t position{};

    void EnsureOpen() const {
        if (!isOpen) {
            throw InvalidOperation("Generated read stream is not open.");
        }
    }

public:
    GeneratedReader(std::size_t countValue, std::function<T(std::size_t)> valueAtValue)
        : count(countValue),
          valueAt(valueAtValue) {
        if (!valueAt) {
            throw InvalidOperation("Generated read stream requires a generation rule.");
        }
    }

    void Open() override {
        isOpen = true;
        position = 0;
    }

    void Close() override {
        isOpen = false;
    }

    bool IsOpen() const override {
        return isOpen;
    }

    bool IsEndOfStream() const override {
        EnsureOpen();
        return position >= count;
    }

    T Read() override {
        EnsureOpen();
        if (IsEndOfStream()) {
            throw EndOfStream("Generated read stream reached the end.");
        }
        const T value = valueAt(position);
        ++position;
        return value;
    }

    std::size_t GetPosition() const {
        return position;
    }
};
