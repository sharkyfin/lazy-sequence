#pragma once

#include <cstddef>
#include <memory>

#include "common/IEnumerator.hpp"

template <class T>
class Sequence {
public:
    virtual ~Sequence() = default;

    virtual T GetFirst() const = 0;
    virtual T GetLast() const = 0;
    virtual std::size_t GetCount() const = 0;
    virtual std::unique_ptr<IEnumerator<T>> GetEnumerator() const = 0;
    virtual std::unique_ptr<Sequence<T>> Clone() const = 0;
};
