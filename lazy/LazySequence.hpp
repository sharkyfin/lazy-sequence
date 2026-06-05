#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "common/Buffer.hpp"
#include "common/Exceptions.hpp"
#include "common/Optional.hpp"
#include "common/Pair.hpp"
#include "lazy/Length.hpp"
#include "lazy/Ordinal.hpp"
#include "sequence/Sequence.hpp"

template <class T>
class LazySequence : public Sequence<T> {
private:
    template <class>
    friend class LazySequence;

    Length length;
    std::function<T(const Ordinal&)> valueAt;
    std::shared_ptr<LazySequence<T>> source;
    std::shared_ptr<LazySequence<T>> suffix;
    std::shared_ptr<Buffer<Pair<Ordinal, T>>> cache = std::make_shared<Buffer<Pair<Ordinal, T>>>();
    std::function<std::size_t(Buffer<const void*>&)> countDependencies;

    class LazySequenceEnumerator : public IEnumerator<T> {
    private:
        const LazySequence<T>& sequence;
        std::size_t position{};
        std::size_t count;
        Optional<T> current;

    public:
        LazySequenceEnumerator(const LazySequence<T>& sequence)
            : sequence(sequence),
              count(sequence.GetCount()) {}

        bool MoveNext() override {
            if (position >= count) {
                current.Reset();
                return false;
            }
            current = sequence.Get(position);
            ++position;
            return true;
        }

        const T& CurrentRef() const override {
            if (!current.has_value()) {
                throw InvalidOperation("Lazy sequence enumerator has no current value.");
            }
            return *current;
        }
    };

    static const T* FindCached(const Buffer<Pair<Ordinal, T>>& values, const Ordinal& index) {
        for (std::size_t position = 0; position < values.Size(); ++position) {
            if (values[position].first == index) {
                return &values[position].second;
            }
        }
        return nullptr;
    }

    static void StoreCached(Buffer<Pair<Ordinal, T>>& values, const Ordinal& index, const T& value) {
        if (FindCached(values, index) == nullptr) {
            values.PushBack(Pair<Ordinal, T>(index, value));
        }
    }

    LazySequence(Length lengthValue,
                 std::function<T(const Ordinal&)> valueAtValue,
                 std::function<std::size_t(Buffer<const void*>&)> countDependenciesValue)
        : length(lengthValue),
          valueAt(valueAtValue),
          countDependencies(countDependenciesValue) {}

    LazySequence(const LazySequence<T>& sourceValue, const LazySequence<T>& suffixValue)
        : length(ConcatLength(sourceValue.length, suffixValue.length)),
          source(std::make_shared<LazySequence<T>>(sourceValue)),
          suffix(std::make_shared<LazySequence<T>>(suffixValue)) {
        if (!sourceValue.length.IsKnown()) {
            throw InvalidOperation("Cannot concatenate after a sequence with unknown length.");
        }

        const auto left = source;
        const auto right = suffix;
        valueAt = [left, right](const Ordinal& index) {
            const Ordinal leftLength = left->length.Value();
            if (index < leftLength) {
                return left->Get(index);
            }
            return right->Get(index.SubtractPrefix(leftLength));
        };
    }

    LazySequence(const LazySequence<T>& sourceValue, const Ordinal& start, Length lengthValue)
        : length(lengthValue),
          source(std::make_shared<LazySequence<T>>(sourceValue)) {
        const auto parent = source;
        valueAt = [parent, start](const Ordinal& index) {
            return parent->Get(start.Add(index));
        };
    }

    LazySequence(const LazySequence<T>& sourceValue,
                 Length resultLength,
                 std::function<Ordinal(const Ordinal&)> sourceIndexAt)
        : length(resultLength),
          source(std::make_shared<LazySequence<T>>(sourceValue)) {
        if (!sourceIndexAt) {
            throw InvalidOperation("A filtered lazy sequence requires an index rule.");
        }

        const auto parent = source;
        valueAt = [parent, sourceIndexAt](const Ordinal& index) {
            return parent->Get(sourceIndexAt(index));
        };
    }

    static T ResolveRecurrence(
        const std::shared_ptr<Buffer<Pair<Ordinal, T>>>& values,
        const std::shared_ptr<const std::function<T(
            const Ordinal&, const std::function<T(const Ordinal&)>&)>>& recurrence,
        const Ordinal& index) {
        const T* found = FindCached(*values, index);
        if (found) {
            return *found;
        }

        const std::function<T(const Ordinal&)> resolvePrevious = [values, recurrence, index](
                                                                    const Ordinal& dependency) {
            if (!(dependency < index)) {
                throw InvalidOperation("A recurrence can only access strictly preceding positions.");
            }
            return ResolveRecurrence(values, recurrence, dependency);
        };
        const T value = (*recurrence)(index, resolvePrevious);
        StoreCached(*values, index, value);
        return value;
    }

    static Length ConcatLength(const Length& leftLength, const Length& rightLength) {
        if (!leftLength.IsKnown() || !rightLength.IsKnown()) {
            return Length::Unknown();
        }
        return Length::Known(leftLength.Value().Add(rightLength.Value()));
    }

    static Length MinLength(const Length& leftLength, const Length& rightLength) {
        if (!leftLength.IsKnown() || !rightLength.IsKnown()) {
            return Length::Unknown();
        }
        return Length::Known(MinOrdinal(leftLength.Value(), rightLength.Value()));
    }

    static std::function<T(const Ordinal&)> ArrayRule(const Buffer<T>& values) {
        auto data = std::make_shared<Buffer<T>>(values);
        return [data](const Ordinal& index) {
            if (!index.IsFinite()) {
                throw IndexOutOfRange("A finite buffer-backed sequence has no transfinite positions.");
            }
            return (*data)[index.Value()];
        };
    }

    static Buffer<T> BufferFromSequence(const Sequence<T>& sequence) {
        Buffer<T> values;
        values.Reserve(sequence.GetCount());
        auto enumerator = sequence.GetEnumerator();
        while (enumerator->MoveNext()) {
            values.PushBack(enumerator->CurrentRef());
        }
        return values;
    }

    static Buffer<T> SingleBuffer(const T& item) {
        Buffer<T> values(1);
        values[0] = item;
        return values;
    }

    T Cached(const Ordinal& index) const {
        const T* found = FindCached(*cache, index);
        if (found) {
            return *found;
        }

        const T value = valueAt(index);
        StoreCached(*cache, index, value);
        return value;
    }

    LazySequence<T> range(const Ordinal& startIndex, Length rangeLength) const {
        if (length.IsKnown() && startIndex > length.Value()) {
            throw IndexOutOfRange("Slice start index is outside the sequence bounds.");
        }
        if (rangeLength.IsKnown()) {
            const Ordinal end = startIndex.Add(rangeLength.Value());
            if (length.IsKnown() && end > length.Value()) {
                throw IndexOutOfRange("Slice end index is outside the sequence bounds.");
            }
        }
        return LazySequence<T>(*this, startIndex, rangeLength);
    }

    static bool ContainsNode(const Buffer<const void*>& nodes, const void* node) {
        for (std::size_t position = 0; position < nodes.Size(); ++position) {
            if (nodes[position] == node) {
                return true;
            }
        }
        return false;
    }

    std::size_t MaterializedCount(Buffer<const void*>& nodes) const {
        if (ContainsNode(nodes, this)) {
            return 0;
        }

        nodes.PushBack(this);
        std::size_t total = 0;
        if (!ContainsNode(nodes, cache.get())) {
            nodes.PushBack(cache.get());
            total += cache->Size();
        }
        if (source) {
            total += source->MaterializedCount(nodes);
        }
        if (suffix) {
            total += suffix->MaterializedCount(nodes);
        }
        if (countDependencies) {
            total += countDependencies(nodes);
        }
        return total;
    }

public:
    LazySequence()
        : LazySequence(Length::Known(Ordinal::Finite(0)), [](const Ordinal&) -> T {
              throw IndexOutOfRange("Cannot access an empty lazy sequence.");
          }) {}

    LazySequence(const Buffer<T>& values)
        : LazySequence(Length::Known(Ordinal::Finite(values.Size())), ArrayRule(values)) {}

    LazySequence(const Sequence<T>& sequence)
        : LazySequence(BufferFromSequence(sequence)) {}

    LazySequence(Buffer<T> initialValues,
                 Length lengthValue,
                 std::function<T(const Ordinal&, const std::function<T(const Ordinal&)>&)> recurrence)
        : length(lengthValue) {
        if (lengthValue.IsKnown() && lengthValue.Value().IsFinite()
            && initialValues.Size() > lengthValue.Value().Value()) {
            throw InvalidOperation("Initial value count cannot exceed finite sequence length.");
        }
        if (!recurrence) {
            throw InvalidOperation("A recurrence-based lazy sequence requires a recurrence rule.");
        }

        for (std::size_t index = 0; index < initialValues.Size(); ++index) {
            StoreCached(*cache, Ordinal::Finite(index), initialValues[index]);
        }

        const auto recurrenceRule = std::make_shared<const std::function<T(
            const Ordinal&, const std::function<T(const Ordinal&)>&)>>(recurrence);
        valueAt = [values = cache, recurrenceRule](const Ordinal& index) {
            return ResolveRecurrence(values, recurrenceRule, index);
        };
    }

    LazySequence(Buffer<T> initialValues,
                 Ordinal lengthValue,
                 std::function<T(const Ordinal&, const std::function<T(const Ordinal&)>&)> recurrence)
        : LazySequence(initialValues, Length::Known(lengthValue), recurrence) {}

    LazySequence(Length lengthValue,
                 std::function<T(const Ordinal&)> valueAtValue)
        : length(lengthValue),
          valueAt(valueAtValue) {
        if (!valueAt) {
            throw InvalidOperation("A lazy sequence requires a value rule.");
        }
    }

    LazySequence(Ordinal lengthValue,
                 std::function<T(const Ordinal&)> valueAtValue)
        : LazySequence(Length::Known(lengthValue), valueAtValue) {}

    T GetFirst() const override {
        return Get(Ordinal::Finite(0));
    }

    T GetLast() const override {
        if (!length.IsKnown()) {
            throw InvalidOperation("Cannot get the last element of a sequence with unknown length.");
        }
        if (length.Value().IsZero()) {
            throw IndexOutOfRange("Cannot get the last element of an empty lazy sequence.");
        }

        const auto lastIndex = length.Value().Predecessor();
        if (!lastIndex.has_value()) {
            throw InvalidOperation("A limit ordinal length has no last element.");
        }
        return Get(*lastIndex);
    }

    T Get(const Ordinal& index) const {
        if (length.IsKnown() && !(index < length.Value())) {
            throw IndexOutOfRange("Lazy sequence index " + index.ToString()
                                  + " is outside length " + length.Value().ToString() + ".");
        }
        return Cached(index);
    }

    T Get(std::size_t index) const {
        return Get(Ordinal::Finite(index));
    }

    LazySequence<T> slice(const Ordinal& startIndex, const Ordinal& endIndex) const {
        if (endIndex < startIndex) {
            throw IndexOutOfRange("Subsequence end index must be greater than or equal to start index.");
        }
        if (length.IsKnown() && !(endIndex < length.Value())) {
            throw IndexOutOfRange("Slice end index is outside the sequence bounds.");
        }

        const Ordinal resultLength = endIndex.SubtractPrefix(startIndex).Successor();
        return range(startIndex, Length::Known(resultLength));
    }

    LazySequence<T> slice(std::size_t startIndex, std::size_t endIndex) const {
        return slice(Ordinal::Finite(startIndex), Ordinal::Finite(endIndex));
    }

    Length GetLength() const {
        return length;
    }

    std::size_t GetMaterializedCount() const {
        Buffer<const void*> nodes;
        return MaterializedCount(nodes);
    }

    std::size_t GetCount() const override {
        if (!length.IsKnown() || !length.Value().IsFinite()) {
            throw InvalidOperation("Cannot enumerate a non-finite lazy sequence as Sequence.");
        }
        return length.Value().Value();
    }

    std::unique_ptr<IEnumerator<T>> GetEnumerator() const override {
        return std::make_unique<LazySequenceEnumerator>(*this);
    }

    std::unique_ptr<Sequence<T>> Clone() const override {
        return std::make_unique<LazySequence<T>>(*this);
    }

    LazySequence<T> concat(const LazySequence<T>& other) const {
        return LazySequence<T>(*this, other);
    }

    LazySequence<T> append(const T& item) const {
        return concat(LazySequence<T>(SingleBuffer(item)));
    }

    LazySequence<T> append(const LazySequence<T>& items) const {
        return concat(items);
    }

    LazySequence<T> prepend(const T& item) const {
        return LazySequence<T>(SingleBuffer(item)).concat(*this);
    }

    LazySequence<T> prepend(const LazySequence<T>& items) const {
        return items.concat(*this);
    }

    LazySequence<T> insertAt(const T& item, const Ordinal& index) const {
        return insertAt(LazySequence<T>(SingleBuffer(item)), index);
    }

    LazySequence<T> insertAt(const T& item, std::size_t index) const {
        return insertAt(item, Ordinal::Finite(index));
    }

    LazySequence<T> insertAt(const LazySequence<T>& items, const Ordinal& index) const {
        if (!items.GetLength().IsKnown()) {
            throw InvalidOperation("Cannot insert a sequence with unknown length.");
        }
        if (length.IsKnown() && index > length.Value()) {
            throw IndexOutOfRange("Insert index is outside the sequence bounds.");
        }

        const LazySequence<T> prefix = range(Ordinal::Finite(0), Length::Known(index));
        const Length suffixLength = length.IsKnown()
            ? Length::Known(length.Value().SubtractPrefix(index))
            : Length::Unknown();
        const LazySequence<T> suffixPart = range(index, suffixLength);
        return prefix.concat(items).concat(suffixPart);
    }

    LazySequence<T> insertAt(const LazySequence<T>& items, std::size_t index) const {
        return insertAt(items, Ordinal::Finite(index));
    }

    template <class U>
    LazySequence<U> map(std::function<U(const T&)> mapper) const {
        auto parent = std::make_shared<LazySequence<T>>(*this);
        return LazySequence<U>(
            length,
            [parent, mapper](const Ordinal& index) {
                return mapper(parent->Get(index));
            },
            [parent](Buffer<const void*>& nodes) {
                return parent->MaterializedCount(nodes);
            });
    }

    LazySequence<T> where(std::function<bool(const T&)> predicate) const {
        if (!predicate) {
            throw InvalidOperation("A filtered lazy sequence requires a predicate.");
        }
        if (length.IsKnown() && length.Value() > Ordinal::Omega()) {
            throw InvalidOperation("Filtering beyond omega requires an explicit filter index.");
        }

        auto parent = std::make_shared<LazySequence<T>>(*this);
        auto next = std::make_shared<Ordinal>(Ordinal::Finite(0));
        auto matches = std::make_shared<Buffer<Ordinal>>();
        const Length parentLength = length;
        std::function<Ordinal(const Ordinal&)> sourceIndexAt =
            [parent, predicate, next, matches, parentLength](const Ordinal& resultIndex) {
                if (!resultIndex.IsFinite()) {
                    throw IndexOutOfRange("A sequential filter only supports finite result positions.");
                }

                while (matches->Size() <= resultIndex.Value()) {
                    if (parentLength.IsKnown() && !(*next < parentLength.Value())) {
                        throw IndexOutOfRange("Filtered sequence does not contain the requested element.");
                    }

                    const Ordinal sourceIndex = *next;
                    const T value = parent->Get(sourceIndex);
                    const bool accepted = predicate(value);
                    *next = next->Successor();
                    if (accepted) {
                        matches->PushBack(sourceIndex);
                    }
                }
                return (*matches)[resultIndex.Value()];
            };
        return LazySequence<T>(*this, Length::Unknown(), sourceIndexAt);
    }

    template <class U>
    U reduce(std::function<U(const U&, const T&)> reducer, const U& initialValue) const {
        if (!length.IsKnown() || !length.Value().IsFinite()) {
            throw InvalidOperation("Cannot reduce a non-finite or unknown-length sequence without an explicit finite limit.");
        }

        U result = initialValue;
        const std::size_t count = length.Value().Value();
        for (std::size_t index = 0; index < count; ++index) {
            result = reducer(result, Get(index));
        }
        return result;
    }

    template <class U>
    LazySequence<Pair<T, U>> zip(const LazySequence<U>& second) const {
        using Result = Pair<T, U>;
        auto left = std::make_shared<LazySequence<T>>(*this);
        auto right = std::make_shared<LazySequence<U>>(second);
        const Length resultLength = MinLength(length, second.GetLength());
        return LazySequence<Result>(
            resultLength,
            [left, right](const Ordinal& index) {
                return Result(left->Get(index), right->Get(index));
            },
            [left, right](Buffer<const void*>& nodes) {
                return left->MaterializedCount(nodes) + right->MaterializedCount(nodes);
            });
    }
};
