#pragma once

#include <cstddef>
#include <memory>

#include "common/Exceptions.hpp"

template <class T>
class Buffer {
private:
    std::unique_ptr<T[]> data;
    std::size_t count;
    std::size_t capacity;

    void CheckIndex(std::size_t index) const {
        if (index >= count) {
            throw IndexOutOfRange("Buffer index is outside bounds.");
        }
    }

    void EnsureCapacity(std::size_t required) {
        if (required <= capacity) {
            return;
        }
        std::size_t nextCapacity = capacity == 0 ? 1 : capacity;
        while (nextCapacity < required) {
            nextCapacity *= 2;
        }
        Reallocate(nextCapacity);
    }

    void Reallocate(std::size_t newCapacity) {
        auto next = std::make_unique<T[]>(newCapacity);
        for (std::size_t index = 0; index < count; ++index) {
            next[index] = data[index];
        }
        data.swap(next);
        capacity = newCapacity;
    }

public:
    Buffer()
        : data(nullptr), count(0), capacity(0) {}

    explicit Buffer(std::size_t initialSize)
        : data(initialSize == 0 ? nullptr : std::make_unique<T[]>(initialSize)),
          count(initialSize),
          capacity(initialSize) {}

    Buffer(std::size_t initialSize, const T& value)
        : Buffer(initialSize) {
        for (std::size_t index = 0; index < count; ++index) {
            data[index] = value;
        }
    }

    Buffer(const Buffer& other)
        : Buffer(other.count) {
        for (std::size_t index = 0; index < count; ++index) {
            data[index] = other.data[index];
        }
    }

    Buffer& operator=(const Buffer& other) {
        if (this == &other) {
            return *this;
        }
        Buffer copy(other);
        Swap(copy);
        return *this;
    }

    std::size_t Size() const {
        return count;
    }

    std::size_t size() const {
        return Size();
    }

    std::size_t Capacity() const {
        return capacity;
    }

    bool Empty() const {
        return count == 0;
    }

    bool empty() const {
        return Empty();
    }

    const T& Get(std::size_t index) const {
        CheckIndex(index);
        return data[index];
    }

    T& Get(std::size_t index) {
        CheckIndex(index);
        return data[index];
    }

    const T& operator[](std::size_t index) const {
        return Get(index);
    }

    T& operator[](std::size_t index) {
        return Get(index);
    }

    const T& Front() const {
        if (Empty()) {
            throw IndexOutOfRange("Buffer is empty.");
        }
        return data[0];
    }

    T& Front() {
        if (Empty()) {
            throw IndexOutOfRange("Buffer is empty.");
        }
        return data[0];
    }

    const T& front() const {
        return Front();
    }

    T& front() {
        return Front();
    }

    const T& Back() const {
        if (Empty()) {
            throw IndexOutOfRange("Buffer is empty.");
        }
        return data[count - 1];
    }

    T& Back() {
        if (Empty()) {
            throw IndexOutOfRange("Buffer is empty.");
        }
        return data[count - 1];
    }

    const T& back() const {
        return Back();
    }

    T& back() {
        return Back();
    }

    void PushBack(const T& item) {
        EnsureCapacity(count + 1);
        data[count++] = item;
    }

    void push_back(const T& item) {
        PushBack(item);
    }

    void Reserve(std::size_t requestedCapacity) {
        if (requestedCapacity > capacity) {
            Reallocate(requestedCapacity);
        }
    }

    void reserve(std::size_t requestedCapacity) {
        Reserve(requestedCapacity);
    }

    void Resize(std::size_t newSize) {
        EnsureCapacity(newSize);
        count = newSize;
    }

    void resize(std::size_t newSize) {
        Resize(newSize);
    }

    void Clear() {
        count = 0;
    }

    void clear() {
        Clear();
    }

    void Insert(std::size_t index, const T& item) {
        if (index > count) {
            throw IndexOutOfRange("Buffer insert index is outside bounds.");
        }
        EnsureCapacity(count + 1);
        for (std::size_t current = count; current > index; --current) {
            data[current] = data[current - 1];
        }
        data[index] = item;
        ++count;
    }

    void Erase(std::size_t index) {
        CheckIndex(index);
        for (std::size_t current = index; current + 1 < count; ++current) {
            data[current] = data[current + 1];
        }
        --count;
    }

    void erase(std::size_t index) {
        Erase(index);
    }

    void Swap(Buffer& other) noexcept {
        data.swap(other.data);

        std::size_t countCopy = count;
        count = other.count;
        other.count = countCopy;

        std::size_t capacityCopy = capacity;
        capacity = other.capacity;
        other.capacity = capacityCopy;
    }

    T* begin() {
        return data.get();
    }

    T* end() {
        return data.get() + count;
    }

    const T* begin() const {
        return data.get();
    }

    const T* end() const {
        return data.get() + count;
    }
};
