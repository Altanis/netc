# Compiler settings
CC := clang
CFLAGS := -Iinclude -Ofast
LDLIBS :=

# Source file directories
INCLUDE_DIR := include
SRC_DIR := src
TEST_DIR := tests

# Source files
TCP_SRCS := $(wildcard $(SRC_DIR)/tcp/*.c)
HTTP_SRCS := $(wildcard $(SRC_DIR)/http/*.c)
UTILS_SRCS := $(wildcard $(SRC_DIR)/utils/*.c)

# Test files
TEST_TCP_SRCS := $(wildcard $(TEST_DIR)/tcp/*.c)

MAIN_SRCS := $(wildcard main.c)

# Output binary
OUTPUT := netc

all: $(OUTPUT)

$(OUTPUT): $(TCP_SRCS) $(UTILS_SRCS) $(TEST_TCP_SRCS) $(MAIN_SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(OUTPUT)
