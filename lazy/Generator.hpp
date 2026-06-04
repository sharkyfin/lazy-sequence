#pragma once

#include "common/Exceptions.hpp"
#include "common/Optional.hpp"
#include "lazy/LazySequence.hpp"

template <class T>
class Generator {
private:
    LazySequence<T> sequence;
    Ordinal position;
    bool reachedEnd = false;

    T ConsumeNext() {
        const T value = sequence.Get(position);
        position = position.Successor();
        return value;
    }

public:
    Generator(LazySequence<T> sequence)
        : sequence(sequence),
          position(Ordinal::Finite(0)) {}

    T GetNext() {
        if (!HasNext()) {
            throw EndOfStream("Generator reached the end of the sequence.");
        }
        return ConsumeNext();
    }

    Optional<T> TryGetNext() {
        if (!HasNext()) {
            return NullOpt;
        }
        return ConsumeNext();
    }

    bool HasNext() {
        if (sequence.GetLength().IsKnown()) {
            return position < sequence.GetLength().Value();
        }
        if (reachedEnd) {
            return false;
        }
        try {
            (void)sequence.Get(position);
            return true;
        } catch (const IndexOutOfRange&) {
            reachedEnd = true;
            return false;
        }
    }

    Ordinal GetPosition() const {
        return position;
    }

};
