#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#include "common/Exceptions.hpp"
#include "lazy/Generator.hpp"
#include "sequence/ArraySequence.hpp"
#include "statistics/OnlineStatistics.hpp"
#include "statistics/StatisticsStream.hpp"
#include "stream/FileReader.hpp"
#include "stream/FileWriter.hpp"
#include "stream/GeneratedReader.hpp"
#include "stream/LazyReader.hpp"
#include "stream/SequenceReader.hpp"
#include "stream/BufferWriter.hpp"

class TestRunner {
private:
    int passed{};
    int failed{};

public:
    void Run(const std::string& name, const std::function<void()>& test) {
        try {
            test();
            ++passed;
        } catch (const std::exception& exception) {
            ++failed;
            std::cerr << "[FAILED] " << name << ": " << exception.what() << '\n';
        }
    }

    int Finish() const {
        std::cout << "Passed: " << passed << ", failed: " << failed << '\n';
        return failed == 0 ? 0 : 1;
    }
};

void AssertTrue(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <class T, class U>
void AssertEqual(const T& actual, const U& expected, const std::string& message) {
    if (!(actual == expected)) {
        throw std::runtime_error(message);
    }
}

void AssertNear(double actual, double expected, double epsilon, const std::string& message) {
    if (std::abs(actual - expected) > epsilon) {
        throw std::runtime_error(message);
    }
}

template <class Exception>
void AssertThrows(const std::function<void()>& action, const std::string& message) {
    try {
        action();
    } catch (const Exception&) {
        return;
    } catch (...) {
        throw std::runtime_error(message + " (wrong exception type)");
    }
    throw std::runtime_error(message + " (no exception)");
}

template <class T, class... Values>
Buffer<T> MakeBuffer(const Values&... values) {
    Buffer<T> result(sizeof...(values));
    std::size_t index = 0;
    ((result[index++] = static_cast<T>(values)), ...);
    return result;
}

LazySequence<long long> Fibonacci() {
    return LazySequence<long long>(
        MakeBuffer<long long>(0, 1),
        Ordinal::Omega(),
        [](const Ordinal& index, const auto& resolvePrevious) {
            const std::size_t finiteIndex = index.Value();
            return resolvePrevious(Ordinal::Finite(finiteIndex - 1))
                + resolvePrevious(Ordinal::Finite(finiteIndex - 2));
        });
}

LazySequence<int> IndexedInts(const Ordinal& length, int offset = 0) {
    return LazySequence<int>(length, [offset](const Ordinal& index) {
        return offset + static_cast<int>(index.Value());
    });
}

LazySequence<int> Naturals() {
    return IndexedInts(Ordinal::Omega());
}

LazySequence<int> ConstantInts(const Ordinal& length, int value) {
    return LazySequence<int>(length, [value](const Ordinal&) {
        return value;
    });
}

template <class T>
void AssertLength(const LazySequence<T>& sequence, const Ordinal& expected, const std::string& message) {
    AssertEqual(sequence.GetLength().Value(), expected, message);
}

void LazySequenceTests(TestRunner& runner) {
    runner.Run("lazy sequence is usable through sequence interface", [] {
        LazySequence<int> lazy(MakeBuffer<int>(3, 4, 5));
        const Sequence<int>& polymorphicBase = lazy;
        AssertEqual(polymorphicBase.GetFirst(), 3, "polymorphic lazy access");
        AssertEqual(lazy.Get(1), 4, "lazy sequence access");
        AssertEqual(lazy.Get(2), 5, "lazy sequence access");
        AssertLength(lazy, Ordinal::Finite(3), "lazy length");
    });

    runner.Run("empty sequence throws", [] {
        LazySequence<int> empty;
        AssertThrows<IndexOutOfRange>([&] { (void)empty.GetFirst(); }, "empty GetFirst should throw");
        AssertThrows<IndexOutOfRange>([&] { (void)empty.GetLast(); }, "empty GetLast should throw");
        AssertThrows<IndexOutOfRange>([&] { (void)empty.Get(0); }, "empty Get should throw");
    });

    runner.Run("lazy sequence rejects an empty value rule", [] {
        AssertThrows<InvalidOperation>([] {
            (void)LazySequence<int>(Ordinal::Finite(1), std::function<int(const Ordinal&)>());
        }, "empty value rule should be rejected during construction");
    });

    runner.Run("finite construction copies elements", [] {
        Buffer<int> values = MakeBuffer<int>(1, 2, 3);
        LazySequence<int> sequence(values);
        values[0] = 99;
        AssertEqual(sequence.Get(0), 1, "sequence should copy input vector");
        AssertEqual(sequence.GetLast(), 3, "last element mismatch");
    });

    runner.Run("materializes only needed elements", [] {
        auto sequence = IndexedInts(Ordinal::Finite(100));
        AssertEqual(sequence.GetMaterializedCount(), static_cast<std::size_t>(0), "initial materialized count");
        AssertEqual(sequence.Get(4), 4, "Get(4)");
        AssertEqual(sequence.GetMaterializedCount(), static_cast<std::size_t>(1), "materialized count after Get(4)");
        AssertEqual(sequence.Get(2), 2, "Get(2)");
        AssertEqual(sequence.GetMaterializedCount(), static_cast<std::size_t>(2), "sparse cache should materialize requested values");
    });

    runner.Run("recurrence materialization counts computed prefix", [] {
        auto fibonacci = Fibonacci();
        AssertEqual(fibonacci.GetMaterializedCount(), static_cast<std::size_t>(2), "initial recurrence seed count");
        AssertEqual(fibonacci.Get(10), 55LL, "Fibonacci(10)");
        AssertEqual(fibonacci.GetMaterializedCount(), static_cast<std::size_t>(11), "computed recurrence prefix count");
        AssertEqual(fibonacci.Get(5), 5LL, "Fibonacci(5)");
        AssertEqual(fibonacci.GetMaterializedCount(), static_cast<std::size_t>(11), "cached recurrence prefix should not shrink");
    });

    runner.Run("tree materialization count includes child nodes", [] {
        auto left = IndexedInts(Ordinal::Finite(10));
        auto right = IndexedInts(Ordinal::Finite(10), 100);
        auto joined = left.concat(right);

        AssertEqual(joined.GetMaterializedCount(), static_cast<std::size_t>(0), "initial concat materialized count");
        AssertEqual(joined.Get(11), 101, "concat reads right child");
        AssertEqual(joined.GetMaterializedCount(), static_cast<std::size_t>(2), "concat materialized count includes right child");
        AssertEqual(joined.Get(1), 1, "concat reads left child");
        AssertEqual(joined.GetMaterializedCount(), static_cast<std::size_t>(4), "concat materialized count includes both children");
    });

    runner.Run("tree materialization count does not double count shared base", [] {
        auto source = IndexedInts(Ordinal::Finite(10));
        auto slice = source.slice(0, 2);
        auto joined = slice.concat(slice);

        AssertEqual(joined.Get(0), 0, "shared base left lookup");
        AssertEqual(joined.Get(4), 1, "shared base right lookup");
        AssertEqual(joined.GetMaterializedCount(), static_cast<std::size_t>(6), "shared base should be counted once");
    });

    runner.Run("where scans sequentially by default", [] {
        auto naturals = Naturals();
        auto even = naturals.where([](const int& value) {
            return value % 2 == 0;
        });

        AssertEqual(even.GetMaterializedCount(), static_cast<std::size_t>(0), "initial where materialized count");
        AssertEqual(even.Get(3), 6, "where finite lookup");
        AssertEqual(even.GetMaterializedCount(), static_cast<std::size_t>(8), "where materialized count includes source reads");
        AssertThrows<IndexOutOfRange>([&] { (void)even.Get(Ordinal::Omega()); }, "sequential where has no transfinite lookup");
    });

    runner.Run("where over transfinite concat is rejected", [] {
        LazySequence<int> sparse(Ordinal::Omega(), [](const Ordinal& index) {
            return index.Value() == 0 ? 1 : 0;
        });
        auto twos = ConstantInts(Ordinal::Omega(), 2);
        auto twoBlocks = sparse.concat(twos);
        const auto positive = [](const int& value) {
            return value > 0;
        };
        AssertThrows<InvalidOperation>([&] {
            (void)twoBlocks.where(positive);
        }, "transfinite where cannot scan beyond omega");
    });

    runner.Run("sequential where evaluates predicate once per inspected element", [] {
        auto calls = std::make_shared<std::size_t>(0);
        LazySequence<int> sequence(MakeBuffer<int>(1, 2, 3, 4));
        auto even = sequence.where([calls](const int& value) {
            ++(*calls);
            return value % 2 == 0;
        });

        AssertEqual(even.Get(1), 4, "second even value");
        AssertEqual(*calls, static_cast<std::size_t>(4), "predicate call count");
    });

    runner.Run("map and zip are materialized as real lazy nodes", [] {
        auto left = IndexedInts(Ordinal::Finite(10));
        auto right = IndexedInts(Ordinal::Finite(10), 100);

        auto mapped = left.map<int>([](const int& value) {
            return value * 10;
        });
        AssertEqual(mapped.GetMaterializedCount(), static_cast<std::size_t>(0), "initial map materialization");
        AssertEqual(mapped.Get(3), 30, "map value");
        AssertEqual(mapped.GetMaterializedCount(), static_cast<std::size_t>(2), "map materializes node and source");

        auto zipped = left.zip(right);
        AssertEqual(zipped.GetMaterializedCount(), static_cast<std::size_t>(1), "zip sees already materialized source");
        const auto zippedValue = zipped.Get(2);
        AssertEqual(zippedValue.first, 2, "zip left value");
        AssertEqual(zippedValue.second, 102, "zip right value");
        AssertEqual(zipped.GetMaterializedCount(), static_cast<std::size_t>(4), "zip counts reachable node and sources");
    });

    runner.Run("factorial recurrence", [] {
        LazySequence<long long> factorials(
            MakeBuffer<long long>(1, 1),
            Ordinal::Finite(7),
            [](const Ordinal& index, const auto& resolvePrevious) {
                const std::size_t finiteIndex = index.Value();
                return resolvePrevious(Ordinal::Finite(finiteIndex - 1))
                    * static_cast<long long>(finiteIndex);
            });
        AssertEqual(factorials.Get(6), 720LL, "factorial(6)");
    });

    runner.Run("ordinal recurrence supports limit and successor positions", [] {
        LazySequence<int> sequence(
            Buffer<int>(),
            Ordinal(1, 2),
            [](const Ordinal& index, const auto& resolvePrevious) {
                if (index.IsFinite()) {
                    return static_cast<int>(index.Value());
                }
                if (index.IsLimit()) {
                    return resolvePrevious(Ordinal::Finite(3)) + 100;
                }
                const auto predecessor = index.Predecessor();
                return resolvePrevious(*predecessor) + 1;
            });

        AssertEqual(sequence.Get(Ordinal::Omega()), 103, "recurrence value at omega");
        AssertEqual(sequence.Get({1, 1}), 104, "recurrence value after omega");
        AssertEqual(sequence.GetMaterializedCount(), static_cast<std::size_t>(3), "transfinite recurrence caches dependencies");
    });

    runner.Run("ordinal recurrence rejects current and future dependencies", [] {
        LazySequence<int> current(
            Buffer<int>(),
            Ordinal::Finite(1),
            [](const Ordinal& index, const auto& resolvePrevious) {
                return resolvePrevious(index);
            });
        AssertThrows<InvalidOperation>([&] {
            (void)current.Get(0);
        }, "recurrence cannot read current position");

        LazySequence<int> future(
            Buffer<int>(),
            Ordinal::Finite(1),
            [](const Ordinal&, const auto& resolvePrevious) {
                return resolvePrevious(Ordinal::Finite(1));
            });
        AssertThrows<InvalidOperation>([&] {
            (void)future.Get(0);
        }, "recurrence cannot read future position");
    });

    runner.Run("subsequence append prepend insert concat", [] {
        LazySequence<int> sequence(MakeBuffer<int>(1, 2, 3, 4));
        auto sub = sequence.slice(1, 2);
        AssertEqual(sub.Get(0), 2, "subsequence first");
        AssertEqual(sub.Get(1), 3, "subsequence second");
        AssertEqual(sequence.append(5).GetLast(), 5, "append");
        AssertEqual(sequence.prepend(0).GetFirst(), 0, "prepend");
        AssertEqual(sequence.insertAt(9, 2).Get(2), 9, "insert");
        AssertEqual(sequence.concat(LazySequence<int>(MakeBuffer<int>(5, 6))).Get(5), 6, "concat");
    });

    runner.Run("map where reduce zip", [] {
        LazySequence<int> sequence(MakeBuffer<int>(1, 2, 3, 4, 5));
        auto doubled = sequence.map<int>([](const int& value) { return value * 2; });
        AssertEqual(doubled.Get(3), 8, "map");
        auto even = sequence.where([](const int& value) { return value % 2 == 0; });
        AssertEqual(even.Get(0), 2, "where first");
        AssertEqual(even.Get(1), 4, "where second");
        const int sum = sequence.reduce<int>([](const int& acc, const int& value) { return acc + value; }, 0);
        AssertEqual(sum, 15, "reduce");
        auto zipped = sequence.zip(doubled);
        const auto zippedValue = zipped.Get(2);
        AssertEqual(zippedValue.first, 3, "zip left");
        AssertEqual(zippedValue.second, 6, "zip right");
    });

    runner.Run("zip returns pairs and uses shortest length", [] {
        auto first = IndexedInts(Ordinal::Finite(10));
        LazySequence<std::string> second(MakeBuffer<std::string>("a", "b", "c"));

        auto zipped = first.zip(second);
        AssertLength(zipped, Ordinal::Finite(3), "zip should use shortest length");
        AssertEqual(zipped.GetMaterializedCount(), static_cast<std::size_t>(0), "initial zip materialization");

        const auto value = zipped.Get(2);
        AssertEqual(value.first, 2, "zip first value");
        AssertEqual(value.second, std::string("c"), "zip second value");
        AssertEqual(zipped.GetMaterializedCount(), static_cast<std::size_t>(3), "zip materializes result and parents");
        AssertThrows<IndexOutOfRange>([&] { (void)zipped.Get(3); }, "zip should stop at shortest length");
    });

    runner.Run("invalid operations throw", [] {
        LazySequence<int> sequence(MakeBuffer<int>(1, 2));
        AssertThrows<IndexOutOfRange>([&] { (void)sequence.Get(2); }, "out of range");
        AssertThrows<InvalidOperation>([] { (void)Fibonacci().GetLast(); }, "infinite GetLast");
        AssertThrows<InvalidOperation>([] {
            (void)Fibonacci().reduce<int>([](const int& acc, const long long& value) {
                return acc + static_cast<int>(value);
            }, 0);
        }, "reduce infinite");
    });

    runner.Run("ordinal concat and append keep block boundaries", [] {
        auto naturals = Naturals();
        LazySequence<int> finite(MakeBuffer<int>(100, 200));

        auto omegaPlusTwo = naturals.concat(finite);
        AssertLength(omegaPlusTwo, Ordinal(1, 2), "omega + 2 length");
        AssertEqual(omegaPlusTwo.Get({1, 0}), 100, "first finite tail after omega");
        AssertEqual(omegaPlusTwo.Get({1, 1}), 200, "second finite tail after omega");

        auto omegaPlusOne = naturals.append(999);
        AssertLength(omegaPlusOne, Ordinal(1, 1), "append after omega length");
        AssertEqual(omegaPlusOne.Get({1, 0}), 999, "append after omega value");

        auto omegaTwice = naturals.concat(naturals);
        AssertLength(omegaTwice, Ordinal(2, 0), "omega + omega length");
        AssertEqual(omegaTwice.Get({1, 0}), 0, "first value in second omega block");
        AssertEqual(omegaTwice.Get({1, 1}), 1, "second value in second omega block");

        auto a = Naturals();
        auto b = IndexedInts(Ordinal::Omega(), 1000);
        auto c = IndexedInts(Ordinal::Omega(), 2000);
        auto abc = a.concat(b).concat(c);
        AssertLength(abc, Ordinal(3, 0), "three omega blocks length");
        AssertEqual(abc.Get({1, 1050}), 2050, "second omega block access");
        AssertEqual(abc.Get({2, 3}), 2003, "third omega block access");
    });

    runner.Run("ordinal insert handles finite and omega positions", [] {
        auto naturals = Naturals();
        auto insertedInsideOmega = naturals.insertAt(77, 2);

        AssertLength(insertedInsideOmega, Ordinal::Omega(), "finite insert inside omega length");
        AssertEqual(insertedInsideOmega.Get(0), 0, "insert inside omega before");
        AssertEqual(insertedInsideOmega.Get(2), 77, "insert inside omega item");
        AssertEqual(insertedInsideOmega.Get(3), 2, "insert inside omega shifted value");

        auto insertedAtOmega = naturals.insertAt(88, {1, 0});
        AssertLength(insertedAtOmega, Ordinal(1, 1), "insert at omega length");
        AssertEqual(insertedAtOmega.Get({1, 0}), 88, "insert at omega value");

        auto block = IndexedInts(Ordinal::Omega(), 1000);
        auto omegaInsideOmega = naturals.insertAt(block, 2);
        AssertLength(omegaInsideOmega, Ordinal(2, 0), "omega inserted into omega length");
        AssertEqual(omegaInsideOmega.Get(0), 0, "omega insert prefix first");
        AssertEqual(omegaInsideOmega.Get(1), 1, "omega insert prefix second");
        AssertEqual(omegaInsideOmega.Get(2), 1000, "omega insert first inserted");
        AssertEqual(omegaInsideOmega.Get(100), 1098, "omega insert finite inserted");
        AssertEqual(omegaInsideOmega.Get({1, 0}), 2, "omega insert suffix first");
        AssertEqual(omegaInsideOmega.Get({1, 3}), 5, "omega insert suffix shifted");

        LazySequence<int> tail(MakeBuffer<int>(100, 200));
        auto omegaIntoTail = naturals.concat(tail).insertAt(block, {1, 1});
        AssertLength(omegaIntoTail, Ordinal(2, 1), "omega inserted into omega tail length");
        AssertEqual(omegaIntoTail.Get({1, 0}), 100, "omega tail insert prefix");
        AssertEqual(omegaIntoTail.Get({1, 1}), 1000, "omega tail insert first inserted");
        AssertEqual(omegaIntoTail.Get({1, 5}), 1004, "omega tail insert inserted block");
        AssertEqual(omegaIntoTail.Get({2, 0}), 200, "omega tail insert suffix");
    });

    runner.Run("insert append and prepend accept sequence inputs", [] {
        auto finiteInsert = LazySequence<int>(MakeBuffer<int>(1, 4)).insertAt(LazySequence<int>(MakeBuffer<int>(2, 3)), 1);
        AssertLength(finiteInsert, Ordinal::Finite(4), "finite sequence insert length");
        AssertEqual(finiteInsert.Get(0), 1, "finite insert before");
        AssertEqual(finiteInsert.Get(1), 2, "finite insert first inserted");
        AssertEqual(finiteInsert.Get(2), 3, "finite insert second inserted");
        AssertEqual(finiteInsert.Get(3), 4, "finite insert suffix");

        ArraySequence<int> arrayItems(MakeBuffer<int>(8, 9));
        const Sequence<int>& sequenceItems = arrayItems;
        auto sequenceInsert = LazySequence<int>(MakeBuffer<int>(1, 4)).insertAt(sequenceItems, 1);
        AssertEqual(sequenceInsert.Get(0), 1, "sequence insert before");
        AssertEqual(sequenceInsert.Get(1), 8, "sequence insert first item");
        AssertEqual(sequenceInsert.Get(2), 9, "sequence insert second item");
        AssertEqual(sequenceInsert.Get(3), 4, "sequence insert suffix");

        auto naturals = Naturals();
        LazySequence<int> finite(MakeBuffer<int>(100, 200));
        auto appendedSequence = naturals.append(finite);
        AssertLength(appendedSequence, Ordinal(1, 2), "append sequence length");
        AssertEqual(appendedSequence.Get({1, 0}), 100, "append sequence first tail");
        AssertEqual(appendedSequence.Get({1, 1}), 200, "append sequence second tail");

        auto prependedSequence = naturals.prepend(finite);
        AssertLength(prependedSequence, Ordinal::Omega(), "prepend sequence before omega length");
        AssertEqual(prependedSequence.Get(0), 100, "prepend sequence first");
        AssertEqual(prependedSequence.Get(1), 200, "prepend sequence second");
        AssertEqual(prependedSequence.Get(2), 0, "prepend sequence shifted omega start");
    });

    runner.Run("ordinal slice map and zip work across omega blocks", [] {
        auto a = Naturals();
        auto b = IndexedInts(Ordinal::Omega(), 1000);
        auto c = IndexedInts(Ordinal::Omega(), 2000);
        auto abc = a.concat(b).concat(c);

        auto sliced = abc.slice({1, 2}, {2, 1});
        AssertLength(sliced, Ordinal(1, 2), "ordinal slice length");
        AssertEqual(sliced.Get(0), 1002, "ordinal slice first");
        AssertEqual(sliced.Get({1, 0}), 2000, "ordinal slice second block first");
        AssertEqual(sliced.Get({1, 1}), 2001, "ordinal slice second block second");

        auto mappedOmega = abc.map<int>([](const int& value) { return value * 10; });
        AssertEqual(mappedOmega.Get({2, 3}), 20030, "ordinal map value");

        auto zippedOmega = b.zip(c);
        AssertLength(zippedOmega, Ordinal::Omega(), "ordinal zip omega length");
        const auto zippedOmegaValue = zippedOmega.Get(3);
        AssertEqual(zippedOmegaValue.first, 1003, "ordinal zip left");
        AssertEqual(zippedOmegaValue.second, 2003, "ordinal zip right");
    });

    runner.Run("ordinal arithmetic rejects overflow and identifies limits", [] {
        AssertTrue(Ordinal::Omega().IsLimit(), "omega is a limit ordinal");
        AssertTrue(Ordinal(2, 0).IsLimit(), "omega times two is a limit ordinal");
        AssertTrue(!Ordinal::Finite(0).IsLimit(), "zero is not treated as a limit ordinal");
        AssertTrue(!Ordinal(1, 1).IsLimit(), "successor ordinal is not a limit ordinal");

        const auto maximum = std::numeric_limits<std::size_t>::max();
        AssertThrows<InvalidOperation>([&] {
            (void)Ordinal::Finite(maximum).Successor();
        }, "ordinal successor overflow");
        AssertThrows<InvalidOperation>([&] {
            (void)Ordinal::Finite(maximum).Add(Ordinal::Finite(1));
        }, "ordinal finite addition overflow");
        AssertThrows<InvalidOperation>([&] {
            (void)Ordinal(maximum, 0).Add(Ordinal::Omega());
        }, "ordinal omega block addition overflow");
    });

}

void GeneratorTests(TestRunner& runner) {
    runner.Run("generator finite sequence", [] {
        Generator<int> generator(LazySequence<int>(MakeBuffer<int>(7, 8)));
        AssertTrue(generator.HasNext(), "should have first");
        AssertEqual(generator.GetNext(), 7, "first");
        AssertEqual(generator.GetPosition(), static_cast<std::size_t>(1), "position after first");
        AssertEqual(generator.GetNext(), 8, "second");
        AssertTrue(!generator.HasNext(), "should be ended");
        AssertTrue(!generator.TryGetNext().has_value(), "TryGetNext end");
    });

    runner.Run("generator infinite sequence", [] {
        Generator<long long> generator(Fibonacci());
        AssertTrue(generator.HasNext(), "infinite has next");
        AssertEqual(generator.GetNext(), 0LL, "fib 0");
        AssertEqual(generator.GetNext(), 1LL, "fib 1");
        AssertTrue(generator.HasNext(), "infinite still has next");
    });

    runner.Run("generator unknown finite sequence", [] {
        LazySequence<int> sequence(Length::Unknown(), [](const Ordinal& index) {
            if (!index.IsFinite() || index.Value() >= 2) {
                throw IndexOutOfRange("end");
            }
            return static_cast<int>(10 + index.Value());
        });
        Generator<int> generator(sequence);

        const auto first = generator.TryGetNext();
        AssertTrue(first.has_value(), "first value should exist");
        AssertEqual(*first, 10, "first unknown-length value");
        AssertEqual(generator.GetNext(), 11, "second unknown-length value");
        AssertTrue(!generator.HasNext(), "unknown finite generator should end");
        AssertThrows<EndOfStream>([&] { (void)generator.GetNext(); }, "unknown finite generator end");
    });
}

void StreamTests(TestRunner& runner) {
    runner.Run("sequence read stream", [] {
        ArraySequence<int> sequence(MakeBuffer<int>(1, 2, 3));
        SequenceReader<int> stream(sequence);
        AssertThrows<InvalidOperation>([&] { (void)stream.Read(); }, "read before open");
        stream.Open();
        AssertEqual(stream.Read(), 1, "read 1");
        AssertEqual(stream.GetPosition(), static_cast<std::size_t>(1), "position");
        AssertEqual(stream.Read(), 2, "read 2");
        AssertEqual(stream.GoBack(), static_cast<std::size_t>(1), "go back to second element");
        AssertEqual(stream.Read(), 2, "read after go back");
        stream.GoBack();
        stream.GoBack();
        AssertThrows<IndexOutOfRange>([&] { stream.GoBack(); }, "go back before start");
        stream.Seek(0);
        AssertEqual(stream.Read(), 1, "seek");
        stream.Seek(3);
        AssertTrue(stream.IsEndOfStream(), "end");
        AssertThrows<EndOfStream>([&] { (void)stream.Read(); }, "end exception");
        stream.Close();
    });

    runner.Run("lazy read stream and buffer write stream", [] {
        LazyReader<int> readStream(LazySequence<int>(MakeBuffer<int>(4, 5)));
        readStream.Open();
        AssertEqual(readStream.Read(), 4, "lazy read 4");
        readStream.Close();

        auto naturals = Naturals();
        LazyReader<int> ordinalStream(naturals.append(100));
        ordinalStream.Open();
        ordinalStream.Seek(Ordinal(1, 1));
        AssertEqual(ordinalStream.GoBack(), Ordinal(1, 0), "lazy stream goes back to omega tail");
        AssertEqual(ordinalStream.Read(), 100, "lazy stream reads omega tail after go back");
        ordinalStream.Seek(Ordinal::Omega());
        AssertThrows<IndexOutOfRange>([&] { ordinalStream.GoBack(); }, "limit ordinal has no predecessor");
        ordinalStream.Close();

        BufferWriter<int> writeStream;
        AssertThrows<InvalidOperation>([&] { (void)writeStream.Write(1); }, "write before open");
        writeStream.Open();
        AssertEqual(writeStream.Write(10), static_cast<std::size_t>(1), "write position");
        AssertEqual(writeStream.Values()[0], 10, "written value");
        writeStream.Close();
    });

    runner.Run("lazy reader detects end of unknown length sequence", [] {
        LazySequence<int> sequence(MakeBuffer<int>(1, 2, 3));
        auto even = sequence.where([](const int& value) {
            return value % 2 == 0;
        });
        LazyReader<int> reader(even);
        reader.Open();
        AssertEqual(reader.Read(), 2, "lazy reader filtered read");
        AssertTrue(reader.IsEndOfStream(), "lazy reader filtered end");
        AssertThrows<EndOfStream>([&] { (void)reader.Read(); }, "lazy reader filtered end exception");
        reader.Close();
    });

    runner.Run("lazy reader consumes peeked unknown value once", [] {
        auto calls = std::make_shared<std::size_t>(0);
        LazySequence<int> sequence(Length::Unknown(), [calls](const Ordinal& index) {
            ++(*calls);
            if (!index.IsFinite() || index.Value() != 0) {
                throw IndexOutOfRange("Counting lazy sequence end.");
            }
            return 42;
        });
        LazyReader<int> reader(sequence);

        reader.Open();
        AssertTrue(!reader.IsEndOfStream(), "peeked value should exist");
        AssertEqual(*calls, static_cast<std::size_t>(1), "peek should read once");
        AssertEqual(reader.Read(), 42, "read should consume peeked value");
        AssertEqual(*calls, static_cast<std::size_t>(1), "read should not re-read peeked value");
        AssertTrue(reader.IsEndOfStream(), "reader should end after one value");
        AssertEqual(*calls, static_cast<std::size_t>(2), "end check should probe next value once");
        reader.Close();
    });

    runner.Run("file streams", [] {
        const std::string path = "test_numbers.txt";
        FileWriter<int> writer(path);
        writer.Open();
        writer.Write(11);
        writer.Write(22);
        writer.Close();

        FileReader<int> reader(path);
        reader.Open();
        AssertEqual(reader.Read(), 11, "file read first");
        AssertEqual(reader.GetPosition(), static_cast<std::size_t>(1), "file position");
        AssertEqual(reader.Read(), 22, "file read second");
        AssertTrue(reader.IsEndOfStream(), "file end");
        reader.Close();
    });

    runner.Run("generated reader produces finite stream on demand", [] {
        GeneratedReader<int> reader(3, [](std::size_t index) {
            return static_cast<int>(index * 10);
        });
        reader.Open();
        AssertEqual(reader.Read(), 0, "generated first");
        AssertEqual(reader.Read(), 10, "generated second");
        AssertEqual(reader.Read(), 20, "generated third");
        AssertTrue(reader.IsEndOfStream(), "generated end");
        AssertThrows<EndOfStream>([&] { (void)reader.Read(); }, "generated end exception");
        reader.Close();
    });

    runner.Run("generated reader preserves position when generation fails", [] {
        GeneratedReader<int> reader(2, [](std::size_t index) {
            if (index == 0) {
                throw InvalidOperation("generation failed");
            }
            return static_cast<int>(index);
        });
        reader.Open();
        AssertThrows<InvalidOperation>([&] {
            (void)reader.Read();
        }, "generated reader forwards generation failure");
        AssertEqual(reader.GetPosition(), static_cast<std::size_t>(0), "failed generation does not consume position");
        reader.Close();
    });
}

void StatisticsTests(TestRunner& runner) {
    runner.Run("online statistics reports every prefix", [] {
        OnlineStatistics<double> statistics;
        AssertThrows<InvalidOperation>([&] { (void)statistics.GetSnapshot(); }, "empty snapshot");

        statistics.Add(5.0);
        auto snapshot = statistics.GetSnapshot();
        AssertEqual(snapshot.count, static_cast<std::size_t>(1), "first count");
        AssertNear(snapshot.mean, 5.0, 1e-12, "first mean");
        AssertNear(snapshot.median, 5.0, 1e-12, "first median");
        AssertTrue(!snapshot.sampleVariance.has_value(), "sample variance is undefined for one value");

        statistics.Add(1.0);
        snapshot = statistics.GetSnapshot();
        AssertNear(snapshot.mean, 3.0, 1e-12, "second mean");
        AssertNear(snapshot.median, 3.0, 1e-12, "second median");
        AssertNear(snapshot.populationVariance, 4.0, 1e-12, "second population variance");
        AssertNear(*snapshot.sampleVariance, 8.0, 1e-12, "second sample variance");

        statistics.Add(9.0);
        statistics.Add(3.0);
        snapshot = statistics.GetSnapshot();
        AssertEqual(snapshot.minimum, 1.0, "minimum");
        AssertEqual(snapshot.maximum, 9.0, "maximum");
        AssertNear(snapshot.mean, 4.5, 1e-12, "final mean");
        AssertNear(snapshot.median, 4.0, 1e-12, "final median");
        AssertNear(snapshot.populationVariance, 8.75, 1e-12, "final population variance");
        AssertNear(*snapshot.sampleVariance, 35.0 / 3.0, 1e-12, "final sample variance");
    });

    runner.Run("welford variance is stable for large nearby values", [] {
        OnlineStatistics<double> statistics;
        statistics.Add(1000000000004.0);
        statistics.Add(1000000000007.0);
        statistics.Add(1000000000013.0);
        statistics.Add(1000000000016.0);

        const auto snapshot = statistics.GetSnapshot();
        AssertNear(snapshot.mean, 1000000000010.0, 1e-6, "stable mean");
        AssertNear(snapshot.populationVariance, 22.5, 1e-12, "stable variance");
        AssertNear(snapshot.populationStandardDeviation, std::sqrt(22.5), 1e-12, "stable deviation");
    });

    runner.Run("online statistics handles duplicates and negative values", [] {
        OnlineStatistics<int> statistics;
        statistics.Add(-2);
        statistics.Add(-2);
        statistics.Add(4);
        statistics.Add(4);

        const auto snapshot = statistics.GetSnapshot();
        AssertEqual(snapshot.minimum, -2, "negative minimum");
        AssertEqual(snapshot.maximum, 4, "positive maximum");
        AssertNear(snapshot.mean, 1.0, 1e-12, "integer mean");
        AssertNear(snapshot.median, 1.0, 1e-12, "even integer median");
    });

    runner.Run("online statistics rejects non-finite floating values", [] {
        OnlineStatistics<double> statistics;
        AssertThrows<InvalidOperation>([&] {
            statistics.Add(std::numeric_limits<double>::infinity());
        }, "infinity should be rejected");
        AssertThrows<InvalidOperation>([&] {
            statistics.Add(std::numeric_limits<double>::quiet_NaN());
        }, "NaN should be rejected");

        OnlineStatistics<long double> overflowing;
        overflowing.Add(std::numeric_limits<long double>::max());
        AssertThrows<InvalidOperation>([&] {
            overflowing.Add(-std::numeric_limits<long double>::max());
        }, "arithmetic overflow should be rejected");
    });

    runner.Run("statistics stream consumes exactly one source item per read", [] {
        auto reads = std::make_shared<std::size_t>(0);
        GeneratedReader<double> source(3, [reads](std::size_t index) {
            ++(*reads);
            const double values[] = {5.0, 1.0, 9.0};
            return values[index];
        });
        StatisticsStream<double> statistics(source);

        AssertThrows<InvalidOperation>([&] { (void)statistics.Read(); }, "read before open");
        statistics.Open();
        AssertEqual(*reads, static_cast<std::size_t>(0), "open must not consume input");
        AssertNear(statistics.Read().median, 5.0, 1e-12, "first stream snapshot");
        AssertEqual(*reads, static_cast<std::size_t>(1), "first read consumes one item");
        AssertNear(statistics.Read().median, 3.0, 1e-12, "second stream snapshot");
        AssertEqual(*reads, static_cast<std::size_t>(2), "second read consumes one item");
        AssertNear(statistics.Read().median, 5.0, 1e-12, "third stream snapshot");
        AssertEqual(*reads, static_cast<std::size_t>(3), "third read consumes one item");
        AssertTrue(statistics.IsEndOfStream(), "statistics stream end");
        AssertThrows<EndOfStream>([&] { (void)statistics.Read(); }, "statistics stream end exception");
        statistics.Close();
    });

    runner.Run("statistics stream resets state when reopened", [] {
        SequenceReader<int> source(MakeBuffer<int>(2, 6));
        StatisticsStream<int> statistics(source);
        statistics.Open();
        AssertNear(statistics.Read().mean, 2.0, 1e-12, "first run first item");
        AssertNear(statistics.Read().mean, 4.0, 1e-12, "first run second item");
        statistics.Close();

        statistics.Open();
        const auto snapshot = statistics.Read();
        AssertEqual(snapshot.count, static_cast<std::size_t>(1), "reopened count");
        AssertNear(snapshot.mean, 2.0, 1e-12, "reopened mean");
        statistics.Close();
    });

    runner.Run("statistics stream accepts file reader", [] {
        const std::string path = "test_numbers.txt";
        FileWriter<int> writer(path);
        writer.Open();
        writer.Write(2);
        writer.Write(4);
        writer.Write(9);
        writer.Close();

        FileReader<int> source(path);
        StatisticsStream<int> statistics(source);
        statistics.Open();
        statistics.Read();
        statistics.Read();
        const auto snapshot = statistics.Read();
        AssertEqual(snapshot.count, static_cast<std::size_t>(3), "file statistics count");
        AssertNear(snapshot.mean, 5.0, 1e-12, "file statistics mean");
        AssertNear(snapshot.median, 4.0, 1e-12, "file statistics median");
        statistics.Close();
    });

    runner.Run("statistics stream remains lazy over lazy reader", [] {
        auto calls = std::make_shared<std::size_t>(0);
        LazySequence<double> sequence(Ordinal::Finite(100), [calls](const Ordinal& index) {
            ++(*calls);
            return static_cast<double>(index.Value());
        });
        LazyReader<double> source(sequence);
        StatisticsStream<double> statistics(source);

        statistics.Open();
        AssertEqual(*calls, static_cast<std::size_t>(0), "statistics open should not materialize lazy input");
        AssertNear(statistics.Read().mean, 0.0, 1e-12, "first lazy mean");
        AssertEqual(*calls, static_cast<std::size_t>(1), "first snapshot materializes one value");
        AssertNear(statistics.Read().mean, 0.5, 1e-12, "second lazy mean");
        AssertEqual(*calls, static_cast<std::size_t>(2), "second snapshot materializes one more value");
        statistics.Close();
    });

    runner.Run("online statistics handles large prefix", [] {
        OnlineStatistics<std::size_t> statistics;
        constexpr std::size_t count = 100000;
        for (std::size_t value = 0; value < count; ++value) {
            statistics.Add(value);
        }

        const auto snapshot = statistics.GetSnapshot();
        AssertEqual(snapshot.count, count, "large prefix count");
        AssertNear(snapshot.mean, 49999.5, 1e-12, "large prefix mean");
        AssertNear(snapshot.median, 49999.5, 1e-12, "large prefix median");
    });
}

int main() {
    TestRunner runner;
    LazySequenceTests(runner);
    GeneratorTests(runner);
    StreamTests(runner);
    StatisticsTests(runner);
    return runner.Finish();
}
