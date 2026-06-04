#pragma once

#include "common/Exceptions.hpp"
#include "common/Buffer.hpp"
#include "stream/WriteStream.hpp"

template <class T>
class BufferWriter : public WriteStream<T> {
private:
    Buffer<T> values;
    bool isOpen{};

    void EnsureOpen() const {
        if (!isOpen) {
            throw InvalidOperation("Buffer write stream is not open.");
        }
    }

public:
    void Open() override {
        isOpen = true;
    }

    void Close() override {
        isOpen = false;
    }

    bool IsOpen() const override {
        return isOpen;
    }

    std::size_t Write(const T& item) override {
        EnsureOpen();
        values.PushBack(item);
        return values.Size();
    }

    std::size_t GetPosition() const override {
        return values.Size();
    }

    const Buffer<T>& Values() const {
        return values;
    }
};
