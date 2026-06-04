#pragma once

template <class T>
class IEnumerator {
public:
    virtual ~IEnumerator() = default;

    virtual bool MoveNext() = 0;
    virtual const T& CurrentRef() const = 0;

    T Current() const {
        return CurrentRef();
    }
};
