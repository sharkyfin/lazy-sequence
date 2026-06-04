#pragma once

#include <stdexcept>
#include <string>

class IndexOutOfRange : public std::out_of_range {
public:
    IndexOutOfRange(const std::string& message)
        : std::out_of_range(message) {}
};

class EndOfStream : public std::runtime_error {
public:
    EndOfStream(const std::string& message)
        : std::runtime_error(message) {}
};

class InvalidOperation : public std::runtime_error {
public:
    InvalidOperation(const std::string& message)
        : std::runtime_error(message) {}
};

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message)
        : std::runtime_error(message) {}
};
