# Compiler settings
CC := clang
CFLAGS := -g
LDLIBS :=

# Source file directories
INCLUDE_DIR := include
SRC_DIR := src
TEST_DIR := tests

# Source files
TCP_SRCS := $(wildcard $(SRC_DIR)/tcp/*.c)
UDP_SRCS := $(wildcard $(SRC_DIR)/udp/*.c)
HTTP_HELPERS_SRCS := $(wildcard $(SRC_DIR)/http/*.c)
WS_HELPER_SRCS := $(wildcard $(SRC_DIR)/ws/*.c)
WEB_SRCS := $(wildcard $(SRC_DIR)/web/*.c)
UTILS_SRCS := $(wildcard $(SRC_DIR)/utils/*.c)
COMMON_SRC := $(wildcard $(SRC_DIR)/socket.c)

# Test files
TEST_HTTP_SRCS := $(wildcard $(TEST_DIR)/http/*.c)
TEST_TCP_SRCS := $(wildcard $(TEST_DIR)/tcp/*.c)
TEST_UDP_SRCS := $(wildcard $(TEST_DIR)/udp/*.c)

MAIN_SRCS := $(wildcard main.c)

# Output binary
OUTPUT := netc

all: $(OUTPUT)

$(OUTPUT): $(COMMON_SRC) $(TCP_SRCS) $(UDP_SRCS) $(HTTP_HELPERS_SRCS) $(WS_HELPER_SRCS) $(WEB_SRCS) $(UTILS_SRCS) $(TEST_HTTP_SRCS) $(TEST_TCP_SRCS) $(TEST_UDP_SRCS) $(MAIN_SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(OUTPUT)