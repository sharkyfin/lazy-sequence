#pragma once

#include <cstddef>
#include <fstream>
#include <string>

#include "common/Exceptions.hpp"
#include "stream/ReadStream.hpp"

template <class T>
class FileReader : public ReadStream<T> {
private:
    std::string path;
    mutable std::ifstream input;
    bool isOpen{};
    std::size_t position{};
    mutable T bufferedValue{};
    mutable bool hasBufferedValue{};

    void EnsureOpen() const {
        if (!isOpen) {
            throw InvalidOperation("File read stream is not open.");
        }
    }

    void BufferNextIfNeeded() const {
        if (hasBufferedValue) {
            return;
        }
        T value{};
        if (input >> value) {
            bufferedValue = value;
            hasBufferedValue = true;
            return;
        }
        if (!input.eof()) {
            throw ParseError("Cannot parse value in file: " + path);
        }
    }

public:
    FileReader(std::string path)
        : path(path) {}

    void Open() override {
        input.open(path);
        if (!input) {
            throw InvalidOperation("Cannot open file for reading: " + path);
        }
        isOpen = true;
        position = 0;
        hasBufferedValue = false;
    }

    void Close() override {
        input.close();
        isOpen = false;
        hasBufferedValue = false;
    }

    bool IsOpen() const override {
        return isOpen;
    }

    bool IsEndOfStream() const override {
        EnsureOpen();
        BufferNextIfNeeded();
        return !hasBufferedValue;
    }

    T Read() override {
        EnsureOpen();
        BufferNextIfNeeded();
        if (!hasBufferedValue) {
            throw EndOfStream("File read stream reached the end.");
        }
        hasBufferedValue = false;
        ++position;
        return bufferedValue;
    }

    std::size_t GetPosition() const {
        return position;
    }

};
