#pragma once

#include "common/Exceptions.hpp"
#include "common/Optional.hpp"
#include "lazy/LazySequence.hpp"
#include "stream/ReadStream.hpp"

template <class T>
class LazyReader : public ReadStream<T> {
private:
    LazySequence<T> sequence;
    bool isOpen{};
    Ordinal position = Ordinal::Finite(0);
    mutable bool reachedEnd{};
    mutable Optional<T> peeked;

    void EnsureOpen() const {
        if (!isOpen) {
            throw InvalidOperation("Lazy read stream is not open.");
        }
    }

    T ConsumeNext() {
        const T value = peeked.has_value() ? *peeked : sequence.Get(position);
        peeked.Reset();
        position = position.Successor();
        return value;
    }

public:
    LazyReader(LazySequence<T> sequence)
        : sequence(sequence) {}

    void Open() override {
        isOpen = true;
        position = Ordinal::Finite(0);
        reachedEnd = false;
        peeked.Reset();
    }

    void Close() override {
        isOpen = false;
        reachedEnd = false;
        peeked.Reset();
    }

    bool IsOpen() const override {
        return isOpen;
    }

    bool IsEndOfStream() const override {
        EnsureOpen();
        if (sequence.GetLength().IsKnown()) {
            return position >= sequence.GetLength().Value();
        }
        if (reachedEnd) {
            return true;
        }
        if (peeked.has_value()) {
            return false;
        }
        try {
            peeked = sequence.Get(position);
            return false;
        } catch (const IndexOutOfRange&) {
            reachedEnd = true;
            return true;
        }
    }

    T Read() override {
        EnsureOpen();
        if (IsEndOfStream()) {
            throw EndOfStream("Lazy read stream reached the end.");
        }
        return ConsumeNext();
    }

    Ordinal GetPosition() const {
        return position;
    }

    Ordinal Seek(const Ordinal& index) {
        EnsureOpen();
        if (sequence.GetLength().IsKnown() && index > sequence.GetLength().Value()) {
            throw IndexOutOfRange("Seek index is outside the lazy stream bounds.");
        }
        position = index;
        reachedEnd = false;
        peeked.Reset();
        return position;
    }

    Ordinal Seek(std::size_t index) {
        return Seek(Ordinal::Finite(index));
    }

    Ordinal GoBack() {
        EnsureOpen();
        const auto previous = position.Predecessor();
        if (!previous.has_value()) {
            throw IndexOutOfRange("Lazy read stream cannot go back before its first position.");
        }
        position = *previous;
        reachedEnd = false;
        peeked.Reset();
        return position;
    }
};
