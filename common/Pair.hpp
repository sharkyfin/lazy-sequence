#pragma once

template <class First, class Second>
struct Pair {
    First first;
    Second second;

    Pair()
        : first(),
          second() {}

    Pair(const First& firstValue, const Second& secondValue)
        : first(firstValue),
          second(secondValue) {}

    bool operator==(const Pair<First, Second>& other) const {
        return first == other.first && second == other.second;
    }

    bool operator!=(const Pair<First, Second>& other) const {
        return !(*this == other);
    }
};
