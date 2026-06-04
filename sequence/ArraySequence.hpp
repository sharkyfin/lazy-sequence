#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "common/Exceptions.hpp"
#include "common/Buffer.hpp"
#include "sequence/Sequence.hpp"

template <class T>
class ArraySequence : public Sequence<T> {
private:
    Buffer<T> values;

    class ArraySequenceEnumerator : public IEnumerator<T> {
    private:
        const Buffer<T>& values;
        std::size_t position{};
        bool hasCurrent{};

    public:
        ArraySequenceEnumerator(const Buffer<T>& values)
            : values(values) {}

        bool MoveNext() override {
            if (position >= values.Size()) {
                hasCurrent = false;
                return false;
            }
            ++position;
            hasCurrent = true;
            return true;
        }

        const T& CurrentRef() const override {
            if (!hasCurrent) {
                throw InvalidOperation("Array sequence enumerator has no current value.");
            }
            return values[position - 1];
        }
    };

public:
    ArraySequence() = default;

    ArraySequence(const Buffer<T>& items)
        : values(items) {}

    ArraySequence(const Sequence<T>& sequence) {
        values.Reserve(sequence.GetCount());
        auto enumerator = sequence.GetEnumerator();
        while (enumerator->MoveNext()) {
            values.PushBack(enumerator->CurrentRef());
        }
    }

    T GetFirst() const override {
        if (values.Empty()) {
            throw IndexOutOfRange("Cannot get the first element of an empty array sequence.");
        }
        return values.Front();
    }

    T GetLast() const override {
        if (values.Empty()) {
            throw IndexOutOfRange("Cannot get the last element of an empty array sequence.");
        }
        return values.Back();
    }

    T Get(std::size_t index) const {
        if (index >= values.Size()) {
            throw IndexOutOfRange("Array sequence index " + std::to_string(index)
                                  + " is outside length " + std::to_string(values.Size()) + ".");
        }
        return values[index];
    }

    std::size_t GetLength() const {
        return values.Size();
    }

    std::size_t GetCount() const override {
        return values.Size();
    }

    std::unique_ptr<IEnumerator<T>> GetEnumerator() const override {
        return std::make_unique<ArraySequenceEnumerator>(values);
    }

    std::unique_ptr<Sequence<T>> Clone() const override {
        return std::make_unique<ArraySequence<T>>(*this);
    }

    const Buffer<T>& Values() const {
        return values;
    }
};
