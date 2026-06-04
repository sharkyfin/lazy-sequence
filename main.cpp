#include <iostream>
#include <string>

#include "common/Buffer.hpp"
#include "lazy/Generator.hpp"
#include "lazy/LazySequence.hpp"
#include "statistics/StatisticsStream.hpp"
#include "stream/LazyReader.hpp"

int ReadInt(const std::string& prompt, int minValue, int defaultValue) {
    while (true) {
        std::cout << prompt << " [" << defaultValue << "]: ";
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) {
            return defaultValue;
        }
        try {
            const int value = std::stoi(line);
            if (value >= minValue) {
                return value;
            }
        } catch (...) {
        }
        std::cout << "Некорректное значение.\n";
    }
}

long long ReadLongLong(const std::string& prompt, long long defaultValue) {
    while (true) {
        std::cout << prompt << " [" << defaultValue << "]: ";
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) {
            return defaultValue;
        }
        try {
            return std::stoll(line);
        } catch (...) {
        }
        std::cout << "Некорректное значение.\n";
    }
}

void PrintDivider() {
    std::cout << "------------------------------------------------------------\n";
}

Buffer<long long> ReadFiniteValues() {
    const int count = ReadInt("Количество элементов", 0, 5);
    Buffer<long long> values(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        values[static_cast<std::size_t>(index)] = ReadLongLong("Элемент " + std::to_string(index), index + 1);
    }
    return values;
}

Ordinal ReadOrdinal(const std::string& prompt) {
    std::cout << prompt << '\n';
    const int omegaBlocks = ReadInt("omega-блоков", 0, 0);
    const int finiteOffset = ReadInt("конечное смещение", 0, 0);
    return Ordinal(static_cast<std::size_t>(omegaBlocks), static_cast<std::size_t>(finiteOffset));
}

LazySequence<long long> MakeNaturals() {
    return LazySequence<long long>(Ordinal::Omega(), [](const Ordinal& index) {
        return static_cast<long long>(index.Value());
    });
}

LazySequence<long long> MakeFibonacci() {
    Buffer<long long> initialValues(2);
    initialValues[0] = 0;
    initialValues[1] = 1;

    return LazySequence<long long>(
        initialValues,
        Ordinal::Omega(),
        [](const Ordinal& index, const auto& previous) {
            const std::size_t finiteIndex = index.Value();
            return previous(Ordinal::Finite(finiteIndex - 1))
                + previous(Ordinal::Finite(finiteIndex - 2));
        });
}

LazySequence<long long> MakeConstant(long long value) {
    return LazySequence<long long>(Ordinal::Omega(), [value](const Ordinal&) {
        return value;
    });
}

LazySequence<long long> ChooseSequence() {
    PrintDivider();
    std::cout << "Выберите исходную LazySequence\n"
              << "1. Конечная, значения вводятся вручную\n"
              << "2. Naturals: 0, 1, 2, ...\n"
              << "3. Fibonacci\n"
              << "4. Бесконечная константа\n"
              << "5. Naturals + конечный хвост\n";

    switch (ReadInt("Пункт", 1, 2)) {
    case 1:
        return LazySequence<long long>(ReadFiniteValues());
    case 2:
        return MakeNaturals();
    case 3:
        return MakeFibonacci();
    case 4:
        return MakeConstant(ReadLongLong("Значение", 7));
    case 5: {
        std::cout << "Введите конечный хвост, который будет после omega.\n";
        return MakeNaturals().concat(LazySequence<long long>(ReadFiniteValues()));
    }
    default:
        std::cout << "Неизвестный пункт, выбраны Naturals.\n";
        return MakeNaturals();
    }
}

void PrintLength(const LazySequence<long long>& sequence) {
    const Length length = sequence.GetLength();
    std::cout << "Длина: ";
    if (!length.IsKnown()) {
        std::cout << "unknown\n";
        return;
    }
    std::cout << length.Value().ToString() << '\n';
}

void PrintPrefix(const LazySequence<long long>& sequence) {
    const int count = ReadInt("Сколько первых элементов вывести", 0, 10);
    for (int index = 0; index < count; ++index) {
        if (index > 0) {
            std::cout << ' ';
        }
        std::cout << sequence.Get(static_cast<std::size_t>(index));
    }
    std::cout << '\n';
}

void PrintGet(const LazySequence<long long>& sequence) {
    const Ordinal index = ReadOrdinal("Индекс элемента");
    std::cout << "value = " << sequence.Get(index) << '\n';
}

void ApplyAppend(LazySequence<long long>& sequence) {
    sequence = sequence.append(ReadLongLong("Значение для append", 100));
    std::cout << "append применен.\n";
}

void ApplyPrepend(LazySequence<long long>& sequence) {
    sequence = sequence.prepend(ReadLongLong("Значение для prepend", -1));
    std::cout << "prepend применен.\n";
}

void ApplyConcat(LazySequence<long long>& sequence) {
    std::cout << "Введите конечную последовательность для concat.\n";
    sequence = sequence.concat(LazySequence<long long>(ReadFiniteValues()));
    std::cout << "concat применен.\n";
}

void ApplySlice(LazySequence<long long>& sequence) {
    const int start = ReadInt("start index", 0, 0);
    const int end = ReadInt("end index включительно", start, start);
    sequence = sequence.slice(static_cast<std::size_t>(start), static_cast<std::size_t>(end));
    std::cout << "slice применен.\n";
}

void ApplyMap(LazySequence<long long>& sequence) {
    std::cout << "Map\n"
              << "1. x * 2\n"
              << "2. x * x\n"
              << "3. x + c\n";
    const int command = ReadInt("Пункт", 1, 1);
    if (command == 2) {
        sequence = sequence.map<long long>([](const long long& value) {
            return value * value;
        });
    } else if (command == 3) {
        const long long delta = ReadLongLong("c", 10);
        sequence = sequence.map<long long>([delta](const long long& value) {
            return value + delta;
        });
    } else {
        sequence = sequence.map<long long>([](const long long& value) {
            return value * 2;
        });
    }
    std::cout << "map применен.\n";
}

void ApplyWhere(LazySequence<long long>& sequence) {
    std::cout << "Where\n"
              << "1. even\n"
              << "2. odd\n"
              << "3. positive\n"
              << "4. non-zero\n";
    const int command = ReadInt("Пункт", 1, 1);
    if (command == 2) {
        sequence = sequence.where([](const long long& value) {
            return value % 2 != 0;
        });
    } else if (command == 3) {
        sequence = sequence.where([](const long long& value) {
            return value > 0;
        });
    } else if (command == 4) {
        sequence = sequence.where([](const long long& value) {
            return value != 0;
        });
    } else {
        sequence = sequence.where([](const long long& value) {
            return value % 2 == 0;
        });
    }
    std::cout << "where применен.\n";
}

void ApplyInsert(LazySequence<long long>& sequence) {
    const long long value = ReadLongLong("Значение", 42);
    const Ordinal index = ReadOrdinal("Индекс вставки");
    sequence = sequence.insertAt(value, index);
    std::cout << "insertAt применен.\n";
}

void PrintZipWithNaturals(const LazySequence<long long>& sequence) {
    auto zipped = sequence.zip(MakeNaturals());
    const int count = ReadInt("Сколько пар вывести", 0, 5);
    for (int index = 0; index < count; ++index) {
        const auto value = zipped.Get(static_cast<std::size_t>(index));
        std::cout << index << ": (" << value.first << ", " << value.second << ")\n";
    }
}

void PrintStatisticsPrefix(const LazySequence<long long>& sequence) {
    const int count = ReadInt("Сколько элементов обработать статистикой", 0, 5);
    LazyReader<long long> reader(sequence);
    StatisticsStream<long long> statistics(reader);

    statistics.Open();
    for (int index = 0; index < count && !statistics.IsEndOfStream(); ++index) {
        const auto snapshot = statistics.Read();
        std::cout << "n=" << snapshot.count
                  << " min=" << snapshot.minimum
                  << " max=" << snapshot.maximum
                  << " mean=" << static_cast<double>(snapshot.mean)
                  << " median=" << static_cast<double>(snapshot.median)
                  << '\n';
    }
    statistics.Close();
}

void PrintGeneratorState(const Generator<long long>& generator, const LazySequence<long long>& sequence) {
    std::cout << "position = " << generator.GetPosition().ToString()
              << ", materialized = " << sequence.GetMaterializedCount()
              << '\n';
}

void RunGeneratorMode(const LazySequence<long long>& sequence) {
    Generator<long long> generator(sequence);

    while (true) {
        try {
            PrintDivider();
            std::cout << "Generator mode\n";
            PrintGeneratorState(generator, sequence);
            std::cout << "1. HasNext()\n"
                      << "2. GetNext()\n"
                      << "3. TryGetNext()\n"
                      << "4. Показать состояние\n"
                      << "0. Назад\n";

            const int command = ReadInt("Пункт", 0, 1);
            switch (command) {
            case 1:
                std::cout << (generator.HasNext() ? "true" : "false") << '\n';
                break;
            case 2:
                std::cout << "value = " << generator.GetNext() << '\n';
                break;
            case 3: {
                const Optional<long long> value = generator.TryGetNext();
                if (value.has_value()) {
                    std::cout << "value = " << *value << '\n';
                } else {
                    std::cout << "empty\n";
                }
                break;
            }
            case 4:
                PrintGeneratorState(generator, sequence);
                break;
            case 0:
                return;
            default:
                std::cout << "Неизвестный пункт.\n";
                break;
            }
        } catch (const std::exception& exception) {
            std::cout << "Ошибка: " << exception.what() << '\n';
        }
    }
}

void PrintOperationMenu() {
    PrintDivider();
    std::cout << "Операции над текущей последовательностью\n"
              << "1. Показать длину\n"
              << "2. Вывести первые n элементов\n"
              << "3. Get по ordinal-индексу\n"
              << "4. append(value)\n"
              << "5. prepend(value)\n"
              << "6. concat(конечная последовательность)\n"
              << "7. slice(start, end)\n"
              << "8. map(...)\n"
              << "9. where(...)\n"
              << "10. insertAt(value, index)\n"
              << "11. zip с Naturals и вывести пары\n"
              << "12. статистика первых n элементов\n"
              << "13. выбрать другую исходную sequence\n"
              << "14. Generator mode\n"
              << "0. Выход\n";
}

int main() {
    LazySequence<long long> sequence = ChooseSequence();

    while (true) {
        try {
            PrintOperationMenu();
            const int command = ReadInt("Пункт", 0, 2);
            switch (command) {
            case 1:
                PrintLength(sequence);
                break;
            case 2:
                PrintPrefix(sequence);
                break;
            case 3:
                PrintGet(sequence);
                break;
            case 4:
                ApplyAppend(sequence);
                break;
            case 5:
                ApplyPrepend(sequence);
                break;
            case 6:
                ApplyConcat(sequence);
                break;
            case 7:
                ApplySlice(sequence);
                break;
            case 8:
                ApplyMap(sequence);
                break;
            case 9:
                ApplyWhere(sequence);
                break;
            case 10:
                ApplyInsert(sequence);
                break;
            case 11:
                PrintZipWithNaturals(sequence);
                break;
            case 12:
                PrintStatisticsPrefix(sequence);
                break;
            case 13:
                sequence = ChooseSequence();
                break;
            case 14:
                RunGeneratorMode(sequence);
                break;
            case 0:
                return 0;
            default:
                std::cout << "Неизвестный пункт.\n";
                break;
            }
        } catch (const std::exception& exception) {
            std::cout << "Ошибка: " << exception.what() << '\n';
        }
    }
}
