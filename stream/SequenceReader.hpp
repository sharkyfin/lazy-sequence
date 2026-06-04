#pragma once

#include <cstddef>

#include "common/Exceptions.hpp"
#include "sequence/ArraySequence.hpp"
#include "stream/ReadStream.hpp"

template <class T>
class SequenceReader : public ReadStream<T> {
private:
    ArraySequence<T> sequence;
    bool isOpen{};
    std::size_t position{};

    void EnsureOpen() const {
        if (!isOpen) {
            throw InvalidOperation("Sequence read stream is not open.");
        }
    }

public:
    SequenceReader(const Buffer<T>& values)
        : sequence(values) {}

    SequenceReader(const ArraySequence<T>& sequenceValue)
        : sequence(sequenceValue) {}

    SequenceReader(const Sequence<T>& sequenceValue)
        : sequence(sequenceValue) {}

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
        return position >= sequence.GetLength();
    }

    T Read() override {
        EnsureOpen();
        if (IsEndOfStream()) {
            throw EndOfStream("Sequence read stream reached the end.");
        }
        const T value = sequence.Get(position);
        ++position;
        return value;
    }

    std::size_t GetPosition() const {
        return position;
    }

    std::size_t Seek(std::size_t index) {
        EnsureOpen();
        if (index > sequence.GetLength()) {
            throw IndexOutOfRange("Seek index is outside the sequence stream bounds.");
        }
        position = index;
        return position;
    }

    std::size_t GoBack() {
        EnsureOpen();
        if (position == 0) {
            throw IndexOutOfRange("Sequence read stream cannot go back before its first position.");
        }
        --position;
        return position;
    }

};
