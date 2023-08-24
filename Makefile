# Compiler settings
CC := clang
CFLAGS := -g -Iinclude
LDLIBS :=

# Source file directories
INCLUDE_DIR := include
SRC_DIR := src
TEST_DIR := tests

# Source files
TCP_SRCS := $(wildcard $(SRC_DIR)/tcp/*.c)
UDP_SRCS := $(wildcard $(SRC_DIR)/udp/*.c)
HTTP_SRCS := $(wildcard $(SRC_DIR)/http/*.c)
UTILS_SRCS := $(wildcard $(SRC_DIR)/utils/*.c)
COMMON_SRC := $(wildcard $(SRC_DIR)/socket.c)

# Test files
TEST_TCP_SRCS := $(wildcard $(TEST_DIR)/tcp/*.c)
TEST_UDP_SRCS := $(wildcard $(TEST_DIR)/udp/*.c)

MAIN_SRCS := $(wildcard main.c)

# Output binary
OUTPUT := netc

all: $(OUTPUT)

$(OUTPUT): $(COMMON_SRC) $(TCP_SRCS) $(UDP_SRCS) $(HTTP_SRCS) $(UTILS_SRCS) $(TEST_TCP_SRCS) $(TEST_UDP_SRCS) $(MAIN_SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(OUTPUT)