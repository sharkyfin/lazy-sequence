#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "common/Buffer.hpp"
#include "common/Exceptions.hpp"
#include "common/Optional.hpp"

template <class T>
struct StatisticsSnapshot {
    std::size_t count{};
    T minimum{};
    T maximum{};
    long double mean{};
    long double populationVariance{};
    Optional<long double> sampleVariance;
    long double populationStandardDeviation{};
    Optional<long double> sampleStandardDeviation;
    long double median{};
};

template <class T>
class OnlineStatistics {
private:
    std::size_t count{};
    T minimum{};
    T maximum{};
    long double mean{};
    long double secondMoment{};
    Buffer<long double> sortedValues;

    static long double AsFiniteNumber(const T& value) {
        const long double number = static_cast<long double>(value);
        if (!std::isfinite(number)) {
            throw InvalidOperation("Online statistics cannot process a non-finite value.");
        }
        return number;
    }

    void AddToSortedValues(long double value) {
        std::size_t position = 0;
        while (position < sortedValues.Size() && sortedValues[position] < value) {
            ++position;
        }
        sortedValues.Insert(position, value);
    }

    long double Median() const {
        const std::size_t middle = sortedValues.Size() / 2;
        if (sortedValues.Size() % 2 == 0) {
            return sortedValues[middle - 1] / 2.0L + sortedValues[middle] / 2.0L;
        }
        return sortedValues[middle];
    }

public:
    void Add(const T& value) {
        const long double number = AsFiniteNumber(value);
        if (count == std::numeric_limits<std::size_t>::max()) {
            throw InvalidOperation("Online statistics element count overflow.");
        }

        const std::size_t nextCount = count + 1;
        const long double delta = number - mean;
        const long double nextMean = mean + delta / static_cast<long double>(nextCount);
        const long double deltaAfterUpdate = number - nextMean;
        const long double nextSecondMoment = secondMoment + delta * deltaAfterUpdate;
        if (!std::isfinite(nextMean) || !std::isfinite(nextSecondMoment)) {
            throw InvalidOperation("Online statistics arithmetic overflow.");
        }

        if (count == 0) {
            minimum = value;
            maximum = value;
        } else {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }

        count = nextCount;
        mean = nextMean;
        secondMoment = nextSecondMoment;
        AddToSortedValues(number);
    }

    void Reset() {
        count = 0;
        minimum = T{};
        maximum = T{};
        mean = 0.0L;
        secondMoment = 0.0L;
        sortedValues.Clear();
    }

    std::size_t GetCount() const {
        return count;
    }

    StatisticsSnapshot<T> GetSnapshot() const {
        if (count == 0) {
            throw InvalidOperation("Online statistics has no values.");
        }

        StatisticsSnapshot<T> snapshot;
        snapshot.count = count;
        snapshot.minimum = minimum;
        snapshot.maximum = maximum;
        snapshot.mean = mean;
        snapshot.populationVariance = std::max(0.0L, secondMoment / static_cast<long double>(count));
        snapshot.populationStandardDeviation = std::sqrt(snapshot.populationVariance);
        snapshot.median = Median();

        if (count > 1) {
            snapshot.sampleVariance = std::max(0.0L, secondMoment / static_cast<long double>(count - 1));
            snapshot.sampleStandardDeviation = std::sqrt(*snapshot.sampleVariance);
        }
        return snapshot;
    }
};
