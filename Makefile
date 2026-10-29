CXX ?= clang++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -pedantic -O2 -Iinclude -MMD -MP
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

SRCS = $(SRC_DIR)/lexer.cpp \
       $(SRC_DIR)/ast.cpp \
       $(SRC_DIR)/parser.cpp \
       $(SRC_DIR)/typeck.cpp \
       $(SRC_DIR)/monomorphize.cpp \
       $(SRC_DIR)/cfg.cpp \
       $(SRC_DIR)/borrowck.cpp \
       $(SRC_DIR)/optimizer.cpp \
       $(SRC_DIR)/codegen_c.cpp \
       $(SRC_DIR)/aerospace.cpp \
       $(SRC_DIR)/main.cpp

OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)
TARGET = $(BIN_DIR)/oxide

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) a.out.c *.dSYM

test: $(TARGET)
	python3 tests/test_runner.py
