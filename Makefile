CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -I.

BUILD_DIR = build
APP = $(BUILD_DIR)/lazy_sequence_app
TESTS = $(BUILD_DIR)/lazy_sequence_tests
HEADERS = $(wildcard common/*.hpp lazy/*.hpp sequence/*.hpp statistics/*.hpp stream/*.hpp)

.PHONY: all app tests test run clean

all: app tests

app: $(APP)

tests: $(TESTS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(APP): main.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) main.cpp -o $(APP)

$(TESTS): tests/test_main.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) tests/test_main.cpp -o $(TESTS)

test: $(TESTS)
	$(TESTS)
	rm -f test_numbers.txt

run: $(APP)
	$(APP)

clean:
	rm -rf $(BUILD_DIR) test_numbers.txt
