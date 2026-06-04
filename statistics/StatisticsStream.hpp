#pragma once

#include "common/Exceptions.hpp"
#include "statistics/OnlineStatistics.hpp"
#include "stream/ReadStream.hpp"

template <class T>
class StatisticsStream : public ReadStream<StatisticsSnapshot<T>> {
private:
    ReadStream<T>& source;
    OnlineStatistics<T> statistics;
    bool isOpen{};

    void EnsureOpen() const {
        if (!isOpen) {
            throw InvalidOperation("Statistics stream is not open.");
        }
    }

public:
    StatisticsStream(ReadStream<T>& sourceValue)
        : source(sourceValue) {}

    void Open() override {
        statistics.Reset();
        source.Open();
        isOpen = true;
    }

    void Close() override {
        if (source.IsOpen()) {
            source.Close();
        }
        isOpen = false;
    }

    bool IsOpen() const override {
        return isOpen;
    }

    bool IsEndOfStream() const override {
        EnsureOpen();
        return source.IsEndOfStream();
    }

    StatisticsSnapshot<T> Read() override {
        EnsureOpen();
        if (IsEndOfStream()) {
            throw EndOfStream("Statistics stream reached the end.");
        }

        statistics.Add(source.Read());
        return statistics.GetSnapshot();
    }

    const OnlineStatistics<T>& GetStatistics() const {
        return statistics;
    }
};
