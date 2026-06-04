#pragma once

#include "common/Exceptions.hpp"
#include "lazy/Ordinal.hpp"

class Length {
private:
    bool known;
    Ordinal ordinal;

    Length(bool knownValue, Ordinal ordinalValue)
        : known(knownValue), ordinal(ordinalValue) {}

public:
    static Length Known(const Ordinal& value) {
        return Length(true, value);
    }

    static Length Unknown() {
        return Length(false, Ordinal::Finite(0));
    }

    bool IsKnown() const {
        return known;
    }

    const Ordinal& Value() const {
        if (!known) {
            throw InvalidOperation("Cannot get value of an unknown sequence length.");
        }
        return ordinal;
    }
};
