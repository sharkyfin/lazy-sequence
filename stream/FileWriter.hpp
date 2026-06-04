#pragma once

#include <fstream>
#include <string>

#include "common/Exceptions.hpp"
#include "stream/WriteStream.hpp"

template <class T>
class FileWriter : public WriteStream<T> {
private:
    std::string path;
    std::ofstream output;
    bool isOpen{};
    std::size_t position{};

    void EnsureOpen() const {
        if (!isOpen) {
            throw InvalidOperation("File write stream is not open.");
        }
    }

public:
    FileWriter(std::string path)
        : path(path) {}

    void Open() override {
        output.open(path);
        if (!output) {
            throw InvalidOperation("Cannot open file for writing: " + path);
        }
        isOpen = true;
        position = 0;
    }

    void Close() override {
        output.close();
        isOpen = false;
    }

    bool IsOpen() const override {
        return isOpen;
    }

    std::size_t Write(const T& item) override {
        EnsureOpen();
        output << item << '\n';
        if (!output) {
            throw InvalidOperation("Cannot write value to file: " + path);
        }
        return ++position;
    }

    std::size_t GetPosition() const override {
        return position;
    }

};
