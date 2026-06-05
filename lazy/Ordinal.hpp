#pragma once

#include <cstddef>
#include <string>

#include "common/Exceptions.hpp"
#include "common/Optional.hpp"

class Ordinal {
private:
    std::size_t omegaBlocks;
    std::size_t finiteOffset;

    static std::size_t CheckedAdd(std::size_t left, std::size_t right) {
        if (right > static_cast<std::size_t>(-1) - left) {
            throw InvalidOperation("Ordinal arithmetic overflow.");
        }
        return left + right;
    }

public:
    Ordinal()
        : omegaBlocks(0), finiteOffset(0) {}

    Ordinal(std::size_t omegaBlocksValue, std::size_t finiteOffsetValue)
        : omegaBlocks(omegaBlocksValue), finiteOffset(finiteOffsetValue) {}

    static Ordinal Finite(std::size_t value) {
        return Ordinal(0, value);
    }

    static Ordinal Omega() {
        return Ordinal(1, 0);
    }

    bool IsFinite() const {
        return omegaBlocks == 0;
    }

    bool IsZero() const {
        return omegaBlocks == 0 && finiteOffset == 0;
    }

    bool IsLimit() const {
        return omegaBlocks > 0 && finiteOffset == 0;
    }

    std::size_t Value() const {
        if (!IsFinite()) {
            throw InvalidOperation("Cannot get finite value of a transfinite ordinal.");
        }
        return finiteOffset;
    }

    Ordinal Successor() const {
        return Ordinal(omegaBlocks, CheckedAdd(finiteOffset, 1));
    }

    Optional<Ordinal> Predecessor() const {
        if (finiteOffset == 0) {
            return NullOpt;
        }
        return Ordinal(omegaBlocks, finiteOffset - 1);
    }

    Ordinal Add(const Ordinal& other) const {
        if (other.omegaBlocks == 0) {
            return Ordinal(omegaBlocks, CheckedAdd(finiteOffset, other.finiteOffset));
        }
        return Ordinal(CheckedAdd(omegaBlocks, other.omegaBlocks), other.finiteOffset);
    }

    Ordinal SubtractPrefix(const Ordinal& prefix) const {
        if (*this < prefix) {
            throw IndexOutOfRange("Ordinal is smaller than the prefix.");
        }
        if (omegaBlocks == prefix.omegaBlocks) {
            return Ordinal::Finite(finiteOffset - prefix.finiteOffset);
        }
        return Ordinal(omegaBlocks - prefix.omegaBlocks, finiteOffset);
    }

    std::string ToString() const {
        if (omegaBlocks == 0) {
            return std::to_string(finiteOffset);
        }
        std::string result = "omega";
        if (omegaBlocks > 1) {
            result += "*" + std::to_string(omegaBlocks);
        }
        if (finiteOffset > 0) {
            result += "+" + std::to_string(finiteOffset);
        }
        return result;
    }

    bool operator==(const Ordinal& other) const {
        return omegaBlocks == other.omegaBlocks && finiteOffset == other.finiteOffset;
    }

    bool operator!=(const Ordinal& other) const {
        return !(*this == other);
    }

    bool operator<(const Ordinal& other) const {
        if (omegaBlocks != other.omegaBlocks) {
            return omegaBlocks < other.omegaBlocks;
        }
        return finiteOffset < other.finiteOffset;
    }

    bool operator<=(const Ordinal& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const Ordinal& other) const {
        return other < *this;
    }

    bool operator>=(const Ordinal& other) const {
        return other <= *this;
    }

    bool operator==(std::size_t value) const {
        return IsFinite() && finiteOffset == value;
    }
};

inline bool operator==(std::size_t value, const Ordinal& ordinal) {
    return ordinal == value;
}

inline Ordinal MinOrdinal(const Ordinal& left, const Ordinal& right) {
    return left < right ? left : right;
}
